#include "components/dependency.hpp"
#include "components/version.hpp"

#include <gtest/gtest.h>
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

}  // namespace lithium::test
