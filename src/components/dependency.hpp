// DEPENDENCY.hpp
#ifndef LITHIUM_ADDON_DEPENDENCY_HPP
#define LITHIUM_ADDON_DEPENDENCY_HPP

#include <concepts>
#include <coroutine>
#include <optional>
#include <shared_mutex>
#include <span>
#include <stack>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "atom/type/json_fwd.hpp"
#include "version.hpp"

using json = nlohmann::json;

namespace lithium {

template <typename T>
concept StringLike = std::convertible_to<T, std::string_view>;

/**
 * @brief A class that represents a directed dependency graph.
 *
 * This class allows for managing nodes and their dependencies, detecting
 * cycles, performing topological sorting, and resolving dependencies for a
 * given set of directories.
 */
class DependencyGraph {
public:
    DependencyGraph() noexcept;

    using Node = std::string;

    void clear();

    /**
     * @brief Adds a node to the dependency graph.
     *
     * @param node The name of the node to be added.
     * @param version The version of the node.
     * @throws std::invalid_argument If the node is invalid.
     */
    void addNode(const Node& node, Version version);
    void addNode(Node&& node, Version version);

    /**
     * @brief Adds a directed dependency from one node to another.
     *
     * This establishes a relationship where 'from' depends on 'to'.
     *
     * @param from The node that has a dependency.
     * @param to The node that is being depended upon.
     * @param requiredVersion The required version of the dependent node.
     * @throws std::invalid_argument If the nodes don't exist or version
     * requirements aren't met.
     */
    void addDependency(const Node& from, const Node& to,
                       Version requiredVersion);
    void addDependency(Node&& from, Node&& to, Version requiredVersion);

    /**
     * @brief Removes a node from the dependency graph.
     *
     * This will also remove any dependencies associated with the node.
     *
     * @param node The name of the node to be removed.
     */
    void removeNode(const Node& node) noexcept;

    /**
     * @brief Removes a directed dependency from one node to another.
     *
     * @param from The node that previously had a dependency.
     * @param to The node that is being removed from the dependency list.
     */
    void removeDependency(const Node& from, const Node& to) noexcept;

    /**
     * @brief Checks if a node exists in the dependency graph.
     *
     * @param node The node to check for.
     * @return True if the node exists, false otherwise.
     */
    [[nodiscard]] bool nodeExists(const Node& node) const noexcept;

    /**
     * @brief Retrieves the direct dependencies of a node.
     *
     * @param node The node for which to get dependencies.
     * @return A vector containing the names of the dependent nodes.
     */
    [[nodiscard]] auto getDependencies(const Node& node) const noexcept
        -> std::vector<Node>;

    /**
     * @brief Retrieves the direct dependents of a node.
     *
     * @param node The node for which to get dependents.
     * @return A vector containing the names of the dependents.
     */
    [[nodiscard]] auto getDependents(const Node& node) const noexcept
        -> std::vector<Node>;

    /**
     * @brief Checks if the dependency graph contains a cycle.
     *
     * @return True if there is a cycle in the graph, false otherwise.
     */
    [[nodiscard]] auto hasCycle() const noexcept -> bool;

    /**
     * @brief Performs a topological sort of the nodes in the dependency graph.
     *
     * @return An optional vector containing the nodes in topological order, or
     * std::nullopt if a cycle is detected.
     */
    [[nodiscard]] auto topologicalSort() const noexcept
        -> std::optional<std::vector<Node>>;

    /**
     * @brief Retrieves all dependencies of a node, including transitive
     * dependencies.
     *
     * @param node The node for which to get all dependencies.
     * @return A set containing all dependency node names.
     */
    [[nodiscard]] auto getAllDependencies(const Node& node) const noexcept
        -> std::unordered_set<Node>;

    /**
     * @brief Loads nodes in parallel using a specified loading function.
     *
     * This function applies a provided function to each node concurrently.
     *
     * @param loadFunction The function to apply to each node.
     */
    void loadNodesInParallel(
        std::invocable<const Node&> auto loadFunction) const;

