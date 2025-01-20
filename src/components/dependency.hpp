// DEPENDENCY.hpp
#ifndef LITHIUM_ADDON_DEPENDENCY_HPP
#define LITHIUM_ADDON_DEPENDENCY_HPP

#include <functional>
#include <optional>
#include <shared_mutex>
#include <stack>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "atom/type/json_fwd.hpp"
#include "version.hpp"

using json = nlohmann::json;

namespace lithium {
/**
 * @brief A class that represents a directed dependency graph.
 *
 * This class allows for managing nodes and their dependencies, detecting
 * cycles, performing topological sorting, and resolving dependencies for a
 * given set of directories.
 */
class DependencyGraph {
public:
    DependencyGraph();

    using Node = std::string;

    /**
     * @brief Adds a node to the dependency graph.
     *
     * @param node The name of the node to be added.
     * @param version The version of the node.
     */
    void addNode(const Node& node, const Version& version);

    /**
     * @brief Adds a directed dependency from one node to another.
     *
     * This establishes a relationship where 'from' depends on 'to'.
     *
     * @param from The node that has a dependency.
     * @param to The node that is being depended upon.
     * @param requiredVersion The required version of the dependent node.
     */
    void addDependency(const Node& from, const Node& to,
                       const Version& requiredVersion);

    /**
     * @brief Removes a node from the dependency graph.
     *
     * This will also remove any dependencies associated with the node.
     *
     * @param node The name of the node to be removed.
     */
    void removeNode(const Node& node);

    /**
     * @brief Removes a directed dependency from one node to another.
     *
     * @param from The node that previously had a dependency.
     * @param to The node that is being removed from the dependency list.
     */
    void removeDependency(const Node& from, const Node& to);

    /**
     * @brief Retrieves the direct dependencies of a node.
     *
     * @param node The node for which to get dependencies.
     * @return A vector containing the names of the dependent nodes.
     */
    auto getDependencies(const Node& node) const -> std::vector<Node>;

    /**
     * @brief Retrieves the direct dependents of a node.
     *
     * @param node The node for which to get dependents.
     * @return A vector containing the names of the dependents.
     */
    auto getDependents(const Node& node) const -> std::vector<Node>;

    /**
     * @brief Checks if the dependency graph contains a cycle.
     *
     * @return True if there is a cycle in the graph, false otherwise.
     */
    auto hasCycle() const -> bool;

    /**
     * @brief Performs a topological sort of the nodes in the dependency graph.
     *
     * @return An optional vector containing the nodes in topological order, or
     * std::nullopt if a cycle is detected.
     */
    auto topologicalSort() const -> std::optional<std::vector<Node>>;

    /**
     * @brief Retrieves all dependencies of a node, including transitive
     * dependencies.
     *
     * @param node The node for which to get all dependencies.
     * @return A set containing all dependency node names.
     */
    auto getAllDependencies(const Node& node) const -> std::unordered_set<Node>;

    /**
     * @brief Loads nodes in parallel using a specified loading function.
     *
     * This function applies a provided function to each node concurrently.
     *
     * @param loadFunction The function to apply to each node.
     */
    void loadNodesInParallel(
        std::function<void(const Node&)> loadFunction) const;

    /**
     * @brief Resolves dependencies for a given list of directories.
     *
     * This function analyzes the specified directories and determines their
     * dependencies.
     *
     * @param directories A vector containing the paths of directories to
     * resolve.
     * @return A vector containing resolved dependency paths.
     */
    auto resolveDependencies(const std::vector<Node>& directories)
        -> std::vector<Node>;

    /**
     * @brief Resolves system dependencies for a given list of directories.
     *
     * @param directories A vector containing the paths of directories to
     * resolve.
     * @return A map containing system dependency names and their versions.
     */
    auto resolveSystemDependencies(const std::vector<Node>& directories)
        -> std::unordered_map<std::string, Version>;

    /**
     * @brief Sets the priority of a dependency node.
     * @param node The dependency node
     * @param priority The priority value to set
     */
    void setPriority(const Node& node, int priority);

    /**
     * @brief Detects version conflicts among dependencies.
     * @return A vector of tuples containing conflicting nodes and their
     * versions
     */
    auto detectVersionConflicts() const
        -> std::vector<std::tuple<Node, Node, Version, Version>>;

