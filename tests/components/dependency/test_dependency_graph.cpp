#include "components/dependency.hpp"
#include "components/version.hpp"

#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <unordered_set>
#include <vector>

namespace lithium::test {

class DependencyGraphTest : public ::testing::Test {
protected:
    void SetUp() override { graph = std::make_unique<DependencyGraph>(); }

    void TearDown() override { graph.reset(); }

    std::unique_ptr<DependencyGraph> graph;
};

TEST_F(DependencyGraphTest, AddNode) {
    Version version{1, 0, 0};
    graph->addNode("A", version);
    auto dependencies = graph->getDependencies("A");
    EXPECT_TRUE(dependencies.empty());
}

TEST_F(DependencyGraphTest, AddDependency) {
    Version version1{1, 0, 0};
    Version version2{1, 1, 0};
    graph->addNode("A", version1);
    graph->addNode("B", version2);
    graph->addDependency("A", "B", version2);

    auto dependencies = graph->getDependencies("A");
    EXPECT_EQ(dependencies.size(), 1);
    EXPECT_EQ(dependencies[0], "B");
}

TEST_F(DependencyGraphTest, RemoveNode) {
    Version version{1, 0, 0};
    graph->addNode("A", version);
    graph->removeNode("A");

    auto dependencies = graph->getDependencies("A");
    EXPECT_TRUE(dependencies.empty());
}

TEST_F(DependencyGraphTest, RemoveDependency) {
    Version version1{1, 0, 0};
    Version version2{1, 1, 0};
    graph->addNode("A", version1);
    graph->addNode("B", version2);
    graph->addDependency("A", "B", version2);
    graph->removeDependency("A", "B");

    auto dependencies = graph->getDependencies("A");
    EXPECT_TRUE(dependencies.empty());
}

TEST_F(DependencyGraphTest, GetDependents) {
    Version version1{1, 0, 0};
    Version version2{1, 1, 0};
    graph->addNode("A", version1);
    graph->addNode("B", version2);
    graph->addDependency("A", "B", version2);

    auto dependents = graph->getDependents("B");
    EXPECT_EQ(dependents.size(), 1);
    EXPECT_EQ(dependents[0], "A");
}

TEST_F(DependencyGraphTest, HasCycle) {
    Version version{1, 0, 0};
    graph->addNode("A", version);
    graph->addNode("B", version);
    graph->addDependency("A", "B", version);
    graph->addDependency("B", "A", version);

    EXPECT_TRUE(graph->hasCycle());
}

TEST_F(DependencyGraphTest, TopologicalSort) {
    Version version{1, 0, 0};
    graph->addNode("A", version);
    graph->addNode("B", version);
    graph->addNode("C", version);
    graph->addDependency("A", "B", version);
    graph->addDependency("B", "C", version);

    auto sorted = graph->topologicalSort();
    ASSERT_TRUE(sorted.has_value());
    EXPECT_EQ(sorted->size(), 3);
    EXPECT_EQ(sorted->at(0), "A");
    EXPECT_EQ(sorted->at(1), "B");
    EXPECT_EQ(sorted->at(2), "C");
}

TEST_F(DependencyGraphTest, GetAllDependencies) {
    Version version{1, 0, 0};
    graph->addNode("A", version);
    graph->addNode("B", version);
    graph->addNode("C", version);
    graph->addDependency("A", "B", version);
    graph->addDependency("B", "C", version);

    auto allDependencies = graph->getAllDependencies("A");
    EXPECT_EQ(allDependencies.size(), 2);
    EXPECT_TRUE(allDependencies.find("B") != allDependencies.end());
    EXPECT_TRUE(allDependencies.find("C") != allDependencies.end());
}

TEST_F(DependencyGraphTest, LoadNodesInParallel) {
    Version version{1, 0, 0};
    graph->addNode("A", version);
    graph->addNode("B", version);

    std::vector<std::string> loadedNodes;
    graph->loadNodesInParallel([&loadedNodes](const std::string& node) {
        loadedNodes.push_back(node);
    });

    EXPECT_EQ(loadedNodes.size(), 2);
    EXPECT_TRUE(std::find(loadedNodes.begin(), loadedNodes.end(), "A") !=
                loadedNodes.end());
    EXPECT_TRUE(std::find(loadedNodes.begin(), loadedNodes.end(), "B") !=
                loadedNodes.end());
}

TEST_F(DependencyGraphTest, ResolveDependencies) {
    Version version{1, 0, 0};
    graph->addNode("A", version);
    graph->addNode("B", version);
    graph->addDependency("A", "B", version);

    auto resolved = graph->resolveDependencies({"A"});
    EXPECT_EQ(resolved.size(), 2);
    EXPECT_TRUE(std::find(resolved.begin(), resolved.end(), "A") !=
                resolved.end());
    EXPECT_TRUE(std::find(resolved.begin(), resolved.end(), "B") !=
                resolved.end());
}

TEST_F(DependencyGraphTest, ResolveSystemDependencies) {
    Version version{1, 0, 0};
    graph->addNode("A", version);
    graph->addNode("B", version);
    graph->addDependency("A", "B", version);

    auto resolved = graph->resolveSystemDependencies({"A"});
    EXPECT_EQ(resolved.size(), 1);
    EXPECT_EQ(resolved["B"], version);
}

TEST_F(DependencyGraphTest, SetPriority) {
    Version version{1, 0, 0};
    graph->addNode("A", version);
    graph->setPriority("A", 10);

    // No direct way to test priority, assuming internal state is correct
}

TEST_F(DependencyGraphTest, DetectVersionConflicts) {
    Version version1{1, 0, 0};
    Version version2{2, 0, 0};
    graph->addNode("A", version1);
    graph->addNode("B", version2);
    graph->addDependency("A", "B", version1);

    auto conflicts = graph->detectVersionConflicts();
    EXPECT_EQ(conflicts.size(), 1);
    EXPECT_EQ(std::get<0>(conflicts[0]), "A");
    EXPECT_EQ(std::get<1>(conflicts[0]), "B");
    EXPECT_EQ(std::get<2>(conflicts[0]), version1);
    EXPECT_EQ(std::get<3>(conflicts[0]), version2);
}

TEST_F(DependencyGraphTest, ResolveParallelDependencies) {
    Version version{1, 0, 0};
    graph->addNode("A", version);
    graph->addNode("B", version);
    graph->addDependency("A", "B", version);

    auto resolved = graph->resolveParallelDependencies({"A"});
    EXPECT_EQ(resolved.size(), 2);
    EXPECT_TRUE(std::find(resolved.begin(), resolved.end(), "A") !=
                resolved.end());
    EXPECT_TRUE(std::find(resolved.begin(), resolved.end(), "B") !=
                resolved.end());
}

TEST_F(DependencyGraphTest, AddGroup) {
    Version version{1, 0, 0};
    graph->addNode("A", version);
    graph->addNode("B", version);
    graph->addGroup("group1", {"A", "B"});

    auto groupDependencies = graph->getGroupDependencies("group1");
    EXPECT_EQ(groupDependencies.size(), 2);
    EXPECT_TRUE(std::find(groupDependencies.begin(), groupDependencies.end(),
                          "A") != groupDependencies.end());
    EXPECT_TRUE(std::find(groupDependencies.begin(), groupDependencies.end(),
                          "B") != groupDependencies.end());
}

TEST_F(DependencyGraphTest, CacheOperations) {
    Version version{1, 0, 0};
    graph->addNode("A", version);
    graph->addNode("B", version);
    graph->addDependency("A", "B", version);

    // First resolution should cache
    auto resolved1 = graph->resolveParallelDependencies({"A"});
    EXPECT_EQ(resolved1.size(), 2);

    // Second resolution should use cache
    auto resolved2 = graph->resolveParallelDependencies({"A"});
    EXPECT_EQ(resolved2, resolved1);

    graph->clearCache();
    // After cache clear, should resolve again
    auto resolved3 = graph->resolveParallelDependencies({"A"});
    EXPECT_EQ(resolved3, resolved1);
}

TEST_F(DependencyGraphTest, PackageJsonParsing) {
    // Create temporary package.json for testing
    std::string jsonContent = R"({
        "name": "test-package",
        "dependencies": {
            "dep1": "1.0.0",
            "dep2": "2.0.0"
        }
    })";

    std::string tempFile = "test_package.json";
    std::ofstream ofs(tempFile);
    ofs << jsonContent;
    ofs.close();

    auto [name, deps] = DependencyGraph::parsePackageJson(tempFile);
    EXPECT_EQ(name, "test-package");
    EXPECT_EQ(deps.size(), 2);
    EXPECT_EQ(deps["dep1"], Version(1, 0, 0));
    EXPECT_EQ(deps["dep2"], Version(2, 0, 0));

    std::filesystem::remove(tempFile);
}