    /**
     * @brief Builds the dependency graph from a given list of directories.
     *
     * This function parses package files in the specified directories and
     * populates the graph with nodes and dependencies.
     *
     * @param directories A view of directory paths to resolve.
     * @throws std::runtime_error If there is an error resolving dependencies.
     */
    void buildFromDirectories(std::span<const Node> directories);

    /**
     * @brief Resolves system dependencies for a given list of directories.
     *
     * @param directories A view of directory paths to resolve.
     * @return A map containing system dependency names and their versions.
     */
    [[nodiscard]] auto resolveSystemDependencies(
        std::span<const Node> directories)
        -> std::unordered_map<std::string, Version>;

    /**
     * @brief Sets the priority of a dependency node.
     * @param node The dependency node
     * @param priority The priority value to set
     */
    void setPriority(const Node& node, int priority) noexcept;

    /**
     * @brief Detects version conflicts among dependencies.
     * @return A vector of tuples containing conflicting nodes and their
     * versions
     */
    [[nodiscard]] auto detectVersionConflicts() const noexcept
        -> std::vector<std::tuple<Node, Node, Version, Version>>;

    /**
     * @brief Builds the dependency graph from directories in parallel.
     * @param directories A view of directories to resolve dependencies from
     * @throws std::runtime_error If there is an error resolving dependencies.
     */
    void buildFromDirectoriesParallel(std::span<const Node> directories);

    /**
     * @brief Adds a group of dependencies.
     * @param groupName The name of the group
     * @param nodes A view of dependency nodes to add to the group
     */
    void addGroup(std::string_view groupName, std::span<const Node> nodes);

    /**
     * @brief Gets all dependencies within a group.
     * @param groupName The name of the group
     * @return A vector of dependency nodes within the group
     */
    [[nodiscard]] auto getGroupDependencies(
        std::string_view groupName) const noexcept -> std::vector<Node>;

    /**
     * @brief Clears the dependency cache.
     */
    void clearCache() noexcept;

    /**
     * @brief Parses a package.json file.
     * @param path The path to the package.json file
     * @return A pair containing the package name and a map of dependencies
     * @throws std::runtime_error If there is an error parsing the file.
     */
    [[nodiscard]] static auto parsePackageJson(std::string_view path)
        -> std::pair<Node, std::unordered_map<Node, Version>>;

    /**
     * @brief Validates all dependencies of a node
     * @param node The name of the node to validate
     * @return true if all dependencies are valid, false otherwise
     */
    [[nodiscard]] auto validateDependencies(const Node& node) const noexcept
        -> bool;

    /**
     * @brief Async dependency resolution generator.
     */
    struct DependencyGenerator {
        struct promise_type {
            Node value;
            auto get_return_object() { return DependencyGenerator{this}; }
            auto initial_suspend() { return std::suspend_always{}; }
            auto final_suspend() noexcept { return std::suspend_always{}; }
            void return_void() {}
            void unhandled_exception() { std::terminate(); }
            auto yield_value(Node v) {
                value = std::move(v);
                return std::suspend_always{};
            }
        };

        explicit DependencyGenerator(promise_type* p)
            : coro_(std::coroutine_handle<promise_type>::from_promise(*p)) {}

        ~DependencyGenerator() {
            if (coro_) {
                coro_.destroy();
            }
        }

        DependencyGenerator(const DependencyGenerator&) = delete;
        DependencyGenerator& operator=(const DependencyGenerator&) = delete;
        DependencyGenerator(DependencyGenerator&& other) noexcept
            : coro_(other.coro_) {
            other.coro_ = {};
        }
        DependencyGenerator& operator=(DependencyGenerator&& other) noexcept {
            if (this != &other) {
                if (coro_) {
                    coro_.destroy();
                }
                coro_ = other.coro_;
                other.coro_ = {};
            }
            return *this;
        }

        bool next() {
            if (coro_ && !coro_.done()) {
                coro_.resume();
                return !coro_.done();
            }
            return false;
        }

        const Node& value() const { return coro_.promise().value; }

    private:
        std::coroutine_handle<promise_type> coro_{};
    };

