#!/usr/bin/env python3
"""
Enhanced compiler implementation with modern Python features and async support.

This module provides a comprehensive compiler abstraction with async-first design,
enhanced error handling, and performance monitoring capabilities.
"""

from __future__ import annotations

import asyncio
import os
import platform
import re
import shutil
import time
from pathlib import Path
from typing import Any, Dict, List, Optional, Set
from dataclasses import dataclass, field

from loguru import logger
from pydantic import BaseModel, ConfigDict, Field, field_validator

from .core_types import (
    CommandResult, PathLike, CompilationResult, CompilerFeatures, CompilerType,
    CppVersion, CompileOptions, LinkOptions, CompilationError,
    CompilerNotFoundError, OptimizationLevel
)
from .utils import ProcessManager, SystemInfo


class CompilerConfig(BaseModel):
    """Enhanced compiler configuration with validation using Pydantic v2."""
    
    model_config = ConfigDict(
        extra='forbid',
        validate_assignment=True,
        str_strip_whitespace=True
    )
    
    name: str = Field(description="Compiler display name")
    command: str = Field(description="Path to compiler executable")
    compiler_type: CompilerType = Field(description="Type of compiler")
    version: str = Field(description="Compiler version string")
    
    cpp_flags: Dict[CppVersion, str] = Field(
        default_factory=dict,
        description="C++ standard flags for each version"
    )
    additional_compile_flags: List[str] = Field(
        default_factory=list,
        description="Additional default compilation flags"
    )
    additional_link_flags: List[str] = Field(
        default_factory=list,
        description="Additional default linking flags"
    )
    features: CompilerFeatures = Field(
        default_factory=CompilerFeatures,
        description="Compiler capabilities and features"
    )
    
    @field_validator('command')
    @classmethod
    def validate_command_path(cls, v: str) -> str:
        """Validate that the compiler command exists and is executable."""
        if not os.path.isabs(v):
            resolved_path = shutil.which(v)
            if resolved_path:
                v = resolved_path
            else:
                raise ValueError(f"Compiler command not found in PATH: {v}")
        
        if not os.access(v, os.X_OK):
            raise ValueError(f"Compiler is not executable: {v}")
        
        return v


class DiagnosticParser:
    """Enhanced diagnostic parser for compiler output."""
    
    def __init__(self, compiler_type: CompilerType) -> None:
        self.compiler_type = compiler_type
        self._setup_patterns()
    
    def _setup_patterns(self) -> None:
        """Setup regex patterns for parsing compiler diagnostics."""
        if self.compiler_type == CompilerType.MSVC:
            self.error_pattern = re.compile(
                r'([^\(]+)\((\d+)(?:,(\d+))?\)\s*:\s*error\s+([^:]+):\s*(.+)',
                re.IGNORECASE
            )
            self.warning_pattern = re.compile(
                r'([^\(]+)\((\d+)(?:,(\d+))?\)\s*:\s*warning\s+([^:]+):\s*(.+)',
                re.IGNORECASE
            )
            self.note_pattern = re.compile(
                r'([^\(]+)\((\d+)(?:,(\d+))?\)\s*:\s*note:\s*(.+)',
                re.IGNORECASE
            )
        else:
            # GCC/Clang style diagnostics
            self.error_pattern = re.compile(
                r'([^:]+):(\d+):(\d+):\s*error:\s*(.+)'
            )
            self.warning_pattern = re.compile(
                r'([^:]+):(\d+):(\d+):\s*warning:\s*(.+)'
            )
            self.note_pattern = re.compile(
                r'([^:]+):(\d+):(\d+):\s*note:\s*(.+)'
            )
    
    def parse_diagnostics(
        self,
        output: str
    ) -> tuple[List[str], List[str], List[str]]:
        """
        Parse compiler output to extract errors, warnings, and notes.
        
        Returns:
            Tuple of (errors, warnings, notes)
        """
        errors = []
        warnings = []
        notes = []
        
        for line in output.splitlines():
            line = line.strip()
            if not line:
                continue
            
            if self.error_pattern.match(line):
                errors.append(line)
            elif self.warning_pattern.match(line):
                warnings.append(line)
            elif self.note_pattern.match(line):
                notes.append(line)
        
        return errors, warnings, notes


