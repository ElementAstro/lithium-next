#include "fuzzy_matcher.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>

#include "spdlog/spdlog.h"

namespace lithium::target::index {

FuzzyMatcher::FuzzyMatcher() {
    spdlog::info("FuzzyMatcher initialized");
}

FuzzyMatcher::~FuzzyMatcher() {
    spdlog::info("FuzzyMatcher destroyed");
}

FuzzyMatcher::FuzzyMatcher(FuzzyMatcher&& other) noexcept
    : root_(std::move(other.root_)), termMap_(std::move(other.termMap_)) {
    spdlog::debug("FuzzyMatcher moved");
}

FuzzyMatcher& FuzzyMatcher::operator=(FuzzyMatcher&& other) noexcept {
    if (this != &other) {
        std::unique_lock lock(mutex_);
        root_ = std::move(other.root_);
        termMap_ = std::move(other.termMap_);
        spdlog::debug("FuzzyMatcher move assigned");
    }
    return *this;
}

void FuzzyMatcher::addTerm(const std::string& term,
                           const std::string& objectId) {
    std::unique_lock lock(mutex_);

    spdlog::debug("Adding term '{}' -> '{}'", term, objectId);

    auto normalized = normalize(term);

    // Check if term already exists
    if (termMap_.find(normalized) != termMap_.end()) {
        spdlog::warn("Term '{}' already exists, skipping", term);
        return;
    }

    termMap_[normalized] = objectId;
    root_ = insertNode(std::move(root_), normalized, objectId);

    spdlog::debug("Successfully added term '{}'", term);
}

void FuzzyMatcher::addTerms(
    std::span<const std::pair<std::string, std::string>> terms) {
    std::unique_lock lock(mutex_);

    spdlog::info("Performing batch insertion of {} terms", terms.size());

    for (const auto& [term, objectId] : terms) {
        auto normalized = normalize(term);

        // Skip if already exists
        if (termMap_.find(normalized) == termMap_.end()) {
            termMap_[normalized] = objectId;
            root_ = insertNode(std::move(root_), normalized, objectId);
        }
    }

    spdlog::info("Batch insertion completed for {} terms", terms.size());
}

auto FuzzyMatcher::match(const std::string& query, int maxDistance, int limit)
    const -> std::vector<MatchResult> {
    std::shared_lock lock(mutex_);

    spdlog::debug("Fuzzy matching query '{}' with maxDistance={}, limit={}",
                  query, maxDistance, limit);

    std::vector<MatchResult> results;

    if (!root_) {
        spdlog::debug("No terms indexed yet");
        return results;
    }

    auto normalizedQuery = normalize(query);
    searchNode(root_.get(), normalizedQuery, maxDistance, results, limit,
               termMap_);

    // Sort by distance
    std::sort(results.begin(), results.end(),
              [](const auto& a, const auto& b) { return a.second < b.second; });

    // Limit results
    if (results.size() > static_cast<size_t>(limit)) {
        results.resize(limit);
    }

    spdlog::debug("Fuzzy match found {} results", results.size());

    return results;
}

void FuzzyMatcher::clear() {
    std::unique_lock lock(mutex_);

    spdlog::info("Clearing FuzzyMatcher");

    root_ = nullptr;
    termMap_.clear();

    spdlog::info("FuzzyMatcher cleared");
}

auto FuzzyMatcher::size() const -> size_t {
    std::shared_lock lock(mutex_);
    return termMap_.size();
}

auto FuzzyMatcher::contains(std::string_view term) const -> bool {
    std::shared_lock lock(mutex_);
    auto normalized = normalize(term);
    return termMap_.find(normalized) != termMap_.end();
}

auto FuzzyMatcher::getObjectId(std::string_view term) const -> std::string {
    std::shared_lock lock(mutex_);
    auto normalized = normalize(term);
    auto it = termMap_.find(normalized);
    if (it != termMap_.end()) {
        return it->second;
    }
    return "";
}

auto FuzzyMatcher::getStats() const -> std::string {
    std::shared_lock lock(mutex_);

    std::stringstream ss;
    ss << "FuzzyMatcher Statistics:\n"
       << "  Terms: " << termMap_.size() << "\n"
       << "  Tree Depth: " << getTreeDepth(root_.get()) << "\n"
       << "  Tree Nodes: " << countNodes(root_.get()) << "\n";

    return ss.str();
}

auto FuzzyMatcher::levenshteinDistance(std::string_view s1,
                                        std::string_view s2) noexcept -> int {
    size_t m = s1.length();
    size_t n = s2.length();

    if (m == 0) {
        return static_cast<int>(n);
    }
    if (n == 0) {
        return static_cast<int>(m);
    }

    // Use optimized space approach - only need 2 rows
    std::vector<int> prev(n + 1);
    std::vector<int> curr(n + 1);

    // Initialize first row
    for (size_t j = 0; j <= n; ++j) {
        prev[j] = static_cast<int>(j);
    }

    // Process each character of s1
    for (size_t i = 1; i <= m; ++i) {
        curr[0] = static_cast<int>(i);

        for (size_t j = 1; j <= n; ++j) {
            int cost = (s1[i - 1] == s2[j - 1]) ? 0 : 1;
            curr[j] = std::min({prev[j] + 1,  // deletion
                                curr[j - 1] + 1,  // insertion
                                prev[j - 1] + cost});  // substitution
        }

        std::swap(prev, curr);
    }

    return prev[n];
}

auto FuzzyMatcher::normalize(std::string_view s) -> std::string {
    std::string result;
    result.reserve(s.length());

    for (char ch : s) {
        result.push_back(std::tolower(static_cast<unsigned char>(ch)));
    }

    return result;
}

auto FuzzyMatcher::insertNode(std::unique_ptr<BKTreeNode> node,
                               const std::string& term,
                               const std::string& objectId)
    -> std::unique_ptr<BKTreeNode> {
    // Base case: create new node
    if (!node) {
        auto newNode = std::make_unique<BKTreeNode>();
        newNode->term = term;
        newNode->objectId = objectId;
        return newNode;
    }

    // Calculate distance from current node's term
    int dist = levenshteinDistance(node->term, term);

    // Distance 0 means duplicate - skip
    if (dist == 0) {
        return node;
    }

    // Recursively insert into appropriate child
    node->children[dist] = insertNode(std::move(node->children[dist]), term,
                                      objectId);

    return node;
}

void FuzzyMatcher::searchNode(
    const BKTreeNode* node, const std::string& query, int maxDistance,
    std::vector<MatchResult>& results, int limit,
    const std::unordered_map<std::string, std::string>& termMap) const {
    if (!node || results.size() >= static_cast<size_t>(limit)) {
        return;
    }

    // Calculate distance from query to current node's term
    int dist = levenshteinDistance(query, node->term);

    // If distance is within threshold, add to results
    if (dist <= maxDistance) {
        results.push_back({termMap.at(node->term), dist});
        spdlog::trace("Fuzzy match found: '{}' at distance {}", node->term,
                      dist);
    }

    // Use triangle inequality to prune search
    // Only explore children where distance d could satisfy:
    // |d - dist| <= maxDistance, which means:
    // dist - maxDistance <= d <= dist + maxDistance
    int minChildDist = dist - maxDistance;
    int maxChildDist = dist + maxDistance;

    for (const auto& [childDist, child] : node->children) {
        if (childDist >= minChildDist && childDist <= maxChildDist) {
            searchNode(child.get(), query, maxDistance, results, limit,
                       termMap);
        }
    }
}

auto FuzzyMatcher::countNodes(const BKTreeNode* node) const -> size_t {
    if (!node) {
        return 0;
    }

    size_t count = 1;
    for (const auto& [dist, child] : node->children) {
        count += countNodes(child.get());
    }

    return count;
}

auto FuzzyMatcher::getTreeDepth(const BKTreeNode* node) const -> int {
    if (!node) {
        return 0;
    }

    int maxDepth = 0;
    for (const auto& [dist, child] : node->children) {
        maxDepth = std::max(maxDepth, getTreeDepth(child.get()));
    }

    return maxDepth + 1;
}

}  // namespace lithium::target::index
