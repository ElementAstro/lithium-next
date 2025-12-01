#ifndef LITHIUM_TARGET_INDEX_FUZZY_MATCHER_HPP
#define LITHIUM_TARGET_INDEX_FUZZY_MATCHER_HPP

#include <memory>
#include <shared_mutex>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace lithium::target::index {

/**
 * @brief BK-tree based fuzzy string matching using Levenshtein distance
 *
 * A fuzzy matching index optimized for:
 * - Efficient approximate string matching with edit distance
 * - Fast maximum-distance range queries using BK-tree (Burkhard-Keller tree)
 * - Thread-safe concurrent access
 * - Support for typo tolerance and name variations
 *
 * The implementation uses:
 * - BK-tree data structure for metric space indexing
 * - Levenshtein distance (edit distance) as the similarity metric
 * - Triangle inequality for efficient tree traversal and pruning
 * - Support for normalized string queries
 *
 * A BK-tree partitions metric space by distances to a pivot point,
 * allowing efficient range queries while avoiding unnecessary distance
 * calculations.
 *
 * @thread_safe All public methods are fully thread-safe
 *
 * @example
 * ```cpp
 * FuzzyMatcher matcher;
 * matcher.addTerm("Andromeda", "M31");
 * matcher.addTerm("Androemda", "M31_typo");  // Note the typo
 * matcher.addTerm("Triangulum", "M33");
 *
 * // Find matches within edit distance 2
 * auto results = matcher.match("Andromeda", 2, 10);
 * // Returns: [("M31", 0), ("M31_typo", 1)]
 * ```
 */
class FuzzyMatcher {
public:
    /**
     * @brief Result of a fuzzy match query
     *
     * Contains the object ID and the edit distance between query and match.
     */
    using MatchResult = std::pair<std::string, int>;  ///< (objectId, distance)

    /**
     * @brief Create a new fuzzy matcher
     */
    FuzzyMatcher();

    /**
     * @brief Destructor
     */
    ~FuzzyMatcher();

    // Deleted copy operations
    FuzzyMatcher(const FuzzyMatcher&) = delete;
    FuzzyMatcher& operator=(const FuzzyMatcher&) = delete;

    // Move operations allowed
    FuzzyMatcher(FuzzyMatcher&&) noexcept;
    FuzzyMatcher& operator=(FuzzyMatcher&&) noexcept;

    /**
     * @brief Add a single term to the fuzzy matcher
     *
     * Associates a search term (e.g., an object name or alias) with an
     * object ID. Multiple terms can map to the same object ID, and the
     * same term cannot be added twice.
     *
     * @param term The search term to index (e.g., "Andromeda")
     * @param objectId The object ID this term refers to (e.g., "M31")
     * @thread_safe Thread-safe for concurrent insertion
     *
     * @example
     * ```cpp
     * matcher.addTerm("Sirius", "alpha_cma");
     * matcher.addTerm("Dog Star", "alpha_cma");  // Different term, same object
     * ```
     */
    void addTerm(const std::string& term, const std::string& objectId);

    /**
     * @brief Add multiple terms in batch mode
     *
     * Efficiently adds multiple terms by acquiring the lock once.
     * This is preferred for bulk loading datasets.
     *
     * @param terms Span of (term, objectId) pairs
     * @thread_safe Thread-safe for concurrent insertion
     *
     * @example
     * ```cpp
     * std::vector<std::pair<std::string, std::string>> terms = {
     *     {"Betelgeuse", "alpha_ori"},
     *     {"Rigel", "beta_ori"},
     *     {"Bellatrix", "gamma_ori"}
     * };
     * matcher.addTerms(terms);
     * ```
     */
    void addTerms(std::span<const std::pair<std::string, std::string>> terms);

    /**
     * @brief Find all matches within a maximum edit distance
     *
     * Performs a fuzzy search for the query string, returning all indexed
     * terms that are within the specified maximum edit distance.
     * Results are sorted by distance (closest first).
     *
     * The search uses the BK-tree structure to efficiently prune the search
     * space, avoiding unnecessary distance calculations.
     *
     * @param query The search query string
     * @param maxDistance Maximum Levenshtein distance for matches
     * @param limit Maximum number of results to return (default: 50)
     * @return Vector of (objectId, distance) pairs sorted by distance
     * @thread_safe Thread-safe for concurrent reads
     *
     * @example
     * ```cpp
     * // Find matches within edit distance 2
     * auto results = matcher.match("Andromaeda", 2, 10);
     * // Results might include: [("M31", 1), ("M31_alt", 2)]
     * ```
     */
    [[nodiscard]] auto match(const std::string& query, int maxDistance,
                              int limit = 50) const
        -> std::vector<MatchResult>;

