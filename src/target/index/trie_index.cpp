#include "trie_index.hpp"

#include <algorithm>

#include "spdlog/spdlog.h"

namespace lithium::target::index {

auto TrieIndex::instance() -> TrieIndex& {
    static TrieIndex instance;
    return instance;
}

TrieIndex::TrieIndex() : root_(std::make_unique<TrieNode>()) {
    spdlog::info("TrieIndex initialized");
}

TrieIndex::~TrieIndex() {
    spdlog::info("TrieIndex destroyed");
}

void TrieIndex::insert(std::string_view word) {
    std::unique_lock lock(mutex_);

    spdlog::debug("Inserting word into TrieIndex: {}", word);

    TrieNode* current = root_.get();
    for (char ch : word) {
        if (current->children.find(ch) == current->children.end()) {
            current->children[ch] = std::make_unique<TrieNode>();
        }
        current = current->children[ch].get();
    }

    if (!current->isEndOfWord) {
        current->isEndOfWord = true;
        spdlog::debug("Successfully inserted word: {}", word);
    } else {
        spdlog::debug("Word already exists in TrieIndex: {}", word);
    }
}

void TrieIndex::insertBatch(std::span<const std::string> words) {
    std::unique_lock lock(mutex_);

    spdlog::info("Performing batch insertion of {} words", words.size());

    size_t inserted = 0;
    for (const auto& word : words) {
        TrieNode* current = root_.get();
        for (char ch : word) {
            if (current->children.find(ch) == current->children.end()) {
                current->children[ch] = std::make_unique<TrieNode>();
            }
            current = current->children[ch].get();
        }

        if (!current->isEndOfWord) {
            current->isEndOfWord = true;
            inserted++;
        }
    }

    spdlog::info("Batch insertion completed: {} new words inserted out of {}",
                 inserted, words.size());
}

auto TrieIndex::autocomplete(std::string_view prefix, size_t limit) const
    -> std::vector<std::string> {
    std::shared_lock lock(mutex_);

    spdlog::debug("Autocompleting prefix: {} with limit: {}", prefix, limit);

    std::vector<std::string> suggestions;

    // Navigate to the node representing the prefix
    const TrieNode* current = root_.get();
    for (char ch : prefix) {
        auto it = current->children.find(ch);
        if (it == current->children.end()) {
            spdlog::debug("Prefix '{}' not found in TrieIndex", prefix);
            return suggestions;  // Prefix not found
        }
        current = it->second.get();
    }

    spdlog::debug("Prefix '{}' found. Performing DFS for suggestions", prefix);

    // Start DFS from the found node
    std::string mutablePrefix(prefix);
    dfs(current, mutablePrefix, suggestions, limit);

    spdlog::debug("Autocomplete found {} suggestions for prefix: {}",
                  suggestions.size(), prefix);

    return suggestions;
}

void TrieIndex::dfs(const TrieNode* node, std::string& prefix,
                    std::vector<std::string>& suggestions, size_t limit) const {
    // Early exit if we've reached the limit
    if (suggestions.size() >= limit) {
        return;
    }

    // If this node marks the end of a word, add it to suggestions
    if (node->isEndOfWord) {
        suggestions.push_back(prefix);
        spdlog::trace("Found word during DFS: {}", prefix);
    }

    // Recursively explore children
    for (const auto& [ch, child] : node->children) {
        if (suggestions.size() >= limit) {
            break;
        }

        prefix.push_back(ch);
        dfs(child.get(), prefix, suggestions, limit);
        prefix.pop_back();
    }
}

void TrieIndex::clear() {
    std::unique_lock lock(mutex_);

    spdlog::info("Clearing TrieIndex");

    root_ = std::make_unique<TrieNode>();

    spdlog::info("TrieIndex cleared successfully");
}

auto TrieIndex::size() const -> size_t {
    std::shared_lock lock(mutex_);
    return subtreeSize(root_.get());
}

auto TrieIndex::subtreeSize(const TrieNode* node) const -> size_t {
    if (!node) {
        return 0;
    }

    size_t size = 1;  // Count current node
    for (const auto& [ch, child] : node->children) {
        size += subtreeSize(child.get());
    }

    return size;
}

}  // namespace lithium::target::index
