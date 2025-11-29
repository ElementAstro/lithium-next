#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Compiler Manager for detecting and managing compilers.
"""
import os
import platform
import re
import shutil
import subprocess
from typing import Dict, Optional

from loguru import logger

from .core_types import CompilerNotFoundError, CppVersion, CompilerType
from .compiler import Compiler, CompilerFeatures


class CompilerManager:
    """
    Manages compiler detection, selection, and operations.
    """

    def __init__(self):
        """Initialize the compiler manager."""
        self.compilers: Dict[str, Compiler] = {}
        self.default_compiler: Optional[str] = None

    def detect_compilers(self) -> Dict[str, Compiler]:
        """
        Detect available compilers on the system.
        """
        # Clear existing compilers
        self.compilers.clear()

        # Detect GCC
        gcc_path = self._find_command("g++") or self._find_command("gcc")
        if gcc_path:
            version = self._get_compiler_version(gcc_path)
            try:
                compiler = Compiler(
                    name="GCC",
                    command=gcc_path,
                    compiler_type=CompilerType.GCC,
                    version=version,
                    cpp_flags={
                        CppVersion.CPP98: "-std=c++98",
                        CppVersion.CPP03: "-std=c++03",
                        CppVersion.CPP11: "-std=c++11",
                        CppVersion.CPP14: "-std=c++14",
                        CppVersion.CPP17: "-std=c++17",
                        CppVersion.CPP20: "-std=c++20",
                        CppVersion.CPP23: "-std=c++23",
                    },
                    additional_compile_flags=["-Wall", "-Wextra"],
                    additional_link_flags=[],
                    features=CompilerFeatures(
                        supports_parallel=True,
                        supports_pch=True,
                        supports_modules=(version >= "11.0"),
                        supported_cpp_versions={
                            CppVersion.CPP98,
                            CppVersion.CPP03,
                            CppVersion.CPP11,
                            CppVersion.CPP14,
                            CppVersion.CPP17,
                            CppVersion.CPP20,
                        }
                        | ({CppVersion.CPP23} if version >= "11.0" else set()),
                        supported_sanitizers={"address", "thread", "undefined", "leak"},
                        supported_optimizations={
                            "-O0",
                            "-O1",
                            "-O2",
                            "-O3",
                            "-Ofast",
                            "-Os",
                            "-Og",
                        },
                        feature_flags={"lto": "-flto", "coverage": "--coverage"},
                    ),
                )
                self.compilers["GCC"] = compiler
                if not self.default_compiler:
                    self.default_compiler = "GCC"
            except CompilerNotFoundError:
                pass

        # Detect Clang
        clang_path = self._find_command("clang++") or self._find_command("clang")
        if clang_path:
            version = self._get_compiler_version(clang_path)
            try:
                compiler = Compiler(
                    name="Clang",
                    command=clang_path,
                    compiler_type=CompilerType.CLANG,
                    version=version,
                    cpp_flags={
                        CppVersion.CPP98: "-std=c++98",
                        CppVersion.CPP03: "-std=c++03",
                        CppVersion.CPP11: "-std=c++11",
                        CppVersion.CPP14: "-std=c++14",
                        CppVersion.CPP17: "-std=c++17",
                        CppVersion.CPP20: "-std=c++20",
                        CppVersion.CPP23: "-std=c++23",
                    },
                    additional_compile_flags=["-Wall", "-Wextra"],
                    additional_link_flags=[],
                    features=CompilerFeatures(
                        supports_parallel=True,
                        supports_pch=True,
                        supports_modules=(version >= "16.0"),
                        supported_cpp_versions={
                            CppVersion.CPP98,
                            CppVersion.CPP03,
                            CppVersion.CPP11,
                            CppVersion.CPP14,
                            CppVersion.CPP17,
                            CppVersion.CPP20,
                        }
                        | ({CppVersion.CPP23} if version >= "15.0" else set()),
                        supported_sanitizers={
                            "address",
                            "thread",
                            "undefined",
                            "memory",
                            "dataflow",
                        },
                        supported_optimizations={
                            "-O0",
                            "-O1",
                            "-O2",
                            "-O3",
                            "-Ofast",
                            "-Os",
                            "-Oz",
                        },
                        feature_flags={"lto": "-flto", "coverage": "--coverage"},
                    ),
                )
                self.compilers["Clang"] = compiler
                if not self.default_compiler:
                    self.default_compiler = "Clang"
            except CompilerNotFoundError:
                pass

        # Detect MSVC (on Windows)
        if platform.system() == "Windows":
            msvc_path = self._find_msvc()
            if msvc_path:
                version = self._get_compiler_version(msvc_path)
                try:
                    compiler = Compiler(
                        name="MSVC",
                        command=msvc_path,
                        compiler_type=CompilerType.MSVC,
                        version=version,
                        cpp_flags={
                            CppVersion.CPP98: "/std:c++98",
                            CppVersion.CPP03: "/std:c++03",
                            CppVersion.CPP11: "/std:c++11",
                            CppVersion.CPP14: "/std:c++14",
                            CppVersion.CPP17: "/std:c++17",
                            CppVersion.CPP20: "/std:c++20",
                            CppVersion.CPP23: "/std:c++latest",
                        },
                        additional_compile_flags=["/W4", "/EHsc"],
                        additional_link_flags=["/DEBUG"],
                        features=CompilerFeatures(
                            supports_parallel=True,
                            supports_pch=True,
                            # Visual Studio 2019 16.10+
                            supports_modules=(version >= "19.29"),
                            supported_cpp_versions={
                                CppVersion.CPP11,
                                CppVersion.CPP14,
                                CppVersion.CPP17,
                                CppVersion.CPP20,
                            }
                            | ({CppVersion.CPP23} if version >= "19.35" else set()),
                            supported_sanitizers={"address"},
                            supported_optimizations={"/O1", "/O2", "/Ox", "/Od"},
                            feature_flags={"lto": "/GL", "whole_program": "/GL"},
                        ),
                    )
                    self.compilers["MSVC"] = compiler
                    if not self.default_compiler:
                        self.default_compiler = "MSVC"
                except CompilerNotFoundError:
                    pass

        return self.compilers

    def get_compiler(self, name: Optional[str] = None) -> Compiler:
        """
        Get a compiler by name, or return the default compiler.
        """
        if not self.compilers:
            self.detect_compilers()

        if not name:
            # Return default compiler
            if self.default_compiler and self.default_compiler in self.compilers:
                return self.compilers[self.default_compiler]
            elif self.compilers:
                # Return first available
                return next(iter(self.compilers.values()))
            else:
                raise CompilerNotFoundError("No compilers detected on the system")

        if name in self.compilers:
            return self.compilers[name]
        else:
            raise CompilerNotFoundError(
                f"Compiler '{name}' not found. Available compilers: {', '.join(self.compilers.keys())}"
            )

    def _find_command(self, command: str) -> Optional[str]:
        """Find a command in the system path."""
        path = shutil.which(command)
        return path

    def _find_msvc(self) -> Optional[str]:
        """Find the MSVC compiler (cl.exe) on Windows."""
        # Try direct path first
        cl_path = shutil.which("cl")
        if cl_path:
            return cl_path

        # Check Visual Studio installation locations
        if platform.system() == "Windows":
            # Use vswhere.exe if available
            vswhere = os.path.join(
                os.environ.get("ProgramFiles(x86)", ""),
                "Microsoft Visual Studio",
                "Installer",
                "vswhere.exe",
            )

            if os.path.exists(vswhere):
                result = subprocess.run(
                    [
                        vswhere,
                        "-latest",
                        "-products",
                        "*",
                        "-requires",
                        "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
                        "-property",
                        "installationPath",
                        "-format",
                        "value",
                    ],
                    capture_output=True,
                    text=True,
                    check=False,
                )

                if result.returncode == 0 and result.stdout.strip():
                    vs_path = result.stdout.strip()
                    cl_path = os.path.join(vs_path, "VC", "Tools", "MSVC")

                    # Find the latest version
                    if os.path.exists(cl_path):
                        versions = os.listdir(cl_path)
                        if versions:
                            latest = sorted(versions)[-1]  # Get latest version
                            for arch in ["x64", "x86"]:
                                candidate = os.path.join(
                                    cl_path,
                                    latest,
                                    "bin",
                                    "Host" + arch,
                                    arch,
                                    "cl.exe",
                                )
                                if os.path.exists(candidate):
                                    return candidate

        return None

    def _get_compiler_version(self, compiler_path: str) -> str:
        """Get version string from a compiler."""
        try:
            if "cl" in os.path.basename(compiler_path).lower():
                # MSVC
                result = subprocess.run(
                    [compiler_path],
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                    universal_newlines=True,
                )
                match = re.search(r"Version\s+(\d+\.\d+\.\d+)", result.stderr)
                if match:
                    return match.group(1)
                return "unknown"
            else:
                # GCC or Clang
                result = subprocess.run(
                    [compiler_path, "--version"],
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                    universal_newlines=True,
                )
                first_line = result.stdout.splitlines()[0]
                # Extract version number
                match = re.search(r"(\d+\.\d+\.\d+)", first_line)
                if match:
                    return match.group(1)
                return "unknown"
        except Exception as e:
            logger.warning(f"Failed to get compiler version: {e}")
            return "unknown"