    /**
     * @brief Clear all entries from the fuzzy matcher
     *
     * Removes all terms and object IDs, resetting the matcher to
     * its initial state.
     *
     * @thread_safe Thread-safe for exclusive write
     */
    void clear();

    /**
     * @brief Get the total number of indexed terms
     *
     * @return Number of unique search terms in the index
     * @thread_safe Thread-safe for concurrent reads
     */
    [[nodiscard]] auto size() const -> size_t;

    /**
     * @brief Check if a term exists in the matcher
     *
     * @param term The term to search for
     * @return True if the term has been indexed
     * @thread_safe Thread-safe for concurrent reads
     */
    [[nodiscard]] auto contains(std::string_view term) const -> bool;

    /**
     * @brief Get the object ID associated with a term
     *
     * @param term The search term
     * @return Object ID if found, empty string if not found
     * @thread_safe Thread-safe for concurrent reads
     */
    [[nodiscard]] auto getObjectId(std::string_view term) const
        -> std::string;

    /**
     * @brief Get statistics about the fuzzy matcher
     *
     * @return String containing statistics (size, depth, etc.)
     * @thread_safe Thread-safe for concurrent reads
     */
    [[nodiscard]] auto getStats() const -> std::string;

private:
    /**
     * @brief Node in the BK-tree
     *
     * Each node stores a term (pivot string) and references to child nodes
     * grouped by distance from the pivot.
     */
    struct BKTreeNode {
        std::string term;  ///< The pivot term for this node
        std::string objectId;  ///< Object ID associated with this term
        std::unordered_map<int, std::unique_ptr<BKTreeNode>>
            children;  ///< Children indexed by distance from this node
    };

    /**
     * @brief Calculate Levenshtein distance between two strings
     *
     * Returns the minimum number of single-character edits (insertions,
     * deletions, or substitutions) required to transform one string into
     * another.
     *
     * Time complexity: O(m*n) where m and n are string lengths
     * Space complexity: O(min(m,n)) using optimized space approach
     *
     * @param s1 First string
     * @param s2 Second string
     * @return Edit distance (0 if strings are identical)
     */
    [[nodiscard]] static auto levenshteinDistance(std::string_view s1,
                                                   std::string_view s2)
        noexcept -> int;

    /**
     * @brief Normalize a string for matching
     *
     * Converts to lowercase for case-insensitive matching.
     * Can be extended for other normalization (remove accents, etc.)
     *
     * @param s String to normalize
     * @return Normalized string
     */
    [[nodiscard]] static auto normalize(std::string_view s) -> std::string;

    /**
     * @brief Insert into BK-tree recursively
     *
     * @param node Current node (may be null for first insertion)
     * @param term Term to insert
     * @param objectId Object ID for this term
     * @return Root node after insertion
     */
    [[nodiscard]] auto insertNode(std::unique_ptr<BKTreeNode> node,
                                   const std::string& term,
                                   const std::string& objectId)
        -> std::unique_ptr<BKTreeNode>;

    /**
     * @brief Search BK-tree recursively
     *
     * @param node Current node to search
     * @param query Query string
     * @param maxDistance Maximum distance for matches
     * @param results Result vector to accumulate matches
     * @param limit Result limit
     * @param termMap Map for quick object ID lookup
     */
    void searchNode(const BKTreeNode* node, const std::string& query,
                    int maxDistance, std::vector<MatchResult>& results,
                    int limit, const std::unordered_map<std::string, std::string>&
                        termMap) const;

    /**
     * @brief Count nodes in the BK-tree
     *
     * @param node Root of tree/subtree
     * @return Number of nodes
     */
    [[nodiscard]] auto countNodes(const BKTreeNode* node) const -> size_t;

    /**
     * @brief Get tree depth for statistics
     *
     * @param node Root of tree/subtree
     * @return Maximum depth of tree
     */
    [[nodiscard]] auto getTreeDepth(const BKTreeNode* node) const -> int;

    // BK-tree root
    std::unique_ptr<BKTreeNode> root_;

    // Quick lookup for term -> objectId mapping
    std::unordered_map<std::string, std::string> termMap_;

    // Thread synchronization
    mutable std::shared_mutex mutex_;
};

}  // namespace lithium::target::index

#endif  // LITHIUM_TARGET_INDEX_FUZZY_MATCHER_HPP
