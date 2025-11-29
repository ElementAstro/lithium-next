#include "dependency.hpp"
#include "version.hpp"

#include <algorithm>
#include <exception>
#include <filesystem>
#include <fstream>
#include <future>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <utility>

#include "atom/error/exception.hpp"
#include "atom/type/json.hpp"
#include "atom/utils/container.hpp"
#include "extra/tinyxml2/tinyxml2.h"
#include "spdlog/spdlog.h"

#if __has_include(<yaml-cpp/yaml.h>)
#include <yaml-cpp/yaml.h>
#endif

#include "constant/constant.hpp"

using namespace tinyxml2;

namespace lithium {

DependencyGraph::DependencyGraph() noexcept {
    spdlog::info("Creating dependency graph.");
}

void DependencyGraph::clear() {
    std::unique_lock lock(mutex_);
    spdlog::info("Clearing dependency graph.");
    adjList_.clear();
    incomingEdges_.clear();
    nodeVersions_.clear();
    priorities_.clear();
    dependencyCache_.clear();
}

void DependencyGraph::addNode(const Node& node, const Version& version) {
    if (node.empty()) {
        spdlog::error("Cannot add node with empty name.");
        THROW_INVALID_ARGUMENT("Node name cannot be empty");
    }

    std::unique_lock lock(mutex_);
    spdlog::info("Adding node: {} with version: {}", node, version.toString());

    adjList_.try_emplace(node);
    incomingEdges_.try_emplace(node);
    nodeVersions_[node] = version;

    spdlog::info("Node {} added successfully.", node);
}

bool DependencyGraph::nodeExists(const Node& node) const noexcept {
    std::shared_lock lock(mutex_);
    return adjList_.contains(node);
}

auto DependencyGraph::getNodeVersion(const Node& node) const noexcept
    -> std::optional<Version> {
    std::shared_lock lock(mutex_);
    auto it = nodeVersions_.find(node);
    if (it != nodeVersions_.end()) {
        return it->second;
    }
    return std::nullopt;
}

void DependencyGraph::validateVersion(const Node& from, const Node& to,
                                      const Version& requiredVersion) const {
    std::shared_lock lock(mutex_);

    auto toVersion = getNodeVersion(to);
    if (!toVersion) {
        spdlog::error("Dependency {} not found for node {}.", to, from);
        THROW_INVALID_ARGUMENT("Dependency " + to + " not found for node " +
                               from);
    }

    if (*toVersion < requiredVersion) {
        spdlog::error(
            "Version requirement not satisfied for dependency {} -> {}. "
            "Required: {}, Found: {}",
            from, to, requiredVersion.toString(), toVersion->toString());
        THROW_INVALID_ARGUMENT(
            "Version requirement not satisfied for dependency " + from +
            " -> " + to + ". Required: " + requiredVersion.toString() +
            ", Found: " + toVersion->toString());
    }
}

void DependencyGraph::addDependency(const Node& from, const Node& to,
                                    const Version& requiredVersion) {
    if (from.empty() || to.empty()) {
        spdlog::error(
            "Cannot add dependency with empty node name. From: '{}', To: '{}'",
            from, to);
        THROW_INVALID_ARGUMENT("Node names cannot be empty");
    }

    if (from == to) {
        spdlog::error("Self-dependency detected: {}", from);
        THROW_INVALID_ARGUMENT("Self-dependency not allowed: " + from);
    }

    try {
        validateVersion(from, to, requiredVersion);
    } catch (const std::exception& e) {
        spdlog::error("Version validation failed: {}", e.what());
        throw;
    }

    std::unique_lock lock(mutex_);
    spdlog::info("Adding dependency from {} to {} with required version: {}",
                 from, to, requiredVersion.toString());

    adjList_[from].insert(to);
    incomingEdges_[to].insert(from);
    spdlog::info("Dependency from {} to {} added successfully.", from, to);
}

void DependencyGraph::removeNode(const Node& node) noexcept {
    std::unique_lock lock(mutex_);
    spdlog::info("Removing node: {}", node);

    adjList_.erase(node);
    incomingEdges_.erase(node);
    nodeVersions_.erase(node);
    priorities_.erase(node);
    dependencyCache_.erase(node);

    for (auto& [key, neighbors] : adjList_) {
        neighbors.erase(node);
    }
    for (auto& [key, sources] : incomingEdges_) {
        sources.erase(node);
    }

    spdlog::info("Node {} removed successfully.", node);
}

void DependencyGraph::removeDependency(const Node& from,
                                       const Node& to) noexcept {
    std::unique_lock lock(mutex_);
    spdlog::info("Removing dependency from {} to {}", from, to);

    if (adjList_.find(from) != adjList_.end()) {
        adjList_[from].erase(to);
    }
    if (incomingEdges_.find(to) != incomingEdges_.end()) {
        incomingEdges_[to].erase(from);
    }

    spdlog::info("Dependency from {} to {} removed successfully.", from, to);
}

auto DependencyGraph::getDependencies(const Node& node) const noexcept
    -> std::vector<Node> {
    std::shared_lock lock(mutex_);
    if (adjList_.find(node) == adjList_.end()) {
        spdlog::warn("Node {} not found when retrieving dependencies.", node);
        return {};
    }

    const auto& dependencies = adjList_.at(node);
    std::vector<Node> deps(dependencies.begin(), dependencies.end());
    spdlog::info("Retrieved {} dependencies for node {}.", deps.size(), node);
    return deps;
}

auto DependencyGraph::getDependents(const Node& node) const noexcept
    -> std::vector<Node> {
    std::shared_lock lock(mutex_);
    if (incomingEdges_.find(node) == incomingEdges_.end()) {
        spdlog::warn("Node {} not found when retrieving dependents.", node);
        return {};
    }

    const auto& dependents = incomingEdges_.at(node);
    std::vector<Node> result(dependents.begin(), dependents.end());
    spdlog::info("Retrieved {} dependents for node {}.", result.size(), node);
    return result;
}

auto DependencyGraph::hasCycle() const noexcept -> bool {
    std::shared_lock lock(mutex_);
    spdlog::info("Checking for cycles in the dependency graph.");
    std::unordered_set<Node> visited;
    std::unordered_set<Node> recStack;

    try {
        for (const auto& [node, _] : adjList_) {
            if (hasCycleUtil(node, visited, recStack)) {
                spdlog::error("Cycle detected in the graph.");
                return true;
            }
        }
    } catch (const std::exception& e) {
        spdlog::error("Error while checking for cycles: {}", e.what());
        return true;
    }

    spdlog::info("No cycles detected.");
    return false;
}

auto DependencyGraph::topologicalSort() const noexcept
    -> std::optional<std::vector<Node>> {
    try {
        std::shared_lock lock(mutex_);
        spdlog::info("Performing topological sort.");
        std::unordered_set<Node> visited;
        std::stack<Node> stack;

        for (const auto& [node, _] : adjList_) {
            if (!visited.contains(node)) {
                if (!topologicalSortUtil(node, visited, stack)) {
                    spdlog::error("Cycle detected during topological sort.");
                    return std::nullopt;
                }
            }
        }

        std::vector<Node> sortedNodes;
        sortedNodes.reserve(stack.size());

        while (!stack.empty()) {
            sortedNodes.push_back(stack.top());
            stack.pop();
        }

        spdlog::info("Topological sort completed successfully with {} nodes.",
                     sortedNodes.size());
        return sortedNodes;
    } catch (const std::exception& e) {
        spdlog::error("Error during topological sort: {}", e.what());
        return std::nullopt;
    }
}

auto DependencyGraph::resolveDependencies(std::span<const Node> directories)
    -> std::vector<Node> {
    spdlog::info("Resolving dependencies for {} directories.",
                 directories.size());

    if (directories.empty()) {
        spdlog::warn("No directories provided for dependency resolution.");
        return {};
    }

    try {
        DependencyGraph graph;
        const std::vector<std::string> FILE_TYPES = {
            "package.json", "package.xml", "package.yaml"};

        for (const auto& dir : directories) {
            bool fileFound = false;

            for (const auto& file : FILE_TYPES) {
                std::string filePath = dir;
                filePath.append(Constants::PATH_SEPARATOR).append(file);

                if (std::filesystem::exists(filePath)) {
                    spdlog::info("Parsing {} in directory: {}", file, dir);
                    fileFound = true;

                    auto [package_name, deps] =
                        (file == "package.json")  ? parsePackageJson(filePath)
                        : (file == "package.xml") ? parsePackageXml(filePath)
                                                  : parsePackageYaml(filePath);

                    if (package_name.empty()) {
                        spdlog::error("Empty package name in {}", filePath);
                        continue;
                    }

                    graph.addNode(package_name, deps.at(package_name));

                    for (const auto& [depName, version] : deps) {
                        if (depName != package_name) {
                            graph.addNode(depName, version);
                            graph.addDependency(package_name, depName, version);
                        }
                    }
                }
            }

            if (!fileFound) {
                spdlog::warn("No package files found in directory: {}", dir);
            }
        }

        if (graph.hasCycle()) {
            spdlog::error("Circular dependency detected.");
            THROW_RUNTIME_ERROR("Circular dependency detected.");
        }

        auto sortedPackagesOpt = graph.topologicalSort();
        if (!sortedPackagesOpt) {
            spdlog::error("Failed to sort packages.");
            THROW_RUNTIME_ERROR(
                "Failed to perform topological sort on dependencies.");
        }

        spdlog::info("Dependencies resolved successfully with {} packages.",
                     sortedPackagesOpt->size());

        return removeDuplicates(*sortedPackagesOpt);
    } catch (const std::exception& e) {
        spdlog::error("Error resolving dependencies: {}", e.what());
        throw;
    }
}

auto DependencyGraph::resolveSystemDependencies(
    std::span<const Node> directories)
    -> std::unordered_map<std::string, Version> {
    spdlog::info("Resolving system dependencies for {} directories.",
                 directories.size());

    try {
        std::unordered_map<std::string, Version> systemDeps;
        const std::vector<std::string> FILE_TYPES = {
            "package.json", "package.xml", "package.yaml"};

        for (const auto& dir : directories) {
            for (const auto& file : FILE_TYPES) {
                std::string filePath = dir;
                filePath.append(Constants::PATH_SEPARATOR).append(file);

                if (std::filesystem::exists(filePath)) {
                    spdlog::info("Parsing {} in directory: {}", file, dir);

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
                                spdlog::info(
                                    "Added system dependency: {} with "
                                    "version {}",
                                    systemDepName, version.toString());
                            } else {
                                if (systemDeps[systemDepName] < version) {
                                    systemDeps[systemDepName] = version;
                                    spdlog::info(
                                        "Updated system dependency: {} to "
                                        "version {}",
                                        systemDepName, version.toString());
                                }
                            }
                        }
                    }
                }
            }
        }

        spdlog::info(
            "System dependencies resolved successfully with {} system "
            "dependencies.",
            systemDeps.size());

        return atom::utils::unique(systemDeps);
    } catch (const std::exception& e) {
        spdlog::error("Error resolving system dependencies: {}", e.what());
        throw;
    }
}

