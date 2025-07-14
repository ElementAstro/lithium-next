import asyncio
import hashlib
import json
import os
import shutil
import time
from pathlib import Path
from unittest.mock import AsyncMock, MagicMock, patch
import pytest
from .build_manager import BuildManager, BuildCacheEntry
from .compiler import EnhancedCompiler as Compiler, CompilerConfig, CompilerType
from .compiler_manager import CompilerManager
from .utils import FileManager, ProcessManager

# filepath: /home/max/lithium-next/python/tools/compiler_helper/test_build_manager.py


# Use relative imports as the directory is a package
from .core_types import (
    CompilationResult,
    CompileOptions,
    LinkOptions,
    CppVersion,
    PathLike,
)


# Mock CompilerConfig
@pytest.fixture
def mock_compiler_config():
    config = MagicMock(spec=CompilerConfig)
    config.name = "mock_compiler"
    config.command = "/usr/bin/mock_compiler"
    config.compiler_type = CompilerType.GCC
    config.version = "10.2.0"
    config.cpp_flags = {CppVersion.CPP17: "-std=c++17", CppVersion.CPP20: "-std=c++20"}
    config.additional_compile_flags = []
    config.additional_link_flags = []
    config.features = MagicMock()
    config.features.supported_sanitizers = []
    return config


# Mock Compiler


@pytest.fixture
def mock_compiler(mock_compiler_config):
    compiler = AsyncMock(spec=Compiler)
    compiler.config = mock_compiler_config
    # Mock compile_async to simulate success

    async def mock_compile_async(
        source_files, output_file, cpp_version, options, timeout=None
    ):
        # Simulate creating the output file
        Path(output_file).parent.mkdir(parents=True, exist_ok=True)
        Path(output_file).touch()
        return CompilationResult(
            success=True,
            output_file=Path(output_file),
            duration_ms=100,
            warnings=[],
            errors=[],
        )

    compiler.compile_async.side_effect = mock_compile_async

    # Mock link_async to simulate success
    async def mock_link_async(object_files, output_file, options, timeout=None):
        # Simulate creating the output file
        Path(output_file).parent.mkdir(parents=True, exist_ok=True)
        Path(output_file).touch()
        return CompilationResult(
            success=True,
            output_file=Path(output_file),
            duration_ms=200,
            warnings=[],
            errors=[],
        )

    compiler.link_async.side_effect = mock_link_async

    return compiler


# Mock CompilerManager


@pytest.fixture
def mock_compiler_manager(mock_compiler):
    manager = AsyncMock(spec=CompilerManager)
    manager.get_compiler_async.return_value = mock_compiler
    return manager


# Mock FileManager and ProcessManager (BuildManager uses these, but their methods are not directly called in the tested logic)


@pytest.fixture
def mock_file_manager():
    return MagicMock(spec=FileManager)


@pytest.fixture
def mock_process_manager():
    return MagicMock(spec=ProcessManager)


# Fixture for BuildManager with a temporary build directory
@pytest.fixture
def build_manager(
    tmp_path, mock_compiler_manager, mock_file_manager, mock_process_manager
):
    build_dir = tmp_path / "build"
    # Patch FileManager and ProcessManager in the BuildManager class for the fixture
    with (
        patch(
            "tools.compiler_helper.build_manager.FileManager",
            return_value=mock_file_manager,
        ),
        patch(
            "tools.compiler_helper.build_manager.ProcessManager",
            return_value=mock_process_manager,
        ),
    ):
        manager = BuildManager(
            compiler_manager=mock_compiler_manager,
            build_dir=build_dir,
            parallel=True,
            cache_enabled=True,
        )
        yield manager
        # Clean up the temporary directory
        if build_dir.exists():
            shutil.rmtree(build_dir)


# Fixture for creating dummy source files


@pytest.fixture
def create_source_files(tmp_path):
    def _create_files(file_names, content="int main() { return 0; }"):
        files = []
        for name in file_names:
            file_path = tmp_path / name
            file_path.parent.mkdir(parents=True, exist_ok=True)
            file_path.write_text(content)
            files.append(file_path)
        return files

    return _create_files


# Fixture for simulating file hash calculation


