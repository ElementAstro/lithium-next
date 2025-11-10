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
    Enhanced by [Your Name]

License:
    GPL-3.0-or-later
"""

import subprocess
import sys
import os
import platform
import shutil
import json
import logging
from dataclasses import dataclass, field
from enum import Enum, auto
from pathlib import Path
from typing import List, Dict, Optional, Union, Callable, Set, Any, TypedDict, Literal
import argparse
import concurrent.futures
import functools
import time
from collections import defaultdict
import hashlib
import re

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger("CompilerHelper")

# Type definitions for improved type hinting
PathLike = Union[str, Path]
CompilerName = Literal["GCC", "Clang", "MSVC"]
CommandResult = tuple[int, str, str]  # return_code, stdout, stderr


class CppVersion(Enum):
    """
    Enum representing supported C++ language standard versions.

    These values map to standard compiler flags for specifying the
    desired C++ language standard to use during compilation.
    """
    CPP98 = "c++98"   # Published in 1998, first standardized version
    CPP03 = "c++03"   # Published in 2003, minor update to 98
    CPP11 = "c++11"   # Major update published in 2011 (auto, lambda, move semantics)
    CPP14 = "c++14"   # Published in 2014 (generic lambdas, return type deduction)
    CPP17 = "c++17"   # Published in 2017 (structured bindings, if constexpr)
    CPP20 = "c++20"   # Published in 2020 (concepts, ranges, coroutines)
    CPP23 = "c++23"   # Latest standard (modules improvements, stacktrace)


class CompilerType(Enum):
    """Enum representing supported compiler types."""
    GCC = auto()      # GNU Compiler Collection
    CLANG = auto()    # LLVM Clang Compiler
    MSVC = auto()     # Microsoft Visual C++ Compiler
    ICC = auto()      # Intel C++ Compiler
    MINGW = auto()    # MinGW (GCC for Windows)
    EMSCRIPTEN = auto()  # Emscripten for WebAssembly


class CompileOptions(TypedDict, total=False):
    """TypedDict for compiler options with optional fields."""
    include_paths: List[PathLike]      # Directories to search for include files
    defines: Dict[str, Optional[str]]  # Preprocessor definitions
    warnings: List[str]                # Warning flags
    optimization: str                  # Optimization level
    debug: bool                        # Enable debug information
    position_independent: bool         # Generate position-independent code
    standard_library: Optional[str]    # Specify standard library implementation
    sanitizers: List[str]              # Enable sanitizers (e.g., address, undefined)
    extra_flags: List[str]             # Additional compiler flags


class LinkOptions(TypedDict, total=False):
    """TypedDict for linker options with optional fields."""
    library_paths: List[PathLike]      # Directories to search for libraries
    libraries: List[str]               # Libraries to link against
    runtime_library_paths: List[PathLike]  # Runtime library search paths
    shared: bool                       # Create shared library
    static: bool                       # Prefer static linking
    strip: bool                        # Strip debug symbols
    map_file: Optional[PathLike]       # Generate map file
    extra_flags: List[str]             # Additional linker flags


class CompilationError(Exception):
    """Exception raised when compilation fails."""
    def __init__(self, message: str, command: List[str], return_code: int, stderr: str):
        self.message = message
        self.command = command
        self.return_code = return_code
        self.stderr = stderr
        super().__init__(f"{message} (Return code: {return_code})\nCommand: {' '.join(command)}\nError: {stderr}")


class CompilerNotFoundError(Exception):
    """Exception raised when a requested compiler is not available."""
    pass


@dataclass
class CompilationResult:
    """Represents the result of a compilation operation."""
    success: bool
    output_file: Optional[Path] = None
    duration_ms: float = 0.0
    command_line: Optional[List[str]] = None
    errors: List[str] = field(default_factory=list)
    warnings: List[str] = field(default_factory=list)


@dataclass
class CompilerFeatures:
    """Represents capabilities and features of a specific compiler."""
    supports_parallel: bool = False
    supports_pch: bool = False  # Precompiled headers
    supports_modules: bool = False
    supported_cpp_versions: Set[CppVersion] = field(default_factory=set)
    supported_sanitizers: Set[str] = field(default_factory=set)
    supported_optimizations: Set[str] = field(default_factory=set)
    feature_flags: Dict[str, str] = field(default_factory=dict)


@dataclass
class Compiler:
    """
    Class representing a compiler with its command and compilation capabilities.

    This class encapsulates compiler-specific behavior and provides methods for
    compilation and linking operations.
    """
    name: str
    command: str
    compiler_type: CompilerType
    version: str
    cpp_flags: Dict[CppVersion, str] = field(default_factory=dict)
    additional_compile_flags: List[str] = field(default_factory=list)
    additional_link_flags: List[str] = field(default_factory=list)
    features: CompilerFeatures = field(default_factory=CompilerFeatures)

    def __post_init__(self):
        """Initialize and validate the compiler after creation."""
        # Ensure command is absolute path
        if self.command and not os.path.isabs(self.command):
            resolved_path = shutil.which(self.command)
            if resolved_path:
                self.command = resolved_path

        # Validate compiler exists and is executable
        if not os.access(self.command, os.X_OK):
            raise CompilerNotFoundError(f"Compiler {self.name} not found or not executable: {self.command}")

    def compile(self,
                source_files: List[PathLike],
                output_file: PathLike,
                cpp_version: CppVersion,
                options: Optional[CompileOptions] = None) -> CompilationResult:
        """
        Compile source files into an object file or executable.

        Args:
            source_files: List of source files to compile
            output_file: Path to the output file
            cpp_version: C++ standard version to use
            options: Additional compilation options

        Returns:
            CompilationResult object with compilation details

        Raises:
            CompilationError: If compilation fails
        """
        start_time = time.time()
        options = options or {}
        output_path = Path(output_file)

        # Ensure output directory exists
        output_path.parent.mkdir(parents=True, exist_ok=True)

        # Start building command
        if cpp_version in self.cpp_flags:
            version_flag = self.cpp_flags[cpp_version]
        else:
            supported = ", ".join(v.value for v in self.cpp_flags.keys())
            message = f"Unsupported C++ version: {cpp_version}. Supported versions: {supported}"
            logger.error(message)
            return CompilationResult(
                success=False,
                errors=[message],
                duration_ms=(time.time() - start_time) * 1000
            )

        # Build command with all options
        cmd = [self.command, version_flag]

        # Add include paths
        for path in options.get('include_paths', []):
            if self.compiler_type == CompilerType.MSVC:
                cmd.append(f"/I{path}")
            else:
                cmd.append("-I")
                cmd.append(str(path))

        # Add preprocessor definitions
        for name, value in options.get('defines', {}).items():
            if self.compiler_type == CompilerType.MSVC:
                if value is None:
                    cmd.append(f"/D{name}")
                else:
                    cmd.append(f"/D{name}={value}")
            else:
                if value is None:
                    cmd.append(f"-D{name}")
                else:
                    cmd.append(f"-D{name}={value}")

        # Add warning flags
        cmd.extend(options.get('warnings', []))

        # Add optimization level
        if 'optimization' in options:
            cmd.append(options['optimization'])

        # Add debug flag if requested
        if options.get('debug', False):
            if self.compiler_type == CompilerType.MSVC:
                cmd.append("/Zi")
            else:
                cmd.append("-g")

        # Position independent code
        if options.get('position_independent', False) and self.compiler_type != CompilerType.MSVC:
            cmd.append("-fPIC")

        # Add sanitizers
        for sanitizer in options.get('sanitizers', []):
            if sanitizer in self.features.supported_sanitizers:
                if self.compiler_type == CompilerType.MSVC:
                    if sanitizer == "address":
                        cmd.append("/fsanitize=address")
                else:
                    cmd.append(f"-fsanitize={sanitizer}")

        # Add standard library specification
        if 'standard_library' in options and self.compiler_type != CompilerType.MSVC:
            cmd.append(f"-stdlib={options['standard_library']}")

        # Add default compile flags for this compiler
        cmd.extend(self.additional_compile_flags)

        # Add extra flags
        cmd.extend(options.get('extra_flags', []))

        # Add compile flag
        if self.compiler_type == CompilerType.MSVC:
            cmd.append("/c")
        else:
            cmd.append("-c")

        # Add source files
        cmd.extend([str(f) for f in source_files])

        # Add output file
        if self.compiler_type == CompilerType.MSVC:
            cmd.extend(["/Fo:", str(output_path)])
        else:
            cmd.extend(["-o", str(output_path)])

        # Execute the command
        logger.debug(f"Running compile command: {' '.join(cmd)}")
        result = self._run_command(cmd)

        # Process result
        elapsed_time = (time.time() - start_time) * 1000

        if result[0] != 0:
            # Parse errors and warnings from stderr
            errors, warnings = self._parse_diagnostics(result[2])
            return CompilationResult(
                success=False,
                command_line=cmd,
                duration_ms=elapsed_time,
                errors=errors,
                warnings=warnings
            )

        # Check if output file was created
        if not output_path.exists():
            return CompilationResult(
                success=False,
                command_line=cmd,
                duration_ms=elapsed_time,
                errors=[f"Compilation completed but output file was not created: {output_path}"]
            )

        # Parse warnings (even if successful)
        _, warnings = self._parse_diagnostics(result[2])

        return CompilationResult(
            success=True,
            output_file=output_path,
            command_line=cmd,
            duration_ms=elapsed_time,
            warnings=warnings
        )

    def link(self,
             object_files: List[PathLike],
             output_file: PathLike,
             options: Optional[LinkOptions] = None) -> CompilationResult:
        """
        Link object files into an executable or library.

        Args:
            object_files: List of object files to link
            output_file: Path to the output executable/library
            options: Additional linking options

        Returns:
            CompilationResult object with linking details

        Raises:
            CompilationError: If linking fails
        """
        start_time = time.time()
        options = options or {}
        output_path = Path(output_file)

        # Ensure output directory exists
        output_path.parent.mkdir(parents=True, exist_ok=True)

        # Start building command
        cmd = [self.command]

        # Handle shared library creation
        if options.get('shared', False):
            if self.compiler_type == CompilerType.MSVC:
                cmd.append("/DLL")
            else:
                cmd.append("-shared")

        # Handle static linking preference
        if options.get('static', False) and self.compiler_type != CompilerType.MSVC:
            cmd.append("-static")

        # Add library paths
        for path in options.get('library_paths', []):
            if self.compiler_type == CompilerType.MSVC:
                cmd.append(f"/LIBPATH:{path}")
            else:
                cmd.append(f"-L{path}")

        # Add runtime library paths
        if self.compiler_type != CompilerType.MSVC:
            for path in options.get('runtime_library_paths', []):
                if platform.system() == "Darwin":
                    cmd.append(f"-Wl,-rpath,{path}")
                else:
                    cmd.append(f"-Wl,-rpath={path}")

        # Add libraries
        for lib in options.get('libraries', []):
            if self.compiler_type == CompilerType.MSVC:
                cmd.append(f"{lib}.lib")
            else:
                cmd.append(f"-l{lib}")

        # Strip debug symbols if requested
        if options.get('strip', False):
            if self.compiler_type == CompilerType.MSVC:
                pass  # MSVC handles this differently
            else:
                cmd.append("-s")

        # Add map file if requested
        if 'map_file' in options:
            map_path = Path(options['map_file'])
            if self.compiler_type == CompilerType.MSVC:
                cmd.append(f"/MAP:{map_path}")
            else:
                cmd.append(f"-Wl,-Map={map_path}")

        # Add default link flags
        cmd.extend(self.additional_link_flags)

        # Add extra flags
        cmd.extend(options.get('extra_flags', []))

        # Add object files
        cmd.extend([str(f) for f in object_files])

        # Add output file
        if self.compiler_type == CompilerType.MSVC:
            cmd.extend([f"/OUT:{output_path}"])
        else:
            cmd.extend(["-o", str(output_path)])

        # Execute the command
        logger.debug(f"Running link command: {' '.join(cmd)}")
        result = self._run_command(cmd)

        # Process result
        elapsed_time = (time.time() - start_time) * 1000

        if result[0] != 0:
            # Parse errors and warnings from stderr
            errors, warnings = self._parse_diagnostics(result[2])
            return CompilationResult(
                success=False,
                command_line=cmd,
                duration_ms=elapsed_time,
                errors=errors,
                warnings=warnings
            )

        # Check if output file was created
        if not output_path.exists():
            return CompilationResult(
                success=False,
                command_line=cmd,
                duration_ms=elapsed_time,
                errors=[f"Linking completed but output file was not created: {output_path}"]
            )

        # Parse warnings (even if successful)
        _, warnings = self._parse_diagnostics(result[2])

        return CompilationResult(
            success=True,
            output_file=output_path,
            command_line=cmd,
            duration_ms=elapsed_time,
            warnings=warnings
        )

    def _run_command(self, cmd: List[str]) -> CommandResult:
        """Execute a command and return its exit code, stdout, and stderr."""
        try:
            process = subprocess.Popen(
                cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                universal_newlines=True
            )
            stdout, stderr = process.communicate()
            return process.returncode, stdout, stderr
        except Exception as e:
            return 1, "", str(e)

    def _parse_diagnostics(self, output: str) -> tuple[List[str], List[str]]:
        """Parse compiler output to extract errors and warnings."""
        errors = []
        warnings = []

        # Different parsing based on compiler type
        if self.compiler_type == CompilerType.MSVC:
            error_pattern = re.compile(r'.*?[Ee]rror\s+[A-Za-z0-9]+:.*')
            warning_pattern = re.compile(r'.*?[Ww]arning\s+[A-Za-z0-9]+:.*')
        else:
            error_pattern = re.compile(r'.*?:[0-9]+:[0-9]+:\s+error:.*')
            warning_pattern = re.compile(r'.*?:[0-9]+:[0-9]+:\s+warning:.*')

        for line in output.splitlines():
            if error_pattern.match(line):
                errors.append(line.strip())
            elif warning_pattern.match(line):
                warnings.append(line.strip())

        return errors, warnings

    def get_version_info(self) -> Dict[str, str]:
        """Get detailed version information about the compiler."""
        if self.compiler_type == CompilerType.GCC:
            result = self._run_command([self.command, "--version"])
            if result[0] == 0:
                return {"version": result[1].splitlines()[0]}
        elif self.compiler_type == CompilerType.CLANG:
            result = self._run_command([self.command, "--version"])
            if result[0] == 0:
                return {"version": result[1].splitlines()[0]}
        elif self.compiler_type == CompilerType.MSVC:
            # MSVC version info requires special handling
            result = self._run_command([self.command, "/Bv"])
            if result[0] == 0:
                return {"version": result[1].strip()}

        return {"version": "unknown"}


class CompilerManager:
    """
    Manages compiler detection, selection, and operations.

    This class provides a centralized way to work with compilers including
    automatically detecting available compilers and managing preferences.
    """
    def __init__(self):
        """Initialize the compiler manager."""
        self.compilers: Dict[str, Compiler] = {}
        self.default_compiler: Optional[str] = None

    def detect_compilers(self) -> Dict[str, Compiler]:
        """
        Detect available compilers on the system.

        Returns:
            Dictionary of compiler names to Compiler objects
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
                            CppVersion.CPP98, CppVersion.CPP03, CppVersion.CPP11,
                            CppVersion.CPP14, CppVersion.CPP17, CppVersion.CPP20
                        } | ({CppVersion.CPP23} if version >= "11.0" else set()),
                        supported_sanitizers={"address", "thread", "undefined", "leak"},
                        supported_optimizations={"-O0", "-O1", "-O2", "-O3", "-Ofast", "-Os", "-Og"},
                        feature_flags={"lto": "-flto", "coverage": "--coverage"}
                    )
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
                            CppVersion.CPP98, CppVersion.CPP03, CppVersion.CPP11,
                            CppVersion.CPP14, CppVersion.CPP17, CppVersion.CPP20
                        } | ({CppVersion.CPP23} if version >= "15.0" else set()),
                        supported_sanitizers={"address", "thread", "undefined", "memory", "dataflow"},
                        supported_optimizations={"-O0", "-O1", "-O2", "-O3", "-Ofast", "-Os", "-Oz"},
                        feature_flags={"lto": "-flto", "coverage": "--coverage"}
                    )
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
                            supports_modules=(version >= "19.29"),  # Visual Studio 2019 16.10+
                            supported_cpp_versions={
                                CppVersion.CPP11, CppVersion.CPP14,
                                CppVersion.CPP17, CppVersion.CPP20
                            } | ({CppVersion.CPP23} if version >= "19.35" else set()),
                            supported_sanitizers={"address"},
                            supported_optimizations={"/O1", "/O2", "/Ox", "/Od"},
                            feature_flags={"lto": "/GL", "whole_program": "/GL"}
                        )
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

        Args:
            name: Name of the compiler to get

        Returns:
            Compiler object

        Raises:
            CompilerNotFoundError: If the compiler is not found
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
            raise CompilerNotFoundError(f"Compiler '{name}' not found. Available compilers: {', '.join(self.compilers.keys())}")

    def _find_command(self, command: str) -> Optional[str]:
        """
        Find a command in the system path.

        Args:
            command: Command to find

        Returns:
            Path to the command if found, None otherwise
        """
        path = shutil.which(command)
        return path

    def _find_msvc(self) -> Optional[str]:
        """
        Find the MSVC compiler (cl.exe) on Windows.

        Returns:
            Path to cl.exe if found, None otherwise
        """
        # Try direct path first
        cl_path = shutil.which("cl")
        if cl_path:
            return cl_path

        # Check Visual Studio installation locations
        if platform.system() == "Windows":
            # Use vswhere.exe if available
            vswhere = os.path.join(
                os.environ.get("ProgramFiles(x86)", ""),
                "Microsoft Visual Studio", "Installer", "vswhere.exe"
            )

            if os.path.exists(vswhere):
                result = subprocess.run(
                    [vswhere, "-latest", "-products", "*", "-requires", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
                     "-property", "installationPath", "-format", "value"],
                    capture_output=True,
                    text=True,
                    check=False
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
                                candidate = os.path.join(cl_path, latest, "bin", "Host" + arch, arch, "cl.exe")
                                if os.path.exists(candidate):
                                    return candidate

        return None

    def _get_compiler_version(self, compiler_path: str) -> str:
        """
        Get version string from a compiler.

        Args:
            compiler_path: Path to the compiler executable

        Returns:
            Version string, or "unknown" if version cannot be determined
        """
        try:
            if "cl" in os.path.basename(compiler_path).lower():
                # MSVC
                result = subprocess.run([compiler_path], stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                        universal_newlines=True)
                match = re.search(r'Version\s+(\d+\.\d+\.\d+)', result.stderr)
                if match:
                    return match.group(1)
                return "unknown"
            else:
                # GCC or Clang
                result = subprocess.run([compiler_path, "--version"], stdout=subprocess.PIPE,
                                        stderr=subprocess.PIPE, universal_newlines=True)
                first_line = result.stdout.splitlines()[0]
                # Extract version number
                match = re.search(r'(\d+\.\d+\.\d+)', first_line)
                if match:
                    return match.group(1)
                return "unknown"
        except Exception as e:
            logger.warning(f"Failed to get compiler version: {e}")
            return "unknown"


class BuildManager:
    """
    Manages the build process for a collection of source files.

    Features:
    - Dependency scanning and tracking
    - Incremental builds (only compile what changed)
    - Parallel compilation
    - Multiple compiler support
    """
    def __init__(self,
                compiler_manager: Optional[CompilerManager] = None,
                build_dir: Optional[PathLike] = None,
                parallel: bool = True,
                max_workers: Optional[int] = None):
        """
        Initialize the build manager.

        Args:
            compiler_manager: Compiler manager to use
            build_dir: Directory for build artifacts
            parallel: Whether to use parallel compilation
            max_workers: Maximum number of worker threads for parallel compilation
        """
        self.compiler_manager = compiler_manager or CompilerManager()
        self.build_dir = Path(build_dir) if build_dir else Path("build")
        self.parallel = parallel
        self.max_workers = max_workers or min(32, os.cpu_count() or 4)
        self.cache_file = self.build_dir / "build_cache.json"
        self.dependency_graph: Dict[Path, Set[Path]] = defaultdict(set)
        self.file_hashes: Dict[str, str] = {}

        # Create build directory if it doesn't exist
        self.build_dir.mkdir(parents=True, exist_ok=True)

        # Load cache if available
        self._load_cache()

    def build(self,
             source_files: List[PathLike],
             output_file: PathLike,
             compiler_name: Optional[str] = None,
             cpp_version: CppVersion = CppVersion.CPP17,
             compile_options: Optional[CompileOptions] = None,
             link_options: Optional[LinkOptions] = None,
             incremental: bool = True,
             force_rebuild: bool = False) -> CompilationResult:
        """
        Build source files into an executable or library.

        Args:
            source_files: List of source files to compile
            output_file: Output executable/library path
            compiler_name: Name of compiler to use (uses default if None)
            cpp_version: C++ standard version to use
            compile_options: Options for compilation
            link_options: Options for linking
            incremental: Whether to use incremental builds
            force_rebuild: Whether to force rebuilding all files

        Returns:
            CompilationResult with build result
        """
        start_time = time.time()
        source_paths = [Path(f) for f in source_files]
        output_path = Path(output_file)

        # Get compiler
        compiler = self.compiler_manager.get_compiler(compiler_name)

        # Create object directory for this build
        obj_dir = self.build_dir / f"{compiler.name}_{cpp_version.value}"
        obj_dir.mkdir(parents=True, exist_ok=True)

        # Prepare options
        compile_options = compile_options or {}
        link_options = link_options or {}

        # Calculate what needs to be rebuilt
        to_compile: List[Path] = []
        object_files: List[Path] = []

        if incremental and not force_rebuild:
            # Analyze dependencies and determine what files need rebuilding
            to_compile = self._get_files_to_rebuild(source_paths, compiler, cpp_version)
        else:
            # Rebuild everything
            to_compile = source_paths

        # Map source files to object files
        for source_file in source_paths:
            obj_file = obj_dir / f"{source_file.stem}{source_file.suffix}.o"
            object_files.append(obj_file)

        # Compile files that need rebuilding
        compile_results = []

        if to_compile:
            logger.info(f"Compiling {len(to_compile)} of {len(source_paths)} files")

            # Use parallel compilation if enabled and supported
            if self.parallel and compiler.features.supports_parallel and len(to_compile) > 1:
                with concurrent.futures.ThreadPoolExecutor(max_workers=self.max_workers) as executor:
                    future_to_file = {}
                    for source_file in to_compile:
                        idx = source_paths.index(source_file)
                        obj_file = object_files[idx]
                        future = executor.submit(
                            compiler.compile,
                            [source_file],
                            obj_file,
                            cpp_version,
                            compile_options
                        )
                        future_to_file[future] = source_file

                    for future in concurrent.futures.as_completed(future_to_file):
                        source_file = future_to_file[future]
                        try:
                            result = future.result()
                            compile_results.append(result)
                            if not result.success:
                                return CompilationResult(
                                    success=False,
                                    errors=[f"Failed to compile {source_file}: {result.errors}"],
                                    warnings=result.warnings,
                                    duration_ms=(time.time() - start_time) * 1000
                                )
                        except Exception as e:
                            return CompilationResult(
                                success=False,
                                errors=[f"Exception while compiling {source_file}: {str(e)}"],
                                duration_ms=(time.time() - start_time) * 1000
                            )
            else:
                # Sequential compilation
                for source_file in to_compile:
                    idx = source_paths.index(source_file)
                    obj_file = object_files[idx]
                    result = compiler.compile([source_file], obj_file, cpp_version, compile_options)
                    compile_results.append(result)
                    if not result.success:
                        return CompilationResult(
                            success=False,
                            errors=[f"Failed to compile {source_file}: {result.errors}"],
                            warnings=result.warnings,
                            duration_ms=(time.time() - start_time) * 1000
                        )

            # Update cache with new file hashes
            self._update_file_hashes(to_compile)

        # Link object files
        link_result = compiler.link(object_files, output_file, link_options)
        if not link_result.success:
            return CompilationResult(
                success=False,
                errors=[f"Failed to link: {link_result.errors}"],
                warnings=link_result.warnings,
                duration_ms=(time.time() - start_time) * 1000
            )

        # Save cache
        self._save_cache()

        # Aggregate warnings from compilation and linking
        all_warnings = []
        for result in compile_results:
            all_warnings.extend(result.warnings)
        all_warnings.extend(link_result.warnings)

        return CompilationResult(
            success=True,
            output_file=output_path,
            duration_ms=(time.time() - start_time) * 1000,
            warnings=all_warnings
        )

    def _get_files_to_rebuild(self,
                             source_files: List[Path],
                             compiler: Compiler,
                             cpp_version: CppVersion) -> List[Path]:
        """
        Determine which files need to be rebuilt based on changes.

        Args:
            source_files: List of source files
            compiler: Compiler being used
            cpp_version: C++ version being used

        Returns:
            List of files that need to be rebuilt
        """
        to_rebuild = []

        # Update dependency graph
        for file in source_files:
            if not file.exists():
                raise FileNotFoundError(f"Source file not found: {file}")

            # Get dependencies for this file
            self._scan_dependencies(file)

            # Check if this file or any of its dependencies changed
            if self._has_file_changed(file) or any(self._has_file_changed(dep) for dep in self.dependency_graph[file]):
                to_rebuild.append(file)

        return to_rebuild

    def _scan_dependencies(self, file_path: Path):
        """
        Scan a file for its dependencies (include files).

        Args:
            file_path: Path to the source file
        """
        # Reset dependencies for this file
        self.dependency_graph[file_path].clear()

        try:
            with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                for line in f:
                    # Look for #include statements
                    if line.strip().startswith('#include'):
                        include_match = re.search(r'#include\s+["<](.*?)[">]', line)
                        if include_match:
                            include_file = include_match.group(1)
                            # For now, we only track that there is a dependency
                            # In a more advanced implementation, we would resolve these paths
                            # and recursively scan included files
                            self.dependency_graph[file_path].add(Path(include_file))
        except Exception as e:
            logger.warning(f"Failed to scan dependencies for {file_path}: {e}")

    def _has_file_changed(self, file_path: Path) -> bool:
        """
        Check if a file has changed since the last build.

        Args:
            file_path: Path to the file

        Returns:
            True if the file has changed or is new
        """
        if not file_path.exists():
            return False

        # Calculate file hash
        current_hash = self._calculate_file_hash(file_path)

        # Check if hash changed
        str_path = str(file_path.resolve())
        if str_path not in self.file_hashes:
            return True  # New file

        return self.file_hashes[str_path] != current_hash

    def _calculate_file_hash(self, file_path: Path) -> str:
        """
        Calculate MD5 hash of a file's contents.

        Args:
            file_path: Path to the file

        Returns:
            MD5 hash string
        """
        hash_md5 = hashlib.md5()
        with open(file_path, "rb") as f:
            for chunk in iter(lambda: f.read(4096), b""):
                hash_md5.update(chunk)
        return hash_md5.hexdigest()

    def _update_file_hashes(self, files: List[Path]):
        """
        Update stored hashes for files.

        Args:
            files: List of files to update hashes for
        """
        for file_path in files:
            if file_path.exists():
                str_path = str(file_path.resolve())
                self.file_hashes[str_path] = self._calculate_file_hash(file_path)

    def _load_cache(self):
        """Load build cache from disk."""
        if self.cache_file.exists():
            try:
                with open(self.cache_file, 'r') as f:
                    data = json.load(f)
                    self.file_hashes = data.get('file_hashes', {})
            except Exception as e:
                logger.warning(f"Failed to load build cache: {e}")

    def _save_cache(self):
        """Save build cache to disk."""
        try:
            cache_data = {
                'file_hashes': self.file_hashes
            }
            with open(self.cache_file, 'w') as f:
                json.dump(cache_data, f, indent=2)
        except Exception as e:
            logger.warning(f"Failed to save build cache: {e}")


def load_json(file_path: PathLike) -> Dict[str, Any]:
    """
    Load and parse a JSON file.

    Args:
        file_path: Path to the JSON file

    Returns:
        Parsed JSON data as a dictionary

    Raises:
        FileNotFoundError: If the file doesn't exist
        json.JSONDecodeError: If the file contains invalid JSON
    """
    path = Path(file_path)
    if not path.exists():
        raise FileNotFoundError(f"JSON file not found: {path}")

    with open(path, 'r', encoding='utf-8') as f:
        return json.load(f)


def save_json(file_path: PathLike, data: Dict[str, Any], indent: int = 2) -> None:
    """
    Save data to a JSON file.

    Args:
        file_path: Path to save the JSON file
        data: Data to save
        indent: Indentation level for formatted JSON
    """
    path = Path(file_path)
    path.parent.mkdir(parents=True, exist_ok=True)

    with open(path, 'w', encoding='utf-8') as f:
        json.dump(data, f, indent=indent)


# Create a singleton compiler manager for global use
compiler_manager = CompilerManager()


def get_compiler(name: Optional[str] = None) -> Compiler:
    """
    Get a compiler by name, or the default compiler if no name is provided.

    This is a convenience function that uses the global compiler manager.

    Args:
        name: Name of the compiler to get

    Returns:
        Compiler object
    """
    return compiler_manager.get_compiler(name)


def compile_file(source_file: PathLike,
                output_file: PathLike,
                compiler_name: Optional[str] = None,
                cpp_version: Union[str, CppVersion] = CppVersion.CPP17,
                options: Optional[CompileOptions] = None) -> CompilationResult:
    """
    Compile a single source file.

    This is a convenience function for simple compilation tasks.

    Args:
        source_file: Source file to compile
        output_file: Path to the output object file
        compiler_name: Name of compiler to use
        cpp_version: C++ standard version to use
        options: Additional compilation options

    Returns:
        CompilationResult with compilation status
    """
    # Convert string cpp_version to enum if needed
    if isinstance(cpp_version, str):
        try:
            cpp_version = CppVersion(cpp_version)
        except ValueError:
            try:
                cpp_version = CppVersion["CPP" + cpp_version.replace("++", "").replace("c", "")]
            except KeyError:
                raise ValueError(f"Invalid C++ version: {cpp_version}")

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

    This is a convenience function for building projects.

    Args:
        source_files: List of source files to compile
        output_file: Path to the output executable/library
        compiler_name: Name of compiler to use
        cpp_version: C++ standard version to use
        compile_options: Options for compilation
        link_options: Options for linking
        build_dir: Directory for build artifacts
        incremental: Whether to use incremental builds

    Returns:
        CompilationResult with build status
    """
    # Convert string cpp_version to enum if needed
    if isinstance(cpp_version, str):
        try:
            cpp_version = CppVersion(cpp_version)
        except ValueError:
            try:
                cpp_version = CppVersion["CPP" + cpp_version.replace("++", "").replace("c", "")]
            except KeyError:
                raise ValueError(f"Invalid C++ version: {cpp_version}")

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


def main():
    """
    Main function for command-line usage.
    """
    parser = argparse.ArgumentParser(
        description="Enhanced Compiler Helper Tool",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Compile a single file
  python compiler_helper.py source.cpp -o output.o --cpp-version c++20

  # Compile and link multiple files
  python compiler_helper.py source1.cpp source2.cpp -o myprogram --link --compiler GCC

  # Build with specific options
  python compiler_helper.py source.cpp -o output.o --include-path ./include --define DEBUG=1

  # Use incremental builds
  python compiler_helper.py *.cpp -o myprogram --build-dir ./build --incremental
"""
    )

    # Basic arguments
    parser.add_argument("source_files", nargs="+", type=Path, help="Source files to compile")
    parser.add_argument("-o", "--output", type=Path, required=True, help="Output file (object or executable)")
    parser.add_argument("--compiler", type=str, help="Compiler to use (GCC, Clang, MSVC)")
    parser.add_argument("--cpp-version", type=str, default="c++17", help="C++ standard version (e.g., c++17, c++20)")
    parser.add_argument("--link", action="store_true", help="Link the object files into an executable")

    # Build options
    build_group = parser.add_argument_group("Build options")
    build_group.add_argument("--build-dir", type=Path, help="Directory for build artifacts")
    build_group.add_argument("--incremental", action="store_true", help="Use incremental builds (only compile changed files)")
    build_group.add_argument("--force-rebuild", action="store_true", help="Force rebuilding all files")
    build_group.add_argument("--parallel", action="store_true", default=True, help="Use parallel compilation")
    build_group.add_argument("--jobs", type=int, help="Number of parallel compilation jobs")

    # Compilation options
    compile_group = parser.add_argument_group("Compilation options")
    compile_group.add_argument("--include-path", "-I", action="append", dest="include_paths", help="Add include directory")
    compile_group.add_argument("--define", "-D", action="append", dest="defines", help="Add preprocessor definition (e.g., DEBUG=1)")
    compile_group.add_argument("--warnings", "-W", action="append", help="Add warning flags")
    compile_group.add_argument("--optimization", "-O", help="Set optimization level")
    compile_group.add_argument("--debug", "-g", action="store_true", help="Include debug information")
    compile_group.add_argument("--pic", action="store_true", help="Generate position-independent code")
    compile_group.add_argument("--stdlib", help="Specify standard library to use")
    compile_group.add_argument("--sanitize", action="append", dest="sanitizers", help="Enable sanitizer")

    # Linking options
    link_group = parser.add_argument_group("Linking options")
    link_group.add_argument("--library-path", "-L", action="append", dest="library_paths", help="Add library directory")
    link_group.add_argument("--library", "-l", action="append", dest="libraries", help="Add library to link against")
    link_group.add_argument("--shared", action="store_true", help="Create a shared library")
    link_group.add_argument("--static", action="store_true", help="Prefer static linking")
    link_group.add_argument("--strip", action="store_true", help="Strip debug symbols")
    link_group.add_argument("--map-file", help="Generate map file")

    # Additional flags
    parser.add_argument("--compile-flags", nargs="*", help="Additional compilation flags")
    parser.add_argument("--link-flags", nargs="*", help="Additional linking flags")
    parser.add_argument("--flags", nargs="*", help="Additional flags for both compilation and linking")
    parser.add_argument("--config", type=Path, help="Load options from configuration file (JSON)")

    # Output control
    parser.add_argument("--verbose", "-v", action="count", default=0, help="Increase verbosity")
    parser.add_argument("--quiet", "-q", action="store_true", help="Suppress non-error output")
    parser.add_argument("--list-compilers", action="store_true", help="List available compilers and exit")

    args = parser.parse_args()

    # Configure logging based on verbosity
    if args.quiet:
        logger.setLevel(logging.WARNING)
    elif args.verbose == 1:
        logger.setLevel(logging.INFO)
    elif args.verbose >= 2:
        logger.setLevel(logging.DEBUG)

    # Handle list-compilers flag
    if args.list_compilers:
        compilers = compiler_manager.detect_compilers()
        if compilers:
            print("Available compilers:")
            for name, compiler in compilers.items():
                print(f"  {name}: {compiler.command} (version: {compiler.version})")
            print(f"Default compiler: {compiler_manager.default_compiler}")
        else:
            print("No supported compilers found.")
        return 0

    # Parse C++ version
    cpp_version = args.cpp_version

    # Prepare compile options
    compile_options: CompileOptions = {}

    if args.include_paths:
        compile_options['include_paths'] = args.include_paths

    if args.defines:
        defines = {}
        for define in args.defines:
            if '=' in define:
                name, value = define.split('=', 1)
                defines[name] = value
            else:
                defines[define] = None
        compile_options['defines'] = defines

    if args.warnings:
        compile_options['warnings'] = args.warnings

    if args.optimization:
        compile_options['optimization'] = args.optimization

    if args.debug:
        compile_options['debug'] = True

    if args.pic:
        compile_options['position_independent'] = True

    if args.stdlib:
        compile_options['standard_library'] = args.stdlib

    if args.sanitizers:
        compile_options['sanitizers'] = args.sanitizers

    if args.compile_flags:
        compile_options['extra_flags'] = args.compile_flags

    # Prepare link options
    link_options: LinkOptions = {}

    if args.library_paths:
        link_options['library_paths'] = args.library_paths

    if args.libraries:
        link_options['libraries'] = args.libraries

    if args.shared:
        link_options['shared'] = True

    if args.static:
        link_options['static'] = True

    if args.strip:
        link_options['strip'] = True

    if args.map_file:
        link_options['map_file'] = args.map_file

    if args.link_flags:
        link_options['extra_flags'] = args.link_flags

    # Load configuration from file if provided
    if args.config:
        try:
            config = load_json(args.config)

            # Update compile options
            if 'compile_options' in config:
                for key, value in config['compile_options'].items():
                    compile_options[key] = value

            # Update link options
            if 'link_options' in config:
                for key, value in config['link_options'].items():
                    link_options[key] = value

            # General options can override specific ones
            if 'options' in config:
                if 'compiler' in config['options'] and not args.compiler:
                    args.compiler = config['options']['compiler']
                if 'cpp_version' in config['options'] and cpp_version == "c++17":
                    cpp_version = config['options']['cpp_version']
                if 'incremental' in config['options'] and not args.incremental:
                    args.incremental = config['options']['incremental']
                if 'build_dir' in config['options'] and not args.build_dir:
                    args.build_dir = config['options']['build_dir']

        except Exception as e:
            logger.error(f"Failed to load configuration file: {e}")
            return 1

    # Combine extra flags if provided
    if args.flags:
        if 'extra_flags' not in compile_options:
            compile_options['extra_flags'] = []
        compile_options['extra_flags'].extend(args.flags)

        if 'extra_flags' not in link_options:
            link_options['extra_flags'] = []
        link_options['extra_flags'].extend(args.flags)

    # Set up build manager
    build_manager = BuildManager(
        build_dir=args.build_dir,
        parallel=args.parallel,
        max_workers=args.jobs
    )

    # Execute build
    result = build_manager.build(
        source_files=args.source_files,
        output_file=args.output,
        compiler_name=args.compiler,
        cpp_version=cpp_version,
        compile_options=compile_options,
        link_options=link_options if args.link else None,
        incremental=args.incremental,
        force_rebuild=args.force_rebuild
    )

    # Print result
    if result.success:
        logger.info(f"Build successful: {result.output_file} (took {result.duration_ms:.2f}ms)")
        for warning in result.warnings:
            logger.warning(warning)
        return 0
    else:
        logger.error("Build failed:")
        for error in result.errors:
            logger.error(f"  {error}")
        for warning in result.warnings:
            logger.warning(f"  {warning}")
        return 1