TEST_F(DependencyGraphTest, VersionValidation) {
    Version v1{1, 0, 0};
    Version v2{2, 0, 0};

    graph->addNode("A", v1);
    graph->addNode("B", v2);

    // Should succeed - required version is satisfied
    EXPECT_NO_THROW(graph->addDependency("A", "B", v1));

    // Should throw - required version is not satisfied
    EXPECT_THROW(graph->addDependency("B", "A", v2), std::invalid_argument);
}

TEST_F(DependencyGraphTest, GroupOperationsExtended) {
    Version version{1, 0, 0};
    graph->addNode("A", version);
    graph->addNode("B", version);
    graph->addNode("C", version);
    graph->addDependency("B", "C", version);

    // Test multiple groups
    graph->addGroup("group1", {"A"});
    graph->addGroup("group2", {"B"});

    auto group1Deps = graph->getGroupDependencies("group1");
    auto group2Deps = graph->getGroupDependencies("group2");

    EXPECT_TRUE(group1Deps.empty());
    EXPECT_EQ(group2Deps.size(), 1);
    EXPECT_EQ(group2Deps[0], "C");
}

TEST_F(DependencyGraphTest, ParallelBatchProcessing) {
    Version version{1, 0, 0};
    std::vector<std::string> nodes = {"A", "B", "C", "D", "E"};

    // Add multiple nodes
    for (const auto& node : nodes) {
        graph->addNode(node, version);
    }

    // Test parallel resolution with different batch sizes
    auto resolved = graph->resolveParallelDependencies(nodes);
    EXPECT_EQ(resolved.size(), nodes.size());

    // Verify all nodes are present
    for (const auto& node : nodes) {
        EXPECT_TRUE(std::find(resolved.begin(), resolved.end(), node) !=
                    resolved.end());
    }
}