@pytest.fixture
def mock_calculate_file_hash_async(mocker):
    mock_hash = mocker.patch(
        "tools.compiler_helper.build_manager.BuildManager._calculate_file_hash_async",
        new_callable=AsyncMock,
    )
    # Default behavior: return a hash based on file content (simple simulation)

    async def _calculate_hash(file_path: Path):
        return hashlib.md5(file_path.read_bytes()).hexdigest()

    mock_hash.side_effect = _calculate_hash
    return mock_hash


# Fixture for simulating dependency scanning


@pytest.fixture
def mock_scan_dependencies_async(mocker):
    mock_scan = mocker.patch(
        "tools.compiler_helper.build_manager.BuildManager._scan_dependencies_async",
        new_callable=AsyncMock,
    )
    # Default behavior: return empty set
    mock_scan.return_value = set()
    return mock_scan


@pytest.mark.asyncio
async def test_build_async_success(
    build_manager, mock_compiler, create_source_files, tmp_path
):
    source_files = create_source_files(["src/file1.cpp", "src/file2.cpp"])
    output_file = tmp_path / "app"

    result = await build_manager.build_async(
        source_files=source_files, output_file=output_file, cpp_version=CppVersion.CPP17
    )

    assert result.success is True
    assert result.output_file == output_file
    assert output_file.exists()
    assert len(result.artifacts) > 0
    assert output_file in result.artifacts

    # Check if compile_async was called for each source file
    assert mock_compiler.compile_async.call_count == len(source_files)
    # Check if link_async was called once with the correct object files
    mock_compiler.link_async.assert_called_once()
    linked_objects = mock_compiler.link_async.call_args[0][0]
    assert len(linked_objects) == len(source_files)
    assert all(
        Path(obj).exists() for obj in linked_objects
    )  # Check if mock compile created them

    # Check cache update and save
    assert len(build_manager.dependency_cache) == len(source_files)
    assert build_manager.cache_file.exists()


@pytest.mark.asyncio
async def test_build_async_incremental_no_changes(
    build_manager,
    mock_compiler,
    create_source_files,
    tmp_path,
    mock_calculate_file_hash_async,
    mock_scan_dependencies_async,
):
    source_files = create_source_files(["src/file1.cpp", "src/file2.cpp"])
    output_file = tmp_path / "app"

    # First build
    await build_manager.build_async(
        source_files=source_files,
        output_file=output_file,
        cpp_version=CppVersion.CPP17,
        incremental=True,
    )

    # Reset mocks to check calls during the second build
    mock_compiler.compile_async.reset_mock()
    mock_compiler.link_async.reset_mock()
    mock_calculate_file_hash_async.reset_mock()
    mock_scan_dependencies_async.reset_mock()

    # Second build with no changes
    result = await build_manager.build_async(
        source_files=source_files,
        output_file=output_file,
        cpp_version=CppVersion.CPP17,
        incremental=True,
    )

    assert result.success is True
    assert result.output_file == output_file
    assert output_file.exists()

    # Check that compile_async was NOT called (files should be cached)
    mock_compiler.compile_async.assert_not_called()
    # Check that link_async WAS called (linking always happens)
    mock_compiler.link_async.assert_called_once()

    # Check metrics reflect cached files
    assert build_manager.get_metrics()["cache_entries"] == len(source_files)
    # Note: BuildMetrics are per-build, not cumulative in the manager instance
    # We'd need to inspect the metrics object returned by build_async if we wanted to assert that.
    # For now, checking mock calls is sufficient.