# Pybind11 module definition (to be used when imported as a module)
def create_pybind11_module(module):
    """
    Create pybind11 bindings for this module.

    Args:
        module: pybind11 module object
    """
    import pybind11

    # Bind enums
    pybind11.enum_<CppVersion>(module, "CppVersion")
        .value("CPP98", CppVersion.CPP98)
        .value("CPP03", CppVersion.CPP03)
        .value("CPP11", CppVersion.CPP11)
        .value("CPP14", CppVersion.CPP14)
        .value("CPP17", CppVersion.CPP17)
        .value("CPP20", CppVersion.CPP20)
        .value("CPP23", CppVersion.CPP23)
        .export_values()

    pybind11.enum_<CompilerType>(module, "CompilerType")
        .value("GCC", CompilerType.GCC)
        .value("CLANG", CompilerType.CLANG)
        .value("MSVC", CompilerType.MSVC)
        .value("ICC", CompilerType.ICC)
        .value("MINGW", CompilerType.MINGW)
        .value("EMSCRIPTEN", CompilerType.EMSCRIPTEN)
        .export_values()

    # Bind CompilationResult
    pybind11.class_<CompilationResult>(module, "CompilationResult")
        .def_readonly("success", &CompilationResult.success)
        .def_readonly("output_file", &CompilationResult.output_file)
        .def_readonly("duration_ms", &CompilationResult.duration_ms)
        .def_readonly("command_line", &CompilationResult.command_line)
        .def_readonly("errors", &CompilationResult.errors)
        .def_readonly("warnings", &CompilationResult.warnings)

    # Bind Compiler
    pybind11.class_<Compiler>(module, "Compiler")
        .def("compile", &Compiler.compile)
        .def("link", &Compiler.link)
        .def("get_version_info", &Compiler.get_version_info)

    # Bind CompilerManager
    pybind11.class_<CompilerManager>(module, "CompilerManager")
        .def(pybind11.init<>())
        .def("detect_compilers", &CompilerManager.detect_compilers)
        .def("get_compiler", &CompilerManager.get_compiler)

    # Bind BuildManager
    pybind11.class_<BuildManager>(module, "BuildManager")
        .def(pybind11.init<CompilerManager*, PathLike, bool, int>(),
             pybind11.arg("compiler_manager") = nullptr,
             pybind11.arg("build_dir") = "build",
             pybind11.arg("parallel") = true,
             pybind11.arg("max_workers") = nullptr)
        .def("build", &BuildManager.build)

    # Add convenience functions
    module.def("get_compiler", &get_compiler, pybind11.arg("name") = nullptr)
    module.def("compile_file", &compile_file)
    module.def("build_project", &build_project)

    # Add globals
    module.attr("compiler_manager") = compiler_manager


if __name__ == "__main__":
    sys.exit(main())
