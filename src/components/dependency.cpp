#include "dependency.hpp"
#include "version.hpp"

#include <exception>
#include <filesystem>
#include <fstream>
#include <future>
#include <mutex>
#include <unordered_map>
#include <utility>

#include "atom/error/exception.hpp"
#include "atom/log/loguru.hpp"
#include "atom/type/json.hpp"
#include "atom/utils/container.hpp"
#include "extra/tinyxml2/tinyxml2.h"

#if __has_include(<yaml-cpp/yaml.h>)
#include <yaml-cpp/yaml.h>
#endif

#include "constant/constant.hpp"

using namespace tinyxml2;

namespace lithium {

DependencyGraph::DependencyGraph() {
    LOG_F(INFO, "Creating dependency graph.");
}

void DependencyGraph::addNode(const Node& node, const Version& version) {
    std::unique_lock lock(mutex_);
    LOG_F(INFO, "Adding node: {} with version: {}", node, version.toString());
    adjList_.try_emplace(node);
    incomingEdges_.try_emplace(node);
    nodeVersions_[node] = version;
    LOG_F(INFO, "Node {} added successfully.", node);
}

void DependencyGraph::validateVersion(const Node& from, const Node& to,
                                      const Version& requiredVersion) const {
    std::shared_lock lock(mutex_);
    if (nodeVersions_.find(to) != nodeVersions_.end()) {
        if (nodeVersions_.at(to) < requiredVersion) {
            LOG_F(ERROR,
                  "Version requirement not satisfied for dependency {} -> {}. "
                  "Required: {}, Found: {}",
                  from, to, requiredVersion.toString(),
                  nodeVersions_.at(to).toString());
            THROW_INVALID_ARGUMENT(
                "Version requirement not satisfied for dependency " + from +
                " -> " + to + ". Required: " + requiredVersion.toString() +
                ", Found: " + nodeVersions_.at(to).toString());
        }
    } else {
        LOG_F(ERROR, "Dependency {} not found for node {}.", to, from);
        THROW_INVALID_ARGUMENT("Dependency " + to + " not found for node " +
                               from);
    }
}

void DependencyGraph::addDependency(const Node& from, const Node& to,
                                    const Version& requiredVersion) {
    std::unique_lock lock(mutex_);
    LOG_F(INFO, "Adding dependency from {} to {} with required version: {}",
          from, to, requiredVersion.toString());

    validateVersion(from, to, requiredVersion);

    adjList_[from].insert(to);
    incomingEdges_[to].insert(from);
    LOG_F(INFO, "Dependency from {} to {} added successfully.", from, to);
}

void DependencyGraph::removeNode(const Node& node) {
    std::unique_lock lock(mutex_);
    LOG_F(INFO, "Removing node: {}", node);

    adjList_.erase(node);
    incomingEdges_.erase(node);
    nodeVersions_.erase(node);

    for (auto& [key, neighbors] : adjList_) {
        neighbors.erase(node);
    }
    for (auto& [key, sources] : incomingEdges_) {
        sources.erase(node);
    }

    LOG_F(INFO, "Node {} removed successfully.", node);
}

void DependencyGraph::removeDependency(const Node& from, const Node& to) {
    std::unique_lock lock(mutex_);
    LOG_F(INFO, "Removing dependency from {} to {}", from, to);

    if (adjList_.find(from) != adjList_.end()) {
        adjList_[from].erase(to);
    }
    if (incomingEdges_.find(to) != incomingEdges_.end()) {
        incomingEdges_[to].erase(from);
    }

    LOG_F(INFO, "Dependency from {} to {} removed successfully.", from, to);
}

auto DependencyGraph::getDependencies(const Node& node) const
    -> std::vector<Node> {
    std::shared_lock lock(mutex_);
    if (adjList_.find(node) == adjList_.end()) {
        LOG_F(WARNING, "Node {} not found when retrieving dependencies.", node);
        return {};
    }
    std::vector<Node> deps(adjList_.at(node).begin(), adjList_.at(node).end());
    LOG_F(INFO, "Retrieved {} dependencies for node {}.", deps.size(), node);
    return deps;
}

auto DependencyGraph::getDependents(const Node& node) const
    -> std::vector<Node> {
    std::shared_lock lock(mutex_);
    if (incomingEdges_.find(node) == incomingEdges_.end()) {
        LOG_F(WARNING, "Node {} not found when retrieving dependents.", node);
        return {};
    }
    std::vector<Node> dependents(incomingEdges_.at(node).begin(),
                                 incomingEdges_.at(node).end());
    LOG_F(INFO, "Retrieved {} dependents for node {}.", dependents.size(),
          node);
    return dependents;
}

auto DependencyGraph::hasCycle() const -> bool {
    std::shared_lock lock(mutex_);
    LOG_F(INFO, "Checking for cycles in the dependency graph.");
    std::unordered_set<Node> visited;
    std::unordered_set<Node> recStack;

    for (const auto& [node, _] : adjList_) {
        if (hasCycleUtil(node, visited, recStack)) {
            LOG_F(ERROR, "Cycle detected in the graph.");
            return true;
        }
    }
    LOG_F(INFO, "No cycles detected.");
    return false;
}

auto DependencyGraph::topologicalSort() const
    -> std::optional<std::vector<Node>> {
    std::shared_lock lock(mutex_);
    LOG_F(INFO, "Performing topological sort.");
    std::unordered_set<Node> visited;
    std::stack<Node> stack;

    for (const auto& [node, _] : adjList_) {
        if (!visited.contains(node)) {
            if (!topologicalSortUtil(node, visited, stack)) {
                LOG_F(ERROR, "Cycle detected during topological sort.");
                return std::nullopt;
            }
        }
    }

    std::vector<Node> sortedNodes;
    while (!stack.empty()) {
        sortedNodes.emplace_back(stack.top());
        stack.pop();
    }

    LOG_F(INFO, "Topological sort completed successfully with {} nodes.",
          sortedNodes.size());
    return sortedNodes;
}

auto DependencyGraph::resolveDependencies(const std::vector<Node>& directories)
    -> std::vector<Node> {
    LOG_F(INFO, "Resolving dependencies for directories.");
    DependencyGraph graph;

    const std::vector<std::string> FILE_TYPES = {"package.json", "package.xml",
                                                 "package.yaml"};

    for (const auto& dir : directories) {
        for (const auto& file : FILE_TYPES) {
            std::string filePath = dir;
            filePath.append(Constants::PATH_SEPARATOR).append(file);
            if (std::filesystem::exists(filePath)) {
                LOG_F(INFO, "Parsing {} in directory: {}", file, dir);
                auto [package_name, deps] =
                    (file == "package.json")  ? parsePackageJson(filePath)
                    : (file == "package.xml") ? parsePackageXml(filePath)
                                              : parsePackageYaml(filePath);

                graph.addNode(package_name, deps.at(package_name));

                for (const auto& [depName, version] : deps) {
                    if (depName != package_name) {
                        graph.addNode(depName, version);
                        graph.addDependency(package_name, depName, version);
                    }
                }
            } else {
                LOG_F(WARNING, "File {} does not exist in directory: {}", file,
                      dir);
            }
        }
    }

    if (graph.hasCycle()) {
        LOG_F(ERROR, "Circular dependency detected.");
        return {};
    }

    auto sortedPackagesOpt = graph.topologicalSort();
    if (!sortedPackagesOpt) {
        LOG_F(ERROR, "Failed to sort packages.");
        return {};
    }

    LOG_F(INFO, "Dependencies resolved successfully with {} packages.",
          sortedPackagesOpt->size());
    return removeDuplicates(sortedPackagesOpt.value());
}

auto DependencyGraph::resolveSystemDependencies(
    const std::vector<Node>& directories)
    -> std::unordered_map<std::string, Version> {
    LOG_F(INFO, "Resolving system dependencies for directories.");
    std::unordered_map<std::string, Version> systemDeps;
    const std::vector<std::string> FILE_TYPES = {"package.json", "package.xml",
                                                 "package.yaml"};

    for (const auto& dir : directories) {
        for (const auto& file : FILE_TYPES) {
            std::string filePath = dir;
            filePath.append(Constants::PATH_SEPARATOR).append(file);
            if (std::filesystem::exists(filePath)) {
                LOG_F(INFO, "Parsing {} in directory: {}", file, dir);
                auto [package_name, deps] =
                    (file == "package.json")  ? parsePackageJson(filePath)
                    : (file == "package.xml") ? parsePackageXml(filePath)
                                              : parsePackageYaml(filePath);

                for (const auto& [depName, version] : deps) {
                    if (depName.rfind("system:", 0) == 0) {
                        std::string systemDepName = depName.substr(7);
                        if (systemDeps.find(systemDepName) ==
                            systemDeps.end()) {
                            systemDeps[systemDepName] = version;
                            LOG_F(INFO,
                                  "Added system dependency: {} with version {}",
                                  systemDepName, version.toString());
                        } else {
                            if (systemDeps[systemDepName] < version) {
                                systemDeps[systemDepName] = version;
                                LOG_F(INFO,
                                      "Updated system dependency: {} to "
                                      "version {}",
                                      systemDepName, version.toString());
                            }
                        }
                    }
                }
            } else {
                LOG_F(WARNING, "File {} does not exist in directory: {}", file,
                      dir);
            }
        }
    }

    LOG_F(INFO,
          "System dependencies resolved successfully with {} system "
          "dependencies.",
          systemDeps.size());
    return atom::utils::unique(systemDeps);
}

auto DependencyGraph::removeDuplicates(const std::vector<Node>& input)
    -> std::vector<Node> {
    LOG_F(INFO, "Removing duplicates from dependency list.");
    std::unordered_set<Node> uniqueNodes;
    std::vector<Node> result;
    for (const auto& node : input) {
        if (uniqueNodes.find(node) == uniqueNodes.end()) {
            uniqueNodes.insert(node);
            result.emplace_back(node);
        }
    }
    LOG_F(INFO, "Duplicates removed. {} unique nodes remain.", result.size());
    return result;
}

auto DependencyGraph::parsePackageJson(const Node& path)
    -> std::pair<Node, std::unordered_map<Node, Version>> {
    LOG_F(INFO, "Parsing package.json file: {}", path);
    std::ifstream file(path);
    if (!file.is_open()) {
        LOG_F(ERROR, "Failed to open package.json file: {}", path);
        THROW_FAIL_TO_OPEN_FILE("Failed to open " + path);
    }

    json packageJson;
    try {
        file >> packageJson;
    } catch (const json::exception& e) {
        LOG_F(ERROR, "Error parsing JSON in file: {}: {}", path, e.what());
        THROW_JSON_PARSE_ERROR("Error parsing JSON in " + path + ": " +
                               e.what());
    }

    if (!packageJson.contains("name")) {
        LOG_F(ERROR, "Missing package name in file: {}", path);
        THROW_MISSING_ARGUMENT("Missing package name in " + path);
    }

    std::string packageName = packageJson["name"];
    std::unordered_map<std::string, Version> deps;

    if (packageJson.contains("dependencies")) {
        for (const auto& [key, value] : packageJson["dependencies"].items()) {
            try {
                deps[key] = Version::parse(value.get<std::string>());
            } catch (const std::exception& e) {
                LOG_F(ERROR, "Error parsing version for dependency {}: {}", key,
                      e.what());
                THROW_INVALID_ARGUMENT("Error parsing version for dependency " +
                                       key + ": " + e.what());
            }
        }
    }

    file.close();
    LOG_F(INFO, "Parsed package.json file: {} successfully.", path);
    return {packageName, deps};
}

auto DependencyGraph::parsePackageXml(const Node& path)
    -> std::pair<Node, std::unordered_map<Node, Version>> {
    LOG_F(INFO, "Parsing package.xml file: {}", path);
    XMLDocument doc;
    if (doc.LoadFile(path.c_str()) != XML_SUCCESS) {
        LOG_F(ERROR, "Failed to open package.xml file: {}", path);
        THROW_FAIL_TO_OPEN_FILE("Failed to open " + path);
    }

    XMLElement* root = doc.FirstChildElement("package");
    if (!root) {
        LOG_F(ERROR, "Missing root element in package.xml file: {}", path);
        THROW_MISSING_ARGUMENT("Missing root element in " + path);
    }

    XMLElement* nameElement = root->FirstChildElement("name");
    if (!nameElement || !nameElement->GetText()) {
        LOG_F(ERROR, "Missing package name in package.xml file: {}", path);
        THROW_MISSING_ARGUMENT("Missing package name in " + path);
    }
    std::string packageName = nameElement->GetText();

    std::unordered_map<std::string, Version> deps;
    for (XMLElement* dependElement = root->FirstChildElement("depend");
         dependElement;
         dependElement = dependElement->NextSiblingElement("depend")) {
        if (dependElement->GetText()) {
            deps[dependElement->GetText()] = Version{};
        }
    }

    LOG_F(INFO, "Parsed package.xml file: {} successfully.", path);
    return {packageName, deps};
}

auto DependencyGraph::parsePackageYaml(const std::string& path)
    -> std::pair<std::string, std::unordered_map<std::string, Version>> {
    LOG_F(INFO, "Parsing package.yaml file: {}", path);
    YAML::Node config;
    try {
        config = YAML::LoadFile(path);
    } catch (const YAML::Exception& e) {
        LOG_F(ERROR, "Error loading YAML file: {}: {}", path, e.what());
        THROW_FAIL_TO_OPEN_FILE("Error loading YAML file: " + path + ": " +
                                e.what());
    }

    if (!config["name"]) {
        LOG_F(ERROR, "Missing package name in file: {}", path);
        THROW_MISSING_ARGUMENT("Missing package name in " + path);
    }

    auto packageName = config["name"].as<std::string>();
    std::unordered_map<std::string, Version> deps;

    if (config["dependencies"]) {
        for (const auto& dep : config["dependencies"]) {
            try {
                deps[dep.first.as<std::string>()] =
                    Version::parse(dep.second.as<std::string>());
            } catch (const std::exception& e) {
                LOG_F(ERROR, "Error parsing version for dependency {}: {}",
                      dep.first.as<std::string>(), e.what());
                THROW_INVALID_ARGUMENT("Error parsing version for dependency " +
                                       dep.first.as<std::string>() + ": " +
                                       e.what());
            }
        }
    }

    LOG_F(INFO, "Parsed package.yaml file: {} successfully.", path);
    return {packageName, deps};
}

auto DependencyGraph::hasCycleUtil(
    const Node& node, std::unordered_set<Node>& visited,
    std::unordered_set<Node>& recStack) const -> bool {
    if (!visited.contains(node)) {
        visited.insert(node);
        recStack.insert(node);

        for (const auto& neighbor : adjList_.at(node)) {
            if (!visited.contains(neighbor) &&
                hasCycleUtil(neighbor, visited, recStack)) {
                return true;
            }
            if (recStack.contains(neighbor)) {
                return true;
            }
        }
    }
    recStack.erase(node);
    return false;
}

auto DependencyGraph::topologicalSortUtil(
    const Node& node, std::unordered_set<Node>& visited,
    std::stack<Node>& stack) const -> bool {
    visited.insert(node);

    for (const auto& neighbor : adjList_.at(node)) {
        if (!visited.contains(neighbor)) {
            if (!topologicalSortUtil(neighbor, visited, stack)) {
                return false;
            }
        }
    }

    stack.push(node);
    return true;
}

auto DependencyGraph::getAllDependencies(const Node& node) const
    -> std::unordered_set<Node> {
    std::shared_lock lock(mutex_);
    LOG_F(INFO, "Getting all dependencies for node: {}", node);
    std::unordered_set<Node> allDependencies;
    getAllDependenciesUtil(node, allDependencies);
    LOG_F(INFO,
          "All dependencies for node {} retrieved successfully. {} "
          "dependencies found.",
          node, allDependencies.size());
    return allDependencies;
}

void DependencyGraph::getAllDependenciesUtil(
    const Node& node, std::unordered_set<Node>& allDependencies) const {
    for (const auto& neighbor : adjList_.at(node)) {
        if (allDependencies.find(neighbor) == allDependencies.end()) {
            allDependencies.insert(neighbor);
            getAllDependenciesUtil(neighbor, allDependencies);
        }
    }
}

void DependencyGraph::loadNodesInParallel(
    std::function<void(const Node&)> loadFunction) const {
    LOG_F(INFO, "Loading nodes in parallel.");
    std::shared_lock lock(mutex_);
    std::vector<std::future<void>> futures;
    for (const auto& [node, _] : adjList_) {
        futures.emplace_back(
            std::async(std::launch::async, loadFunction, node));
    }
    for (auto& fut : futures) {
        try {
            fut.get();
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Error loading node: {}", e.what());
        }
    }
    LOG_F(INFO, "All nodes loaded in parallel successfully.");
}

void DependencyGraph::setPriority(const Node& node, int priority) {
    std::unique_lock lock(mutex_);
    LOG_F(INFO, "Setting priority {} for node {}", priority, node);
    priorities_[node] = priority;
}

auto DependencyGraph::detectVersionConflicts() const
    -> std::vector<std::tuple<Node, Node, Version, Version>> {
    std::shared_lock lock(mutex_);
    std::vector<std::tuple<Node, Node, Version, Version>> conflicts;

    for (const auto& [node, deps] : adjList_) {
        for (const auto& dep : deps) {
            const auto& required = nodeVersions_.at(dep);
            for (const auto& [otherNode, otherDeps] : adjList_) {
                if (otherNode != node && otherDeps.contains(dep)) {
                    const auto& otherRequired = nodeVersions_.at(dep);
                    if (required != otherRequired) {
                        conflicts.emplace_back(node, otherNode, required,
                                               otherRequired);
                    }
                }
            }
        }
    }
    return conflicts;
}

void DependencyGraph::addGroup(const std::string& groupName,
                               const std::vector<Node>& nodes) {
    std::unique_lock lock(mutex_);
    LOG_F(INFO, "Adding group {} with {} nodes", groupName, nodes.size());
    groups_[groupName] = nodes;
}

auto DependencyGraph::getGroupDependencies(const std::string& groupName) const
    -> std::vector<Node> {
    std::shared_lock lock(mutex_);
    if (!groups_.contains(groupName)) {
        LOG_F(WARNING, "Group {} not found", groupName);
        return {};
    }

    std::unordered_set<Node> result;
    for (const auto& node : groups_.at(groupName)) {
        auto deps = getAllDependencies(node);
        result.insert(deps.begin(), deps.end());
    }
    return std::vector<Node>(result.begin(), result.end());
}

void DependencyGraph::clearCache() {
    std::unique_lock lock(mutex_);
    LOG_F(INFO, "Clearing dependency cache");
    dependencyCache_.clear();
}

auto DependencyGraph::resolveParallelDependencies(
    const std::vector<Node>& directories) -> std::vector<Node> {
    const size_t BATCH_SIZE = 4;
    std::vector<std::future<std::vector<Node>>> futures;
    std::vector<Node> result;

    for (size_t i = 0; i < directories.size(); i += BATCH_SIZE) {
        auto end = std::min(i + BATCH_SIZE, directories.size());
        std::vector<Node> batch(directories.begin() + i,
                                directories.begin() + end);

        futures.push_back(std::async(std::launch::async, [this, batch]() {
            return resolveParallelBatch(batch);
        }));
    }

    for (auto& future : futures) {
        auto batchResult = future.get();
        result.insert(result.end(), batchResult.begin(), batchResult.end());
    }

    return removeDuplicates(result);
}

auto DependencyGraph::resolveParallelBatch(const std::vector<Node>& batch)
    -> std::vector<Node> {
    std::vector<Node> batchResult;
    for (const auto& dir : batch) {
        if (dependencyCache_.contains(dir)) {
            batchResult.insert(batchResult.end(), dependencyCache_[dir].begin(),
                               dependencyCache_[dir].end());
            continue;
        }

        auto deps = resolveDependencies({dir});
        {
            std::unique_lock lock(mutex_);
            dependencyCache_[dir] = deps;
        }
        batchResult.insert(batchResult.end(), deps.begin(), deps.end());
    }
    return batchResult;
}

auto DependencyGraph::validateDependencies(const Node& node) const -> bool {
    std::shared_lock lock(mutex_);
    DLOG_F(INFO, "Validating dependencies for node: {}", node);

    // 检查节点是否存在
    if (adjList_.find(node) == adjList_.end()) {
        LOG_F(ERROR, "Node {} not found in dependency graph", node);
        return false;
    }

    // 获取该节点的所有依赖
    auto deps = getAllDependencies(node);

    // 验证每个依赖
    for (const auto& dep : deps) {
        // 检查依赖节点是否存在
        if (adjList_.find(dep) == adjList_.end()) {
            LOG_F(ERROR, "Dependency {} not found for node {}", dep, node);
            return false;
        }

        // 检查是否存在版本冲突
        if (nodeVersions_.find(dep) != nodeVersions_.end()) {
            try {
                for (const auto& [otherNode, otherDeps] : adjList_) {
                    if (otherDeps.contains(dep)) {
                        // 验证版本兼容性
                        const auto& requiredVersion = nodeVersions_.at(dep);
                        validateVersion(node, dep, requiredVersion);
                    }
                }
            } catch (const std::exception& e) {
                LOG_F(ERROR,
                      "Version validation failed for dependency {} of node {}: "
                      "{}",
                      dep, node, e.what());
                return false;
            }
        }
    }

    DLOG_F(INFO, "All dependencies validated successfully for node {}", node);
    return true;
}

}  // namespace lithium
