#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
High-level API for the compiler helper module.
"""
from pathlib import Path
from typing import List, Optional, Union

from .core_types import CompilationResult, CompileOptions, LinkOptions, CppVersion, PathLike
from .compiler_manager import CompilerManager
from .compiler import Compiler
from .build_manager import BuildManager


# Create a singleton compiler manager for global use
compiler_manager = CompilerManager()


def get_compiler(name: Optional[str] = None) -> Compiler:
    """
    Get a compiler by name, or the default compiler if no name is provided.
    """
    return compiler_manager.get_compiler(name)


def compile_file(source_file: PathLike,
                 output_file: PathLike,
                 compiler_name: Optional[str] = None,
                 cpp_version: Union[str, CppVersion] = CppVersion.CPP17,
                 options: Optional[CompileOptions] = None) -> CompilationResult:
    """
    Compile a single source file.
    """
    cpp_version = CppVersion.resolve_cpp_version(cpp_version)

    compiler = get_compiler(compiler_name)
    return compiler.compile([Path(source_file)], Path(output_file), cpp_version, options)


def build_project(source_files: List[PathLike],
                  output_file: PathLike,
                  compiler_name: Optional[str] = None,
                  cpp_version: Union[str, CppVersion] = CppVersion.CPP17,
                  compile_options: Optional[CompileOptions] = None,
                  link_options: Optional[LinkOptions] = None,
                  build_dir: Optional[PathLike] = None,
                  incremental: bool = True) -> CompilationResult:
    """
    Build a project from multiple source files.
    """
    cpp_version = CppVersion.resolve_cpp_version(cpp_version)

    build_manager = BuildManager(build_dir=build_dir or "build")
    return build_manager.build(
        source_files=source_files,
        output_file=output_file,
        compiler_name=compiler_name,
        cpp_version=cpp_version,
        compile_options=compile_options,
        link_options=link_options,
        incremental=incremental
    )
