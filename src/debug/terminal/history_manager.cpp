/**
 * @file history_manager.cpp
 * @brief Implementation of command history manager
 */

#include "history_manager.hpp"

#include <algorithm>
#include <deque>
#include <fstream>
#include <mutex>
#include <regex>
#include <unordered_map>
#include <unordered_set>

#include "atom/type/json.hpp"

namespace lithium::debug::terminal {

class HistoryManager::Impl {
public:
    HistoryConfig config_;
    std::deque<HistoryEntry> entries_;
    size_t currentPos_{0};
    mutable std::mutex mutex_;

    Impl(const HistoryConfig& config) : config_(config) {
        if (!config_.historyFile.empty()) {
            load(config_.historyFile);
        }
    }

    void add(const HistoryEntry& entry) {
        std::lock_guard<std::mutex> lock(mutex_);

        // Check for consecutive duplicates
        if (config_.deduplicateConsecutive && !entries_.empty()) {
            if (entries_.back().command == entry.command) {
                return;
            }
        }

        entries_.push_back(entry);

        // Trim to max size
        while (entries_.size() > config_.maxEntries) {
            entries_.pop_front();
        }

        // Reset navigation position
        currentPos_ = entries_.size();

        // Auto-save if configured
        if (config_.persistOnAdd && !config_.historyFile.empty()) {
            save(config_.historyFile);
        }
    }

    std::optional<HistoryEntry> get(size_t index) const {
        std::lock_guard<std::mutex> lock(mutex_);

        if (index >= entries_.size()) {
            return std::nullopt;
        }

        return entries_[index];
    }

    std::optional<HistoryEntry> previous() {
        std::lock_guard<std::mutex> lock(mutex_);

        if (entries_.empty() || currentPos_ == 0) {
            return std::nullopt;
        }

        currentPos_--;
        return entries_[currentPos_];
    }

    std::optional<HistoryEntry> next() {
        std::lock_guard<std::mutex> lock(mutex_);

        if (currentPos_ >= entries_.size()) {
            return std::nullopt;
        }

        currentPos_++;

        if (currentPos_ >= entries_.size()) {
            return std::nullopt;
        }

        return entries_[currentPos_];
    }

    std::vector<HistoryEntry> search(
        const std::string& pattern, const HistorySearchOptions& options) const {
        std::lock_guard<std::mutex> lock(mutex_);

        std::vector<HistoryEntry> results;

        std::regex regex;
        if (options.regexSearch) {
            try {
                auto flags = std::regex::ECMAScript;
                if (!options.caseSensitive) {
                    flags |= std::regex::icase;
                }
                regex = std::regex(pattern, flags);
            } catch (const std::regex_error&) {
                return results;
            }
        }

        auto matchEntry = [&](const HistoryEntry& entry) -> bool {
            // Time filters
            if (options.afterTime && entry.timestamp < *options.afterTime) {
                return false;
            }
            if (options.beforeTime && entry.timestamp > *options.beforeTime) {
                return false;
            }

            // Tag filter
            if (!options.tags.empty()) {
                bool hasTag = false;
                for (const auto& tag : options.tags) {
                    if (std::find(entry.tags.begin(), entry.tags.end(), tag) !=
                        entry.tags.end()) {
                        hasTag = true;
                        break;
                    }
                }
                if (!hasTag)
                    return false;
            }

            // Pattern matching
            if (options.regexSearch) {
                return std::regex_search(entry.command, regex);
            } else if (options.prefixMatch) {
                if (options.caseSensitive) {
                    return entry.command.find(pattern) == 0;
                } else {
                    std::string lowerCmd = entry.command;
                    std::string lowerPattern = pattern;
                    std::transform(lowerCmd.begin(), lowerCmd.end(),
                                   lowerCmd.begin(), ::tolower);
                    std::transform(lowerPattern.begin(), lowerPattern.end(),
                                   lowerPattern.begin(), ::tolower);
                    return lowerCmd.find(lowerPattern) == 0;
                }
            } else {
                if (options.caseSensitive) {
                    return entry.command.find(pattern) != std::string::npos;
                } else {
                    std::string lowerCmd = entry.command;
                    std::string lowerPattern = pattern;
                    std::transform(lowerCmd.begin(), lowerCmd.end(),
                                   lowerCmd.begin(), ::tolower);
                    std::transform(lowerPattern.begin(), lowerPattern.end(),
                                   lowerPattern.begin(), ::tolower);
                    return lowerCmd.find(lowerPattern) != std::string::npos;
                }
            }
        };

        if (options.reverseOrder) {
            for (auto it = entries_.rbegin();
                 it != entries_.rend() && results.size() < options.maxResults;
                 ++it) {
                if (matchEntry(*it)) {
                    results.push_back(*it);
                }
            }
        } else {
            for (const auto& entry : entries_) {
                if (results.size() >= options.maxResults)
                    break;
                if (matchEntry(entry)) {
                    results.push_back(entry);
                }
            }
        }

        return results;
    }