    /**
     * @brief Resolves dependencies in parallel.
     * @param directories A vector of directories to resolve dependencies from
     * @return A vector of resolved dependency nodes
     */
    auto resolveParallelDependencies(const std::vector<Node>& directories)
        -> std::vector<Node>;

    /**
     * @brief Adds a group of dependencies.
     * @param groupName The name of the group
     * @param nodes A vector of dependency nodes to add to the group
     */
    void addGroup(const std::string& groupName, const std::vector<Node>& nodes);

    /**
     * @brief Gets all dependencies within a group.
     * @param groupName The name of the group
     * @return A vector of dependency nodes within the group
     */
    auto getGroupDependencies(const std::string& groupName) const
        -> std::vector<Node>;

    /**
     * @brief Clears the dependency cache.
     */
    void clearCache();

    /**
     * @brief Parses a package.json file.
     * @param path The path to the package.json file
     * @return A pair containing the package name and a map of dependencies
     */
    static auto parsePackageJson(const Node& path)
        -> std::pair<Node, std::unordered_map<Node, Version>>;

    /**
     * @brief 验证一个节点的所有依赖是否有效
     * @param node 要验证的节点名称
     * @return 如果所有依赖都有效返回true,否则返回false
     */
    auto validateDependencies(const Node& node) const -> bool;

private:
    mutable std::shared_mutex mutex_;
    std::unordered_map<Node, std::unordered_set<Node>>
        adjList_;  ///< Adjacency list representation of the graph.
    std::unordered_map<Node, std::unordered_set<Node>>
        incomingEdges_;  ///< Map to track incoming edges for each node.
    std::unordered_map<Node, Version>
        nodeVersions_;  ///< Map to track node versions.

    std::unordered_map<Node, int> priorities_;  // 节点优先级
    std::unordered_map<std::string, std::vector<Node>> groups_;  // 依赖分组
    mutable std::unordered_map<Node, std::vector<Node>>
        dependencyCache_;  // 依赖缓存

    /**
     * @brief Utility function to check for cycles in the graph.
     * @param node The current node being visited
     * @param visited A set of visited nodes
     * @param recStack A set of nodes in the recursion stack
     * @return True if a cycle is detected, false otherwise
     */
    auto hasCycleUtil(const Node& node, std::unordered_set<Node>& visited,
                      std::unordered_set<Node>& recStack) const -> bool;

    /**
     * @brief Utility function to perform topological sort.
     * @param node The current node being visited
     * @param visited A set of visited nodes
     * @param stack A stack to store the topological order
     * @return True if the sort is successful, false otherwise
     */
    auto topologicalSortUtil(const Node& node,
                             std::unordered_set<Node>& visited,
                             std::stack<Node>& stack) const -> bool;

    /**
     * @brief Utility function to get all dependencies of a node.
     * @param node The node for which to get dependencies
     * @param allDependencies A set to store all dependencies
     */
    void getAllDependenciesUtil(
        const Node& node, std::unordered_set<Node>& allDependencies) const;

    /**
     * @brief Utility function to remove duplicates from a vector of nodes.
     * @param input The input vector of nodes
     * @return A vector of unique nodes
     */
    static auto removeDuplicates(const std::vector<Node>& input)
        -> std::vector<Node>;

    /**
     * @brief Parses a package.json file.
     * @param path The path to the package.json file
     * @return A pair containing the package name and a map of dependencies
     */
    static auto parsePackageXml(const Node& path)
        -> std::pair<Node, std::unordered_map<Node, Version>>;

    /**
     * @brief Parses a package.yaml file.
     * @param path The path to the package.yaml file
     * @return A pair containing the package name and a map of dependencies
     */
    static auto parsePackageYaml(const std::string& path)
        -> std::pair<std::string, std::unordered_map<std::string, Version>>;

    /**
     * @brief Validates the version compatibility between dependent and
     * dependency.
     *
     * @param from The dependent node.
     * @param to The dependency node.
     * @param requiredVersion The required version of the dependency.
     */
    void validateVersion(const Node& from, const Node& to,
                         const Version& requiredVersion) const;

    /**
     * @brief Resolves dependencies in parallel.
     *
     * @param batch A vector of nodes to resolve in parallel.
     * @return A vector of resolved nodes.
     */
    auto resolveParallelBatch(const std::vector<Node>& batch)
        -> std::vector<Node>;
};
}  // namespace lithium

#endif  // LITHIUM_ADDON_DEPENDENCY_HPP