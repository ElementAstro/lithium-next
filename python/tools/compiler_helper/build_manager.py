#!/usr/bin/env python3
"""
Build Manager with async support and intelligent caching.
"""

from __future__ import annotations

import asyncio
import hashlib
import json
import os
import re
import time
from dataclasses import dataclass, field
from functools import lru_cache
from pathlib import Path
from typing import Any, Dict, List, Optional, Set, Union

import aiofiles
from loguru import logger

from .core_types import (
    CompilationResult, CompileOptions, LinkOptions,
    CppVersion, PathLike
)
from .compiler_manager import CompilerManager
from .compiler import EnhancedCompiler as Compiler
from .utils import FileManager, ProcessManager


@dataclass
class BuildCacheEntry:
    """Represents a cached build entry."""
    file_hash: str
    dependencies: Set[str] = field(default_factory=set)
    object_file: Optional[str] = None
    timestamp: float = field(default_factory=time.time)

    def to_dict(self) -> Dict[str, Any]:
        return {
            "file_hash": self.file_hash,
            "dependencies": list(self.dependencies),
            "object_file": self.object_file,
            "timestamp": self.timestamp
        }

    @classmethod
    def from_dict(cls, data: Dict[str, Any]) -> BuildCacheEntry:
        return cls(
            file_hash=data["file_hash"],
            dependencies=set(data.get("dependencies", [])),
            object_file=data.get("object_file"),
            timestamp=data.get("timestamp", time.time())
        )


@dataclass
class BuildMetrics:
    """Build performance metrics."""
    total_files: int = 0
    compiled_files: int = 0
    cached_files: int = 0
    total_time: float = 0.0
    compile_time: float = 0.0
    link_time: float = 0.0
    cache_hit_rate: float = 0.0

    def to_dict(self) -> Dict[str, Any]:
        return {
            "total_files": self.total_files,
            "compiled_files": self.compiled_files,
            "cached_files": self.cached_files,
            "total_time": self.total_time,
            "compile_time": self.compile_time,
            "link_time": self.link_time,
            "cache_hit_rate": self.cache_hit_rate
        }


