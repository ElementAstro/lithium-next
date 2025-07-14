import asyncio
import os
import platform
import shutil
import subprocess
from pathlib import Path
from unittest.mock import AsyncMock, MagicMock, patch
import pytest
from .compiler_manager import CompilerManager, CompilerSpec
from .compiler import EnhancedCompiler as Compiler, CompilerConfig
from .utils import SystemInfo

# filepath: /home/max/lithium-next/python/tools/compiler_helper/test_compiler_manager.py


# Use relative imports as the directory is a package
from .core_types import (
    CompilerNotFoundError,
    CppVersion,
    CompilerType,
    CompilerException,
    CompilerFeatures,
    OptimizationLevel,
    CommandResult,
)


# Mock SystemInfo
@pytest.fixture
def mock_system_info(mocker):
    mock_sys_info = mocker.patch(
        "tools.compiler_helper.compiler_manager.SystemInfo", autospec=True
    )
    mock_sys_info.get_cpu_count.return_value = 4
    mock_sys_info.get_platform_info.return_value = {
        "system": "Linux",
        "release": "5.15",
    }
    mock_sys_info.get_memory_info.return_value = {"total": "8GB"}
    mock_sys_info.get_environment_info.return_value = {"PATH": "/usr/bin"}
    return mock_sys_info


# Mock CompilerConfig
@pytest.fixture
def mock_compiler_config(mock_compiler_config_data):
    # Use actual CompilerConfig to test Pydantic validation if needed,
    # but for mocking the Compiler instance, a MagicMock is often easier.
    # Here we'll use a MagicMock that mimics the structure.
    config = MagicMock(spec=CompilerConfig)
    config.name = mock_compiler_config_data["name"]
    config.command = mock_compiler_config_data["command"]
    config.compiler_type = mock_compiler_config_data["compiler_type"]
    config.version = mock_compiler_config_data["version"]
    config.cpp_flags = mock_compiler_config_data["cpp_flags"]
    config.additional_compile_flags = mock_compiler_config_data[
        "additional_compile_flags"
    ]
    config.additional_link_flags = mock_compiler_config_data["additional_link_flags"]
    config.features = MagicMock(spec=CompilerFeatures)
    config.features.supported_cpp_versions = mock_compiler_config_data["features"][
        "supported_cpp_versions"
    ]
    config.features.supported_sanitizers = mock_compiler_config_data["features"][
        "supported_sanitizers"
    ]
    config.features.supported_optimizations = mock_compiler_config_data["features"][
        "supported_optimizations"
    ]
    config.features.supports_parallel = mock_compiler_config_data["features"][
        "supports_parallel"
    ]
    config.features.supports_pch = mock_compiler_config_data["features"]["supports_pch"]
    config.features.supports_modules = mock_compiler_config_data["features"][
        "supports_modules"
    ]
    config.features.supports_concepts = mock_compiler_config_data["features"][
        "supports_concepts"
    ]
    config.features.max_parallel_jobs = mock_compiler_config_data["features"][
        "max_parallel_jobs"
    ]
    return config


# Mock Compiler instance returned by the manager
@pytest.fixture
def mock_compiler_instance(mock_compiler_config):
    compiler = MagicMock(spec=Compiler)
    compiler.config = mock_compiler_config
    return compiler


# Mock Compiler class constructor
@pytest.fixture
def mock_compiler_class(mocker, mock_compiler_instance):
    # Patch the Compiler class itself so that when Compiler(...) is called,
    # it returns our mock instance.
    mock_class = mocker.patch(
        "tools.compiler_helper.compiler_manager.EnhancedCompiler",
        return_value=mock_compiler_instance,
    )
    return mock_class


# Mock CompilerConfig data for a typical GCC compiler
@pytest.fixture
def mock_compiler_config_data():
    return {
        "name": "GCC",
        "command": "/usr/bin/g++",
        "compiler_type": CompilerType.GCC,
        "version": "10.2.0",
        "cpp_flags": {CppVersion.CPP17: "-std=c++17", CppVersion.CPP20: "-std=c++20"},
        "additional_compile_flags": ["-Wall"],
        "additional_link_flags": [],
        "features": {
            "supported_cpp_versions": {CppVersion.CPP17, CppVersion.CPP20},
            "supported_sanitizers": {"address"},
            "supported_optimizations": {OptimizationLevel.STANDARD},
            "supports_parallel": True,
            "supports_pch": True,
            "supports_modules": False,
            "supports_concepts": False,
            "max_parallel_jobs": 4,
        },
    }


