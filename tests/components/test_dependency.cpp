'''#include <catch2/catch_test_macros.hpp>
#include "components/dependency.hpp"
#include "components/version.hpp"

using namespace lithium;

TEST_CASE("DependencyGraph Basic Operations", "[dependency]") {
    DependencyGraph graph;

    SECTION("Add and retrieve a node") {
        graph.addNode("A", Version(1, 0, 0));
        REQUIRE(graph.nodeExists("A"));
        REQUIRE(graph.getNodeVersion("A").value() == Version(1, 0, 0));
    }

    SECTION("Add and retrieve a dependency") {
        graph.addNode("A", Version(1, 0, 0));
        graph.addNode("B", Version(1, 0, 0));
        graph.addDependency("A", "B", Version(1, 0, 0));
        auto deps = graph.getDependencies("A");
        REQUIRE(deps.size() == 1);
        REQUIRE(deps[0] == "B");
    }

    SECTION("Remove a node") {
        graph.addNode("A", Version(1, 0, 0));
        graph.removeNode("A");
        REQUIRE_FALSE(graph.nodeExists("A"));
    }

    SECTION("Remove a dependency") {
        graph.addNode("A", Version(1, 0, 0));
        graph.addNode("B", Version(1, 0, 0));
        graph.addDependency("A", "B", Version(1, 0, 0));
        graph.removeDependency("A", "B");
        REQUIRE(graph.getDependencies("A").empty());
    }
}

TEST_CASE("DependencyGraph Cycle Detection", "[dependency]") {
    DependencyGraph graph;
    graph.addNode("A", Version(1, 0, 0));
    graph.addNode("B", Version(1, 0, 0));
    graph.addNode("C", Version(1, 0, 0));

    SECTION("No cycle") {
        graph.addDependency("A", "B", Version(1, 0, 0));
        graph.addDependency("B", "C", Version(1, 0, 0));
        REQUIRE_FALSE(graph.hasCycle());
    }

    SECTION("Simple cycle") {
        graph.addDependency("A", "B", Version(1, 0, 0));
        graph.addDependency("B", "A", Version(1, 0, 0));
        REQUIRE(graph.hasCycle());
    }

    SECTION("Longer cycle") {
        graph.addDependency("A", "B", Version(1, 0, 0));
        graph.addDependency("B", "C", Version(1, 0, 0));
        graph.addDependency("C", "A", Version(1, 0, 0));
        REQUIRE(graph.hasCycle());
    }
}

TEST_CASE("DependencyGraph Topological Sort", "[dependency]") {
    DependencyGraph graph;
    graph.addNode("A", Version(1, 0, 0));
    graph.addNode("B", Version(1, 0, 0));
    graph.addNode("C", Version(1, 0, 0));
    graph.addDependency("A", "B", Version(1, 0, 0));
    graph.addDependency("B", "C", Version(1, 0, 0));

    auto sorted = graph.topologicalSort();
    REQUIRE(sorted.has_value());
    auto sorted_nodes = sorted.value();
    REQUIRE(sorted_nodes.size() == 3);
    // A possible valid topological sort is C, B, A
    // We need to check for valid order, not a specific one.
    auto pos_A = std::find(sorted_nodes.begin(), sorted_nodes.end(), "A");
    auto pos_B = std::find(sorted_nodes.begin(), sorted_nodes.end(), "B");
    auto pos_C = std::find(sorted_nodes.begin(), sorted_nodes.end(), "C");

    REQUIRE(std::distance(pos_C, pos_B) > 0);
    REQUIRE(std::distance(pos_B, pos_A) > 0);
}

TEST_CASE("DependencyGraph Async Resolution", "[dependency]") {
    // This test requires a mock filesystem or actual files.
    // For now, we'll just test the coroutine machinery.
    DependencyGraph graph;
    auto gen = graph.resolveDependenciesAsync("dummy_dir");
    REQUIRE_FALSE(gen.next()); // No files, so should be done immediately.
}
''