class BuildManager:
    """
    Build manager with async support, intelligent caching, and parallel builds.

    Features:
    - Async-first design for non-blocking operations
    - Smart dependency tracking with header scanning
    - Incremental builds with hash-based change detection
    - Parallel compilation with configurable worker pools
    - Build artifact caching and reuse
    - Detailed build metrics and reporting
    """

    def __init__(
        self,
        compiler_manager: Optional[CompilerManager] = None,
        build_dir: Optional[PathLike] = None,
        parallel: bool = True,
        max_workers: Optional[int] = None,
        cache_enabled: bool = True
    ) -> None:
        """Initialize the build manager."""
        self.compiler_manager = compiler_manager or CompilerManager()
        self.build_dir = Path(build_dir) if build_dir else Path("build")
        self.parallel = parallel
        self.max_workers = max_workers or min(32, os.cpu_count() or 4)
        self.cache_enabled = cache_enabled

        # Cache and dependency tracking
        self.cache_file = self.build_dir / "build_cache.json"
        self.dependency_cache: Dict[str, BuildCacheEntry] = {}
        self.file_manager = FileManager()
        self.process_manager = ProcessManager()

        # Ensure build directory exists
        self.build_dir.mkdir(parents=True, exist_ok=True)

        # Load cache if enabled
        if self.cache_enabled:
            self._load_cache()

        logger.debug(
            f"Initialized BuildManager: dir={self.build_dir}, "
            f"parallel={self.parallel}, workers={self.max_workers}, "
            f"cache={self.cache_enabled}"
        )

    async def build_async(
        self,
        source_files: List[PathLike],
        output_file: PathLike,
        compiler_name: Optional[str] = None,
        cpp_version: CppVersion = CppVersion.CPP17,
        compile_options: Optional[CompileOptions] = None,
        link_options: Optional[LinkOptions] = None,
        incremental: bool = True,
        force_rebuild: bool = False
    ) -> CompilationResult:
        """
        Build source files asynchronously.

        Args:
            source_files: List of source files to compile
            output_file: Output executable/library path
            compiler_name: Name of compiler to use
            cpp_version: C++ standard version
            compile_options: Compilation options
            link_options: Linking options
            incremental: Enable incremental builds
            force_rebuild: Force rebuilding all files

        Returns:
            CompilationResult with detailed information
        """
        start_time = time.time()
        metrics = BuildMetrics()

        # Ensure cpp_version is a CppVersion enum
        if isinstance(cpp_version, str):
            cpp_version = CppVersion.resolve_version(cpp_version)

        source_paths = [Path(f) for f in source_files]
        output_path = Path(output_file)

        logger.info(
            f"Starting async build: {len(source_paths)} files -> {output_path}",
            extra={
                "source_count": len(source_paths),
                "output_file": str(output_path),
                "cpp_version": cpp_version.value,
                "incremental": incremental
            }
        )

        try:
            # Get compiler
            compiler = await self.compiler_manager.get_compiler_async(compiler_name)

            # Prepare options
            compile_options = compile_options or CompileOptions()
            link_options = link_options or LinkOptions()

            # Create object directory
            obj_dir = self.build_dir / f"{compiler.config.name}_{cpp_version.value}"
            obj_dir.mkdir(parents=True, exist_ok=True)

            # Determine what needs to be compiled
            compilation_plan = await self._create_compilation_plan(
                source_paths, compiler, cpp_version, obj_dir,
                incremental and not force_rebuild
            )

            metrics.total_files = len(source_paths)
            metrics.compiled_files = len(compilation_plan.to_compile)
            metrics.cached_files = len(source_paths) - len(compilation_plan.to_compile)

            # Compile files that need rebuilding
            compile_start = time.time()
            compile_results = []

            if compilation_plan.to_compile:
                if self.parallel and len(compilation_plan.to_compile) > 1:
                    compile_results = await self._compile_parallel_async(
                        compilation_plan.to_compile,
                        compilation_plan.object_files,
                        compiler, cpp_version, compile_options
                    )
                else:
                    compile_results = await self._compile_sequential_async(
                        compilation_plan.to_compile,
                        compilation_plan.object_files,
                        compiler, cpp_version, compile_options
                    )

                # Check for compilation errors
                for result in compile_results:
                    if not result.success:
                        return CompilationResult(
                            success=False,
                            errors=result.errors,
                            warnings=result.warnings,
                            duration_ms=(time.time() - start_time) * 1000
                        )

            metrics.compile_time = time.time() - compile_start

            # Link all object files
            link_start = time.time()
            # Convert list of Path to list of str for link_async type hint compatibility

            link_result = await compiler.link_async(
                list(compilation_plan.all_objects.values()), output_path, link_options
            )

            metrics.link_time = time.time() - link_start

            if not link_result.success:
                return CompilationResult(
                    success=False,
                    errors=link_result.errors,
                    warnings=link_result.warnings,
                    duration_ms=(time.time() - start_time) * 1000
                )

            # Update cache
            if self.cache_enabled:
                await self._update_cache_async(compilation_plan.to_compile)
                await self._save_cache_async()

            # Calculate metrics
            metrics.total_time = time.time() - start_time
            metrics.cache_hit_rate = metrics.cached_files / metrics.total_files if metrics.total_files > 0 else 0.0

            # Aggregate warnings
            all_warnings = []
            for result in compile_results:
                all_warnings.extend(result.warnings)
            all_warnings.extend(link_result.warnings)

            logger.info(
                f"Build completed successfully in {metrics.total_time:.2f}s",
                extra={
                    "compiled": metrics.compiled_files,
                    "cached": metrics.cached_files,
                    "cache_hit_rate": f"{metrics.cache_hit_rate:.1%}",
                    "metrics": metrics.to_dict()
                }
            )

            return CompilationResult(
                success=True,
                output_file=output_path,
                duration_ms=metrics.total_time * 1000,
                warnings=all_warnings,
                artifacts=[output_path] + list(compilation_plan.all_objects.values()) # Return Path objects
            )

        except Exception as e:
            duration = (time.time() - start_time) * 1000.0
            logger.error(f"Build failed with exception: {e}")
            return CompilationResult(
                success=False,
                duration_ms=duration,
                errors=[f"Build exception: {e}"]
            )

    def build(
        self,
        source_files: List[PathLike],
        output_file: PathLike,
        compiler_name: Optional[str] = None,
        cpp_version: CppVersion = CppVersion.CPP17,
        compile_options: Optional[CompileOptions] = None,
        link_options: Optional[LinkOptions] = None,
        incremental: bool = True,
        force_rebuild: bool = False
    ) -> CompilationResult:
        """Build source files synchronously."""
        return asyncio.run(
            self.build_async(
                source_files, output_file, compiler_name, cpp_version,
                compile_options, link_options, incremental, force_rebuild
            )
        )

    @dataclass
    class CompilationPlan:
        """Plan for what needs to be compiled."""
        to_compile: List[Path]
        object_files: Dict[Path, Path]
        all_objects: Dict[Path, Path]

    async def _create_compilation_plan(
        self,
        source_files: List[Path],
        compiler: Compiler,
        cpp_version: CppVersion,
        obj_dir: Path,
        incremental: bool
    ) -> CompilationPlan:
        """Create a plan for what needs to be compiled."""
        to_compile = []
        object_files = {}
        all_objects = {}

        for source_file in source_files:
            if not source_file.exists():
                raise FileNotFoundError(f"Source file not found: {source_file}")

            obj_file = obj_dir / f"{source_file.stem}.o"
            all_objects[source_file] = obj_file

            if incremental:
                # Check if file needs rebuilding
                needs_rebuild = await self._needs_rebuild_async(source_file, obj_file)
                if needs_rebuild:
                    to_compile.append(source_file)
                    object_files[source_file] = obj_file
            else:
                to_compile.append(source_file)
                object_files[source_file] = obj_file

        return self.CompilationPlan(
            to_compile=to_compile,
            object_files=object_files,
            all_objects=all_objects
        )

    async def _needs_rebuild_async(self, source_file: Path, obj_file: Path) -> bool:
        """Check if a source file needs to be rebuilt."""
        if not obj_file.exists():
            return True

        # Calculate current file hash
        current_hash = await self._calculate_file_hash_async(source_file)

        # Check cache
        source_str = str(source_file.resolve())
        if source_str in self.dependency_cache:
            cache_entry = self.dependency_cache[source_str]
            if cache_entry.file_hash == current_hash:
                # Check if dependencies changed
                for dep_path in cache_entry.dependencies:
                    dep_file = Path(dep_path)
                    if dep_file.exists():
                        dep_hash = await self._calculate_file_hash_async(dep_file)
                        if (dep_path in self.dependency_cache and
                            self.dependency_cache[dep_path].file_hash != dep_hash):
                            return True
                return False

        return True

    async def _compile_parallel_async(
        self,
        source_files: List[Path],
        object_files: Dict[Path, Path],
        compiler: Compiler,
        cpp_version: CppVersion,
        options: CompileOptions
    ) -> List[CompilationResult]:
        """Compile files in parallel asynchronously."""
        logger.debug(f"Starting parallel compilation of {len(source_files)} files")

        async def compile_single(source_file: Path) -> CompilationResult:
            obj_file = object_files[source_file]
            # Pass source_file as a list of PathLike (which Path is)
            return await compiler.compile_async(
                [source_file], obj_file, cpp_version, options
            )

        # Create compilation tasks
        tasks = [
            asyncio.create_task(compile_single(source_file), name=f"compile_{source_file.name}")
            for source_file in source_files
        ]

        # Wait for all compilations to complete
        results = await asyncio.gather(*tasks, return_exceptions=True)

        # Process results
        compile_results = []
        for source_file, result in zip(source_files, results):
            if isinstance(result, Exception):
                logger.error(f"Compilation task failed for {source_file}: {result}")
                compile_results.append(CompilationResult(
                    success=False,
                    errors=[f"Compilation failed: {result}"]
                ))
            else:
                compile_results.append(result)

        return compile_results

    async def _compile_sequential_async(
        self,
        source_files: List[Path],
        object_files: Dict[Path, Path],
        compiler: Compiler,
        cpp_version: CppVersion,
        options: CompileOptions
    ) -> List[CompilationResult]:
        """Compile files sequentially asynchronously."""
        logger.debug(f"Starting sequential compilation of {len(source_files)} files")

        results = []
        for source_file in source_files:
            obj_file = object_files[source_file]
            # Pass source_file as a list of PathLike (which Path is)
            result = await compiler.compile_async(
                [source_file], obj_file, cpp_version, options
            )
            results.append(result)

            if not result.success:
                break  # Stop on first error

        return results

    @lru_cache(maxsize=1024)
    async def _calculate_file_hash_async(self, file_path: Path) -> str:
        """Calculate file hash asynchronously with caching."""
        hash_md5 = hashlib.md5()

        async with aiofiles.open(file_path, "rb") as f:
            async for chunk in f:
                hash_md5.update(chunk)

        return hash_md5.hexdigest()

    async def _scan_dependencies_async(self, file_path: Path) -> Set[str]:
        """Scan file dependencies asynchronously."""
        dependencies = set()

        try:
            async with aiofiles.open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                async for line in f:
                    line = line.strip()
                    if line.startswith('#include'):
                        match = re.search(r'#include\s+["<](.*?)[">]', line)
                        if match:
                            include_file = match.group(1)
                            dependencies.add(include_file)
        except Exception as e:
            logger.warning(f"Failed to scan dependencies for {file_path}: {e}")

        return dependencies

    async def _update_cache_async(self, compiled_files: List[Path]) -> None:
        """Update build cache asynchronously."""
        for source_file in compiled_files:
            try:
                file_hash = await self._calculate_file_hash_async(source_file)
                dependencies = await self._scan_dependencies_async(source_file)

                cache_entry = BuildCacheEntry(
                    file_hash=file_hash,
                    dependencies=dependencies,
                    timestamp=time.time()
                )

                self.dependency_cache[str(source_file.resolve())] = cache_entry

            except Exception as e:
                logger.warning(f"Failed to update cache for {source_file}: {e}")

    async def _save_cache_async(self) -> None:
        """Save build cache asynchronously."""
        if not self.cache_enabled:
            return

        try:
            cache_data = {
                path: entry.to_dict()
                for path, entry in self.dependency_cache.items()
            }

            async with aiofiles.open(self.cache_file, 'w') as f:
                await f.write(json.dumps(cache_data, indent=2))

            logger.debug(f"Saved build cache with {len(cache_data)} entries")

        except Exception as e:
            logger.warning(f"Failed to save build cache: {e}")

    def _load_cache(self) -> None:
        """Load build cache synchronously."""
        if not self.cache_enabled or not self.cache_file.exists():
            return

        try:
            with open(self.cache_file, 'r') as f:
                cache_data = json.load(f)

            self.dependency_cache = {
                path: BuildCacheEntry.from_dict(data)
                for path, data in cache_data.items()
            }

            logger.debug(f"Loaded build cache with {len(self.dependency_cache)} entries")

        except Exception as e:
            logger.warning(f"Failed to load build cache: {e}")
            self.dependency_cache = {}

    def clean(self, aggressive: bool = False) -> None:
        """Clean build artifacts."""
        try:
            if aggressive and self.build_dir.exists():
                import shutil
                shutil.rmtree(self.build_dir)
                self.build_dir.mkdir(parents=True, exist_ok=True)
                logger.info(f"Aggressively cleaned build directory: {self.build_dir}")
            else:
                # Clean only object files
                for obj_file in self.build_dir.rglob("*.o"):
                    obj_file.unlink()
                logger.info("Cleaned object files")

            if self.cache_enabled:
                self.dependency_cache.clear()
                if self.cache_file.exists():
                    self.cache_file.unlink()
                logger.info("Cleared build cache")

        except Exception as e:
            logger.error(f"Failed to clean build artifacts: {e}")

    def get_metrics(self) -> Dict[str, Any]:
        """Get build manager metrics."""
        return {
            "cache_entries": len(self.dependency_cache),
            "build_dir": str(self.build_dir),
            "cache_enabled": self.cache_enabled,
            "parallel": self.parallel,
            "max_workers": self.max_workers
        }