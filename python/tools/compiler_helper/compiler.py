#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Compiler class implementation for the compiler helper module.
"""
from dataclasses import dataclass, field
import os
import platform
import subprocess
import re
import time
import shutil
from pathlib import Path
from typing import List, Dict, Optional, Set

from loguru import logger

from .core_types import (
    CommandResult,
    PathLike,
    CompilationResult,
    CompilerFeatures,
    CompilerType,
    CppVersion,
    CompileOptions,
    LinkOptions,
    CompilationError,
    CompilerNotFoundError,
)


@dataclass
class Compiler:
    """
    Class representing a compiler with its command and compilation capabilities.
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
            raise CompilerNotFoundError(
                f"Compiler {self.name} not found or not executable: {self.command}"
            )

    def compile(
        self,
        source_files: List[PathLike],
        output_file: PathLike,
        cpp_version: CppVersion,
        options: Optional[CompileOptions] = None,
    ) -> CompilationResult:
        """
        Compile source files into an object file or executable.
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
                duration_ms=(time.time() - start_time) * 1000,
            )

        # Build command with all options
        cmd = [self.command, version_flag]

        # Add include paths
        for path in options.get("include_paths", []):
            if self.compiler_type == CompilerType.MSVC:
                cmd.append(f"/I{path}")
            else:
                cmd.append("-I")
                cmd.append(str(path))

        # Add preprocessor definitions
        for name, value in options.get("defines", {}).items():
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
        cmd.extend(options.get("warnings", []))

        # Add optimization level
        if "optimization" in options:
            cmd.append(options["optimization"])

        # Add debug flag if requested
        if options.get("debug", False):
            if self.compiler_type == CompilerType.MSVC:
                cmd.append("/Zi")
            else:
                cmd.append("-g")

        # Position independent code
        if (
            options.get("position_independent", False)
            and self.compiler_type != CompilerType.MSVC
        ):
            cmd.append("-fPIC")

        # Add sanitizers
        for sanitizer in options.get("sanitizers", []):
            if sanitizer in self.features.supported_sanitizers:
                if self.compiler_type == CompilerType.MSVC:
                    if sanitizer == "address":
                        cmd.append("/fsanitize=address")
                else:
                    cmd.append(f"-fsanitize={sanitizer}")

        # Add standard library specification
        if "standard_library" in options and self.compiler_type != CompilerType.MSVC:
            cmd.append(f"-stdlib={options['standard_library']}")

        # Add default compile flags for this compiler
        cmd.extend(self.additional_compile_flags)

        # Add extra flags
        cmd.extend(options.get("extra_flags", []))

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
                warnings=warnings,
            )

        # Check if output file was created
        if not output_path.exists():
            return CompilationResult(
                success=False,
                command_line=cmd,
                duration_ms=elapsed_time,
                errors=[
                    f"Compilation completed but output file was not created: {output_path}"
                ],
            )

        # Parse warnings (even if successful)
        _, warnings = self._parse_diagnostics(result[2])

        return CompilationResult(
            success=True,
            output_file=output_path,
            command_line=cmd,
            duration_ms=elapsed_time,
            warnings=warnings,
        )

    def link(
        self,
        object_files: List[PathLike],
        output_file: PathLike,
        options: Optional[LinkOptions] = None,
    ) -> CompilationResult:
        """
        Link object files into an executable or library.
        """
        start_time = time.time()
        options = options or {}
        output_path = Path(output_file)

        # Ensure output directory exists
        output_path.parent.mkdir(parents=True, exist_ok=True)

        # Start building command
        cmd = [self.command]

        # Handle shared library creation
        if options.get("shared", False):
            if self.compiler_type == CompilerType.MSVC:
                cmd.append("/DLL")
            else:
                cmd.append("-shared")

        # Handle static linking preference
        if options.get("static", False) and self.compiler_type != CompilerType.MSVC:
            cmd.append("-static")

        # Add library paths
        for path in options.get("library_paths", []):
            if self.compiler_type == CompilerType.MSVC:
                cmd.append(f"/LIBPATH:{path}")
            else:
                cmd.append(f"-L{path}")

        # Add runtime library paths
        if self.compiler_type != CompilerType.MSVC:
            for path in options.get("runtime_library_paths", []):
                if platform.system() == "Darwin":
                    cmd.append(f"-Wl,-rpath,{path}")
                else:
                    cmd.append(f"-Wl,-rpath={path}")

        # Add libraries
        for lib in options.get("libraries", []):
            if self.compiler_type == CompilerType.MSVC:
                cmd.append(f"{lib}.lib")
            else:
                cmd.append(f"-l{lib}")

        # Strip debug symbols if requested
        if options.get("strip", False):
            if self.compiler_type == CompilerType.MSVC:
                pass  # MSVC handles this differently
            else:
                cmd.append("-s")

        # Add map file if requested
        if "map_file" in options and options["map_file"] is not None:
            map_path = Path(options["map_file"])
            if self.compiler_type == CompilerType.MSVC:
                cmd.append(f"/MAP:{map_path}")
            else:
                cmd.append(f"-Wl,-Map={map_path}")

        # Add default link flags
        cmd.extend(self.additional_link_flags)

        # Add extra flags
        cmd.extend(options.get("extra_flags", []))

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
                warnings=warnings,
            )

        # Check if output file was created
        if not output_path.exists():
            return CompilationResult(
                success=False,
                command_line=cmd,
                duration_ms=elapsed_time,
                errors=[
                    f"Linking completed but output file was not created: {output_path}"
                ],
            )

        # Parse warnings (even if successful)
        _, warnings = self._parse_diagnostics(result[2])

        return CompilationResult(
            success=True,
            output_file=output_path,
            command_line=cmd,
            duration_ms=elapsed_time,
            warnings=warnings,
        )

    def _run_command(self, cmd: List[str]) -> CommandResult:
        """Execute a command and return its exit code, stdout, and stderr."""
        try:
            process = subprocess.Popen(
                cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                universal_newlines=True,
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
            error_pattern = re.compile(r".*?[Ee]rror\s+[A-Za-z0-9]+:.*")
            warning_pattern = re.compile(r".*?[Ww]arning\s+[A-Za-z0-9]+:.*")
        else:
            error_pattern = re.compile(r".*?:[0-9]+:[0-9]+:\s+error:.*")
            warning_pattern = re.compile(r".*?:[0-9]+:[0-9]+:\s+warning:.*")

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