auto DependencyGraph::removeDuplicates(std::span<const Node> input) noexcept
    -> std::vector<Node> {
    spdlog::info("Removing duplicates from dependency list with {} items.",
                 input.size());

    std::unordered_set<Node> uniqueNodes;
    std::vector<Node> result;
    result.reserve(input.size());

    for (const auto& node : input) {
        if (!uniqueNodes.contains(node)) {
            uniqueNodes.insert(node);
            result.push_back(node);
        }
    }

    spdlog::info("Duplicates removed. {} unique nodes remain.", result.size());
    return result;
}

auto DependencyGraph::parsePackageJson(std::string_view path)
    -> std::pair<Node, std::unordered_map<Node, Version>> {
    spdlog::info("Parsing package.json file: {}", path);

    std::ifstream file(path.data());
    if (!file.is_open()) {
        spdlog::error("Failed to open package.json file: {}", path);
        THROW_FAIL_TO_OPEN_FILE("Failed to open " + std::string(path));
    }

    json packageJson;
    try {
        file >> packageJson;
    } catch (const json::exception& e) {
        spdlog::error("Error parsing JSON in file: {}: {}", path, e.what());
        THROW_JSON_PARSE_ERROR("Error parsing JSON in " + std::string(path) +
                               ": " + e.what());
    }

    if (!packageJson.contains("name")) {
        spdlog::error("Missing package name in file: {}", path);
        THROW_MISSING_ARGUMENT("Missing package name in " + std::string(path));
    }

    std::string packageName = packageJson["name"];
    std::unordered_map<std::string, Version> deps;

    try {
        std::string version = packageJson.value("version", "0.0.0");
        deps[packageName] = Version::parse(version);
    } catch (const std::exception& e) {
        spdlog::error("Error parsing package version: {}", e.what());
        deps[packageName] = Version{};
    }

    if (packageJson.contains("dependencies") &&
        packageJson["dependencies"].is_object()) {
        for (const auto& [key, value] : packageJson["dependencies"].items()) {
            try {
                if (value.is_string()) {
                    deps[key] = Version::parse(value.get<std::string>());
                } else {
                    spdlog::warn("Dependency {} value is not a string", key);
                    deps[key] = Version{};
                }
            } catch (const std::exception& e) {
                spdlog::error("Error parsing version for dependency {}: {}",
                              key, e.what());
                THROW_INVALID_ARGUMENT("Error parsing version for dependency " +
                                       key + ": " + e.what());
            }
        }
    }

    file.close();
    spdlog::info(
        "Parsed package.json file: {} successfully with {} dependencies.", path,
        deps.size());
    return {packageName, deps};
}

