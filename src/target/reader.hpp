#ifndef LITHIUM_TARGET_READER_CSV
#define LITHIUM_TARGET_READER_CSV

#include <functional>
#include <istream>
#include <memory>
#include <ostream>
#include <string>
#include <unordered_map>
#include <vector>

namespace lithium::target {

/**
 * @brief Supported character encodings for CSV files
 */
enum class Encoding { UTF8, UTF16, UTF16LE, UTF16BE, GBK, ASCII };

/**
 * @brief Error types that can occur during CSV operations
 */
enum class CSVError {
    NONE,            ///< No error
    INVALID_FORMAT,  ///< CSV format is invalid
    ENCODING_ERROR,  ///< Encoding conversion error
    IO_ERROR,        ///< Input/output error
    FIELD_MISMATCH   ///< Field count doesn't match headers
};

/**
 * @brief Callback function type for progress reporting
 * @param current Current position or row number
 * @param total Total size or number of rows (0 if unknown)
 */
using ProgressCallback = std::function<void(size_t current, size_t total)>;

/**
 * @brief Quoting policy for CSV fields
 */
enum class Quoting {
    MINIMAL,     ///< Quote only fields that need it
    ALL,         ///< Quote all fields
    NONNUMERIC,  ///< Quote non-numeric fields
    STRINGS,     ///< Quote string fields
    NOTNULL,     ///< Quote non-null fields
    NONE         ///< Never quote fields
};

/**
 * @brief Configuration for CSV dialect
 */
struct Dialect {
    char delimiter = ',';                ///< Field separator character
    char quotechar = '"';                ///< Quote character
    bool doublequote = true;             ///< Whether to double quote characters
    bool skip_initial_space = false;     ///< Skip spaces after delimiter
    std::string lineterminator = "\n";   ///< Line ending string
    Quoting quoting = Quoting::MINIMAL;  ///< Quoting policy

    // Performance and validation settings
    size_t buffer_size = 8192;    ///< I/O buffer size
    bool validate_fields = true;  ///< Validate field count
    bool ignore_errors = false;   ///< Continue despite errors

    /**
     * @brief Default constructor
     */
    Dialect() = default;

    /**
     * @brief Parameterized constructor
     *
     * @param delim Field delimiter character
     * @param quote Quote character
     * @param dquote Whether to double quotes in fields
     * @param skipspace Whether to skip spaces after delimiter
     * @param lineterm Line terminator string
     * @param quote_mode Quoting policy
     */
    Dialect(char delim, char quote, bool dquote, bool skipspace,
            std::string lineterm, Quoting quote_mode);
};

/**
 * @brief CSV reader that returns rows as dictionaries
 */
class DictReader {
public:
    /**
     * @brief Construct a new Dict Reader object
     *
     * @param input Input stream to read from
     * @param fieldnames Column names for the CSV
     * @param dialect CSV dialect configuration
     * @param encoding Character encoding of the input
     */
    DictReader(std::istream& input, const std::vector<std::string>& fieldnames,
               Dialect dialect = Dialect(), Encoding encoding = Encoding::UTF8);

    /**
     * @brief Destroy the Dict Reader object
     */
    ~DictReader();

    /**
     * @brief Read the next row from the CSV file
     *
     * @param row Map to store the next row
     * @return true if a row was read successfully
     * @return false if end of file or unrecoverable error
     */
    bool next(std::unordered_map<std::string, std::string>& row);

    /**
     * @brief Set progress reporting callback
     *
     * @param callback Function to call with progress updates
     */
    void setProgressCallback(ProgressCallback callback);

    /**
     * @brief Get the last error that occurred
     *
     * @return The last error code
     */
    [[nodiscard]] auto getLastError() const -> CSVError;

    /**
     * @brief Enable or disable field count validation
     *
     * @param enable True to validate field counts match header
     */
    void enableFieldValidation(bool enable);

    /**
     * @brief Get the current line number being processed
     *
     * @return Current line number
     */
    [[nodiscard]] auto getLineNumber() const -> size_t;

    /**
     * @brief Reset the reader to the beginning of the input
     */
    void reset();

    /**
     * @brief Read multiple rows at once
     *
     * @param count Maximum number of rows to read
     * @return Vector of row dictionaries
     */
    auto readRows(size_t count)
        -> std::vector<std::unordered_map<std::string, std::string>>;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

/**
 * @brief CSV writer that takes rows as dictionaries
 */
class DictWriter {
public:
    /**
     * @brief Construct a new Dict Writer object
     *
     * @param output Output stream to write to
     * @param fieldnames Column names for the CSV
     * @param dialect CSV dialect configuration
     * @param quote_all Whether to quote all fields
     * @param encoding Character encoding for the output
     */
    DictWriter(std::ostream& output, const std::vector<std::string>& fieldnames,
               Dialect dialect = Dialect(), bool quote_all = false,
               Encoding encoding = Encoding::UTF8);

    /**
     * @brief Destroy the Dict Writer object
     */
    ~DictWriter();

    /**
     * @brief Write a single row to the CSV file
     *
     * @param row Dictionary containing the field values
     */
    void writeRow(const std::unordered_map<std::string, std::string>& row);

    /**
     * @brief Set progress reporting callback
     *
     * @param callback Function to call with progress updates
     */
    void setProgressCallback(ProgressCallback callback);

    /**
     * @brief Enable or disable checksum calculation
     *
     * @param enable True to calculate checksums
     */
    void enableChecksum(bool enable);

    /**
     * @brief Flush the output stream
     */
    void flush();

    /**
     * @brief Get the number of rows written
     *
     * @return Number of rows written so far
     */
    [[nodiscard]] auto getWrittenRows() const -> size_t;

    /**
     * @brief Write multiple rows at once
     *
     * @param rows Vector of row dictionaries to write
     */
    void writeRows(
        const std::vector<std::unordered_map<std::string, std::string>>& rows);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace lithium::target

#endif  // LITHIUM_TARGET_READER_CSV