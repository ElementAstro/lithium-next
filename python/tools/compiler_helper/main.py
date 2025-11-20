#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Compiler Helper Script - Enhanced Version

This script provides utilities for C++ compiler management, source compilation,
and project building with support for both command-line and programmatic usage via pybind11.

Features:
- Cross-platform compiler detection (GCC, Clang, MSVC)
- Multiple C++ standard support (C++98 through C++23)
- Advanced compilation and linking capabilities
- Configuration via JSON, YAML, or command-line
- Dependency tracking and incremental builds
- Extensive error handling and logging
- Parallel compilation support

Usage as CLI:
    python compiler_helper.py source1.cpp source2.cpp -o output.o --compiler GCC --cpp-version c++20 --link --flags -O3

Usage as library:
    import compiler_helper
    compiler = compiler_helper.get_compiler("GCC")
    compiler.compile(["source.cpp"], "output.o", compiler_helper.CppVersion.CPP20)

Author:
    Max Qian <lightapt.com>

License:
    GPL-3.0-or-later
"""

import sys
from compiler_helper import cli

if __name__ == "__main__":
    sys.exit(cli.main())
