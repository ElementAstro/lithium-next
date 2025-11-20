#include "reader.hpp"

#include <algorithm>
#include <cctype>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <spdlog/spdlog.h>
#include "atom/error/exception.hpp"
#include "atom/utils/string.hpp"

namespace lithium::target {
// Dialect constructor implementation
Dialect::Dialect(char delim, char quote, bool dquote, bool skipspace,
                 std::string lineterm, Quoting quote_mode)
    : delimiter(delim),
      quotechar(quote),
      doublequote(dquote),
      skip_initial_space(skipspace),
      lineterminator(std::move(lineterm)),
      quoting(quote_mode) {}

// DictReader implementation
class DictReader::Impl {
public:
    Impl(std::istream& input, const std::vector<std::string>& fieldnames,
         Dialect dialect, Encoding encoding)
        : dialect_(std::move(dialect)),
          fieldnames_(fieldnames),
          input_(input),
          encoding_(encoding),
          buffer_(dialect_.buffer_size),
          line_number_(0),
          last_error_(CSVError::NONE) {
        try {
            validateAndInitialize();
        } catch (const std::exception& e) {
            last_error_ = CSVError::INVALID_FORMAT;
            if (!dialect_.ignore_errors) {
                throw;
            }
        }
    }

    auto next(std::unordered_map<std::string, std::string>& row) -> bool {
        try {
            if (!readNextLine()) {
                return false;
            }

            processLine(row);
            updateProgress();
            return true;
        } catch (const std::exception& e) {
            handleError(e);
            return dialect_.ignore_errors;
        }
    }

    void setProgressCallback(ProgressCallback callback) {
        progress_callback_ = std::move(callback);
    }

    [[nodiscard]] auto getLastError() const -> CSVError { return last_error_; }

    void enableFieldValidation(bool enable) { validate_fields_ = enable; }

    [[nodiscard]] auto getLineNumber() const -> size_t { return line_number_; }

    auto skipHeader() -> bool {
        if (fieldnames_.empty()) {
            return false;
        }
        return readNextLine();
    }

    auto detectEncoding() -> bool {
        // Read first 4 bytes to detect BOM
        char bom[4];
        input_.read(bom, 4);
        input_.seekg(0);

        // UTF-8 with BOM
        if ((unsigned char)bom[0] == 0xEF && (unsigned char)bom[1] == 0xBB &&
            (unsigned char)bom[2] == 0xBF) {
            encoding_ = Encoding::UTF8;
            input_.seekg(3);  // Skip BOM
            return true;
        }

        // UTF-16 LE
        if ((unsigned char)bom[0] == 0xFF && (unsigned char)bom[1] == 0xFE) {
            encoding_ = Encoding::UTF16;
            input_.seekg(2);  // Skip BOM
            return true;
        }

        // UTF-16 BE
        if ((unsigned char)bom[0] == 0xFE && (unsigned char)bom[1] == 0xFF) {
            encoding_ = Encoding::UTF16BE;
            input_.seekg(2);  // Skip BOM
            return true;
        }

        // UTF-32 LE
        if (bom[0] == 0xFF && bom[1] == 0xFE && bom[2] == 0x00 &&
            bom[3] == 0x00) {
            THROW_RUNTIME_ERROR("UTF-32 encoding is not supported");
        }

        // No BOM found, use default encoding
        return encoding_ == Encoding::UTF8;
    }

    void reset() {
        input_.clear();
        input_.seekg(0);
        line_number_ = 0;
        last_error_ = CSVError::NONE;
        if (!fieldnames_.empty()) {
            skipHeader();
        }
    }

    auto readNextLine() -> bool {
        if (std::getline(input_, current_line_)) {
            ++line_number_;
            // Handle different OS line endings
            if (!current_line_.empty() && current_line_.back() == '\r') {
                current_line_.pop_back();
            }
            return true;
        }
        return false;
    }

private:
    std::vector<char> buffer_;
    size_t line_number_;
    CSVError last_error_;
    ProgressCallback progress_callback_;
    bool validate_fields_{true};

    void validateAndInitialize() {
        if (fieldnames_.empty()) {
            THROW_INVALID_ARGUMENT("Field names cannot be empty");
        }

        // Pre-warm buffer for improved IO performance
        input_.rdbuf()->pubsetbuf(buffer_.data(), buffer_.size());

        // Detect file encoding and BOM
        detectEncoding();

        if (!fieldnames_.empty()) {
            skipHeader();
        }
    }