    std::optional<HistoryEntry> reverseSearch(const std::string& pattern,
                                              size_t startPos) const {
        std::lock_guard<std::mutex> lock(mutex_);

        if (entries_.empty()) {
            return std::nullopt;
        }

        size_t pos = (startPos == 0) ? entries_.size() - 1 : startPos - 1;

        while (true) {
            const auto& entry = entries_[pos];

            // Case-insensitive search
            std::string lowerCmd = entry.command;
            std::string lowerPattern = pattern;
            std::transform(lowerCmd.begin(), lowerCmd.end(), lowerCmd.begin(),
                           ::tolower);
            std::transform(lowerPattern.begin(), lowerPattern.end(),
                           lowerPattern.begin(), ::tolower);

            if (lowerCmd.find(lowerPattern) != std::string::npos) {
                return entry;
            }

            if (pos == 0)
                break;
            pos--;
        }

        return std::nullopt;
    }

    bool load(const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open()) {
            return false;
        }

        try {
            nlohmann::json j;
            file >> j;

            entries_.clear();

            for (const auto& item : j["entries"]) {
                HistoryEntry entry;
                entry.command = item["command"].get<std::string>();

                if (item.contains("timestamp")) {
                    auto ts = item["timestamp"].get<int64_t>();
                    entry.timestamp =
                        std::chrono::system_clock::from_time_t(ts);
                }

                if (item.contains("favorite")) {
                    entry.favorite = item["favorite"].get<bool>();
                }

                if (item.contains("tags")) {
                    entry.tags = item["tags"].get<std::vector<std::string>>();
                }

                entries_.push_back(entry);
            }

            currentPos_ = entries_.size();
            return true;
        } catch (const std::exception&) {
            // Try simple line-by-line format
            file.clear();
            file.seekg(0);

            entries_.clear();
            std::string line;
            while (std::getline(file, line)) {
                if (!line.empty()) {
                    HistoryEntry entry;
                    entry.command = line;
                    entry.timestamp = std::chrono::system_clock::now();
                    entries_.push_back(entry);
                }
            }

            currentPos_ = entries_.size();
            return true;
        }
    }

    bool save(const std::string& path) const {
        std::lock_guard<std::mutex> lock(mutex_);

        std::ofstream file(path);
        if (!file.is_open()) {
            return false;
        }

        nlohmann::json j;
        j["entries"] = nlohmann::json::array();

        for (const auto& entry : entries_) {
            nlohmann::json item;
            item["command"] = entry.command;
            item["timestamp"] =
                std::chrono::system_clock::to_time_t(entry.timestamp);
            item["favorite"] = entry.favorite;
            item["tags"] = entry.tags;
            j["entries"].push_back(item);
        }

        file << j.dump(2);
        return true;
    }