auto DependencyGraph::parsePackageXml(std::string_view path)
    -> std::pair<Node, std::unordered_map<Node, Version>> {
    spdlog::info("Parsing package.xml file: {}", path);
    XMLDocument doc;
    if (doc.LoadFile(path.data()) != XML_SUCCESS) {
        spdlog::error("Failed to open package.xml file: {}", path);
        THROW_FAIL_TO_OPEN_FILE("Failed to open " + std::string(path));
    }

    XMLElement* root = doc.FirstChildElement("package");
    if (!root) {
        spdlog::error("Missing root element in package.xml file: {}", path);
        THROW_MISSING_ARGUMENT("Missing root element in " + std::string(path));
    }

    XMLElement* nameElement = root->FirstChildElement("name");
    if (!nameElement || !nameElement->GetText()) {
        spdlog::error("Missing package name in package.xml file: {}", path);
        THROW_MISSING_ARGUMENT("Missing package name in " + std::string(path));
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

    spdlog::info("Parsed package.xml file: {} successfully.", path);
    return {packageName, deps};
}

auto DependencyGraph::parsePackageYaml(std::string_view path)
    -> std::pair<Node, std::unordered_map<Node, Version>> {
    spdlog::info("Parsing package.yaml file: {}", path);
    YAML::Node config;
    try {
        config = YAML::LoadFile(path.data());
    } catch (const YAML::Exception& e) {
        spdlog::error("Error loading YAML file: {}: {}", path, e.what());
        THROW_FAIL_TO_OPEN_FILE(
            "Error loading YAML file: " + std::string(path) + ": " + e.what());
    }

    if (!config["name"]) {
        spdlog::error("Missing package name in file: {}", path);
        THROW_MISSING_ARGUMENT("Missing package name in " + std::string(path));
    }

    auto packageName = config["name"].as<std::string>();
    std::unordered_map<std::string, Version> deps;

    if (config["dependencies"]) {
        for (const auto& dep : config["dependencies"]) {
            try {
                deps[dep.first.as<std::string>()] =
                    Version::parse(dep.second.as<std::string>());
            } catch (const std::exception& e) {
                spdlog::error("Error parsing version for dependency {}: {}",
                              dep.first.as<std::string>(), e.what());
                THROW_INVALID_ARGUMENT("Error parsing version for dependency " +
                                       dep.first.as<std::string>() + ": " +
                                       e.what());
            }
        }
    }

    spdlog::info("Parsed package.yaml file: {} successfully.", path);
    return {packageName, deps};
}

auto DependencyGraph::hasCycleUtil(
    const Node& node, std::unordered_set<Node>& visited,
    std::unordered_set<Node>& recStack) const noexcept -> bool {
    if (!visited.contains(node)) {
        visited.insert(node);
        recStack.insert(node);

        try {
            const auto& neighbors = adjList_.at(node);

            for (const auto& neighbor : neighbors) {
                if (!visited.contains(neighbor) &&
                    hasCycleUtil(neighbor, visited, recStack)) {
                    return true;
                } else if (recStack.contains(neighbor)) {
                    spdlog::warn("Cycle detected: {} -> {}", node, neighbor);
                    return true;
                }
            }
        } catch (const std::exception& e) {
            spdlog::error("Error checking for cycles at node {}: {}", node,
                          e.what());
            return false;
        }
    }

    recStack.erase(node);
    return false;
}

auto DependencyGraph::topologicalSortUtil(
    const Node& node, std::unordered_set<Node>& visited,
    std::stack<Node>& stack) const noexcept -> bool {
    visited.insert(node);

    try {
        const auto& neighbors = adjList_.at(node);

        for (const auto& neighbor : neighbors) {
            if (!visited.contains(neighbor)) {
                if (!topologicalSortUtil(neighbor, visited, stack)) {
                    return false;
                }
            }
        }
    } catch (const std::exception& e) {
        spdlog::error("Error during topological sort at node {}: {}", node,
                      e.what());
        return false;
    }

    stack.push(node);
    return true;
}

auto DependencyGraph::getAllDependencies(const Node& node) const noexcept
    -> std::unordered_set<Node> {
    std::shared_lock lock(mutex_);
    spdlog::info("Getting all dependencies for node: {}", node);

    std::unordered_set<Node> allDependencies;
    try {
        getAllDependenciesUtil(node, allDependencies);
        spdlog::info(
            "All dependencies for node {} retrieved successfully. {} "
            "dependencies found.",
            node, allDependencies.size());
    } catch (const std::exception& e) {
        spdlog::error("Error getting all dependencies for node {}: {}", node,
                      e.what());
    }

    return allDependencies;
}

void DependencyGraph::getAllDependenciesUtil(
    const Node& node,
    std::unordered_set<Node>& allDependencies) const noexcept {
    try {
        if (adjList_.find(node) == adjList_.end())
            return;

        for (const auto& neighbor : adjList_.at(node)) {
            if (!allDependencies.contains(neighbor)) {
                allDependencies.insert(neighbor);
                getAllDependenciesUtil(neighbor, allDependencies);
            }
        }
    } catch (const std::exception& e) {
        spdlog::error("Error in getAllDependenciesUtil: {}", e.what());
    }
}

void DependencyGraph::loadNodesInParallel(
    std::invocable<const Node&> auto loadFunction) const {
    spdlog::info("Loading nodes in parallel.");
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
            spdlog::error("Error loading node: {}", e.what());
        }
    }

    spdlog::info("All nodes loaded in parallel successfully.");
}