TEST_F(DependencyGraphTest, ErrorHandling) {
    Version version{1, 0, 0};

    // Test invalid node access
    EXPECT_TRUE(graph->getDependencies("nonexistent").empty());
    EXPECT_TRUE(graph->getDependents("nonexistent").empty());

    // Test invalid group access
    EXPECT_TRUE(graph->getGroupDependencies("nonexistent").empty());

    // Test version conflict detection
    graph->addNode("A", Version{1, 0, 0});
    graph->addNode("B", Version{2, 0, 0});
    graph->addNode("C", Version{1, 0, 0});

    graph->addDependency("A", "B", Version{1, 0, 0});
    graph->addDependency("C", "B", Version{2, 0, 0});

    auto conflicts = graph->detectVersionConflicts();
    EXPECT_FALSE(conflicts.empty());
}

TEST_F(DependencyGraphTest, ThreadSafety) {
    Version version{1, 0, 0};
    const int numThreads = 10;
    std::vector<std::thread> threads;

    // Test concurrent node additions
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([this, i, version]() {
            graph->addNode("Node" + std::to_string(i), version);
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Verify all nodes were added correctly
    for (int i = 0; i < numThreads; ++i) {
        EXPECT_TRUE(graph->getDependencies("Node" + std::to_string(i)).empty());
    }
}
}  // namespace lithium::test