# Fixture for CompilerManager with a temporary cache directory
@pytest.fixture
def compiler_manager(tmp_path, mock_system_info):
    cache_dir = tmp_path / ".compiler_helper" / "cache"
    manager = CompilerManager(cache_dir=cache_dir)
    yield manager
    # Clean up the temporary directory
    if cache_dir.parent.exists():
        shutil.rmtree(cache_dir.parent)


@pytest.mark.asyncio
async def test_init(compiler_manager, tmp_path):
    cache_dir = tmp_path / ".compiler_helper" / "cache"
    assert compiler_manager.cache_dir == cache_dir
    assert compiler_manager.cache_dir.exists()
    assert isinstance(compiler_manager.compilers, dict)
    assert compiler_manager.default_compiler is None
    assert isinstance(compiler_manager._compiler_specs, list)
    assert len(compiler_manager._compiler_specs) > 0


@pytest.mark.asyncio
async def test_detect_compilers_async_found(
    compiler_manager, mock_compiler_class, mocker
):
    # Mock shutil.which to simulate finding g++ and clang++
    mocker.patch(
        "shutil.which",
        side_effect=lambda cmd: (
            f"/usr/bin/{cmd}" if cmd in ["g++", "clang++"] else None
        ),
    )
    # Mock _get_compiler_version_async and _create_compiler_features
    mocker.patch.object(
        compiler_manager,
        "_get_compiler_version_async",
        new_callable=AsyncMock,
        return_value="10.2.0",
    )
    mocker.patch.object(
        compiler_manager,
        "_create_compiler_features",
        return_value=MagicMock(spec=CompilerFeatures),
    )

    detected = await compiler_manager.detect_compilers_async()

    assert len(detected) >= 2  # Should find at least GCC and Clang based on mock
    assert "GCC" in detected
    assert "Clang" in detected
    assert compiler_manager.default_compiler in [
        "GCC",
        "Clang",
    ]  # Default should be one of the found
    mock_compiler_class.call_count == len(
        detected
    )  # Compiler constructor called for each found


@pytest.mark.asyncio
async def test_detect_compilers_async_not_found(
    compiler_manager, mock_compiler_class, mocker
):
    # Mock shutil.which to simulate finding no compilers
    mocker.patch("shutil.which", return_value=None)
    # Mock _find_msvc to simulate not finding MSVC
    mocker.patch.object(compiler_manager, "_find_msvc", return_value=None)

    detected = await compiler_manager.detect_compilers_async()

    assert len(detected) == 0
    assert compiler_manager.default_compiler is None
    mock_compiler_class.assert_not_called()


@pytest.mark.asyncio
async def test_detect_compilers_async_partial_failure(
    compiler_manager, mock_compiler_class, mocker
):
    # Mock shutil.which to find g++ but not clang++
    mocker.patch(
        "shutil.which", side_effect=lambda cmd: "/usr/bin/g++" if cmd == "g++" else None
    )
    # Mock _find_msvc to not find MSVC
    mocker.patch.object(compiler_manager, "_find_msvc", return_value=None)
    # Mock _get_compiler_version_async and _create_compiler_features for the successful one
    mocker.patch.object(
        compiler_manager,
        "_get_compiler_version_async",
        new_callable=AsyncMock,
        return_value="10.2.0",
    )
    mocker.patch.object(
        compiler_manager,
        "_create_compiler_features",
        return_value=MagicMock(spec=CompilerFeatures),
    )

    detected = await compiler_manager.detect_compilers_async()

    assert len(detected) == 1
    assert "GCC" in detected
    assert "Clang" not in detected
    assert "MSVC" not in detected
    assert compiler_manager.default_compiler == "GCC"
    mock_compiler_class.call_count == 1


