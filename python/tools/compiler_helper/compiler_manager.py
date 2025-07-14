#!/usr/bin/env python3
"""
Compiler Manager with modern Python features and async support.
"""

from __future__ import annotations

import asyncio
import os
import platform
import re
import shutil
import subprocess
from pathlib import Path
from typing import Dict, Optional, List, Any, Tuple, Set
from dataclasses import dataclass, field

from loguru import logger
from pydantic import ValidationError

from .core_types import (
    CompilerNotFoundError,
    CppVersion,
    CompilerType,
    CompilerException,
    CompilerFeatures,
    OptimizationLevel,
)
from .compiler import EnhancedCompiler as Compiler, CompilerConfig
from .utils import SystemInfo


@dataclass
class CompilerSpec:
    """Specification for a compiler to detect."""

    name: str
    command_names: List[str]
    compiler_type: CompilerType
    cpp_flags: Dict[CppVersion, str]
    additional_compile_flags: List[str] = field(default_factory=list)
    additional_link_flags: List[str] = field(default_factory=list)
    find_method: Optional[str] = None


class CompilerManager:
    """
    Enhanced compiler manager with async support and better detection.

    Features:
    - Async compiler detection
    - Cached compiler discovery
    - Enhanced error handling
    - Platform-specific optimizations
    """

    def __init__(self, cache_dir: Optional[Path] = None) -> None:
        """Initialize the compiler manager."""
        self.compilers: Dict[str, Compiler] = {}
        self.default_compiler: Optional[str] = None
        self.cache_dir = cache_dir or Path.home() / ".compiler_helper" / "cache"
        self.cache_dir.mkdir(parents=True, exist_ok=True)

        self._compiler_specs = self._get_compiler_specs()

        logger.debug(f"Initialized CompilerManager with cache dir: {self.cache_dir}")

    def _get_compiler_specs(self) -> List[CompilerSpec]:
        """Get compiler specifications for detection."""
        return [
            CompilerSpec(
                name="GCC",
                command_names=["g++", "gcc"],
                compiler_type=CompilerType.GCC,
                cpp_flags={
                    CppVersion.CPP98: "-std=c++98",
                    CppVersion.CPP03: "-std=c++03",
                    CppVersion.CPP11: "-std=c++11",
                    CppVersion.CPP14: "-std=c++14",
                    CppVersion.CPP17: "-std=c++17",
                    CppVersion.CPP20: "-std=c++20",
                    CppVersion.CPP23: "-std=c++23",
                    CppVersion.CPP26: "-std=c++26",
                },
                additional_compile_flags=["-Wall", "-Wextra", "-Wpedantic"],
                additional_link_flags=[],
            ),
            CompilerSpec(
                name="Clang",
                command_names=["clang++", "clang"],
                compiler_type=CompilerType.CLANG,
                cpp_flags={
                    CppVersion.CPP98: "-std=c++98",
                    CppVersion.CPP03: "-std=c++03",
                    CppVersion.CPP11: "-std=c++11",
                    CppVersion.CPP14: "-std=c++14",
                    CppVersion.CPP17: "-std=c++17",
                    CppVersion.CPP20: "-std=c++20",
                    CppVersion.CPP23: "-std=c++23",
                    CppVersion.CPP26: "-std=c++26",
                },
                additional_compile_flags=["-Wall", "-Wextra", "-Wpedantic"],
                additional_link_flags=[],
            ),
            CompilerSpec(
                name="MSVC",
                command_names=["cl", "cl.exe"],
                compiler_type=CompilerType.MSVC,
                cpp_flags={
                    CppVersion.CPP11: "/std:c++11",
                    CppVersion.CPP14: "/std:c++14",
                    CppVersion.CPP17: "/std:c++17",
                    CppVersion.CPP20: "/std:c++20",
                    CppVersion.CPP23: "/std:c++latest",
                },
                additional_compile_flags=["/W4", "/EHsc"],
                additional_link_flags=[],
                find_method="_find_msvc",
            ),
        ]

    async def detect_compilers_async(self) -> Dict[str, Compiler]:
        """Asynchronously detect available compilers."""
        self.compilers.clear()

        logger.info("Starting compiler detection...")

        detection_tasks = []
        for spec in self._compiler_specs:
            task = asyncio.create_task(
                self._detect_compiler_async(spec), name=f"detect_{spec.name}"
            )
            detection_tasks.append(task)

        # Wait for all detection tasks to complete
        results = await asyncio.gather(*detection_tasks, return_exceptions=True)

        for spec, result in zip(self._compiler_specs, results):
            if isinstance(result, Exception):
                logger.warning(f"Failed to detect {spec.name}: {result}")
            elif result is not None and isinstance(result, Compiler):
                self.compilers[spec.name] = result
                if not self.default_compiler:
                    self.default_compiler = spec.name
                logger.info(f"Detected {spec.name}: {result.config.command}")

        logger.info(f"Detection complete. Found {len(self.compilers)} compilers.")
        return self.compilers

    def detect_compilers(self) -> Dict[str, Compiler]:
        """Synchronously detect available compilers."""
        return asyncio.run(self.detect_compilers_async())

    async def _detect_compiler_async(self, spec: CompilerSpec) -> Optional[Compiler]:
        """Detect a specific compiler asynchronously."""
        try:
            # Find compiler executable
            compiler_path = None

            if spec.find_method:
                # Use custom find method
                find_func = getattr(self, spec.find_method, None)
                if find_func:
                    compiler_path = await asyncio.get_event_loop().run_in_executor(
                        None, find_func
                    )
            else:
                # Standard PATH search
                for cmd_name in spec.command_names:
                    path = await asyncio.get_event_loop().run_in_executor(
                        None, shutil.which, cmd_name
                    )
                    if path:
                        compiler_path = path
                        break

            if not compiler_path:
                return None

            # Get version information
            version = await self._get_compiler_version_async(
                compiler_path, spec.compiler_type
            )

            # Create compiler features based on type and version
            features = self._create_compiler_features(spec.compiler_type, version)

            # Create compiler configuration
            config = CompilerConfig(
                name=spec.name,
                command=compiler_path,
                compiler_type=spec.compiler_type,
                version=version,
                cpp_flags=spec.cpp_flags,
                additional_compile_flags=spec.additional_compile_flags,
                additional_link_flags=spec.additional_link_flags,
                features=features,
            )

            return Compiler(config)

        except (ValidationError, CompilerException) as e:
            logger.warning(f"Failed to create {spec.name} compiler: {e}")
            return None
        except Exception as e:
            logger.error(f"Unexpected error detecting {spec.name}: {e}")
            return None

    def _create_compiler_features(
        self, compiler_type: CompilerType, version: str
    ) -> CompilerFeatures:
        """Create compiler features based on type and version."""

        # Parse version to compare - handle unknown versions gracefully
        version_parts = []
        for part in version.split("."):
            if part.isdigit():
                version_parts.append(int(part))

        # Ensure at least 3 version components
        while len(version_parts) < 3:
            version_parts.append(0)
        version_tuple = tuple(version_parts[:3])  # Take only first 3 components

        # Default features
        features = CompilerFeatures(
            supports_parallel=True,
            supports_pch=True,
            supports_modules=False,
            supports_coroutines=False,
            supports_concepts=False,
            supported_cpp_versions=set(),
            supported_sanitizers=set(),
            supported_optimizations=set(),
            feature_flags={},
            max_parallel_jobs=SystemInfo.get_cpu_count(),
        )

        if compiler_type in {CompilerType.GCC, CompilerType.CLANG}:
            # GNU-style compilers
            features.supported_cpp_versions = {
                CppVersion.CPP98,
                CppVersion.CPP03,
                CppVersion.CPP11,
                CppVersion.CPP14,
                CppVersion.CPP17,
                CppVersion.CPP20,
            }

            features.supported_sanitizers = {"address", "thread", "undefined", "leak"}

            features.supported_optimizations = {
                OptimizationLevel.NONE,
                OptimizationLevel.BASIC,
                OptimizationLevel.STANDARD,
                OptimizationLevel.AGGRESSIVE,
                OptimizationLevel.SIZE,
                OptimizationLevel.FAST,
                OptimizationLevel.DEBUG,
            }

            features.feature_flags = {
                "lto": "-flto",
                "coverage": "--coverage",
                "profile": "-pg",
            }

            # Version-specific features - safer comparison
            if (
                compiler_type == CompilerType.GCC
                and len(version_tuple) >= 2
                and version_tuple >= (11, 0)
            ):
                features.supports_modules = True
                features.supports_concepts = True
                features.supported_cpp_versions.add(CppVersion.CPP23)

            elif (
                compiler_type == CompilerType.CLANG
                and len(version_tuple) >= 2
                and version_tuple >= (16, 0)
            ):
                features.supports_modules = True
                features.supports_concepts = True
                features.supported_cpp_versions.add(CppVersion.CPP23)
                features.supported_sanitizers.add("memory")
                features.supported_sanitizers.add("dataflow")

        elif compiler_type == CompilerType.MSVC:
            # MSVC-specific features
            features.supported_cpp_versions = {
                CppVersion.CPP11,
                CppVersion.CPP14,
                CppVersion.CPP17,
                CppVersion.CPP20,
            }

            features.supported_sanitizers = {"address"}

            features.supported_optimizations = {
                OptimizationLevel.NONE,
                OptimizationLevel.BASIC,
                OptimizationLevel.STANDARD,
                OptimizationLevel.AGGRESSIVE,
            }

            features.feature_flags = {"lto": "/GL", "whole_program": "/GL"}

            # MSVC version parsing (format: 19.xx.xxxxx) - safer comparison
            if len(version_tuple) >= 2 and version_tuple >= (19, 29):
                features.supports_modules = True
            if len(version_tuple) >= 2 and version_tuple >= (19, 30):
                features.supports_concepts = True
                features.supported_cpp_versions.add(CppVersion.CPP23)

        return features

    async def _get_compiler_version_async(
        self, compiler_path: str, compiler_type: CompilerType
    ) -> str:
        """Get compiler version asynchronously."""
        try:
            if compiler_type == CompilerType.MSVC:
                # MSVC version detection
                process = await asyncio.create_subprocess_exec(
                    compiler_path,
                    stdout=asyncio.subprocess.PIPE,
                    stderr=asyncio.subprocess.PIPE,
                )
                _, stderr = await process.communicate()
                output = stderr.decode("utf-8", errors="ignore")

                match = re.search(r"Version\s+(\d+\.\d+\.\d+)", output)
                if match:
                    return match.group(1)
            else:
                # GCC/Clang version detection
                process = await asyncio.create_subprocess_exec(
                    compiler_path,
                    "--version",
                    stdout=asyncio.subprocess.PIPE,
                    stderr=asyncio.subprocess.PIPE,
                )
                stdout, _ = await process.communicate()
                output = stdout.decode("utf-8", errors="ignore")

                # Extract version from first line
                first_line = output.splitlines()[0] if output.splitlines() else ""
                match = re.search(r"(\d+\.\d+\.\d+)", first_line)
                if match:
                    return match.group(1)

            return "unknown"

        except Exception as e:
            logger.warning(f"Failed to get version for {compiler_path}: {e}")
            return "unknown"

    def _find_msvc(self) -> Optional[str]:
        """Find MSVC compiler on Windows."""
        # Try PATH first
        cl_path = shutil.which("cl")
        if cl_path:
            return cl_path

        if platform.system() != "Windows":
            return None

        # Use vswhere.exe to find Visual Studio installation
        vswhere_path = (
            Path(os.environ.get("ProgramFiles(x86)", ""))
            / "Microsoft Visual Studio"
            / "Installer"
            / "vswhere.exe"
        )

        if not vswhere_path.exists():
            return None

        try:
            result = subprocess.run(
                [
                    str(vswhere_path),
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
                timeout=10,
            )

            if result.returncode == 0 and result.stdout.strip():
                vs_path = Path(result.stdout.strip())
                tools_path = vs_path / "VC" / "Tools" / "MSVC"

                if tools_path.exists():
                    # Find latest MSVC version
                    versions = [d.name for d in tools_path.iterdir() if d.is_dir()]
                    if versions:
                        latest_version = sorted(versions, reverse=True)[0]

                        # Try different architectures
                        for host_arch, target_arch in [("x64", "x64"), ("x86", "x86")]:
                            cl_path = (
                                tools_path
                                / latest_version
                                / "bin"
                                / f"Host{host_arch}"
                                / target_arch
                                / "cl.exe"
                            )
                            if cl_path.exists():
                                return str(cl_path)

        except (subprocess.TimeoutExpired, subprocess.SubprocessError) as e:
            logger.warning(f"Failed to find MSVC with vswhere: {e}")

        return None

    def get_compiler(self, name: Optional[str] = None) -> Compiler:
        """Get a compiler by name or return the default."""
        if not self.compilers:
            self.detect_compilers()

        if not name:
            if self.default_compiler and self.default_compiler in self.compilers:
                return self.compilers[self.default_compiler]
            elif self.compilers:
                return next(iter(self.compilers.values()))
            else:
                raise CompilerNotFoundError(
                    "No compilers detected on the system",
                    error_code="NO_COMPILERS_FOUND",
                )

        if name in self.compilers:
            return self.compilers[name]
        else:
            available = ", ".join(self.compilers.keys())
            raise CompilerNotFoundError(
                f"Compiler '{name}' not found. Available: {available}",
                error_code="COMPILER_NOT_FOUND",
                requested_compiler=name,
                available_compilers=list(self.compilers.keys()),
            )

    async def get_compiler_async(self, name: Optional[str] = None) -> Compiler:
        """Asynchronously get a compiler by name or return the default."""
        if not self.compilers:
            await self.detect_compilers_async()

        return self.get_compiler(name)

    def list_compilers(self) -> Dict[str, Dict[str, Any]]:
        """List all detected compilers with their information."""
        if not self.compilers:
            self.detect_compilers()

        return {
            name: {
                "command": compiler.config.command,
                "type": compiler.config.compiler_type.value,
                "version": compiler.config.version,
                "cpp_versions": [
                    v.value if hasattr(v, "value") else str(v)
                    for v in compiler.config.features.supported_cpp_versions
                ],
                "features": {
                    "parallel": compiler.config.features.supports_parallel,
                    "pch": compiler.config.features.supports_pch,
                    "modules": compiler.config.features.supports_modules,
                    "concepts": compiler.config.features.supports_concepts,
                },
            }
            for name, compiler in self.compilers.items()
        }

    def get_system_info(self) -> Dict[str, Any]:
        """Get system information relevant to compilation."""
        return {
            "platform": SystemInfo.get_platform_info(),
            "cpu_count": SystemInfo.get_cpu_count(),
            "memory": SystemInfo.get_memory_info(),
            "environment": SystemInfo.get_environment_info(),
        }