    void processLine(std::unordered_map<std::string, std::string>& row) {
        std::vector<std::string> parsedFields = parseLine(current_line_);

        if (validate_fields_ && parsedFields.size() != fieldnames_.size()) {
            THROW_RUNTIME_ERROR("Field count mismatch");
        }

        row.clear();
        row.reserve(fieldnames_.size());

        for (size_t i = 0; i < fieldnames_.size(); ++i) {
            row[fieldnames_[i]] =
                i < parsedFields.size() ? std::move(parsedFields[i]) : "";
        }
    }

    void handleError(const std::exception& e) {
        last_error_ = CSVError::INVALID_FORMAT;
        spdlog::error("Error processing line {}: {}", line_number_, e.what());

        if (!dialect_.ignore_errors) {
            throw;
        }
    }

    void updateProgress() {
        if (progress_callback_) {
            auto current_pos = input_.tellg();
            input_.seekg(0, std::ios::end);
            auto total_size = input_.tellg();
            input_.seekg(current_pos);

            progress_callback_(current_pos, total_size);
        }
    }

    auto detectDialect(std::istream& input) -> bool {
        // Simple detection of delimiter and quote character
        std::string line;
        if (std::getline(input, line)) {
            size_t comma = std::count(line.begin(), line.end(), ',');
            size_t semicolon = std::count(line.begin(), line.end(), ';');
            delimiter_ = (semicolon > comma) ? ';' : ',';
            dialect_.delimiter = delimiter_;
            // Check for quotes usage
            size_t quoteCount =
                std::count(line.begin(), line.end(), dialect_.quotechar);
            dialect_.quoting = (quoteCount > 0) ? Quoting::ALL : Quoting::NONE;
            // Reset stream position
            input.clear();
            input.seekg(0, std::ios::beg);
            return true;
        }
        return false;
    }

    [[nodiscard]] auto parseLine(const std::string& line) const
        -> std::vector<std::string> {
        std::vector<std::string> result;
        std::string cell;
        bool insideQuotes = false;

        for (char ch : line) {
            if (ch == dialect_.quotechar) {
                if (dialect_.doublequote) {
                    if (insideQuotes && !cell.empty() &&
                        cell.back() == dialect_.quotechar) {
                        cell.pop_back();
                        cell += ch;
                        continue;
                    }
                }
                insideQuotes = !insideQuotes;
            } else if (ch == dialect_.delimiter && !insideQuotes) {
                result.push_back(atom::utils::trim(cell));
                cell.clear();
            } else {
                cell += ch;
            }
        }
        result.push_back(atom::utils::trim(cell));
        return result;
    }

    Dialect dialect_;
    std::vector<std::string> fieldnames_;
    std::istream& input_;
    std::string current_line_;
    Encoding encoding_;
    char delimiter_;
};

DictReader::DictReader(std::istream& input,
                       const std::vector<std::string>& fieldnames,
                       Dialect dialect, Encoding encoding)
    : impl_(std::make_unique<Impl>(input, fieldnames, std::move(dialect),
                                   encoding)) {}

DictReader::~DictReader() = default;

bool DictReader::next(std::unordered_map<std::string, std::string>& row) {
    return impl_->next(row);
}

void DictReader::setProgressCallback(ProgressCallback callback) {
    impl_->setProgressCallback(std::move(callback));
}

auto DictReader::getLastError() const -> CSVError {
    return impl_->getLastError();
}

void DictReader::enableFieldValidation(bool enable) {
    impl_->enableFieldValidation(enable);
}

auto DictReader::getLineNumber() const -> size_t {
    return impl_->getLineNumber();
}

void DictReader::reset() { impl_->reset(); }

auto DictReader::readRows(size_t count)
    -> std::vector<std::unordered_map<std::string, std::string>> {
    std::vector<std::unordered_map<std::string, std::string>> results;
    results.reserve(count);

    std::unordered_map<std::string, std::string> row;
    size_t readCount = 0;

    while (readCount < count && next(row)) {
        results.push_back(row);
        ++readCount;
    }

    return results;
}

// DictWriter implementation
class DictWriter::Impl {
public:
    Impl(std::ostream& output, const std::vector<std::string>& fieldnames,
         Dialect dialect, bool quote_all, Encoding encoding)
        : dialect_(std::move(dialect)),
          fieldnames_(fieldnames),
          output_(output),
          quote_all_(quote_all),
          encoding_(encoding) {
        writeHeader();
    }