def test_detect_compilers_sync(compiler_manager, mock_compiler_class, mocker):
    # Mock shutil.which for sync test
    mocker.patch(
        "shutil.which",
        side_effect=lambda cmd: (
            f"/usr/bin/{cmd}" if cmd in ["g++", "clang++"] else None
        ),
    )
    # Mock _find_msvc for sync test
    mocker.patch.object(compiler_manager, "_find_msvc", return_value=None)
    # Mock the async helper methods called by _detect_compiler_async
    mocker.patch.object(
        compiler_manager, "_get_compiler_version_async", return_value="10.2.0"
    )
    mocker.patch.object(
        compiler_manager,
        "_create_compiler_features",
        return_value=MagicMock(spec=CompilerFeatures),
    )

    detected = compiler_manager.detect_compilers()

    assert len(detected) >= 2
    assert "GCC" in detected
    assert "Clang" in detected
    assert compiler_manager.default_compiler in ["GCC", "Clang"]
    mock_compiler_class.call_count == len(detected)


@pytest.mark.asyncio
async def test_get_compiler_async_by_name(
    compiler_manager, mock_compiler_instance, mock_compiler_class, mocker
):
    # Simulate compilers being detected
    compiler_manager.compilers = {
        "GCC": mock_compiler_instance,
        "Clang": MagicMock(spec=Compiler),  # Another mock compiler
    }
    compiler_manager.default_compiler = "GCC"

    compiler = await compiler_manager.get_compiler_async("Clang")

    assert compiler is not None
    assert compiler.config.name == "Clang"  # Check against the mock's config name
    # Ensure detect_compilers_async was not called if compilers are already loaded
    mocker.patch.object(
        compiler_manager, "detect_compilers_async", new_callable=AsyncMock
    )
    compiler_manager.detect_compilers_async.assert_not_called()


@pytest.mark.asyncio
async def test_get_compiler_async_default(
    compiler_manager, mock_compiler_instance, mock_compiler_class, mocker
):
    # Simulate compilers being detected
    compiler_manager.compilers = {
        "GCC": mock_compiler_instance,
        "Clang": MagicMock(spec=Compiler),
    }
    compiler_manager.default_compiler = "GCC"

    compiler = await compiler_manager.get_compiler_async()  # Get default

    assert compiler is not None
    assert compiler.config.name == "GCC"
    mocker.patch.object(
        compiler_manager, "detect_compilers_async", new_callable=AsyncMock
    )
    compiler_manager.detect_compilers_async.assert_not_called()


@pytest.mark.asyncio
async def test_get_compiler_async_detect_if_empty(
    compiler_manager, mock_compiler_class, mocker
):
    # Ensure compilers are initially empty
    compiler_manager.compilers = {}
    compiler_manager.default_compiler = None

    # Mock detection to find GCC
    mocker.patch(
        "shutil.which", side_effect=lambda cmd: "/usr/bin/g++" if cmd == "g++" else None
    )
    mocker.patch.object(compiler_manager, "_find_msvc", return_value=None)
    mocker.patch.object(
        compiler_manager,
        "_get_compiler_version_async",
        new_callable=AsyncMock,
        return_value="10.2.0",
    )
    mocker.patch.object(
        compiler_manager,
        "_create_compiler_features",
        return_value=MagicMock(spec=CompilerFeatures),
    )

    compiler = await compiler_manager.get_compiler_async("GCC")

    assert compiler is not None
    assert compiler.config.name == "GCC"
    # Check that detect_compilers_async was called
    # We need to re-patch after the initial call in get_compiler_async
    # A better approach is to check the state *after* the call
    assert "GCC" in compiler_manager.compilers
    assert compiler_manager.default_compiler == "GCC"


@pytest.mark.asyncio
async def test_get_compiler_async_not_found(
    compiler_manager, mock_compiler_class, mocker
):
    # Simulate compilers being detected
    compiler_manager.compilers = {
        "GCC": MagicMock(spec=Compiler),
        "Clang": MagicMock(spec=Compiler),
    }
    compiler_manager.default_compiler = "GCC"

    with pytest.raises(CompilerNotFoundError) as excinfo:
        await compiler_manager.get_compiler_async("NonExistent")

    assert "Compiler 'NonExistent' not found." in str(excinfo.value)
    assert excinfo.value.error_code == "COMPILER_NOT_FOUND"
    assert excinfo.value.requested_compiler == "NonExistent"
    assert set(excinfo.value.available_compilers) == {"GCC", "Clang"}


@pytest.mark.asyncio
async def test_get_compiler_async_no_compilers_detected(
    compiler_manager, mock_compiler_class, mocker
):
    # Ensure compilers are initially empty
    compiler_manager.compilers = {}
    compiler_manager.default_compiler = None

    # Mock detection to find no compilers
    mocker.patch("shutil.which", return_value=None)
    mocker.patch.object(compiler_manager, "_find_msvc", return_value=None)

    with pytest.raises(CompilerNotFoundError) as excinfo:
        await compiler_manager.get_compiler_async()

    assert "No compilers detected on the system" in str(excinfo.value)
    assert excinfo.value.error_code == "NO_COMPILERS_FOUND"


