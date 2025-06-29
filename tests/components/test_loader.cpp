#include <catch2/catch_test_macros.hpp>
#include "components/loader.hpp"
#include <fstream>

// For creating dummy shared libraries for testing
#if defined(_WIN32)
    #include <windows.h>
    const std::string LIB_EXT = ".dll";
    const std::string DUMMY_LIB_A_CONTENT = ""; // Cannot create DLLs on the fly easily
    const std::string DUMMY_LIB_B_CONTENT = "";
#else
    #include <dlfcn.h>
    const std::string LIB_EXT = ".so";
    // Simple C code to compile into a shared library
    const std::string DUMMY_LIB_A_SRC = "extern \"C\" int func_a() { return 42; }";
    const std::string DUMMY_LIB_B_SRC = "extern \"C\" int func_b() { return 84; }";
#endif

// Helper to create a dummy library for testing
void create_dummy_lib(const std::string& name, const std::string& src) {
#ifndef _WIN32
    std::string src_file = name + ".cpp";
    std::string lib_file = "lib" + name + LIB_EXT;
    std::ofstream out(src_file);
    out << src;
    out.close();
    std::string command = "g++ -shared -fPIC -o " + lib_file + " " + src_file;
    system(command.c_str());
#endif
}

TEST_CASE("ModuleLoader Modernized", "[loader]") {
    create_dummy_lib("test_mod_a", DUMMY_LIB_A_SRC);
    create_dummy_lib("test_mod_b", DUMMY_LIB_B_SRC);

    lithium::ModuleLoader loader(".");

    SECTION("Register and Load Modules") {
        auto reg_result_a = loader.registerModule("mod_a", "./libtest_mod_a" + LIB_EXT, {});
        REQUIRE(reg_result_a.has_value());

        auto reg_result_b = loader.registerModule("mod_b", "./libtest_mod_b" + LIB_EXT, {"mod_a"});
        REQUIRE(reg_result_b.has_value());

        auto load_future = loader.loadRegisteredModules();
        auto load_result = load_future.get();

        REQUIRE(load_result.has_value());
        REQUIRE(loader.hasModule("mod_a"));
        REQUIRE(loader.hasModule("mod_b"));
    }

    SECTION("Load non-existent module") {
        auto result = loader.loadModule("./nonexistent.so", "nonexistent");
        REQUIRE_FALSE(result.has_value());
    }

    SECTION("Unload module") {
        loader.registerModule("mod_a", "./libtest_mod_a" + LIB_EXT, {});
        loader.loadRegisteredModules().get();
        REQUIRE(loader.hasModule("mod_a"));
        auto unload_result = loader.unloadModule("mod_a");
        REQUIRE(unload_result.has_value());
        REQUIRE_FALSE(loader.hasModule("mod_a"));
    }

    SECTION("Diagnostics") {
        loader.registerModule("mod_a", "./libtest_mod_a" + LIB_EXT, {});
        loader.loadRegisteredModules().get();
        auto diagnostics = loader.getModuleDiagnostics("mod_a");
        REQUIRE(diagnostics.has_value());
        REQUIRE(diagnostics->status == lithium::ModuleInfo::Status::LOADED);
        REQUIRE(diagnostics->path == "./libtest_mod_a" + LIB_EXT);
    }

    SECTION("Circular Dependency Detection") {
        loader.registerModule("mod_c", "./libtest_mod_a.so", {"mod_d"});
        loader.registerModule("mod_d", "./libtest_mod_b.so", {"mod_c"});
        auto load_future = loader.loadRegisteredModules();
        auto load_result = load_future.get();
        REQUIRE_FALSE(load_result.has_value());
        if(!load_result.has_value()) {
            REQUIRE(load_result.error() == "Circular dependency detected among registered modules.");
        }
    }
}