    std::string exportJson() const {
        std::lock_guard<std::mutex> lock(mutex_);

        nlohmann::json j;
        j["entries"] = nlohmann::json::array();

        for (const auto& entry : entries_) {
            nlohmann::json item;
            item["command"] = entry.command;
            item["timestamp"] =
                std::chrono::system_clock::to_time_t(entry.timestamp);
            item["favorite"] = entry.favorite;
            item["tags"] = entry.tags;
            j["entries"].push_back(item);
        }

        return j.dump(2);
    }

    HistoryStats getStats() const {
        std::lock_guard<std::mutex> lock(mutex_);

        HistoryStats stats;
        stats.totalEntries = entries_.size();

        std::unordered_map<std::string, size_t> commandCounts;
        std::unordered_set<std::string> uniqueCommands;

        for (const auto& entry : entries_) {
            uniqueCommands.insert(entry.command);
            commandCounts[entry.command]++;

            if (entry.favorite) {
                stats.favoriteCount++;
            }
        }

        stats.uniqueCommands = uniqueCommands.size();

        if (!entries_.empty()) {
            stats.oldestEntry = entries_.front().timestamp;
            stats.newestEntry = entries_.back().timestamp;
        }

        // Get top commands
        std::vector<std::pair<std::string, size_t>> sorted(
            commandCounts.begin(), commandCounts.end());
        std::sort(
            sorted.begin(), sorted.end(),
            [](const auto& a, const auto& b) { return a.second > b.second; });

        size_t topN = std::min(sorted.size(), size_t(10));
        stats.topCommands.assign(sorted.begin(), sorted.begin() + topN);

        return stats;
    }

    std::vector<std::pair<std::string, size_t>> getCommandFrequency(
        size_t topN) const {
        std::lock_guard<std::mutex> lock(mutex_);

        std::unordered_map<std::string, size_t> counts;
        for (const auto& entry : entries_) {
            counts[entry.command]++;
        }

        std::vector<std::pair<std::string, size_t>> sorted(counts.begin(),
                                                           counts.end());
        std::sort(
            sorted.begin(), sorted.end(),
            [](const auto& a, const auto& b) { return a.second > b.second; });

        if (sorted.size() > topN) {
            sorted.resize(topN);
        }

        return sorted;
    }
};

// HistoryManager implementation

HistoryManager::HistoryManager(const HistoryConfig& config)
    : impl_(std::make_unique<Impl>(config)) {}

HistoryManager::~HistoryManager() = default;

HistoryManager::HistoryManager(HistoryManager&&) noexcept = default;
HistoryManager& HistoryManager::operator=(HistoryManager&&) noexcept = default;

void HistoryManager::setConfig(const HistoryConfig& config) {
    impl_->config_ = config;
}

const HistoryConfig& HistoryManager::getConfig() const {
    return impl_->config_;
}

void HistoryManager::add(const std::string& command) {
    HistoryEntry entry;
    entry.command = command;
    entry.timestamp = std::chrono::system_clock::now();
    impl_->add(entry);
}

void HistoryManager::add(const std::string& command,
                         const CommandResult& result) {
    HistoryEntry entry;
    entry.command = command;
    entry.timestamp = std::chrono::system_clock::now();
    entry.result = result;
    impl_->add(entry);
}

void HistoryManager::add(const HistoryEntry& entry) { impl_->add(entry); }

std::optional<HistoryEntry> HistoryManager::get(size_t index) const {
    return impl_->get(index);
}

std::optional<HistoryEntry> HistoryManager::getLast() const {
    if (impl_->entries_.empty()) {
        return std::nullopt;
    }
    return impl_->entries_.back();
}

std::optional<HistoryEntry> HistoryManager::getRelative(int offset) const {
    std::lock_guard<std::mutex> lock(impl_->mutex_);

    int pos = static_cast<int>(impl_->currentPos_) + offset;
    if (pos < 0 || pos >= static_cast<int>(impl_->entries_.size())) {
        return std::nullopt;
    }

    return impl_->entries_[pos];
}