    void writeRow(const std::unordered_map<std::string, std::string>& row) {
        std::vector<std::string> outputRow;
        outputRow.reserve(fieldnames_.size());

        for (const auto& fieldname : fieldnames_) {
            auto it = row.find(fieldname);
            if (it != row.end()) {
                outputRow.push_back(escape(it->second));
            } else {
                outputRow.emplace_back("");
            }
        }
        writeLine(outputRow);
        written_rows_++;
        updateProgress();
    }

    void setProgressCallback(ProgressCallback callback) {
        progress_callback_ = std::move(callback);
    }

    void enableChecksum(bool enable) { checksum_enabled_ = enable; }

    void flush() { output_.flush(); }

    [[nodiscard]] auto getWrittenRows() const -> size_t {
        return written_rows_;
    }

private:
    void writeHeader() {
        writeLine(fieldnames_);
        written_rows_++;
    }

    void writeLine(const std::vector<std::string>& line) {
        for (size_t i = 0; i < line.size(); ++i) {
            if (i > 0) {
                output_ << dialect_.delimiter;
            }

            std::string field = line[i];
            if (quote_all_ || needsQuotes(field)) {
                field.insert(field.begin(), dialect_.quotechar);
                field.push_back(dialect_.quotechar);
            }

            if (encoding_ == Encoding::UTF16) {
                spdlog::warn("UTF-16 encoding output not fully implemented");
            }

            output_ << field;

            if (checksum_enabled_) {
                updateChecksum(field);
            }
        }
        output_ << dialect_.lineterminator;
    }

    [[nodiscard]] auto needsQuotes(const std::string& field) const -> bool {
        return field.contains(dialect_.delimiter) ||
               field.contains(dialect_.quotechar) || field.contains('\n');
    }

    [[nodiscard]] auto escape(const std::string& field) const -> std::string {
        if (dialect_.quoting == Quoting::ALL || needsQuotes(field)) {
            std::string escaped = field;
            if (dialect_.doublequote) {
                size_t pos = 0;
                while ((pos = escaped.find(dialect_.quotechar, pos)) !=
                       std::string::npos) {
                    escaped.insert(pos, 1, dialect_.quotechar);
                    pos += 2;
                }
            }
            return escaped;
        }
        return field;
    }

    ProgressCallback progress_callback_;
    bool checksum_enabled_{false};
    size_t written_rows_{0};
    uint32_t checksum_{0};

    void updateChecksum(const std::string& data) {
        if (checksum_enabled_) {
            for (unsigned char c : data) {
                checksum_ = (checksum_ << 8) ^ c;
            }
        }
    }

    void updateProgress() {
        if (progress_callback_) {
            progress_callback_(written_rows_, 0);  // 0 indicates unknown total
        }
    }

    Dialect dialect_;
    std::vector<std::string> fieldnames_;
    std::ostream& output_;
    bool quote_all_;
    Encoding encoding_;
};

DictWriter::DictWriter(std::ostream& output,
                       const std::vector<std::string>& fieldnames,
                       Dialect dialect, bool quote_all, Encoding encoding)
    : impl_(std::make_unique<Impl>(output, fieldnames, std::move(dialect),
                                   quote_all, encoding)) {}

DictWriter::~DictWriter() = default;

void DictWriter::writeRow(
    const std::unordered_map<std::string, std::string>& row) {
    impl_->writeRow(row);
}

void DictWriter::setProgressCallback(ProgressCallback callback) {
    impl_->setProgressCallback(std::move(callback));
}

void DictWriter::enableChecksum(bool enable) { impl_->enableChecksum(enable); }

void DictWriter::flush() { impl_->flush(); }

auto DictWriter::getWrittenRows() const -> size_t {
    return impl_->getWrittenRows();
}

void DictWriter::writeRows(
    const std::vector<std::unordered_map<std::string, std::string>>& rows) {
    for (const auto& row : rows) {
        writeRow(row);
    }
    flush();
}
}  // namespace lithium::target