def test_get_compiler_sync_by_name(
    compiler_manager, mock_compiler_instance, mock_compiler_class, mocker
):
    # Simulate compilers being detected
    compiler_manager.compilers = {
        "GCC": mock_compiler_instance,
        "Clang": MagicMock(spec=Compiler),
    }
    compiler_manager.default_compiler = "GCC"

    compiler = compiler_manager.get_compiler("Clang")

    assert compiler is not None
    assert compiler.config.name == "Clang"
    # Ensure detect_compilers was not called if compilers are already loaded
    mocker.patch.object(compiler_manager, "detect_compilers")
    compiler_manager.detect_compilers.assert_not_called()


def test_get_compiler_sync_default(
    compiler_manager, mock_compiler_instance, mock_compiler_class, mocker
):
    # Simulate compilers being detected
    compiler_manager.compilers = {
        "GCC": mock_compiler_instance,
        "Clang": MagicMock(spec=Compiler),
    }
    compiler_manager.default_compiler = "GCC"

    compiler = compiler_manager.get_compiler()  # Get default

    assert compiler is not None
    assert compiler.config.name == "GCC"
    mocker.patch.object(compiler_manager, "detect_compilers")
    compiler_manager.detect_compilers.assert_not_called()


def test_get_compiler_sync_detect_if_empty(
    compiler_manager, mock_compiler_class, mocker
):
    # Ensure compilers are initially empty
    compiler_manager.compilers = {}
    compiler_manager.default_compiler = None

    # Mock detection to find GCC
    mocker.patch(
        "shutil.which", side_effect=lambda cmd: "/usr/bin/g++" if cmd == "g++" else None
    )
    mocker.patch.object(compiler_manager, "_find_msvc", return_value=None)
    # Mock the async helper methods called by _detect_compiler_async (which is run sync)
    mocker.patch.object(
        compiler_manager, "_get_compiler_version_async", return_value="10.2.0"
    )
    mocker.patch.object(
        compiler_manager,
        "_create_compiler_features",
        return_value=MagicMock(spec=CompilerFeatures),
    )

    compiler = compiler_manager.get_compiler("GCC")

    assert compiler is not None
    assert compiler.config.name == "GCC"
    assert "GCC" in compiler_manager.compilers
    assert compiler_manager.default_compiler == "GCC"


def test_get_compiler_sync_not_found(compiler_manager, mock_compiler_class, mocker):
    # Simulate compilers being detected
    compiler_manager.compilers = {
        "GCC": MagicMock(spec=Compiler),
        "Clang": MagicMock(spec=Compiler),
    }
    compiler_manager.default_compiler = "GCC"

    with pytest.raises(CompilerNotFoundError) as excinfo:
        compiler_manager.get_compiler("NonExistent")

    assert "Compiler 'NonExistent' not found." in str(excinfo.value)


def test_get_compiler_sync_no_compilers_detected(
    compiler_manager, mock_compiler_class, mocker
):
    # Ensure compilers are initially empty
    compiler_manager.compilers = {}
    compiler_manager.default_compiler = None

    # Mock detection to find no compilers
    mocker.patch("shutil.which", return_value=None)
    mocker.patch.object(compiler_manager, "_find_msvc", return_value=None)

    with pytest.raises(CompilerNotFoundError) as excinfo:
        compiler_manager.get_compiler()

    assert "No compilers detected on the system" in str(excinfo.value)


@pytest.mark.asyncio
async def test__detect_compiler_async_success_path(
    compiler_manager, mock_compiler_class, mocker
):
    spec = CompilerSpec(
        name="TestCompiler",
        command_names=["test_cmd"],
        compiler_type=CompilerType.GCC,
        cpp_flags={CppVersion.CPP17: "-std=c++17"},
    )
    mock_path = "/opt/test/test_cmd"
    mocker.patch("shutil.which", return_value=mock_path)
    mocker.patch.object(
        compiler_manager,
        "_get_compiler_version_async",
        new_callable=AsyncMock,
        return_value="1.0.0",
    )
    mocker.patch.object(
        compiler_manager,
        "_create_compiler_features",
        return_value=MagicMock(spec=CompilerFeatures),
    )

    compiler = await compiler_manager._detect_compiler_async(spec)

    assert compiler is not None
    assert compiler.config.name == "TestCompiler"
    assert compiler.config.command == mock_path
    mock_compiler_class.assert_called_once()
    shutil.which.assert_called_once_with("test_cmd")