void DependencyGraph::setPriority(const Node& node, int priority) noexcept {
    std::unique_lock lock(mutex_);
    spdlog::info("Setting priority {} for node {}", priority, node);
    priorities_[node] = priority;
}

auto DependencyGraph::detectVersionConflicts() const noexcept
    -> std::vector<std::tuple<Node, Node, Version, Version>> {
    std::shared_lock lock(mutex_);
    spdlog::info("Detecting version conflicts.");

    std::vector<std::tuple<Node, Node, Version, Version>> conflicts;

    try {
        for (const auto& [node, deps] : adjList_) {
            for (const auto& dep : deps) {
                if (nodeVersions_.find(dep) == nodeVersions_.end())
                    continue;
                const auto& required = nodeVersions_.at(dep);

                for (const auto& [otherNode, otherDeps] : adjList_) {
                    if (otherNode != node && otherDeps.contains(dep)) {
                        if (nodeVersions_.find(dep) == nodeVersions_.end())
                            continue;
                        const auto& otherRequired = nodeVersions_.at(dep);

                        if (required != otherRequired) {
                            conflicts.emplace_back(node, otherNode, required,
                                                   otherRequired);
                            spdlog::info(
                                "Version conflict detected: {} and {} "
                                "require different versions of {}",
                                node, otherNode, dep);
                        }
                    }
                }
            }
        }

        spdlog::info("Detected {} version conflicts.", conflicts.size());
    } catch (const std::exception& e) {
        spdlog::error("Error detecting version conflicts: {}", e.what());
    }

    return conflicts;
}