class CompilerMetrics:
    """Tracks compiler performance metrics."""
    
    def __init__(self) -> None:
        self.total_compilations = 0
        self.successful_compilations = 0
        self.total_compilation_time = 0.0
        self.total_link_time = 0.0
        self.cache_hits = 0
        self.cache_misses = 0
    
    def record_compilation(
        self,
        success: bool,
        duration: float,
        is_link: bool = False
    ) -> None:
        """Record compilation metrics."""
        self.total_compilations += 1
        if success:
            self.successful_compilations += 1
        
        if is_link:
            self.total_link_time += duration
        else:
            self.total_compilation_time += duration
    
    def record_cache_hit(self) -> None:
        """Record cache hit."""
        self.cache_hits += 1
    
    def record_cache_miss(self) -> None:
        """Record cache miss."""
        self.cache_misses += 1
    
    @property
    def success_rate(self) -> float:
        """Calculate compilation success rate."""
        if self.total_compilations == 0:
            return 0.0
        return self.successful_compilations / self.total_compilations
    
    @property
    def average_compilation_time(self) -> float:
        """Calculate average compilation time."""
        if self.successful_compilations == 0:
            return 0.0
        return self.total_compilation_time / self.successful_compilations
    
    @property
    def cache_hit_rate(self) -> float:
        """Calculate cache hit rate."""
        total_accesses = self.cache_hits + self.cache_misses
        if total_accesses == 0:
            return 0.0
        return self.cache_hits / total_accesses
    
    def to_dict(self) -> Dict[str, Any]:
        """Export metrics as dictionary."""
        return {
            "total_compilations": self.total_compilations,
            "successful_compilations": self.successful_compilations,
            "success_rate": self.success_rate,
            "total_compilation_time": self.total_compilation_time,
            "total_link_time": self.total_link_time,
            "average_compilation_time": self.average_compilation_time,
            "cache_hits": self.cache_hits,
            "cache_misses": self.cache_misses,
            "cache_hit_rate": self.cache_hit_rate
        }