bool HistoryManager::remove(size_t index) {
    std::lock_guard<std::mutex> lock(impl_->mutex_);

    if (index >= impl_->entries_.size()) {
        return false;
    }

    impl_->entries_.erase(impl_->entries_.begin() + index);

    if (impl_->currentPos_ > index) {
        impl_->currentPos_--;
    }

    return true;
}

size_t HistoryManager::removeIf(
    std::function<bool(const HistoryEntry&)> predicate) {
    std::lock_guard<std::mutex> lock(impl_->mutex_);

    size_t removed = 0;
    auto it = impl_->entries_.begin();

    while (it != impl_->entries_.end()) {
        if (predicate(*it)) {
            it = impl_->entries_.erase(it);
            removed++;
        } else {
            ++it;
        }
    }

    impl_->currentPos_ = impl_->entries_.size();
    return removed;
}

void HistoryManager::clear() {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    impl_->entries_.clear();
    impl_->currentPos_ = 0;
}

size_t HistoryManager::size() const {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    return impl_->entries_.size();
}

bool HistoryManager::empty() const {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    return impl_->entries_.empty();
}

std::optional<HistoryEntry> HistoryManager::previous() {
    return impl_->previous();
}

std::optional<HistoryEntry> HistoryManager::next() { return impl_->next(); }

void HistoryManager::resetNavigation() {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    impl_->currentPos_ = impl_->entries_.size();
}

size_t HistoryManager::getPosition() const { return impl_->currentPos_; }

void HistoryManager::setPosition(size_t pos) {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    impl_->currentPos_ = std::min(pos, impl_->entries_.size());
}

std::vector<HistoryEntry> HistoryManager::search(
    const std::string& pattern, const HistorySearchOptions& options) const {
    return impl_->search(pattern, options);
}

std::vector<HistoryEntry> HistoryManager::searchPrefix(
    const std::string& prefix, size_t maxResults) const {
    HistorySearchOptions options;
    options.prefixMatch = true;
    options.maxResults = maxResults;
    return impl_->search(prefix, options);
}

std::optional<HistoryEntry> HistoryManager::reverseSearch(
    const std::string& pattern, size_t startPos) const {
    return impl_->reverseSearch(pattern, startPos);
}

std::vector<std::string> HistoryManager::getMatching(
    const std::string& substring) const {
    auto entries = search(substring);
    std::vector<std::string> commands;
    commands.reserve(entries.size());

    for (const auto& entry : entries) {
        commands.push_back(entry.command);
    }

    return commands;
}

bool HistoryManager::setFavorite(size_t index, bool favorite) {
    std::lock_guard<std::mutex> lock(impl_->mutex_);

    if (index >= impl_->entries_.size()) {
        return false;
    }

    impl_->entries_[index].favorite = favorite;
    return true;
}

bool HistoryManager::toggleFavorite(size_t index) {
    std::lock_guard<std::mutex> lock(impl_->mutex_);

    if (index >= impl_->entries_.size()) {
        return false;
    }

    impl_->entries_[index].favorite = !impl_->entries_[index].favorite;
    return true;
}

std::vector<HistoryEntry> HistoryManager::getFavorites() const {
    std::lock_guard<std::mutex> lock(impl_->mutex_);

    std::vector<HistoryEntry> favorites;
    for (const auto& entry : impl_->entries_) {
        if (entry.favorite) {
            favorites.push_back(entry);
        }
    }

    return favorites;
}

bool HistoryManager::addTag(size_t index, const std::string& tag) {
    std::lock_guard<std::mutex> lock(impl_->mutex_);

    if (index >= impl_->entries_.size()) {
        return false;
    }

    auto& tags = impl_->entries_[index].tags;
    if (std::find(tags.begin(), tags.end(), tag) == tags.end()) {
        tags.push_back(tag);
    }

    return true;
}

bool HistoryManager::removeTag(size_t index, const std::string& tag) {
    std::lock_guard<std::mutex> lock(impl_->mutex_);

    if (index >= impl_->entries_.size()) {
        return false;
    }

    auto& tags = impl_->entries_[index].tags;
    auto it = std::find(tags.begin(), tags.end(), tag);
    if (it != tags.end()) {
        tags.erase(it);
        return true;
    }

    return false;
}