void DependencyGraph::addGroup(std::string_view groupName,
                               std::span<const Node> nodes) {
    if (groupName.empty()) {
        spdlog::error("Cannot add group with empty name");
        THROW_INVALID_ARGUMENT("Group name cannot be empty");
    }

    std::string groupNameStr{groupName};
    std::unique_lock lock(mutex_);
    spdlog::info("Adding group {} with {} nodes", groupNameStr, nodes.size());

    groups_[groupNameStr].clear();
    groups_[groupNameStr].reserve(nodes.size());

    for (const auto& node : nodes) {
        groups_[groupNameStr].push_back(node);
    }

    spdlog::info("Group {} added successfully", groupNameStr);
}

auto DependencyGraph::getGroupDependencies(
    std::string_view groupName) const noexcept -> std::vector<Node> {
    std::shared_lock lock(mutex_);
    std::string groupNameStr{groupName};

    if (groups_.find(groupNameStr) == groups_.end()) {
        spdlog::warn("Group {} not found", groupNameStr);
        return {};
    }

    std::unordered_set<Node> result;
    for (const auto& node : groups_.at(groupNameStr)) {
        if (!nodeExists(node))
            continue;

        auto deps = getAllDependencies(node);
        result.insert(deps.begin(), deps.end());
        result.insert(node);
    }

    spdlog::info("Retrieved {} dependencies for group {}", result.size(),
                 groupNameStr);
    return std::vector<Node>(result.begin(), result.end());
}