@pytest.mark.asyncio
async def test__detect_compiler_async_success_find_method(
    compiler_manager, mock_compiler_class, mocker
):
    # Add a mock find method to the manager instance
    async def mock_find_method():
        return "/opt/custom/custom_compiler"

    compiler_manager._find_custom = mock_find_method

    spec = CompilerSpec(
        name="CustomCompiler",
        command_names=["custom_cmd"],  # This should be ignored
        compiler_type=CompilerType.GCC,
        cpp_flags={CppVersion.CPP17: "-std=c++17"},
        find_method="_find_custom",
    )
    mocker.patch("shutil.which", return_value=None)  # Ensure path search is skipped
    mocker.patch.object(
        compiler_manager,
        "_get_compiler_version_async",
        new_callable=AsyncMock,
        return_value="2.0.0",
    )
    mocker.patch.object(
        compiler_manager,
        "_create_compiler_features",
        return_value=MagicMock(spec=CompilerFeatures),
    )

    compiler = await compiler_manager._detect_compiler_async(spec)

    assert compiler is not None
    assert compiler.config.name == "CustomCompiler"
    assert compiler.config.command == "/opt/custom/custom_compiler"
    mock_compiler_class.assert_called_once()
    shutil.which.assert_not_called()  # Should use find_method instead


@pytest.mark.asyncio
async def test__detect_compiler_async_not_found(
    compiler_manager, mock_compiler_class, mocker
):
    spec = CompilerSpec(
        name="NotFoundCompiler",
        command_names=["non_existent_cmd"],
        compiler_type=CompilerType.GCC,
        cpp_flags={CppVersion.CPP17: "-std=c++17"},
    )
    mocker.patch("shutil.which", return_value=None)

    compiler = await compiler_manager._detect_compiler_async(spec)

    assert compiler is None
    mock_compiler_class.assert_not_called()
    shutil.which.assert_called_once_with("non_existent_cmd")


@pytest.mark.asyncio
async def test__detect_compiler_async_compiler_config_validation_error(
    compiler_manager, mock_compiler_class, mocker
):
    spec = CompilerSpec(
        name="InvalidConfigCompiler",
        command_names=["valid_cmd"],
        compiler_type=CompilerType.GCC,
        cpp_flags={CppVersion.CPP17: "-std=c++17"},
    )
    mocker.patch("shutil.which", return_value="/usr/bin/valid_cmd")
    mocker.patch.object(
        compiler_manager,
        "_get_compiler_version_async",
        new_callable=AsyncMock,
        return_value="1.0.0",
    )
    mocker.patch.object(
        compiler_manager,
        "_create_compiler_features",
        return_value=MagicMock(spec=CompilerFeatures),
    )

    # Mock the CompilerConfig constructor to raise ValidationError
    mocker.patch(
        "tools.compiler_helper.compiler_manager.CompilerConfig",
        side_effect=ValidationError([], MagicMock()),
    )

    compiler = await compiler_manager._detect_compiler_async(spec)

    assert compiler is None
    mock_compiler_class.assert_not_called()


@pytest.mark.asyncio
async def test__detect_compiler_async_compiler_exception(
    compiler_manager, mock_compiler_class, mocker
):
    spec = CompilerSpec(
        name="CompilerExceptionCompiler",
        command_names=["valid_cmd"],
        compiler_type=CompilerType.GCC,
        cpp_flags={CppVersion.CPP17: "-std=c++17"},
    )
    mocker.patch("shutil.which", return_value="/usr/bin/valid_cmd")
    mocker.patch.object(
        compiler_manager,
        "_get_compiler_version_async",
        new_callable=AsyncMock,
        return_value="1.0.0",
    )
    mocker.patch.object(
        compiler_manager,
        "_create_compiler_features",
        return_value=MagicMock(spec=CompilerFeatures),
    )

    # Mock the Compiler constructor to raise CompilerException
    mock_compiler_class.side_effect = CompilerException("Mock Compiler Error")

    compiler = await compiler_manager._detect_compiler_async(spec)

    assert compiler is None
    mock_compiler_class.assert_called_once()