    /**
     * @brief Async dependency resolution function returning a generator.
     * @param directory The directory to resolve dependencies from
     * @return A generator that yields dependency nodes asynchronously
     */
    [[nodiscard]] DependencyGenerator resolveDependenciesAsync(
        const Node& directory);

    /**
     * @brief Thread-safe getter for node version.
     * @param node The node to get version for
     * @return The version of the node, or nullopt if not found
     */
    [[nodiscard]] auto getNodeVersion(const Node& node) const noexcept
        -> std::optional<Version>;

private:
    mutable std::shared_mutex mutex_;
    std::unordered_map<Node, std::unordered_set<Node>> adjList_;
    std::unordered_map<Node, std::unordered_set<Node>> incomingEdges_;
    std::unordered_map<Node, Version> nodeVersions_;
    std::unordered_map<Node, int> priorities_;
    std::unordered_map<std::string, std::vector<Node>> groups_;
    mutable std::unordered_map<Node, std::vector<Node>> dependencyCache_;

    struct ParsedInfo {
        Node name;
        Version version;
        std::unordered_map<Node, Version> dependencies;
    };

    /**
     * @brief Utility function to check for cycles in the graph.
     * @param node The current node being visited
     * @param visited A set of visited nodes
     * @param recStack A set of nodes in the recursion stack
     * @return True if a cycle is detected, false otherwise
     */
    [[nodiscard]] auto hasCycleUtil(
        const Node& node, std::unordered_set<Node>& visited,
        std::unordered_set<Node>& recStack) const noexcept -> bool;

    /**
     * @brief Utility function to perform topological sort.
     * @param node The current node being visited
     * @param visited A set of visited nodes
     * @param stack A stack to store the topological order
     * @return True if the sort is successful, false otherwise
     */
    [[nodiscard]] auto topologicalSortUtil(
        const Node& node, std::unordered_set<Node>& visited,
        std::stack<Node>& stack) const noexcept -> bool;

    /**
     * @brief Utility function to get all dependencies of a node.
     * @param node The node for which to get dependencies
     * @param allDependencies A set to store all dependencies
     */
    void getAllDependenciesUtil(
        const Node& node,
        std::unordered_set<Node>& allDependencies) const noexcept;

    /**
     * @brief Utility function to remove duplicates from a vector of nodes.
     * @param input The input vector of nodes
     * @return A vector of unique nodes
     */
    [[nodiscard]] static auto removeDuplicates(
        std::span<const Node> input) noexcept -> std::vector<Node>;

    /**
     * @brief Parses a package.xml file.
     * @param path The path to the package.xml file
     * @return A pair containing the package name and a map of dependencies
     * @throws std::runtime_error If there is an error parsing the file.
     */
    [[nodiscard]] static auto parsePackageXml(std::string_view path)
        -> std::pair<Node, std::unordered_map<Node, Version>>;

    /**
     * @brief Parses a package.yaml file.
     * @param path The path to the package.yaml file
     * @return A pair containing the package name and a map of dependencies
     * @throws std::runtime_error If there is an error parsing the file.
     */
    [[nodiscard]] static auto parsePackageYaml(std::string_view path)
        -> std::pair<Node, std::unordered_map<Node, Version>>;

    /**
     * @brief Parses a package.toml file.
     * @param path The path to the package.toml file
     * @return A pair containing the package name and a map of dependencies
     * @throws std::runtime_error If there is an error parsing the file.
     */
    [[nodiscard]] static auto parsePackageToml(std::string_view path)
        -> std::pair<Node, std::unordered_map<Node, Version>>;

    /**
     * @brief Validates the version compatibility between dependent and
     * dependency.
     *
     * @param from The dependent node.
     * @param to The dependency node.
     * @param requiredVersion The required version of the dependency.
     * @throws std::invalid_argument If the version requirements aren't met.
     */
    void validateVersion(const Node& from, const Node& to,
                         const Version& requiredVersion) const;

    static std::optional<ParsedInfo> parseDirectory(const Node& directory);

    void addParsedInfo(const ParsedInfo& info);
};

}  // namespace lithium

#endif  // LITHIUM_ADDON_DEPENDENCY_HPP
