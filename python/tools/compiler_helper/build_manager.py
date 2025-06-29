#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Build Manager for handling compilation and linking of C++ projects.
"""
import os
import time
import re
import hashlib
import json
from pathlib import Path
import concurrent.futures
from collections import defaultdict
from typing import Dict, List, Optional, Set

from loguru import logger

from .core_types import (
    CompilationResult,
    CompileOptions,
    LinkOptions,
    CppVersion,
    PathLike,
)
from .compiler_manager import CompilerManager
from .compiler import Compiler


class BuildManager:
    """
    Manages the build process for a collection of source files.

    Features:
    - Dependency scanning and tracking
    - Incremental builds (only compile what changed)
    - Parallel compilation
    - Multiple compiler support
    """

    def __init__(
        self,
        compiler_manager: Optional[CompilerManager] = None,
        build_dir: Optional[PathLike] = None,
        parallel: bool = True,
        max_workers: Optional[int] = None,
    ):
        """Initialize the build manager."""
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

    def build(
        self,
        source_files: List[PathLike],
        output_file: PathLike,
        compiler_name: Optional[str] = None,
        cpp_version: CppVersion = CppVersion.CPP17,
        compile_options: Optional[CompileOptions] = None,
        link_options: Optional[LinkOptions] = None,
        incremental: bool = True,
        force_rebuild: bool = False,
    ) -> CompilationResult:
        """
        Build source files into an executable or library.
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
            if (
                self.parallel
                and compiler.features.supports_parallel
                and len(to_compile) > 1
            ):
                with concurrent.futures.ThreadPoolExecutor(
                    max_workers=self.max_workers
                ) as executor:
                    future_to_file = {}
                    for source_file in to_compile:
                        idx = source_paths.index(source_file)
                        obj_file = object_files[idx]
                        future = executor.submit(
                            compiler.compile,
                            [source_file],
                            obj_file,
                            cpp_version,
                            compile_options,
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
                                    errors=[
                                        f"Failed to compile {source_file}: {result.errors}"
                                    ],
                                    warnings=result.warnings,
                                    duration_ms=(time.time() - start_time) * 1000,
                                )
                        except Exception as e:
                            return CompilationResult(
                                success=False,
                                errors=[
                                    f"Exception while compiling {source_file}: {str(e)}"
                                ],
                                duration_ms=(time.time() - start_time) * 1000,
                            )
            else:
                # Sequential compilation
                for source_file in to_compile:
                    idx = source_paths.index(source_file)
                    obj_file = object_files[idx]
                    result = compiler.compile(
                        [source_file], obj_file, cpp_version, compile_options
                    )
                    compile_results.append(result)
                    if not result.success:
                        return CompilationResult(
                            success=False,
                            errors=[
                                f"Failed to compile {source_file}: {result.errors}"
                            ],
                            warnings=result.warnings,
                            duration_ms=(time.time() - start_time) * 1000,
                        )

            # Update cache with new file hashes
            self._update_file_hashes(to_compile)

        # Link object files
        link_result = compiler.link(
            [str(obj) for obj in object_files], output_file, link_options
        )
        if not link_result.success:
            return CompilationResult(
                success=False,
                errors=[f"Failed to link: {link_result.errors}"],
                warnings=link_result.warnings,
                duration_ms=(time.time() - start_time) * 1000,
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
            warnings=all_warnings,
        )

    def _get_files_to_rebuild(
        self, source_files: List[Path], compiler: Compiler, cpp_version: CppVersion
    ) -> List[Path]:
        """Determine which files need to be rebuilt based on changes."""
        to_rebuild = []

        # Update dependency graph
        for file in source_files:
            if not file.exists():
                raise FileNotFoundError(f"Source file not found: {file}")

            # Get dependencies for this file
            self._scan_dependencies(file)

            # Check if this file or any of its dependencies changed
            if self._has_file_changed(file) or any(
                self._has_file_changed(dep) for dep in self.dependency_graph[file]
            ):
                to_rebuild.append(file)

        return to_rebuild

    def _scan_dependencies(self, file_path: Path):
        """Scan a file for its dependencies (include files)."""
        # Reset dependencies for this file
        self.dependency_graph[file_path].clear()

        try:
            with open(file_path, "r", encoding="utf-8", errors="ignore") as f:
                for line in f:
                    # Look for #include statements
                    if line.strip().startswith("#include"):
                        include_match = re.search(r'#include\s+["<](.*?)[">]', line)
                        if include_match:
                            include_file = include_match.group(1)
                            # For now, we only track that there is a dependency
                            self.dependency_graph[file_path].add(Path(include_file))
        except Exception as e:
            logger.warning(f"Failed to scan dependencies for {file_path}: {e}")

    def _has_file_changed(self, file_path: Path) -> bool:
        """Check if a file has changed since the last build."""
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
        """Calculate MD5 hash of a file's contents."""
        hash_md5 = hashlib.md5()
        with open(file_path, "rb") as f:
            for chunk in iter(lambda: f.read(4096), b""):
                hash_md5.update(chunk)
        return hash_md5.hexdigest()

    def _update_file_hashes(self, files: List[Path]):
        """Update stored hashes for files."""
        for file_path in files:
            if file_path.exists():
                str_path = str(file_path.resolve())
                self.file_hashes[str_path] = self._calculate_file_hash(file_path)

    def _load_cache(self):
        """Load build cache from disk."""
        if self.cache_file.exists():
            try:
                with open(self.cache_file, "r") as f:
                    data = json.load(f)
                    self.file_hashes = data.get("file_hashes", {})
            except Exception as e:
                logger.warning(f"Failed to load build cache: {e}")

    def _save_cache(self):
        """Save build cache to disk."""
        try:
            cache_data = {"file_hashes": self.file_hashes}
            with open(self.cache_file, "w") as f:
                json.dump(cache_data, f, indent=2)
        except Exception as e:
            logger.warning(f"Failed to save build cache: {e}")