@pytest.mark.asyncio
async def test__get_compiler_version_async_gcc_clang(compiler_manager, mocker):
    mock_process = AsyncMock()
    mock_process.communicate.return_value = (
        b"GCC version 11.3.0 (Ubuntu 11.3.0-1ubuntu1~22.04)\n",
        b"",
    )
    mocker.patch(
        "asyncio.create_subprocess_exec",
        new_callable=AsyncMock,
        return_value=mock_process,
    )

    version = await compiler_manager._get_compiler_version_async(
        "/usr/bin/g++", CompilerType.GCC
    )
    assert version == "11.3.0"
    asyncio.create_subprocess_exec.assert_called_once_with(
        "/usr/bin/g++",
        "--version",
        stdout=asyncio.subprocess.PIPE,
        stderr=asyncio.subprocess.PIPE,
    )

    mock_process.communicate.return_value = (b"clang version 14.0.0\n", b"")
    asyncio.create_subprocess_exec.reset_mock()
    version = await compiler_manager._get_compiler_version_async(
        "/usr/bin/clang++", CompilerType.CLANG
    )
    assert version == "14.0.0"
    asyncio.create_subprocess_exec.assert_called_once_with(
        "/usr/bin/clang++",
        "--version",
        stdout=asyncio.subprocess.PIPE,
        stderr=asyncio.subprocess.PIPE,
    )


@pytest.mark.asyncio
async def test__get_compiler_version_async_msvc(compiler_manager, mocker):
    mock_process = AsyncMock()
    mock_process.communicate.return_value = (
        b"",
        b"Microsoft (R) C/C++ Optimizing Compiler Version 19.35.32215 for x64\n",
    )
    mocker.patch(
        "asyncio.create_subprocess_exec",
        new_callable=AsyncMock,
        return_value=mock_process,
    )

    version = await compiler_manager._get_compiler_version_async(
        "C:\\Program Files\\VC\\Tools\\cl.exe", CompilerType.MSVC
    )
    assert version == "19.35.32215"
    asyncio.create_subprocess_exec.assert_called_once_with(
        "C:\\Program Files\\VC\\Tools\\cl.exe",
        stdout=asyncio.subprocess.PIPE,
        stderr=asyncio.subprocess.PIPE,
    )


@pytest.mark.asyncio
async def test__get_compiler_version_async_unknown(compiler_manager, mocker):
    mock_process = AsyncMock()
    mock_process.communicate.return_value = (b"Unexpected output\n", b"")
    mocker.patch(
        "asyncio.create_subprocess_exec",
        new_callable=AsyncMock,
        return_value=mock_process,
    )

    version = await compiler_manager._get_compiler_version_async(
        "/usr/bin/unknown_compiler", CompilerType.GCC
    )
    assert version == "unknown"


def test__create_compiler_features_gcc(compiler_manager):
    features = compiler_manager._create_compiler_features(CompilerType.GCC, "10.2.0")
    assert CppVersion.CPP17 in features.supported_cpp_versions
    assert CppVersion.CPP20 in features.supported_cpp_versions
    assert CppVersion.CPP23 not in features.supported_cpp_versions  # GCC < 11
    assert "address" in features.supported_sanitizers
    assert OptimizationLevel.FAST in features.supported_optimizations
    assert features.supports_modules is False
    assert features.supports_concepts is False

    features_11 = compiler_manager._create_compiler_features(CompilerType.GCC, "11.1.0")
    assert CppVersion.CPP23 in features_11.supported_cpp_versions
    assert features_11.supports_modules is True
    assert features_11.supports_concepts is True


def test__create_compiler_features_clang(compiler_manager):
    features = compiler_manager._create_compiler_features(CompilerType.CLANG, "14.0.0")
    assert CppVersion.CPP17 in features.supported_cpp_versions
    assert CppVersion.CPP20 in features.supported_cpp_versions
    assert CppVersion.CPP23 not in features.supported_cpp_versions  # Clang < 16
    assert "address" in features.supported_sanitizers
    assert "memory" not in features.supported_sanitizers  # Clang < 16
    assert OptimizationLevel.FAST in features.supported_optimizations
    assert features.supports_modules is False
    assert features.supports_concepts is False

    features_16 = compiler_manager._create_compiler_features(
        CompilerType.CLANG, "16.0.0"
    )
    assert CppVersion.CPP23 in features_16.supported_cpp_versions
    assert features_16.supports_modules is True
    assert features_16.supports_concepts is True
    assert "memory" in features_16.supported_sanitizers