std::vector<HistoryEntry> HistoryManager::getByTag(
    const std::string& tag) const {
    std::lock_guard<std::mutex> lock(impl_->mutex_);

    std::vector<HistoryEntry> results;
    for (const auto& entry : impl_->entries_) {
        if (std::find(entry.tags.begin(), entry.tags.end(), tag) !=
            entry.tags.end()) {
            results.push_back(entry);
        }
    }

    return results;
}

std::vector<std::string> HistoryManager::getAllTags() const {
    std::lock_guard<std::mutex> lock(impl_->mutex_);

    std::unordered_set<std::string> tagSet;
    for (const auto& entry : impl_->entries_) {
        for (const auto& tag : entry.tags) {
            tagSet.insert(tag);
        }
    }

    return std::vector<std::string>(tagSet.begin(), tagSet.end());
}

bool HistoryManager::load(const std::string& path) { return impl_->load(path); }

bool HistoryManager::load() {
    if (impl_->config_.historyFile.empty()) {
        return false;
    }
    return impl_->load(impl_->config_.historyFile);
}

bool HistoryManager::save(const std::string& path) const {
    return impl_->save(path);
}

bool HistoryManager::save() const {
    if (impl_->config_.historyFile.empty()) {
        return false;
    }
    return impl_->save(impl_->config_.historyFile);
}

std::string HistoryManager::exportJson() const { return impl_->exportJson(); }

bool HistoryManager::importJson(const std::string& json) {
    try {
        auto j = nlohmann::json::parse(json);

        std::lock_guard<std::mutex> lock(impl_->mutex_);
        impl_->entries_.clear();

        for (const auto& item : j["entries"]) {
            HistoryEntry entry;
            entry.command = item["command"].get<std::string>();

            if (item.contains("timestamp")) {
                auto ts = item["timestamp"].get<int64_t>();
                entry.timestamp = std::chrono::system_clock::from_time_t(ts);
            }

            if (item.contains("favorite")) {
                entry.favorite = item["favorite"].get<bool>();
            }

            if (item.contains("tags")) {
                entry.tags = item["tags"].get<std::vector<std::string>>();
            }

            impl_->entries_.push_back(entry);
        }

        impl_->currentPos_ = impl_->entries_.size();
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

HistoryStats HistoryManager::getStats() const { return impl_->getStats(); }

std::vector<std::pair<std::string, size_t>> HistoryManager::getCommandFrequency(
    size_t topN) const {
    return impl_->getCommandFrequency(topN);
}

std::vector<HistoryEntry> HistoryManager::getInTimeRange(
    std::chrono::system_clock::time_point start,
    std::chrono::system_clock::time_point end) const {
    std::lock_guard<std::mutex> lock(impl_->mutex_);

    std::vector<HistoryEntry> results;
    for (const auto& entry : impl_->entries_) {
        if (entry.timestamp >= start && entry.timestamp <= end) {
            results.push_back(entry);
        }
    }

    return results;
}

std::vector<HistoryEntry> HistoryManager::getAll() const {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    return std::vector<HistoryEntry>(impl_->entries_.begin(),
                                     impl_->entries_.end());
}

std::vector<HistoryEntry> HistoryManager::getRecent(size_t count) const {
    std::lock_guard<std::mutex> lock(impl_->mutex_);

    std::vector<HistoryEntry> results;
    size_t start =
        (impl_->entries_.size() > count) ? impl_->entries_.size() - count : 0;

    for (size_t i = start; i < impl_->entries_.size(); ++i) {
        results.push_back(impl_->entries_[i]);
    }

    return results;
}

void HistoryManager::forEach(
    std::function<void(const HistoryEntry&)> callback) const {
    std::lock_guard<std::mutex> lock(impl_->mutex_);

    for (const auto& entry : impl_->entries_) {
        callback(entry);
    }
}

}  // namespace lithium::debug::terminal