class EnhancedCompiler:
    """
    Enhanced compiler class with modern Python features and async support.
    
    Features:
    - Async-first design for non-blocking operations
    - Comprehensive error handling and diagnostics
    - Performance metrics tracking
    - Intelligent caching support
    - Plugin architecture for extensibility
    """
    
    def __init__(self, config: CompilerConfig) -> None:
        self.config = config
        self.diagnostic_parser = DiagnosticParser(config.compiler_type)
        self.metrics = CompilerMetrics()
        self.process_manager = ProcessManager()
        
        # Validate compiler on initialization
        self._validate_compiler()
        
        logger.info(
            f"Initialized compiler: {config.name} ({config.compiler_type.value})",
            extra={
                "compiler_name": config.name,
                "compiler_type": config.compiler_type.value,
                "version": config.version,
                "command": config.command
            }
        )
    
    def _validate_compiler(self) -> None:
        """Validate that the compiler is functional."""
        if not Path(self.config.command).exists():
            raise CompilerNotFoundError(
                f"Compiler executable not found: {self.config.command}",
                error_code="COMPILER_NOT_FOUND",
                compiler_path=self.config.command
            )
        
        if not os.access(self.config.command, os.X_OK):
            raise CompilerNotFoundError(
                f"Compiler is not executable: {self.config.command}",
                error_code="COMPILER_NOT_EXECUTABLE",
                compiler_path=self.config.command
            )
    
    async def compile_async(
        self,
        source_files: List[PathLike],
        output_file: PathLike,
        cpp_version: CppVersion,
        options: Optional[CompileOptions] = None,
        timeout: Optional[float] = None
    ) -> CompilationResult:
        """
        Compile source files asynchronously.
        
        Args:
            source_files: List of source files to compile
            output_file: Output file path
            cpp_version: C++ standard version to use
            options: Compilation options
            timeout: Compilation timeout in seconds
            
        Returns:
            CompilationResult with detailed information
        """
        start_time = time.time()
        options = options or CompileOptions()
        output_path = Path(output_file)
        
        logger.debug(
            f"Starting async compilation of {len(source_files)} files",
            extra={
                "source_files": [str(f) for f in source_files],
                "output_file": str(output_path),
                "cpp_version": cpp_version.value
            }
        )
        
        try:
            # Ensure output directory exists
            output_path.parent.mkdir(parents=True, exist_ok=True)
            
            # Build compilation command
            cmd = await self._build_compile_command(
                source_files, output_path, cpp_version, options
            )
            
            # Execute compilation
            result = await self.process_manager.run_command_async(
                cmd, timeout=timeout
            )
            
            # Process results
            compilation_result = await self._process_compilation_result(
                result, output_path, cmd, start_time
            )
            
            # Record metrics
            duration = compilation_result.duration_ms / 1000.0
            self.metrics.record_compilation(
                compilation_result.success, duration, is_link=False
            )
            
            return compilation_result
            
        except Exception as e:
            duration = (time.time() - start_time) * 1000.0
            logger.error(f"Compilation failed with exception: {e}")
            
            return CompilationResult(
                success=False,
                duration_ms=duration,
                errors=[f"Compilation exception: {e}"]
            )
    
    def compile(
        self,
        source_files: List[PathLike],
        output_file: PathLike,
        cpp_version: CppVersion,
        options: Optional[CompileOptions] = None,
        timeout: Optional[float] = None
    ) -> CompilationResult:
        """
        Compile source files synchronously.
        
        This is a convenience wrapper around compile_async for synchronous usage.
        """
        return asyncio.run(
            self.compile_async(
                source_files, output_file, cpp_version, options, timeout
            )
        )
    
    async def link_async(
        self,
        object_files: List[PathLike],
        output_file: PathLike,
        options: Optional[LinkOptions] = None,
        timeout: Optional[float] = None
    ) -> CompilationResult:
        """
        Link object files asynchronously.
        
        Args:
            object_files: List of object files to link
            output_file: Output executable/library path
            options: Linking options
            timeout: Linking timeout in seconds
            
        Returns:
            CompilationResult with detailed information
        """
        start_time = time.time()
        options = options or LinkOptions()
        output_path = Path(output_file)
        
        logger.debug(
            f"Starting async linking of {len(object_files)} object files",
            extra={
                "object_files": [str(f) for f in object_files],
                "output_file": str(output_path)
            }
        )
        
        try:
            # Ensure output directory exists
            output_path.parent.mkdir(parents=True, exist_ok=True)
            
            # Build linking command
            cmd = await self._build_link_command(
                object_files, output_path, options
            )
            
            # Execute linking
            result = await self.process_manager.run_command_async(
                cmd, timeout=timeout
            )
            
            # Process results
            link_result = await self._process_compilation_result(
                result, output_path, cmd, start_time
            )
            
            # Record metrics
            duration = link_result.duration_ms / 1000.0
            self.metrics.record_compilation(
                link_result.success, duration, is_link=True
            )
            
            return link_result
            
        except Exception as e:
            duration = (time.time() - start_time) * 1000.0
            logger.error(f"Linking failed with exception: {e}")
            
            return CompilationResult(
                success=False,
                duration_ms=duration,
                errors=[f"Linking exception: {e}"]
            )
    
    def link(
        self,
        object_files: List[PathLike],
        output_file: PathLike,
        options: Optional[LinkOptions] = None,
        timeout: Optional[float] = None
    ) -> CompilationResult:
        """
        Link object files synchronously.
        
        This is a convenience wrapper around link_async for synchronous usage.
        """
        return asyncio.run(
            self.link_async(object_files, output_file, options, timeout)
        )
    
    async def _build_compile_command(
        self,
        source_files: List[PathLike],
        output_file: Path,
        cpp_version: CppVersion,
        options: CompileOptions
    ) -> List[str]:
        """Build compilation command with all options."""
        cmd = [self.config.command]
        
        # Add C++ standard flag
        if cpp_version not in self.config.cpp_flags:
            supported = ", ".join(v.value for v in self.config.cpp_flags.keys())
            raise CompilationError(
                f"Unsupported C++ version: {cpp_version.value}. "
                f"Supported versions: {supported}",
                error_code="UNSUPPORTED_CPP_VERSION",
                cpp_version=cpp_version.value,
                supported_versions=list(self.config.cpp_flags.keys())
            )
        
        cmd.append(self.config.cpp_flags[cpp_version])
        
        # Add include paths
        for path in options.include_paths:
            if self.config.compiler_type == CompilerType.MSVC:
                cmd.append(f"/I{path}")
            else:
                cmd.extend(["-I", str(path)])
        
        # Add preprocessor definitions
        for name, value in options.defines.items():
            if self.config.compiler_type == CompilerType.MSVC:
                define_flag = f"/D{name}" if value is None else f"/D{name}={value}"
            else:
                define_flag = f"-D{name}" if value is None else f"-D{name}={value}"
            cmd.append(define_flag)
        
        # Add warning flags
        cmd.extend(options.warnings)
        
        # Add optimization level
        if self.config.compiler_type == CompilerType.MSVC:
            opt_map = {
                OptimizationLevel.NONE: "/Od",
                OptimizationLevel.BASIC: "/O1",
                OptimizationLevel.STANDARD: "/O2",
                OptimizationLevel.AGGRESSIVE: "/Ox",
                OptimizationLevel.SIZE: "/Os",
                OptimizationLevel.FAST: "/O2",  # MSVC doesn't have exact Ofast equivalent
                OptimizationLevel.DEBUG: "/Od"
            }
        else:
            opt_map = {
                OptimizationLevel.NONE: "-O0",
                OptimizationLevel.BASIC: "-O1",
                OptimizationLevel.STANDARD: "-O2",
                OptimizationLevel.AGGRESSIVE: "-O3",
                OptimizationLevel.SIZE: "-Os",
                OptimizationLevel.FAST: "-Ofast",
                OptimizationLevel.DEBUG: "-Og"
            }
        
        if options.optimization in opt_map:
            cmd.append(opt_map[options.optimization])
        
        # Add debug flag
        if options.debug:
            if self.config.compiler_type == CompilerType.MSVC:
                cmd.append("/Zi")
            else:
                cmd.append("-g")
        
        # Position independent code
        if (options.position_independent and 
            self.config.compiler_type != CompilerType.MSVC):
            cmd.append("-fPIC")
        
        # Add sanitizers
        for sanitizer in options.sanitizers:
            if sanitizer in self.config.features.supported_sanitizers:
                if self.config.compiler_type == CompilerType.MSVC:
                    if sanitizer == "address":
                        cmd.append("/fsanitize=address")
                else:
                    cmd.append(f"-fsanitize={sanitizer}")
        
        # Add standard library specification
        if (options.standard_library and 
            self.config.compiler_type != CompilerType.MSVC):
            cmd.append(f"-stdlib={options.standard_library}")
        
        # Add default compile flags
        cmd.extend(self.config.additional_compile_flags)
        
        # Add extra flags
        cmd.extend(options.extra_flags)
        
        # Add compile-only flag
        if self.config.compiler_type == CompilerType.MSVC:
            cmd.append("/c")
        else:
            cmd.append("-c")
        
        # Add source files
        cmd.extend([str(f) for f in source_files])
        
        # Add output file
        if self.config.compiler_type == CompilerType.MSVC:
            cmd.extend(["/Fo:", str(output_file)])
        else:
            cmd.extend(["-o", str(output_file)])
        
        return cmd
    
    async def _build_link_command(
        self,
        object_files: List[PathLike],
        output_file: Path,
        options: LinkOptions
    ) -> List[str]:
        """Build linking command with all options."""
        cmd = [self.config.command]
        
        # Handle shared library creation
        if options.shared:
            if self.config.compiler_type == CompilerType.MSVC:
                cmd.append("/DLL")
            else:
                cmd.append("-shared")
        
        # Handle static linking preference
        if options.static and self.config.compiler_type != CompilerType.MSVC:
            cmd.append("-static")
        
        # Add library paths
        for path in options.library_paths:
            if self.config.compiler_type == CompilerType.MSVC:
                cmd.append(f"/LIBPATH:{path}")
            else:
                cmd.append(f"-L{path}")
        
        # Add runtime library paths
        if self.config.compiler_type != CompilerType.MSVC:
            for path in options.runtime_library_paths:
                if platform.system() == "Darwin":
                    cmd.append(f"-Wl,-rpath,{path}")
                else:
                    cmd.append(f"-Wl,-rpath={path}")
        
        # Add libraries
        for lib in options.libraries:
            if self.config.compiler_type == CompilerType.MSVC:
                cmd.append(f"{lib}.lib")
            else:
                cmd.append(f"-l{lib}")
        
        # Strip debug symbols
        if options.strip_symbols:
            if self.config.compiler_type != CompilerType.MSVC:
                cmd.append("-s")
        
        # Add map file
        if options.generate_map and options.map_file:
            map_path = Path(options.map_file)
            if self.config.compiler_type == CompilerType.MSVC:
                cmd.append(f"/MAP:{map_path}")
            else:
                cmd.append(f"-Wl,-Map={map_path}")
        
        # Add default link flags
        cmd.extend(self.config.additional_link_flags)
        
        # Add extra flags
        cmd.extend(options.extra_flags)
        
        # Add object files
        cmd.extend([str(f) for f in object_files])
        
        # Add output file
        if self.config.compiler_type == CompilerType.MSVC:
            cmd.append(f"/OUT:{output_file}")
        else:
            cmd.extend(["-o", str(output_file)])
        
        return cmd
    
    async def _process_compilation_result(
        self,
        cmd_result: CommandResult,
        output_file: Path,
        command: List[str],
        start_time: float
    ) -> CompilationResult:
        """Process command result into CompilationResult."""
        duration_ms = (time.time() - start_time) * 1000.0
        
        # Parse diagnostics from stderr
        errors, warnings, notes = self.diagnostic_parser.parse_diagnostics(
            cmd_result.stderr
        )
        
        # Check if compilation was successful
        success = cmd_result.success and output_file.exists()
        
        # Create compilation result
        result = CompilationResult(
            success=success,
            output_file=output_file if success else None,
            duration_ms=duration_ms,
            command_line=command,
            errors=errors,
            warnings=warnings,
            notes=notes
        )
        
        # Add additional diagnostics if compilation failed but no errors were parsed
        if not success and not errors and cmd_result.stderr:
            result.add_error(f"Compilation failed: {cmd_result.stderr}")
        
        # Log result
        if success:
            logger.info(
                f"Compilation successful in {duration_ms:.1f}ms",
                extra={
                    "output_file": str(output_file),
                    "duration_ms": duration_ms,
                    "warnings_count": len(warnings)
                }
            )
        else:
            logger.error(
                f"Compilation failed in {duration_ms:.1f}ms",
                extra={
                    "duration_ms": duration_ms,
                    "errors_count": len(errors),
                    "warnings_count": len(warnings)
                }
            )
        
        return result
    
    async def get_version_info_async(self) -> Dict[str, str]:
        """Get detailed version information about the compiler asynchronously."""
        if self.config.compiler_type == CompilerType.GCC:
            result = await self.process_manager.run_command_async(
                [self.config.command, "--version"]
            )
        elif self.config.compiler_type == CompilerType.CLANG:
            result = await self.process_manager.run_command_async(
                [self.config.command, "--version"]
            )
        elif self.config.compiler_type == CompilerType.MSVC:
            result = await self.process_manager.run_command_async(
                [self.config.command, "/Bv"]
            )
        else:
            result = await self.process_manager.run_command_async(
                [self.config.command, "--version"]
            )
        
        if result.success:
            return {
                "version": result.stdout.splitlines()[0] if result.stdout else "unknown",
                "full_output": result.stdout
            }
        else:
            return {
                "version": "unknown",
                "error": result.stderr
            }
    
    def get_version_info(self) -> Dict[str, str]:
        """Get version information synchronously."""
        return asyncio.run(self.get_version_info_async())
    
    def get_metrics(self) -> Dict[str, Any]:
        """Get compiler performance metrics."""
        return self.metrics.to_dict()
    
    def reset_metrics(self) -> None:
        """Reset performance metrics."""
        self.metrics = CompilerMetrics()
        logger.debug("Compiler metrics reset")


# Backward compatibility alias
Compiler = EnhancedCompiler