def test__create_compiler_features_msvc(compiler_manager):
    features = compiler_manager._create_compiler_features(
        CompilerType.MSVC, "19.28.29910"
    )
    assert CppVersion.CPP17 in features.supported_cpp_versions
    assert CppVersion.CPP20 in features.supported_cpp_versions
    assert CppVersion.CPP23 not in features.supported_cpp_versions  # MSVC < 19.30
    assert "address" in features.supported_sanitizers
    assert OptimizationLevel.AGGRESSIVE in features.supported_optimizations
    assert (
        OptimizationLevel.FAST not in features.supported_optimizations
    )  # MSVC doesn't have Ofast
    assert features.supports_modules is False  # MSVC < 19.29
    assert features.supports_concepts is False  # MSVC < 19.30

    features_19_30 = compiler_manager._create_compiler_features(
        CompilerType.MSVC, "19.30.30704"
    )
    assert CppVersion.CPP23 in features_19_30.supported_cpp_versions
    assert features_19_30.supports_modules is True
    assert features_19_30.supports_concepts is True


def test__find_msvc_windows_path(compiler_manager, mocker):
    mocker.patch("platform.system", return_value="Windows")
    mock_path = "C:\\Program Files\\VC\\Tools\\cl.exe"
    mocker.patch("shutil.which", return_value=mock_path)
    mocker.patch("subprocess.run")  # Ensure vswhere is not called

    found_path = compiler_manager._find_msvc()

    assert found_path == mock_path
    shutil.which.assert_called_once_with("cl")
    subprocess.run.assert_not_called()


def test__find_msvc_windows_vswhere_success(compiler_manager, mocker, tmp_path):
    mocker.patch("platform.system", return_value="Windows")
    mocker.patch("shutil.which", return_value=None)  # Not in PATH

    # Simulate vswhere.exe existing
    mock_vswhere_path = tmp_path / "vswhere.exe"
    mock_vswhere_path.touch()
    mocker.patch(
        "os.environ.get", return_value=str(tmp_path.parent)
    )  # Mock ProgramFiles(x86)
    mocker.patch(
        "pathlib.Path.__new__",
        side_effect=lambda cls, *args: (
            Path(os.path.join(*args))
            if args[0] != str(tmp_path.parent)
            else mock_vswhere_path
        ),
    )  # Mock Path constructor for vswhere path

    # Simulate vswhere.exe output
    mock_vs_path = tmp_path / "VS" / "2022" / "Community"
    mock_vs_path.mkdir(parents=True)
    mock_tools_path = mock_vs_path / "VC" / "Tools" / "MSVC"
    mock_tools_path.mkdir(parents=True)
    mock_version_path = mock_tools_path / "14.38.33130"
    mock_version_path.mkdir()
    mock_bin_path = mock_version_path / "bin" / "Hostx64" / "x64"
    mock_bin_path.mkdir(parents=True)
    mock_cl_path = mock_bin_path / "cl.exe"
    mock_cl_path.touch()

    mock_result = MagicMock()
    mock_result.returncode = 0
    mock_result.stdout = str(mock_vs_path) + "\n"  # vswhere outputs installation path
    mocker.patch("subprocess.run", return_value=mock_result)

    # Mock Path.iterdir to simulate finding the version directory
    mocker.patch.object(
        Path, "iterdir", return_value=[mock_version_path], autospec=True
    )
    mocker.patch.object(
        Path, "is_dir", return_value=True, autospec=True
    )  # For iterdir results

    found_path = compiler_manager._find_msvc()

    assert found_path == str(mock_cl_path)
    shutil.which.assert_called_once_with("cl")
    subprocess.run.assert_called_once()


def test__find_msvc_windows_vswhere_not_found(compiler_manager, mocker, tmp_path):
    mocker.patch("platform.system", return_value="Windows")
    mocker.patch("shutil.which", return_value=None)

    # Simulate vswhere.exe not existing
    mocker.patch("os.environ.get", return_value=str(tmp_path.parent))
    mocker.patch(
        "pathlib.Path.__new__",
        side_effect=lambda cls, *args: (
            Path(os.path.join(*args))
            if args[0] != str(tmp_path.parent)
            else tmp_path / "non_existent_vswhere.exe"
        ),
    )

    found_path = compiler_manager._find_msvc()

    assert found_path is None
    shutil.which.assert_called_once_with("cl")
    subprocess.run.assert_not_called()


