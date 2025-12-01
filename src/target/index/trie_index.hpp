#ifndef LITHIUM_TARGET_INDEX_TRIE_INDEX_HPP
#define LITHIUM_TARGET_INDEX_TRIE_INDEX_HPP

#include <memory>
#include <shared_mutex>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>

namespace lithium::target::index {

/**
 * @brief Thread-safe Trie (prefix tree) index for efficient string storage and
 * autocomplete
 *
 * A Trie (prefix tree) data structure optimized for:
 * - Fast prefix-based autocomplete queries
 * - Memory-efficient string storage through prefix sharing
 * - Thread-safe concurrent read/write operations using shared_mutex
 * - Batch insertion for high-performance bulk loading
 *
 * The implementation uses std::string_view to minimize string copies and
 * provides a thread-safe singleton pattern for global access.
 *
 * @thread_safe All public methods are fully thread-safe
 *
 * @example
 * ```cpp
 * auto& index = TrieIndex::instance();
 * index.insert("Orion");
 * index.insertBatch({"Sirius", "Polaris", "Vega"});
 * auto results = index.autocomplete("Ori", 10);  // ["Orion"]
 * ```
 */
class TrieIndex {
public:
    /**
     * @brief Get the singleton instance of TrieIndex
     *
     * Implements thread-safe singleton pattern using Meyer's singleton.
     * The instance is created on first call and destroyed at program exit.
     *
     * @return Reference to the global TrieIndex instance
     * @thread_safe This method is thread-safe
     */
    static auto instance() -> TrieIndex&;

    // Deleted copy constructor and assignment operator
    TrieIndex(const TrieIndex&) = delete;
    TrieIndex& operator=(const TrieIndex&) = delete;

    // Deleted move operations
    TrieIndex(TrieIndex&&) = delete;
    TrieIndex& operator=(TrieIndex&&) = delete;

    /**
     * @brief Insert a single word into the Trie index
     *
     * Adds a word to the Trie structure. If the word already exists,
     * this operation is a no-op (idempotent).
     *
     * Time complexity: O(m) where m is the length of the word
     * Space complexity: O(m) for new branches
     *
     * @param word The word to insert (supports std::string_view for efficiency)
     * @thread_safe Thread-safe for concurrent insertion
     * @note Empty strings are allowed but typically not meaningful
     *
     * @example
     * ```cpp
     * index.insert("Andromeda");
     * index.insert(std::string_view("Triangulum"));
     * ```
     */
    void insert(std::string_view word);

    /**
     * @brief Insert multiple words in batch mode
     *
     * Performs batch insertion of multiple words more efficiently than
     * individual insert calls by acquiring the lock once for all insertions.
     * This is the preferred method for loading large datasets.
     *
     * Time complexity: O(n*m) where n is the number of words and m is
     * average word length
     *
     * @param words Span of strings to insert (supports any contiguous range)
     * @thread_safe Thread-safe for concurrent insertion
     * @note Performs all insertions under a single lock
     *
     * @example
     * ```cpp
     * std::vector<std::string> names = {"Betelgeuse", "Rigel", "Bellatrix"};
     * index.insertBatch(names);
     * ```
     */
    void insertBatch(std::span<const std::string> words);

    /**
     * @brief Get autocomplete suggestions for a given prefix
     *
     * Returns up to `limit` suggestions that start with the given prefix.
     * Results are returned in no guaranteed order but typically in
     * depth-first traversal order.
     *
     * Time complexity: O(n) where n is the number of matching words
     * Space complexity: O(k) where k is the number of results
     *
     * @param prefix The prefix to search for (supports std::string_view)
     * @param limit Maximum number of suggestions to return (default: 10)
     * @return Vector of strings that start with the prefix
     * @thread_safe Thread-safe for concurrent reads
     *
     * @example
     * ```cpp
     * auto results = index.autocomplete("Ori");  // Returns ["Orion"]
     * auto results = index.autocomplete("Bet", 5);  // Returns up to 5 results
     * ```
     */
    [[nodiscard]] auto autocomplete(std::string_view prefix, size_t limit = 10)
        const -> std::vector<std::string>;

    /**
     * @brief Clear all entries from the Trie
     *
     * Removes all words from the index and frees allocated memory.
     * After this call, the Trie is equivalent to a newly constructed instance.
     *
     * Time complexity: O(n) where n is the total number of nodes
     * Space complexity: O(1) - all memory is freed
     *
     * @thread_safe Thread-safe for exclusive write
     *
     * @example
     * ```cpp
     * index.clear();
     * assert(index.size() == 0);
     * ```
     */
    void clear();

    /**
     * @brief Get the total number of nodes in the Trie
     *
     * Returns the count of unique prefixes (nodes) in the Trie.
     * This is an approximation of the index size and includes all
     * intermediate nodes, not just complete words.
     *
     * @return Total number of nodes in the Trie
     * @thread_safe Thread-safe for concurrent reads
     *
     * @example
     * ```cpp
     * index.insert("cat");
     * index.insert("car");
     * size_t nodes = index.size();  // Returns 4 (root + ca + cat + car)
     * ```
     */
    [[nodiscard]] auto size() const -> size_t;

private:
    /**
     * @brief Internal node structure for the Trie
     *
     * Each node represents a single character in the prefix tree.
     * Children are stored in a hash map for O(1) access.
     */
    struct TrieNode {
        std::unordered_map<char, std::unique_ptr<TrieNode>> children;
        bool isEndOfWord = false;
    };

    /**
     * @brief Default constructor (private for singleton)
     *
     * Initializes the Trie with a root node and creates logging entry.
     */
    TrieIndex();

    /**
     * @brief Destructor (automatically frees all nodes)
     *
     * Unique pointers handle automatic cleanup of all TrieNode instances.
     */
    ~TrieIndex();

    /**
     * @brief Depth-first search to collect matching words
     *
     * Recursively traverses the Trie from a given node, collecting all
     * words that can be formed from that point onward. Respects the limit
     * parameter to avoid collecting too many results.
     *
     * @param node Current node in the DFS traversal
     * @param prefix Current prefix being built
     * @param suggestions Vector to accumulate results
     * @param limit Maximum number of results to collect
     */
    void dfs(const TrieNode* node, std::string& prefix,
             std::vector<std::string>& suggestions, size_t limit) const;

    /**
     * @brief Get the size of a subtree rooted at a node
     *
     * Counts all nodes in the subtree including the current node.
     * Used by the public size() method.
     *
     * @param node The root node of the subtree
     * @return Total number of nodes in the subtree
     */
    [[nodiscard]] auto subtreeSize(const TrieNode* node) const -> size_t;

    // Thread synchronization
    mutable std::shared_mutex mutex_;

    // Root node of the Trie
    std::unique_ptr<TrieNode> root_;
};

}  // namespace lithium::target::index

#endif  // LITHIUM_TARGET_INDEX_TRIE_INDEX_HPP
