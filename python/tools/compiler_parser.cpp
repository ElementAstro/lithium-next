#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace py = pybind11;

// This module provides C++ bindings for the compiler_parser Python module
PYBIND11_MODULE(compiler_parser_cpp, m) {
    m.doc() = "C++ bindings for compiler output parser";

    // Expose main functions for use in C++
    m.def(
        "parse_compiler_output",
        [](const std::string& compiler_type, const std::string& output,
           const std::optional<std::vector<std::string>>& filter_severities) {
            py::object parser_module = py::module::import("compiler_parser");
            return parser_module.attr("parse_compiler_output")(
                compiler_type, output, filter_severities);
        },
        py::arg("compiler_type"), py::arg("output"),
        py::arg("filter_severities") = py::none(),
        "Parse compiler output string and return structured data");

    m.def(
        "parse_compiler_file",
        [](const std::string& compiler_type, const std::string& file_path,
           const std::optional<std::vector<std::string>>& filter_severities) {
            py::object parser_module = py::module::import("compiler_parser");
            return parser_module.attr("parse_compiler_file")(
                compiler_type, file_path, filter_severities);
        },
        py::arg("compiler_type"), py::arg("file_path"),
        py::arg("filter_severities") = py::none(),
        "Parse compiler output from a file and return structured data");
}