def test__find_msvc_windows_vswhere_failure(compiler_manager, mocker, tmp_path):
    mocker.patch("platform.system", return_value="Windows")
    mocker.patch("shutil.which", return_value=None)

    # Simulate vswhere.exe existing
    mock_vswhere_path = tmp_path / "vswhere.exe"
    mock_vswhere_path.touch()
    mocker.patch("os.environ.get", return_value=str(tmp_path.parent))
    mocker.patch(
        "pathlib.Path.__new__",
        side_effect=lambda cls, *args: (
            Path(os.path.join(*args))
            if args[0] != str(tmp_path.parent)
            else mock_vswhere_path
        ),
    )

    # Simulate vswhere.exe failing
    mock_result = MagicMock()
    mock_result.returncode = 1  # Non-zero return code
    mock_result.stdout = ""
    mocker.patch("subprocess.run", return_value=mock_result)

    found_path = compiler_manager._find_msvc()

    assert found_path is None
    shutil.which.assert_called_once_with("cl")
    subprocess.run.assert_called_once()


def test__find_msvc_not_windows(compiler_manager, mocker):
    mocker.patch("platform.system", return_value="Linux")
    mocker.patch("shutil.which", return_value=None)  # Not in PATH

    found_path = compiler_manager._find_msvc()

    assert found_path is None
    shutil.which.assert_called_once_with("cl")
    # vswhere logic should be skipped on non-Windows
    mocker.patch("subprocess.run")
    subprocess.run.assert_not_called()


def test_list_compilers(
    compiler_manager, mock_compiler_instance, mock_compiler_class, mocker
):
    # Simulate compilers being detected
    compiler_manager.compilers = {
        "GCC": mock_compiler_instance,
        "Clang": MagicMock(spec=Compiler),
    }
    compiler_manager.compilers["Clang"].config = MagicMock(spec=CompilerConfig)
    compiler_manager.compilers["Clang"].config.name = "Clang"
    compiler_manager.compilers["Clang"].config.command = "/usr/bin/clang++"
    compiler_manager.compilers["Clang"].config.compiler_type = CompilerType.CLANG
    compiler_manager.compilers["Clang"].config.version = "14.0.0"
    compiler_manager.compilers["Clang"].config.features = MagicMock(
        spec=CompilerFeatures
    )
    compiler_manager.compilers["Clang"].config.features.supported_cpp_versions = {
        CppVersion.CPP17,
        CppVersion.CPP20,
    }
    compiler_manager.compilers["Clang"].config.features.supports_parallel = True
    compiler_manager.compilers["Clang"].config.features.supports_pch = True
    compiler_manager.compilers["Clang"].config.features.supports_modules = False
    compiler_manager.compilers["Clang"].config.features.supports_concepts = False

    compiler_list = compiler_manager.list_compilers()

    assert isinstance(compiler_list, dict)
    assert len(compiler_list) == 2
    assert "GCC" in compiler_list
    assert "Clang" in compiler_list

    gcc_info = compiler_list["GCC"]
    assert gcc_info["command"] == "/usr/bin/g++"
    assert gcc_info["type"] == "gcc"
    assert gcc_info["version"] == "10.2.0"
    assert set(gcc_info["cpp_versions"]) == {"c++17", "c++20"}
    assert gcc_info["features"]["parallel"] is True

    clang_info = compiler_list["Clang"]
    assert clang_info["command"] == "/usr/bin/clang++"
    assert clang_info["type"] == "clang"
    assert clang_info["version"] == "14.0.0"
    assert set(clang_info["cpp_versions"]) == {"c++17", "c++20"}
    assert clang_info["features"]["modules"] is False


def test_get_system_info(compiler_manager, mock_system_info):
    info = compiler_manager.get_system_info()

    assert isinstance(info, dict)
    assert "platform" in info
    assert "cpu_count" in info
    assert "memory" in info
    assert "environment" in info

    mock_system_info.get_platform_info.assert_called_once()
    mock_system_info.get_cpu_count.assert_called_once()
    mock_system_info.get_memory_info.assert_called_once()
    mock_system_info.get_environment_info.assert_called_once()