@pytest.mark.asyncio
async def test_build_async_incremental_source_change(
    build_manager,
    mock_compiler,
    create_source_files,
    tmp_path,
    mock_calculate_file_hash_async,
    mock_scan_dependencies_async,
):
    source_files = create_source_files(["src/file1.cpp", "src/file2.cpp"])
    output_file = tmp_path / "app"

    # First build
    await build_manager.build_async(
        source_files=source_files,
        output_file=output_file,
        cpp_version=CppVersion.CPP17,
        incremental=True,
    )

    # Modify one source file
    source_files[0].write_text("int main() { return 1; }")

    # Reset mocks
    mock_compiler.compile_async.reset_mock()
    mock_compiler.link_async.reset_mock()
    mock_calculate_file_hash_async.reset_mock()  # Reset hash mock

    # Second build
    result = await build_manager.build_async(
        source_files=source_files,
        output_file=output_file,
        cpp_version=CppVersion.CPP17,
        incremental=True,
    )

    assert result.success is True
    assert result.output_file == output_file
    assert output_file.exists()

    # Check that compile_async was called only for the changed file
    assert mock_compiler.compile_async.call_count == 1
    # Get the first source file arg
    called_source_file = mock_compiler.compile_async.call_args[0][0][0]
    assert called_source_file == source_files[0]

    # Check that link_async was called
    mock_compiler.link_async.assert_called_once()

    # Check cache update
    assert len(build_manager.dependency_cache) == len(source_files)
    # The hash for the changed file should be updated in the cache
    cached_entry = build_manager.dependency_cache[str(source_files[0].resolve())]
    assert (
        cached_entry.file_hash != hashlib.md5(b"int main() { return 0; }").hexdigest()
    )
    assert (
        cached_entry.file_hash == hashlib.md5(b"int main() { return 1; }").hexdigest()
    )


@pytest.mark.asyncio
async def test_build_async_incremental_dependency_change(
    build_manager,
    mock_compiler,
    create_source_files,
    tmp_path,
    mock_calculate_file_hash_async,
    mock_scan_dependencies_async,
):
    header_file = create_source_files(
        ["include/header.h"], content="#define VERSION 1"
    )[0]
    source_files = create_source_files(
        ["src/file1.cpp"],
        content=f'#include "include/header.h"\nint main() {{ return VERSION; }}',
    )
    output_file = tmp_path / "app"

    # Simulate dependency scanning finding the header
    mock_scan_dependencies_async.return_value = {str(header_file.resolve())}

    # First build
    await build_manager.build_async(
        source_files=source_files,
        output_file=output_file,
        cpp_version=CppVersion.CPP17,
        incremental=True,
    )

    # Check cache entry includes dependency
    source_str = str(source_files[0].resolve())
    assert source_str in build_manager.dependency_cache
    assert (
        str(header_file.resolve())
        in build_manager.dependency_cache[source_str].dependencies
    )

    # Reset mocks
    mock_compiler.compile_async.reset_mock()
    mock_compiler.link_async.reset_mock()
    mock_calculate_file_hash_async.reset_mock()  # Reset hash mock
    mock_scan_dependencies_async.reset_mock()  # Reset scan mock

    # Modify the header file
    header_file.write_text("#define VERSION 2")

    # Simulate hash calculation for the modified header
    async def _calculate_hash_with_change(file_path: Path):
        if file_path.resolve() == header_file.resolve():
            return hashlib.md5(b"#define VERSION 2").hexdigest()
        # Use actual content for others
        return hashlib.md5(file_path.read_bytes()).hexdigest()

    mock_calculate_file_hash_async.side_effect = _calculate_hash_with_change

    # Simulate dependency scanning finding the header again
    mock_scan_dependencies_async.return_value = {str(header_file.resolve())}

    # Second build
    result = await build_manager.build_async(
        source_files=source_files,
        output_file=output_file,
        cpp_version=CppVersion.CPP17,
        incremental=True,
    )

    assert result.success is True
    assert result.output_file == output_file
    assert output_file.exists()

    # Check that compile_async was called because the dependency changed
    assert mock_compiler.compile_async.call_count == 1
    called_source_file = mock_compiler.compile_async.call_args[0][0][0]
    assert called_source_file == source_files[0]

    # Check that link_async was called
    mock_compiler.link_async.assert_called_once()

    # Check cache update
    assert (
        len(build_manager.dependency_cache) == len(source_files) + 1
    )  # Source + Header
    # The hash for the header file should be updated in the cache
    cached_header_entry = build_manager.dependency_cache[str(header_file.resolve())]
    assert (
        cached_header_entry.file_hash == hashlib.md5(b"#define VERSION 2").hexdigest()
    )


