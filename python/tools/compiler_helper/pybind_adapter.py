#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
PyBind11 adapter for Compiler Helper utilities.

This module provides simplified interfaces for C++ bindings via pybind11.
It wraps the compiler_helper functionality with simplified return types that
are compatible with C++ pybind11 bindings.
"""

import asyncio
from typing import Dict, List, Any, Optional
from pathlib import Path

from loguru import logger

from .api import get_compiler, compile_file, build_project, compiler_manager
from .core_types import CppVersion, CompileOptions, LinkOptions, PathLike


class CompilerHelperPyBindAdapter:
    """
    Adapter class to expose compiler_helper functionality to C++ via pybind11.

    This class simplifies the interface and handles conversions between Python
    and C++, providing both synchronous and asynchronous versions of key
    operations with pybind11-compatible return types.
    """

    # ========================
    # Synchronous Operations
    # ========================

    @staticmethod
    def detect_compilers() -> Dict[str, Dict[str, Any]]:
        """
        Detect available compilers on the system.

        Returns:
            Dict with compiler names as keys and compiler info dicts as values.
            Each compiler dict contains: name, version, type, features.
        """
        logger.info("C++ binding: Detecting available compilers")
        try:
            compilers = compiler_manager.detect_compilers()
            result = {}
            for name, compiler in compilers.items():
                result[name] = {
                    "name": compiler.name,
                    "version": compiler.version,
                    "command": compiler.command,
                    "type": compiler.compiler_type.name,
                    "supports_parallel": compiler.features.supports_parallel,
                    "supports_pch": compiler.features.supports_pch,
                    "supports_modules": compiler.features.supports_modules,
                    "supported_cpp_versions": [v.value for v in compiler.features.supported_cpp_versions],
                    "supported_sanitizers": list(compiler.features.supported_sanitizers),
                    "supported_optimizations": list(compiler.features.supported_optimizations),
                }
            return result
        except Exception as e:
            logger.exception(f"Error in detect_compilers: {e}")
            return {"error": str(e)}

    @staticmethod
    def get_compiler_info(compiler_name: Optional[str] = None) -> Dict[str, Any]:
        """
        Get information about a specific compiler.

        Args:
            compiler_name: Name of the compiler (GCC, Clang, MSVC). If None, returns default.

        Returns:
            Dict containing compiler information.
        """
        logger.info(f"C++ binding: Getting info for compiler {compiler_name or 'default'}")
        try:
            compiler = get_compiler(compiler_name)
            return {
                "success": True,
                "name": compiler.name,
                "version": compiler.version,
                "command": compiler.command,
                "type": compiler.compiler_type.name,
                "supports_parallel": compiler.features.supports_parallel,
                "supports_pch": compiler.features.supports_pch,
                "supports_modules": compiler.features.supports_modules,
                "supported_cpp_versions": [v.value for v in compiler.features.supported_cpp_versions],
                "supported_sanitizers": list(compiler.features.supported_sanitizers),
                "supported_optimizations": list(compiler.features.supported_optimizations),
            }
        except Exception as e:
            logger.exception(f"Error in get_compiler_info: {e}")
            return {
                "success": False,
                "error": str(e)
            }

    @staticmethod
    def compile(
        source_file: str,
        output_file: str,
        compiler_name: Optional[str] = None,
        cpp_version: str = "c++17",
        include_paths: Optional[List[str]] = None,
        defines: Optional[Dict[str, Optional[str]]] = None,
        optimization: Optional[str] = None,
        debug: bool = False,
        extra_flags: Optional[List[str]] = None
    ) -> Dict[str, Any]:
        """
        Compile a single source file (synchronous).

        Args:
            source_file: Path to source file
            output_file: Path to output file
            compiler_name: Name of compiler to use
            cpp_version: C++ standard version (e.g., "c++17", "c++20")
            include_paths: List of include directories
            defines: Dict of preprocessor definitions
            optimization: Optimization level (e.g., "-O2", "/O2")
            debug: Enable debug symbols
            extra_flags: Additional compiler flags

        Returns:
            Dict with success status, output_file, duration_ms, errors, warnings.
        """
        logger.info(f"C++ binding: Compiling {source_file} to {output_file}")
        try:
            # Convert cpp_version string to enum
            try:
                cpp_enum = CppVersion(cpp_version)
            except ValueError:
                # Try alternate format (c++17 -> CPP17)
                cpp_name = "CPP" + cpp_version.upper().replace("C++", "").replace("C+", "")
                cpp_enum = CppVersion[cpp_name]

            # Build compile options
            options: CompileOptions = {
                "include_paths": include_paths or [],
                "defines": defines or {},
                "debug": debug,
                "extra_flags": extra_flags or [],
            }
            if optimization:
                options["optimization"] = optimization

            result = compile_file(source_file, output_file, compiler_name, cpp_enum, options)

            return {
                "success": result.success,
                "output_file": str(result.output_file) if result.output_file else None,
                "duration_ms": result.duration_ms,
                "errors": result.errors,
                "warnings": result.warnings,
                "command_line": result.command_line or []
            }
        except Exception as e:
            logger.exception(f"Error in compile: {e}")
            return {
                "success": False,
                "output_file": None,
                "duration_ms": 0,
                "errors": [str(e)],
                "warnings": [],
                "command_line": []
            }

    @staticmethod
    def link(
        object_files: List[str],
        output_file: str,
        compiler_name: Optional[str] = None,
        library_paths: Optional[List[str]] = None,
        libraries: Optional[List[str]] = None,
        shared: bool = False,
        extra_flags: Optional[List[str]] = None
    ) -> Dict[str, Any]:
        """
        Link object files into an executable or library (synchronous).

        Args:
            object_files: List of object file paths
            output_file: Path to output executable/library
            compiler_name: Name of compiler to use
            library_paths: List of library search directories
            libraries: List of libraries to link
            shared: Create shared library instead of executable
            extra_flags: Additional linker flags

        Returns:
            Dict with success status, output_file, duration_ms, errors, warnings.
        """
        logger.info(f"C++ binding: Linking {len(object_files)} objects to {output_file}")
        try:
            compiler = get_compiler(compiler_name)

            # Build link options
            options: LinkOptions = {
                "library_paths": library_paths or [],
                "libraries": libraries or [],
                "shared": shared,
                "extra_flags": extra_flags or [],
            }

            result = compiler.link(object_files, output_file, options)

            return {
                "success": result.success,
                "output_file": str(result.output_file) if result.output_file else None,
                "duration_ms": result.duration_ms,
                "errors": result.errors,
                "warnings": result.warnings,
                "command_line": result.command_line or []
            }
        except Exception as e:
            logger.exception(f"Error in link: {e}")
            return {
                "success": False,
                "output_file": None,
                "duration_ms": 0,
                "errors": [str(e)],
                "warnings": [],
                "command_line": []
            }

    @staticmethod
    def build_project(
        source_files: List[str],
        output_file: str,
        compiler_name: Optional[str] = None,
        cpp_version: str = "c++17",
        compile_options: Optional[Dict[str, Any]] = None,
        link_options: Optional[Dict[str, Any]] = None,
        build_dir: Optional[str] = None,
        incremental: bool = True
    ) -> Dict[str, Any]:
        """
        Build a project from multiple source files (synchronous).

        Args:
            source_files: List of source file paths
            output_file: Path to output executable/library
            compiler_name: Name of compiler to use
            cpp_version: C++ standard version
            compile_options: Additional compilation options dict
            link_options: Additional linking options dict
            build_dir: Build directory path
            incremental: Enable incremental builds

        Returns:
            Dict with build status, output_file, duration_ms, errors, warnings.
        """
        logger.info(f"C++ binding: Building project from {len(source_files)} sources")
        try:
            # Convert cpp_version string to enum
            try:
                cpp_enum = CppVersion(cpp_version)
            except ValueError:
                cpp_name = "CPP" + cpp_version.upper().replace("C++", "").replace("C+", "")
                cpp_enum = CppVersion[cpp_name]

            result = build_project(
                source_files=source_files,
                output_file=output_file,
                compiler_name=compiler_name,
                cpp_version=cpp_enum,
                compile_options=compile_options,
                link_options=link_options,
                build_dir=build_dir,
                incremental=incremental
            )

            return {
                "success": result.success,
                "output_file": str(result.output_file) if result.output_file else None,
                "duration_ms": result.duration_ms,
                "errors": result.errors,
                "warnings": result.warnings,
                "command_line": result.command_line or []
            }
        except Exception as e:
            logger.exception(f"Error in build_project: {e}")
            return {
                "success": False,
                "output_file": None,
                "duration_ms": 0,
                "errors": [str(e)],
                "warnings": [],
                "command_line": []
            }

    # ========================
    # Asynchronous Operations
    # ========================

    @staticmethod
    async def compile_async(
        source_file: str,
        output_file: str,
        compiler_name: Optional[str] = None,
        cpp_version: str = "c++17",
        include_paths: Optional[List[str]] = None,
        defines: Optional[Dict[str, Optional[str]]] = None,
        optimization: Optional[str] = None,
        debug: bool = False,
        extra_flags: Optional[List[str]] = None
    ) -> Dict[str, Any]:
        """
        Compile a single source file (asynchronous).

        This is a wrapper around the synchronous compile operation that
        allows integration with async event loops. The operation itself runs
        in a thread pool to avoid blocking.

        Args:
            source_file: Path to source file
            output_file: Path to output file
            compiler_name: Name of compiler to use
            cpp_version: C++ standard version
            include_paths: List of include directories
            defines: Dict of preprocessor definitions
            optimization: Optimization level
            debug: Enable debug symbols
            extra_flags: Additional compiler flags

        Returns:
            Dict with success status, output_file, duration_ms, errors, warnings.
        """
        loop = asyncio.get_event_loop()
        return await loop.run_in_executor(
            None,
            CompilerHelperPyBindAdapter.compile,
            source_file,
            output_file,
            compiler_name,
            cpp_version,
            include_paths,
            defines,
            optimization,
            debug,
            extra_flags
        )

    @staticmethod
    async def link_async(
        object_files: List[str],
        output_file: str,
        compiler_name: Optional[str] = None,
        library_paths: Optional[List[str]] = None,
        libraries: Optional[List[str]] = None,
        shared: bool = False,
        extra_flags: Optional[List[str]] = None
    ) -> Dict[str, Any]:
        """
        Link object files into an executable or library (asynchronous).

        This is a wrapper around the synchronous link operation that
        allows integration with async event loops.

        Args:
            object_files: List of object file paths
            output_file: Path to output executable/library
            compiler_name: Name of compiler to use
            library_paths: List of library search directories
            libraries: List of libraries to link
            shared: Create shared library instead of executable
            extra_flags: Additional linker flags

        Returns:
            Dict with success status, output_file, duration_ms, errors, warnings.
        """
        loop = asyncio.get_event_loop()
        return await loop.run_in_executor(
            None,
            CompilerHelperPyBindAdapter.link,
            object_files,
            output_file,
            compiler_name,
            library_paths,
            libraries,
            shared,
            extra_flags
        )

    @staticmethod
    async def build_project_async(
        source_files: List[str],
        output_file: str,
        compiler_name: Optional[str] = None,
        cpp_version: str = "c++17",
        compile_options: Optional[Dict[str, Any]] = None,
        link_options: Optional[Dict[str, Any]] = None,
        build_dir: Optional[str] = None,
        incremental: bool = True
    ) -> Dict[str, Any]:
        """
        Build a project from multiple source files (asynchronous).

        This is a wrapper around the synchronous build operation that
        allows integration with async event loops.

        Args:
            source_files: List of source file paths
            output_file: Path to output executable/library
            compiler_name: Name of compiler to use
            cpp_version: C++ standard version
            compile_options: Additional compilation options dict
            link_options: Additional linking options dict
            build_dir: Build directory path
            incremental: Enable incremental builds

        Returns:
            Dict with build status, output_file, duration_ms, errors, warnings.
        """
        loop = asyncio.get_event_loop()
        return await loop.run_in_executor(
            None,
            CompilerHelperPyBindAdapter.build_project,
            source_files,
            output_file,
            compiler_name,
            cpp_version,
            compile_options,
            link_options,
            build_dir,
            incremental
        )

    @staticmethod
    async def detect_compilers_async() -> Dict[str, Dict[str, Any]]:
        """
        Detect available compilers on the system (asynchronous).

        Returns:
            Dict with compiler names as keys and compiler info dicts as values.
        """
        loop = asyncio.get_event_loop()
        return await loop.run_in_executor(
            None,
            CompilerHelperPyBindAdapter.detect_compilers
        )

    @staticmethod
    async def get_compiler_info_async(
        compiler_name: Optional[str] = None
    ) -> Dict[str, Any]:
        """
        Get information about a specific compiler (asynchronous).

        Args:
            compiler_name: Name of the compiler (GCC, Clang, MSVC).

        Returns:
            Dict containing compiler information.
        """
        loop = asyncio.get_event_loop()
        return await loop.run_in_executor(
            None,
            CompilerHelperPyBindAdapter.get_compiler_info,
            compiler_name
        )