void DependencyGraph::clearCache() noexcept {
    try {
        std::unique_lock lock(mutex_);
        spdlog::info("Clearing dependency cache with {} entries",
                     dependencyCache_.size());
        dependencyCache_.clear();
        spdlog::info("Dependency cache cleared successfully");
    } catch (const std::exception& e) {
        spdlog::error("Error clearing dependency cache: {}", e.what());
    }
}

auto DependencyGraph::resolveParallelDependencies(
    std::span<const Node> directories) -> std::vector<Node> {
    if (directories.empty()) {
        spdlog::warn(
            "No directories provided for parallel dependency resolution");
        return {};
    }

    spdlog::info("Resolving dependencies in parallel for {} directories",
                 directories.size());

    try {
        const size_t processorCount =
            std::max(1U, std::thread::hardware_concurrency());
        const size_t BATCH_SIZE =
            std::max(size_t{1}, directories.size() / processorCount);

        spdlog::info("Using {} threads with batch size {}", processorCount,
                     BATCH_SIZE);

        std::vector<std::future<std::vector<Node>>> futures;
        std::vector<Node> result;
        result.reserve(directories.size() * 2);

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

        auto uniqueResult = removeDuplicates(result);
        spdlog::info(
            "Parallel dependency resolution completed with {} unique "
            "dependencies",
            uniqueResult.size());

        return uniqueResult;
    } catch (const std::exception& e) {
        spdlog::error("Error in parallel dependency resolution: {}", e.what());
        THROW_RUNTIME_ERROR("Error resolving dependencies in parallel: " +
                            std::string(e.what()));
    }
}

auto DependencyGraph::resolveParallelBatch(std::span<const Node> batch)
    -> std::vector<Node> {
    try {
        std::vector<Node> batchResult;

        for (const auto& dir : batch) {
            {
                std::shared_lock readLock(mutex_);
                if (dependencyCache_.contains(dir)) {
                    const auto& cachedDeps = dependencyCache_[dir];
                    spdlog::info("Using cached dependencies for {}", dir);
                    batchResult.insert(batchResult.end(), cachedDeps.begin(),
                                       cachedDeps.end());
                    continue;
                }
            }

            std::vector<Node> temp = {dir};
            auto deps = resolveDependencies(temp);
            {
                std::unique_lock writeLock(mutex_);
                dependencyCache_[dir] = deps;
            }
            batchResult.insert(batchResult.end(), deps.begin(), deps.end());
        }

        return batchResult;
    } catch (const std::exception& e) {
        spdlog::error("Error resolving batch dependencies: {}", e.what());
        throw;
    }
}

auto DependencyGraph::validateDependencies(const Node& node) const noexcept
    -> bool {
    try {
        std::shared_lock lock(mutex_);
        spdlog::debug("Validating dependencies for node: {}", node);

        if (!nodeExists(node)) {
            spdlog::error("Node {} not found in dependency graph", node);
            return false;
        }

        auto deps = getAllDependencies(node);

        for (const auto& dep : deps) {
            if (!nodeExists(dep)) {
                spdlog::error("Dependency {} not found for node {}", dep, node);
                return false;
            }

            auto depVersion = getNodeVersion(dep);
            if (depVersion) {
                try {
                    for (const auto& [otherNode, otherDeps] : adjList_) {
                        if (otherDeps.contains(dep)) {
                            validateVersion(node, dep, *depVersion);
                        }
                    }
                } catch (const std::exception& e) {
                    spdlog::error(
                        "Version validation failed for dependency {} of node "
                        "{}: "
                        "{}",
                        dep, node, e.what());
                    return false;
                }
            }
        }

        spdlog::debug("All dependencies validated successfully for node {}",
                      node);
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Error validating dependencies for node {}: {}", node,
                      e.what());
        return false;
    }
}

DependencyGraph::DependencyGenerator DependencyGraph::resolveDependenciesAsync(
    const Node& directory) {
    spdlog::info("Starting async dependency resolution for directory: {}",
                 directory);

    try {
        // Get all dependencies for this directory
        std::vector<Node> directories = {directory};
        auto resolved = resolveDependencies(directories);

        // Yield each resolved dependency one by one
        for (const auto& dep : resolved) {
            spdlog::debug("Yielding dependency: {}", dep);
            co_yield dep;
        }
    } catch (const std::exception& e) {
        spdlog::error("Error in async dependency resolution: {}", e.what());
    }

    spdlog::info("Completed async dependency resolution for directory: {}",
                 directory);
}

}  // namespace lithium