@pytest.mark.asyncio
async def test_build_async_force_rebuild(
    build_manager, mock_compiler, create_source_files, tmp_path
):
    source_files = create_source_files(["src/file1.cpp", "src/file2.cpp"])
    output_file = tmp_path / "app"

    # First build (incremental enabled)
    await build_manager.build_async(
        source_files=source_files,
        output_file=output_file,
        cpp_version=CppVersion.CPP17,
        incremental=True,
    )

    # Reset mocks
    mock_compiler.compile_async.reset_mock()
    mock_compiler.link_async.reset_mock()

    # Second build with force_rebuild=True
    result = await build_manager.build_async(
        source_files=source_files,
        output_file=output_file,
        cpp_version=CppVersion.CPP17,
        incremental=True,  # Incremental is still True, but force_rebuild overrides it
        force_rebuild=True,
    )

    assert result.success is True
    assert result.output_file == output_file
    assert output_file.exists()

    # Check that compile_async was called for ALL files again
    assert mock_compiler.compile_async.call_count == len(source_files)

    # Check that link_async was called
    mock_compiler.link_async.assert_called_once()


@pytest.mark.asyncio
async def test_build_async_compilation_failure(
    build_manager, mock_compiler, create_source_files, tmp_path
):
    source_files = create_source_files(["src/file1.cpp", "src/file2.cpp"])
    output_file = tmp_path / "app"

    # Configure mock compiler to fail compilation for one file
    async def mock_compile_fail(
        source_files_list, output_file, cpp_version, options, timeout=None
    ):
        source_file = source_files_list[0]  # Assuming one file per call
        if "file1" in str(source_file):
            return CompilationResult(
                success=False,
                errors=["Mock compilation error"],
                warnings=[],
                duration_ms=50,
            )
        else:
            # Simulate success for others
            Path(output_file).parent.mkdir(parents=True, exist_ok=True)
            Path(output_file).touch()
            return CompilationResult(
                success=True,
                output_file=Path(output_file),
                duration_ms=100,
                warnings=[],
                errors=[],
            )

    mock_compiler.compile_async.side_effect = mock_compile_fail

    result = await build_manager.build_async(
        source_files=source_files, output_file=output_file, cpp_version=CppVersion.CPP17
    )

    assert result.success is False
    assert len(result.errors) > 0
    assert "Mock compilation error" in result.errors
    assert result.output_file is None  # Output file should not be created on failure

    # Link should not have been called
    mock_compiler.link_async.assert_not_called()


@pytest.mark.asyncio
async def test_build_async_linking_failure(
    build_manager, mock_compiler, create_source_files, tmp_path
):
    source_files = create_source_files(["src/file1.cpp", "src/file2.cpp"])
    output_file = tmp_path / "app"

    # Configure mock compiler to fail linking
    mock_compiler.link_async.return_value = CompilationResult(
        success=False, errors=["Mock linking error"], warnings=[], duration_ms=150
    )

    result = await build_manager.build_async(
        source_files=source_files, output_file=output_file, cpp_version=CppVersion.CPP17
    )

    assert result.success is False
    assert len(result.errors) > 0
    assert "Mock linking error" in result.errors
    assert result.output_file is None  # Output file should not be created on failure

    # Compile should have been called for all files
    assert mock_compiler.compile_async.call_count == len(source_files)
    # Link should have been called once
    mock_compiler.link_async.assert_called_once()


@pytest.mark.asyncio
async def test_build_async_file_not_found(build_manager, create_source_files, tmp_path):
    source_files = create_source_files(["src/file1.cpp"])
    non_existent_file = tmp_path / "non_existent.cpp"
    source_files.append(non_existent_file)
    output_file = tmp_path / "app"

    with pytest.raises(FileNotFoundError) as excinfo:
        await build_manager.build_async(
            source_files=source_files,
            output_file=output_file,
            cpp_version=CppVersion.CPP17,
        )

    assert f"Source file not found: {non_existent_file}" in str(excinfo.value)


@pytest.mark.asyncio
async def test_build_async_parallel_compilation(
    build_manager, mock_compiler, create_source_files, tmp_path
):
    source_files = create_source_files(
        [f"src/file{i}.cpp" for i in range(5)]
    )  # More than 1 file
    output_file = tmp_path / "app"

    # Ensure parallel is enabled in the fixture
    assert build_manager.parallel is True

    await build_manager.build_async(
        source_files=source_files,
        output_file=output_file,
        cpp_version=CppVersion.CPP17,
        parallel=True,  # Explicitly pass True, though fixture sets it
    )

    # Check that compile_async was called for each file
    assert mock_compiler.compile_async.call_count == len(source_files)

    # Note: It's hard to *prove* parallel execution purely from mock call counts.
    # We would need to inspect the call order or use more sophisticated mocks
    # that track execution time or concurrency. For this test, checking that
    # all compile calls were initiated is sufficient to verify the parallel path was taken.


@pytest.mark.asyncio
async def test_build_async_sequential_compilation(
    build_manager, mock_compiler, create_source_files, tmp_path
):
    source_files = create_source_files([f"src/file{i}.cpp" for i in range(3)])
    output_file = tmp_path / "app"

    # Temporarily disable parallel for this test
    build_manager.parallel = False

    await build_manager.build_async(
        source_files=source_files,
        output_file=output_file,
        cpp_version=CppVersion.CPP17,
        parallel=False,  # Explicitly pass False
    )

    # Check that compile_async was called for each file
    assert mock_compiler.compile_async.call_count == len(source_files)

    # Note: Similar to parallel, proving sequential execution order requires
    # more complex mocks. Checking call count is the basic verification.


@pytest.mark.asyncio
async def test_build_async_cache_disabled(
    build_manager, mock_compiler, create_source_files, tmp_path
):
    source_files = create_source_files(["src/file1.cpp"])
    output_file = tmp_path / "app"

    # Temporarily disable cache
    build_manager.cache_enabled = False

    # First build
    await build_manager.build_async(
        source_files=source_files,
        output_file=output_file,
        cpp_version=CppVersion.CPP17,
        incremental=True,  # Incremental should be ignored if cache is off
    )

    # Reset mocks
    mock_compiler.compile_async.reset_mock()
    mock_compiler.link_async.reset_mock()

    # Second build with no changes, cache disabled
    result = await build_manager.build_async(
        source_files=source_files,
        output_file=output_file,
        cpp_version=CppVersion.CPP17,
        incremental=True,  # Incremental should be ignored if cache is off
    )

    assert result.success is True
    assert result.output_file == output_file
    assert output_file.exists()

    # Check that compile_async was called again (cache was not used)
    assert mock_compiler.compile_async.call_count == len(source_files)

    # Check that link_async was called
    mock_compiler.link_async.assert_called_once()

    # Check cache file does not exist or is empty (depending on initial state)
    # The fixture creates the build dir, but cache_enabled=False means it shouldn't be saved to
    assert (
        not build_manager.cache_file.exists()
        or build_manager.cache_file.stat().st_size == 0
    )
    assert len(build_manager.dependency_cache) == 0


def test_clean_object_files(build_manager, create_source_files, tmp_path):
    # Create dummy object files in the build directory
    obj_dir = build_manager.build_dir / "mock_compiler_c++17"
    obj_dir.mkdir(parents=True, exist_ok=True)
    obj_file1 = obj_dir / "file1.o"
    obj_file2 = obj_dir / "file2.o"
    obj_file1.touch()
    obj_file2.touch()

    assert obj_file1.exists()
    assert obj_file2.exists()

    build_manager.clean()

    assert not obj_file1.exists()
    assert not obj_file2.exists()
    assert obj_dir.exists()  # Directory itself is not removed by default clean
    assert build_manager.build_dir.exists()
    assert not build_manager.cache_file.exists()  # Cache file should also be removed


def test_clean_aggressive(build_manager, create_source_files, tmp_path):
    # Create dummy files in the build directory
    dummy_file = build_manager.build_dir / "subdir" / "dummy.txt"
    dummy_file.parent.mkdir(parents=True, exist_ok=True)
    dummy_file.touch()
    cache_file = build_manager.build_dir / "build_cache.json"
    cache_file.touch()

    assert build_manager.build_dir.exists()
    assert dummy_file.exists()
    assert cache_file.exists()

    build_manager.clean(aggressive=True)

    assert not build_manager.build_dir.exists()  # Build directory should be removed
    # BuildManager.__init__ recreates the build_dir, so it should exist after clean, but be empty
    assert build_manager.build_dir.exists()
    assert not any(build_manager.build_dir.iterdir())  # Should be empty


def test_load_cache_success(build_manager, tmp_path):
    cache_data = {
        str(tmp_path / "src/file1.cpp"): {
            "file_hash": "hash1",
            "dependencies": [str(tmp_path / "include/dep1.h")],
            "object_file": str(tmp_path / "build/obj/file1.o"),
            "timestamp": time.time(),
        },
        str(tmp_path / "include/dep1.h"): {
            "file_hash": "hash_dep1",
            "dependencies": [],
            "object_file": None,
            "timestamp": time.time(),
        },
    }
    build_manager.cache_file.parent.mkdir(parents=True, exist_ok=True)
    build_manager.cache_file.write_text(json.dumps(cache_data))

    # Clear initial cache loaded during __init__
    build_manager.dependency_cache.clear()

    build_manager._load_cache()

    assert len(build_manager.dependency_cache) == 2
    file1_entry = build_manager.dependency_cache.get(str(tmp_path / "src/file1.cpp"))
    assert file1_entry is not None
    assert file1_entry.file_hash == "hash1"
    assert str(tmp_path / "include/dep1.h") in file1_entry.dependencies


def test_load_cache_file_not_found(build_manager, tmp_path):
    # Ensure cache file does not exist
    if build_manager.cache_file.exists():
        build_manager.cache_file.unlink()

    # Clear initial cache loaded during __init__
    build_manager.dependency_cache.clear()

    build_manager._load_cache()

    # Cache should remain empty
    assert len(build_manager.dependency_cache) == 0


def test_load_cache_invalid_json(build_manager, tmp_path):
    build_manager.cache_file.parent.mkdir(parents=True, exist_ok=True)
    build_manager.cache_file.write_text("invalid json")

    # Clear initial cache loaded during __init__
    build_manager.dependency_cache.clear()

    build_manager._load_cache()

    # Cache should be cleared on error
    assert len(build_manager.dependency_cache) == 0


@pytest.mark.asyncio
async def test_save_cache_success(build_manager, tmp_path):
    build_manager.dependency_cache = {
        str(tmp_path / "src/file1.cpp"): BuildCacheEntry(
            file_hash="hash1",
            dependencies={str(tmp_path / "include/dep1.h")},
            object_file=str(tmp_path / "build/obj/file1.o"),
            timestamp=time.time(),
        )
    }

    await build_manager._save_cache_async()

    assert build_manager.cache_file.exists()
    loaded_data = json.loads(build_manager.cache_file.read_text())
    assert len(loaded_data) == 1
    assert str(tmp_path / "src/file1.cpp") in loaded_data
    assert loaded_data[str(tmp_path / "src/file1.cpp")]["file_hash"] == "hash1"


@pytest.mark.asyncio
async def test_save_cache_disabled(build_manager, tmp_path):
    build_manager.cache_enabled = False
    build_manager.dependency_cache = {
        str(tmp_path / "src/file1.cpp"): BuildCacheEntry(
            file_hash="hash1",
            dependencies=set(),
            object_file=None,
            timestamp=time.time(),
        )
    }
    # Ensure cache file doesn't exist initially
    if build_manager.cache_file.exists():
        build_manager.cache_file.unlink()

    await build_manager._save_cache_async()

    assert not build_manager.cache_file.exists()  # Cache should not be saved


def test_get_metrics(build_manager):
    # Initial metrics
    metrics = build_manager.get_metrics()
    assert metrics["cache_entries"] == 0  # Initially empty cache
    assert metrics["build_dir"] == str(build_manager.build_dir)
    assert metrics["cache_enabled"] is True
    assert metrics["parallel"] is True
    assert metrics["max_workers"] > 0

    # Simulate adding cache entries (e.g., after a build)
    build_manager.dependency_cache["file1"] = BuildCacheEntry("hash1")
    build_manager.dependency_cache["file2"] = BuildCacheEntry("hash2")

    metrics = build_manager.get_metrics()
    assert metrics["cache_entries"] == 2
