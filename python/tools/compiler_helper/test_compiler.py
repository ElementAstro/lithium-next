import asyncio
import os
import platform
import re
import shutil
import time
from pathlib import Path
from unittest.mock import AsyncMock, MagicMock, patch, call
import pytest
from .compiler import EnhancedCompiler as Compiler, CompilerConfig, DiagnosticParser, CompilerMetrics
from .utils import ProcessManager, SystemInfo

# filepath: /home/max/lithium-next/python/tools/compiler_helper/test_compiler.py



# Use relative imports as the directory is a package
from .core_types import (
    CommandResult, PathLike, CompilationResult, CompilerFeatures, CompilerType,
    CppVersion, CompileOptions, LinkOptions, CompilationError,
    CompilerNotFoundError, OptimizationLevel
)


# --- Fixtures ---

@pytest.fixture
def mock_process_manager():
    """Mock ProcessManager instance."""
    return AsyncMock(spec=ProcessManager)

@pytest.fixture
def mock_diagnostic_parser():
    """Mock DiagnosticParser instance."""
    parser = MagicMock(spec=DiagnosticParser)
    parser.parse_diagnostics.return_value = ([], [], []) # Default: no errors, no warnings, no notes
    return parser

@pytest.fixture
def mock_compiler_metrics():
    """Mock CompilerMetrics instance."""
    metrics = MagicMock(spec=CompilerMetrics)
    metrics.to_dict.return_value = {} # Default empty metrics
    return metrics

@pytest.fixture
def mock_compiler_config_base():
    """Base mock CompilerConfig data."""
    return {
        'name': 'MockCompiler',
        'command': '/usr/bin/mock_compiler',
        'compiler_type': CompilerType.GCC,
        'version': '1.0.0',
        'cpp_flags': {
            CppVersion.CPP17: '-std=c++17',
            CppVersion.CPP20: '-std=c++20'
        },
        'additional_compile_flags': [],
        'additional_link_flags': [],
        'features': MagicMock(spec=CompilerFeatures,
            supports_parallel=True,
            supports_pch=True,
            supports_modules=False,
            supports_coroutines=False,
            supports_concepts=False,
            supported_cpp_versions={CppVersion.CPP17, CppVersion.CPP20},
            supported_sanitizers=set(),
            supported_optimizations={OptimizationLevel.STANDARD},
            feature_flags={},
            max_parallel_jobs=4
        )
    }

@pytest.fixture
def mock_compiler_config_gcc(mock_compiler_config_base):
    """Mock CompilerConfig for GCC."""
    config_data = mock_compiler_config_base.copy()
    config_data['name'] = 'GCC'
    config_data['command'] = '/usr/bin/g++'
    config_data['compiler_type'] = CompilerType.GCC
    config_data['version'] = '11.3.0'
    config_data['cpp_flags'] = {
        CppVersion.CPP17: '-std=c++17',
        CppVersion.CPP20: '-std=c++20',
        CppVersion.CPP23: '-std=c++23'
    }
    config_data['additional_compile_flags'] = ['-Wall', '-Wextra']
    config_data['features'].supported_cpp_versions = {CppVersion.CPP17, CppVersion.CPP20, CppVersion.CPP23}
    config_data['features'].supported_sanitizers = {"address", "thread"}
    config_data['features'].supported_optimizations = {
        OptimizationLevel.NONE, OptimizationLevel.BASIC,
        OptimizationLevel.STANDARD, OptimizationLevel.AGGRESSIVE,
        OptimizationLevel.SIZE, OptimizationLevel.FAST,
        OptimizationLevel.DEBUG
    }
    config_data['features'].supports_modules = True
    config_data['features'].supports_concepts = True
    return MagicMock(spec=CompilerConfig, **config_data)


@pytest.fixture
def mock_compiler_config_clang(mock_compiler_config_base):
    """Mock CompilerConfig for Clang."""
    config_data = mock_compiler_config_base.copy()
    config_data['name'] = 'Clang'
    config_data['command'] = '/usr/bin/clang++'
    config_data['compiler_type'] = CompilerType.CLANG
    config_data['version'] = '14.0.0'
    config_data['cpp_flags'] = {
        CppVersion.CPP17: '-std=c++17',
        CppVersion.CPP20: '-std=c++20'
    }
    config_data['additional_compile_flags'] = ['-Weverything']
    config_data['features'].supported_cpp_versions = {CppVersion.CPP17, CppVersion.CPP20}
    config_data['features'].supported_sanitizers = {"address", "memory"}
    config_data['features'].supported_optimizations = {
        OptimizationLevel.NONE, OptimizationLevel.BASIC,
        OptimizationLevel.STANDARD, OptimizationLevel.AGGRESSIVE,
        OptimizationLevel.SIZE, OptimizationLevel.FAST,
        OptimizationLevel.DEBUG
    }
    return MagicMock(spec=CompilerConfig, **config_data)


@pytest.fixture
def mock_compiler_config_msvc(mock_compiler_config_base):
    """Mock CompilerConfig for MSVC."""
    config_data = mock_compiler_config_base.copy()
    config_data['name'] = 'MSVC'
    config_data['command'] = 'C:\\VC\\cl.exe'
    config_data['compiler_type'] = CompilerType.MSVC
    config_data['version'] = '19.30.30704'
    config_data['cpp_flags'] = {
        CppVersion.CPP17: '/std:c++17',
        CppVersion.CPP20: '/std:c++20',
        CppVersion.CPP23: '/std:c++latest'
    }
    config_data['additional_compile_flags'] = ['/W4']
    config_data['features'].supported_cpp_versions = {CppVersion.CPP17, CppVersion.CPP20, CppVersion.CPP23}
    config_data['features'].supported_sanitizers = {"address"}
    config_data['features'].supported_optimizations = {
        OptimizationLevel.NONE, OptimizationLevel.BASIC,
        OptimizationLevel.STANDARD, OptimizationLevel.AGGRESSIVE
    }
    config_data['features'].supports_modules = True
    config_data['features'].supports_concepts = True
    return MagicMock(spec=CompilerConfig, **config_data)


@pytest.fixture
def compiler_instance(mock_compiler_config_gcc, mock_process_manager, mock_diagnostic_parser, mock_compiler_metrics, mocker):
    """Fixture for a Compiler instance with mocked dependencies."""
    # Patch dependencies during Compiler initialization
    with patch('tools.compiler_helper.compiler.ProcessManager', return_value=mock_process_manager), \
         patch('tools.compiler_helper.compiler.DiagnosticParser', return_value=mock_diagnostic_parser), \
         patch('tools.compiler_helper.compiler.CompilerMetrics', return_value=mock_compiler_metrics), \
         patch('os.access', return_value=True), \
         patch('pathlib.Path.exists', return_value=True): # Simulate compiler executable exists and is executable
        compiler = Compiler(mock_compiler_config_gcc)
        yield compiler


@pytest.fixture
def compiler_instance_msvc(mock_compiler_config_msvc, mock_process_manager, mock_diagnostic_parser, mock_compiler_metrics, mocker):
    """Fixture for a Compiler instance configured as MSVC."""
    with patch('tools.compiler_helper.compiler.ProcessManager', return_value=mock_process_manager), \
         patch('tools.compiler_helper.compiler.DiagnosticParser', return_value=mock_diagnostic_parser), \
         patch('tools.compiler_helper.compiler.CompilerMetrics', return_value=mock_compiler_metrics), \
         patch('os.access', return_value=True), \
         patch('pathlib.Path.exists', return_value=True):
        compiler = Compiler(mock_compiler_config_msvc)
        yield compiler

@pytest.fixture
def compiler_instance_clang(mock_compiler_config_clang, mock_process_manager, mock_diagnostic_parser, mock_compiler_metrics, mocker):
    """Fixture for a Compiler instance configured as Clang."""
    with patch('tools.compiler_helper.compiler.ProcessManager', return_value=mock_process_manager), \
         patch('tools.compiler_helper.compiler.DiagnosticParser', return_value=mock_diagnostic_parser), \
         patch('tools.compiler_helper.compiler.CompilerMetrics', return_value=mock_compiler_metrics), \
         patch('os.access', return_value=True), \
         patch('pathlib.Path.exists', return_value=True):
        compiler = Compiler(mock_compiler_config_clang)
        yield compiler


# --- Tests ---

def test_init_success(compiler_instance, mock_compiler_config_gcc, mock_process_manager, mock_diagnostic_parser, mock_compiler_metrics):
    """Test successful initialization of the Compiler."""
    assert compiler_instance.config == mock_compiler_config_gcc
    assert compiler_instance.process_manager == mock_process_manager
    assert compiler_instance.diagnostic_parser == mock_diagnostic_parser
    assert compiler_instance.metrics == mock_compiler_metrics
    # Check that _validate_compiler was called implicitly by the patches
    # os.access and Path.exists were patched to return True, simulating success


def test_init_validation_error_not_found(mock_compiler_config_gcc, mocker):
    """Test initialization fails if compiler executable is not found."""
    mocker.patch('pathlib.Path.exists', return_value=False)
    mocker.patch('os.access', return_value=True) # Still mock access just in case

    with pytest.raises(CompilerNotFoundError) as excinfo:
        Compiler(mock_compiler_config_gcc)

    assert "Compiler executable not found" in str(excinfo.value)
    assert excinfo.value.error_code == "COMPILER_NOT_FOUND"
    assert excinfo.value.compiler_path == mock_compiler_config_gcc.command


def test_init_validation_error_not_executable(mock_compiler_config_gcc, mocker):
    """Test initialization fails if compiler executable is not executable."""
    mocker.patch('pathlib.Path.exists', return_value=True)
    mocker.patch('os.access', return_value=False)

    with pytest.raises(CompilerNotFoundError) as excinfo:
        Compiler(mock_compiler_config_gcc)

    assert "Compiler is not executable" in str(excinfo.value)
    assert excinfo.value.error_code == "COMPILER_NOT_EXECUTABLE"
    assert excinfo.value.compiler_path == mock_compiler_config_gcc.command


@pytest.mark.asyncio
async def test_compile_async_success(compiler_instance, mock_process_manager, mock_diagnostic_parser, mock_compiler_metrics, tmp_path, mocker):
    """Test successful asynchronous compilation."""
    source_files = [tmp_path / "main.cpp"]
    output_file = tmp_path / "build" / "main.o"
    cpp_version = CppVersion.CPP17
    options = CompileOptions(include_paths=[tmp_path / "include"])

    # Mock ProcessManager to simulate successful command execution
    mock_process_manager.run_command_async.return_value = CommandResult(
        returncode=0, stdout="", stderr="", success=True
    )
    # Mock Path.exists for the output file check after command runs
    mocker.patch.object(Path, 'exists', side_effect=lambda: True)
    # Mock Path.mkdir to allow creating the output directory
    mocker.patch.object(Path, 'mkdir', return_value=None)

    # Mock _build_compile_command to return a predictable command
    mock_cmd = ["mock_compiler", "-c", "main.cpp", "-o", "build/main.o"]
    mocker.patch.object(compiler_instance, '_build_compile_command', new_callable=AsyncMock, return_value=mock_cmd)

    # Mock _process_compilation_result to return a successful result
    mock_compilation_result = CompilationResult(
        success=True, output_file=output_file, duration_ms=100, warnings=[], errors=[]
    )
    mocker.patch.object(compiler_instance, '_process_compilation_result', new_callable=AsyncMock, return_value=mock_compilation_result)

    result = await compiler_instance.compile_async(
        source_files=source_files,
        output_file=output_file,
        cpp_version=cpp_version,
        options=options
    )

    assert result.success is True
    assert result.output_file == output_file
    assert result.duration_ms == 100

    # Check mocks were called
    compiler_instance._build_compile_command.assert_called_once_with(
        source_files, Path(output_file), cpp_version, options
    )
    mock_process_manager.run_command_async.assert_called_once_with(mock_cmd, timeout=None)
    compiler_instance._process_compilation_result.assert_called_once() # Check args more specifically if needed
    mock_compiler_metrics.record_compilation.assert_called_once_with(True, 0.1, is_link=False) # Duration is in seconds for metrics


@pytest.mark.asyncio
async def test_compile_async_command_failure(compiler_instance, mock_process_manager, mock_diagnostic_parser, mock_compiler_metrics, tmp_path, mocker):
    """Test asynchronous compilation failing due to command error."""
    source_files = [tmp_path / "main.cpp"]
    output_file = tmp_path / "build" / "main.o"
    cpp_version = CppVersion.CPP17
    options = CompileOptions()

    # Mock ProcessManager to simulate failed command execution
    mock_process_manager.run_command_async.return_value = CommandResult(
        returncode=1, stdout="", stderr="error output", success=False
    )
    # Mock Path.exists for the output file check after command runs
    mocker.patch.object(Path, 'exists', side_effect=lambda: False) # Output file should not exist on failure
    mocker.patch.object(Path, 'mkdir', return_value=None)

    # Mock _build_compile_command
    mock_cmd = ["mock_compiler", "-c", "main.cpp", "-o", "build/main.o"]
    mocker.patch.object(compiler_instance, '_build_compile_command', new_callable=AsyncMock, return_value=mock_cmd)

    # Mock _process_compilation_result to return a failed result
    mock_compilation_result = CompilationResult(
        success=False, output_file=None, duration_ms=100, warnings=[], errors=["error output"]
    )
    mocker.patch.object(compiler_instance, '_process_compilation_result', new_callable=AsyncMock, return_value=mock_compilation_result)


    result = await compiler_instance.compile_async(
        source_files=source_files,
        output_file=output_file,
        cpp_version=cpp_version,
        options=options
    )

    assert result.success is False
    assert result.output_file is None
    assert len(result.errors) > 0
    assert "error output" in result.errors

    # Check mocks were called
    compiler_instance._build_compile_command.assert_called_once()
    mock_process_manager.run_command_async.assert_called_once()
    compiler_instance._process_compilation_result.assert_called_once()
    mock_compiler_metrics.record_compilation.assert_called_once_with(False, 0.1, is_link=False)


@pytest.mark.asyncio
async def test_compile_async_exception(compiler_instance, mock_process_manager, mock_diagnostic_parser, mock_compiler_metrics, tmp_path, mocker):
    """Test asynchronous compilation failing due to an unexpected exception."""
    source_files = [tmp_path / "main.cpp"]
    output_file = tmp_path / "build" / "main.o"
    cpp_version = CppVersion.CPP17
    options = CompileOptions()

    # Mock _build_compile_command to raise an exception
    mocker.patch.object(compiler_instance, '_build_compile_command', new_callable=AsyncMock, side_effect=Exception("Unexpected build error"))

    result = await compiler_instance.compile_async(
        source_files=source_files,
        output_file=output_file,
        cpp_version=cpp_version,
        options=options
    )

    assert result.success is False
    assert result.output_file is None
    assert len(result.errors) > 0
    assert "Compilation exception: Unexpected build error" in result.errors

    # Check mocks were called/not called as expected
    compiler_instance._build_compile_command.assert_called_once()
    mock_process_manager.run_command_async.assert_not_called()
    compiler_instance._process_compilation_result.assert_not_called()
    mock_compiler_metrics.record_compilation.assert_called_once_with(False, mocker.ANY, is_link=False) # Duration will be non-zero


def test_compile_sync(compiler_instance, mocker):
    """Test synchronous compile wrapper."""
    source_files = ["main.cpp"]
    output_file = "build/main.o"
    cpp_version = CppVersion.CPP17
    options = CompileOptions()

    # Mock asyncio.run
    mock_asyncio_run = mocker.patch('asyncio.run')
    # Mock the async method it calls
    mock_compile_async = mocker.patch.object(compiler_instance, 'compile_async', new_callable=AsyncMock)

    compiler_instance.compile(
        source_files=source_files,
        output_file=output_file,
        cpp_version=cpp_version,
        options=options
    )

    mock_asyncio_run.assert_called_once()
    # Check that asyncio.run was called with the correct coroutine
    # This is a bit tricky to check precisely, but we can check the args passed to compile_async
    mock_compile_async.assert_called_once_with(
        source_files, output_file, cpp_version, options, None
    )


@pytest.mark.asyncio
async def test_link_async_success(compiler_instance, mock_process_manager, mock_diagnostic_parser, mock_compiler_metrics, tmp_path, mocker):
    """Test successful asynchronous linking."""
    object_files = [tmp_path / "build" / "file1.o", tmp_path / "build" / "file2.o"]
    output_file = tmp_path / "app"
    options = LinkOptions(libraries=["mylib"])

    # Mock ProcessManager to simulate successful command execution
    mock_process_manager.run_command_async.return_value = CommandResult(
        returncode=0, stdout="", stderr="", success=True
    )
    # Mock Path.exists for the output file check after command runs
    mocker.patch.object(Path, 'exists', side_effect=lambda: True)
    # Mock Path.mkdir to allow creating the output directory
    mocker.patch.object(Path, 'mkdir', return_value=None)

    # Mock _build_link_command
    mock_cmd = ["mock_compiler", "build/file1.o", "build/file2.o", "-o", "app"]
    mocker.patch.object(compiler_instance, '_build_link_command', new_callable=AsyncMock, return_value=mock_cmd)

    # Mock _process_compilation_result to return a successful result
    mock_link_result = CompilationResult(
        success=True, output_file=output_file, duration_ms=200, warnings=[], errors=[]
    )
    mocker.patch.object(compiler_instance, '_process_compilation_result', new_callable=AsyncMock, return_value=mock_link_result)

    result = await compiler_instance.link_async(
        object_files=object_files,
        output_file=output_file,
        options=options
    )

    assert result.success is True
    assert result.output_file == output_file
    assert result.duration_ms == 200

    # Check mocks were called
    compiler_instance._build_link_command.assert_called_once_with(
        object_files, Path(output_file), options
    )
    mock_process_manager.run_command_async.assert_called_once_with(mock_cmd, timeout=None)
    compiler_instance._process_compilation_result.assert_called_once()
    mock_compiler_metrics.record_compilation.assert_called_once_with(True, 0.2, is_link=True)


@pytest.mark.asyncio
async def test_link_async_command_failure(compiler_instance, mock_process_manager, mock_diagnostic_parser, mock_compiler_metrics, tmp_path, mocker):
    """Test asynchronous linking failing due to command error."""
    object_files = [tmp_path / "build" / "file1.o"]
    output_file = tmp_path / "app"
    options = LinkOptions()

    # Mock ProcessManager to simulate failed command execution
    mock_process_manager.run_command_async.return_value = CommandResult(
        returncode=1, stdout="", stderr="linker error", success=False
    )
    # Mock Path.exists for the output file check after command runs
    mocker.patch.object(Path, 'exists', side_effect=lambda: False) # Output file should not exist on failure
    mocker.patch.object(Path, 'mkdir', return_value=None)

    # Mock _build_link_command
    mock_cmd = ["mock_compiler", "build/file1.o", "-o", "app"]
    mocker.patch.object(compiler_instance, '_build_link_command', new_callable=AsyncMock, return_value=mock_cmd)

    # Mock _process_compilation_result to return a failed result
    mock_link_result = CompilationResult(
        success=False, output_file=None, duration_ms=150, warnings=[], errors=["linker error"]
    )
    mocker.patch.object(compiler_instance, '_process_compilation_result', new_callable=AsyncMock, return_value=mock_link_result)

    result = await compiler_instance.link_async(
        object_files=object_files,
        output_file=output_file,
        options=options
    )

    assert result.success is False
    assert result.output_file is None
    assert len(result.errors) > 0
    assert "linker error" in result.errors

    # Check mocks were called
    compiler_instance._build_link_command.assert_called_once()
    mock_process_manager.run_command_async.assert_called_once()
    compiler_instance._process_compilation_result.assert_called_once()
    mock_compiler_metrics.record_compilation.assert_called_once_with(False, 0.15, is_link=True)


@pytest.mark.asyncio
async def test_link_async_exception(compiler_instance, mock_process_manager, mock_diagnostic_parser, mock_compiler_metrics, tmp_path, mocker):
    """Test asynchronous linking failing due to an unexpected exception."""
    object_files = [tmp_path / "build" / "file1.o"]
    output_file = tmp_path / "app"
    options = LinkOptions()

    # Mock _build_link_command to raise an exception
    mocker.patch.object(compiler_instance, '_build_link_command', new_callable=AsyncMock, side_effect=Exception("Unexpected link error"))

    result = await compiler_instance.link_async(
        object_files=object_files,
        output_file=output_file,
        options=options
    )

    assert result.success is False
    assert result.output_file is None
    assert len(result.errors) > 0
    assert "Linking exception: Unexpected link error" in result.errors

    # Check mocks were called/not called as expected
    compiler_instance._build_link_command.assert_called_once()
    mock_process_manager.run_command_async.assert_not_called()
    compiler_instance._process_compilation_result.assert_not_called()
    mock_compiler_metrics.record_compilation.assert_called_once_with(False, mocker.ANY, is_link=True)


def test_link_sync(compiler_instance, mocker):
    """Test synchronous link wrapper."""
    object_files = ["build/file1.o"]
    output_file = "app"
    options = LinkOptions()

    # Mock asyncio.run
    mock_asyncio_run = mocker.patch('asyncio.run')
    # Mock the async method it calls
    mock_link_async = mocker.patch.object(compiler_instance, 'link_async', new_callable=AsyncMock)

    compiler_instance.link(
        object_files=object_files,
        output_file=output_file,
        options=options
    )

    mock_asyncio_run.assert_called_once()
    # Check that asyncio.run was called with the correct coroutine
    mock_link_async.assert_called_once_with(
        object_files, output_file, options, None
    )


@pytest.mark.asyncio
async def test__build_compile_command_gcc(compiler_instance, tmp_path):
    """Test building compile command for GCC."""
    source_files = [tmp_path / "src" / "file1.cpp", tmp_path / "src" / "file2.cpp"]
    output_file = tmp_path / "build" / "file1.o"
    cpp_version = CppVersion.CPP20
    options = CompileOptions(
        include_paths=[tmp_path / "include", tmp_path / "libs"],
        defines={"DEBUG": None, "VERSION": "1.0"},
        warnings=["-Werror"],
        optimization=OptimizationLevel.AGGRESSIVE,
        debug=True,
        position_independent=True,
        sanitizers={"address"},
        standard_library="libc++",
        extra_flags=["-ftime-report"]
    )

    cmd = await compiler_instance._build_compile_command(
        source_files, output_file, cpp_version, options
    )

    expected_cmd_parts = [
        str(compiler_instance.config.command),
        '-std=c++20',
        f'-I{tmp_path}/include',
        f'-I{tmp_path}/libs',
        '-DDEBUG',
        '-DVERSION=1.0',
        '-Werror',
        '-O3', # AGGRESSIVE for GCC
        '-g',
        '-fPIC',
        '-fsanitize=address',
        '-stdlib=libc++',
        '-Wall', # Additional default flags
        '-Wextra',
        '-ftime-report',
        '-c',
        str(source_files[0]),
        str(source_files[1]),
        '-o',
        str(output_file)
    ]

    # Order of include paths, defines, warnings, extra flags might vary,
    # but all elements should be present.
    # A simple check for presence is sufficient here.
    assert cmd[0] == expected_cmd_parts[0] # Compiler command
    assert cmd[-2:] == expected_cmd_parts[-2:] # Output flag and file
    assert cmd[-1 - len(source_files) - 1 : -2] == expected_cmd_parts[-1 - len(source_files) - 1 : -2] # Source files
    assert '-std=c++20' in cmd
    assert f'-I{tmp_path}/include' in cmd
    assert f'-I{tmp_path}/libs' in cmd
    assert '-DDEBUG' in cmd
    assert '-DVERSION=1.0' in cmd
    assert '-Werror' in cmd
    assert '-O3' in cmd
    assert '-g' in cmd
    assert '-fPIC' in cmd
    assert '-fsanitize=address' in cmd
    assert '-stdlib=libc++' in cmd
    assert '-Wall' in cmd
    assert '-Wextra' in cmd
    assert '-ftime-report' in cmd
    assert '-c' in cmd


@pytest.mark.asyncio
async def test__build_compile_command_clang(compiler_instance_clang, tmp_path):
    """Test building compile command for Clang."""
    source_files = [tmp_path / "src" / "file.c"] # Test C file
    output_file = tmp_path / "build" / "file.o"
    cpp_version = CppVersion.CPP17 # Still use C++ version flag even for C file in this context
    options = CompileOptions(
        include_paths=[tmp_path / "headers"],
        defines={"NDEBUG": None},
        optimization=OptimizationLevel.FAST,
        debug=False,
        position_independent=False,
        sanitizers={"memory"},
        extra_flags=["-fno-exceptions"]
    )

    cmd = await compiler_instance_clang._build_compile_command(
        source_files, output_file, cpp_version, options
    )

    assert cmd[0] == str(compiler_instance_clang.config.command)
    assert cmd[-2:] == ['-o', str(output_file)]
    assert cmd[-1 - len(source_files) - 1 : -2] == [str(source_files[0])]
    assert '-std=c++17' in cmd
    assert f'-I{tmp_path}/headers' in cmd
    assert '-DNDEBUG' in cmd
    assert '-Ofast' in cmd # FAST for Clang
    assert '-g' not in cmd
    assert '-fPIC' not in cmd
    assert '-fsanitize=memory' in cmd
    assert '-stdlib=libc++' not in cmd # Default is not set
    assert '-Weverything' in cmd # Additional default flags
    assert '-fno-exceptions' in cmd
    assert '-c' in cmd


@pytest.mark.asyncio
async def test__build_compile_command_msvc(compiler_instance_msvc, tmp_path):
    """Test building compile command for MSVC."""
    source_files = [tmp_path / "src" / "file.cpp"]
    output_file = tmp_path / "build" / "file.obj" # MSVC uses .obj
    cpp_version = CppVersion.CPP23
    options = CompileOptions(
        include_paths=[tmp_path / "sdk/include"],
        defines={"WIN32": "1"},
        warnings=["/W3"],
        optimization=OptimizationLevel.SIZE,
        debug=True,
        position_independent=False, # MSVC doesn't use -fPIC
        sanitizers={"address"},
        standard_library=None, # MSVC doesn't use -stdlib
        extra_flags=["/GR-"]
    )

    cmd = await compiler_instance_msvc._build_compile_command(
        source_files, Path(output_file), cpp_version, options
    )

    assert cmd[0] == str(compiler_instance_msvc.config.command)
    assert cmd[-2:] == [f'/Fo:{output_file}', str(source_files[0])] # MSVC output flag is different
    assert '/std:c++latest' in cmd # CPP23 for MSVC
    assert f'/I{tmp_path}/sdk/include' in cmd
    assert '/DWIN32=1' in cmd
    assert '/W3' in cmd
    assert '/Os' in cmd # SIZE for MSVC
    assert '/Zi' in cmd
    assert '/fsanitize=address' in cmd
    assert '/W4' in cmd # Additional default flags
    assert '/EHsc' in cmd # Additional default flags
    assert '/GR-' in cmd
    assert '/c' in cmd


@pytest.mark.asyncio
async def test__build_compile_command_unsupported_cpp_version(compiler_instance, tmp_path):
    """Test building compile command with an unsupported C++ version."""
    source_files = [tmp_path / "main.cpp"]
    output_file = tmp_path / "build" / "main.o"
    # Assume compiler_instance (GCC mock) only supports C++17 and C++20
    unsupported_version = CppVersion.CPP11
    options = CompileOptions()

    with pytest.raises(CompilationError) as excinfo:
        await compiler_instance._build_compile_command(
            source_files, output_file, unsupported_version, options
        )

    assert "Unsupported C++ version: c++11." in str(excinfo.value)
    assert excinfo.value.error_code == "UNSUPPORTED_CPP_VERSION"
    assert excinfo.value.cpp_version == "c++11"
    assert set(excinfo.value.supported_versions) == {CppVersion.CPP17, CppVersion.CPP20, CppVersion.CPP23} # Based on mock_compiler_config_gcc


@pytest.mark.asyncio
async def test__build_link_command_gcc(compiler_instance, tmp_path):
    """Test building link command for GCC."""
    object_files = [tmp_path / "build" / "file1.o", tmp_path / "build" / "file2.o"]
    output_file = tmp_path / "app"
    options = LinkOptions(
        shared=False,
        static=True,
        library_paths=[tmp_path / "lib"],
        runtime_library_paths=[tmp_path / "runtime_lib"],
        libraries=["pthread", "m"],
        strip_symbols=True,
        generate_map=True,
        map_file=tmp_path / "app.map",
        extra_flags=["-v"]
    )

    # Mock platform.system for runtime library path test
    mocker = MagicMock()
    mocker.patch('platform.system', return_value='Linux')

    cmd = await compiler_instance._build_link_command(
        object_files, output_file, options
    )

    expected_cmd_parts = [
        str(compiler_instance.config.command),
        '-static',
        f'-L{tmp_path}/lib',
        f'-Wl,-rpath={tmp_path}/runtime_lib', # Linux rpath
        '-lpthread',
        '-lm',
        '-s',
        f'-Wl,-Map={tmp_path}/app.map',
        '-v',
        str(object_files[0]),
        str(object_files[1]),
        '-o',
        str(output_file)
    ]

    # Check presence of key flags
    assert cmd[0] == expected_cmd_parts[0]
    assert cmd[-2:] == expected_cmd_parts[-2:]
    assert cmd[-1 - len(object_files) - 1 : -2] == [str(f) for f in object_files]
    assert '-static' in cmd
    assert f'-L{tmp_path}/lib' in cmd
    assert f'-Wl,-rpath={tmp_path}/runtime_lib' in cmd
    assert '-lpthread' in cmd
    assert '-lm' in cmd
    assert '-s' in cmd
    assert f'-Wl,-Map={tmp_path}/app.map' in cmd
    assert '-v' in cmd
    assert '-shared' not in cmd # Not shared


@pytest.mark.asyncio
async def test__build_link_command_gcc_shared_darwin(compiler_instance, tmp_path, mocker):
    """Test building shared link command for GCC on Darwin (macOS)."""
    object_files = [tmp_path / "build" / "file1.o"]
    output_file = tmp_path / "libmylib.dylib" # macOS shared lib extension
    options = LinkOptions(
        shared=True,
        runtime_library_paths=[tmp_path / "runtime_lib_mac"]
    )

    # Mock platform.system for runtime library path test
    mocker.patch('platform.system', return_value='Darwin')

    cmd = await compiler_instance._build_link_command(
        object_files, output_file, options
    )

    assert cmd[0] == str(compiler_instance.config.command)
    assert '-shared' in cmd
    assert f'-Wl,-rpath,{tmp_path}/runtime_lib_mac' in cmd # Darwin rpath format


@pytest.mark.asyncio
async def test__build_link_command_msvc(compiler_instance_msvc, tmp_path):
    """Test building link command for MSVC."""
    object_files = [tmp_path / "build" / "file1.obj", tmp_path / "build" / "file2.obj"]
    output_file = tmp_path / "app.exe"
    options = LinkOptions(
        shared=False,
        static=False, # MSVC static linking is default or via runtime lib flags
        library_paths=[tmp_path / "sdk/lib"],
        runtime_library_paths=[], # MSVC doesn't use rpath flags like GCC/Clang
        libraries=["kernel32", "user32"],
        strip_symbols=False, # MSVC uses /DEBUG:NO to strip debug info
        generate_map=True,
        map_file=tmp_path / "app.map",
        extra_flags=["/SUBSYSTEM:CONSOLE"]
    )

    cmd = await compiler_instance_msvc._build_link_command(
        object_files, Path(output_file), options
    )

    assert cmd[0] == str(compiler_instance_msvc.config.command)
    assert cmd[-1] == str(output_file) # Output file is last for MSVC /OUT
    assert cmd[-2] == f'/OUT:{output_file}'
    assert cmd[-3 - len(object_files) : -2] == [str(f) for f in object_files] # Object files before output
    assert '/LIBPATH:' + str(tmp_path / "sdk/lib") in cmd
    assert 'kernel32.lib' in cmd
    assert 'user32.lib' in cmd
    assert f'/MAP:{tmp_path}/app.map' in cmd
    assert '/SUBSYSTEM:CONSOLE' in cmd
    assert '/DLL' not in cmd # Not shared


@pytest.mark.asyncio
async def test__build_link_command_msvc_shared(compiler_instance_msvc, tmp_path):
    """Test building shared link command for MSVC."""
    object_files = [tmp_path / "build" / "file1.obj"]
    output_file = tmp_path / "mylib.dll" # MSVC shared lib extension
    options = LinkOptions(shared=True)

    cmd = await compiler_instance_msvc._build_link_command(
        object_files, Path(output_file), options
    )

    assert cmd[0] == str(compiler_instance_msvc.config.command)
    assert '/DLL' in cmd


@pytest.mark.asyncio
async def test__process_compilation_result_success(compiler_instance, mock_diagnostic_parser, tmp_path):
    """Test processing a successful command result."""
    output_file = tmp_path / "build" / "main.o"
    output_file.parent.mkdir(parents=True, exist_ok=True)
    output_file.touch() # Simulate output file exists

    cmd_result = CommandResult(
        returncode=0, stdout="Success", stderr="Warnings here", success=True
    )
    command = ["mock_compiler", "main.cpp"]
    start_time = time.time() - 0.1 # Simulate 100ms duration

    mock_diagnostic_parser.parse_diagnostics.return_value = (
        [], # errors
        ["Warning: something"], # warnings
        [] # notes
    )

    result = await compiler_instance._process_compilation_result(
        cmd_result, output_file, command, start_time
    )

    assert result.success is True
    assert result.output_file == output_file
    assert result.duration_ms >= 100 # Should be around 100ms
    assert result.command_line == command
    assert result.errors == []
    assert result.warnings == ["Warning: something"]
    assert result.notes == []
    mock_diagnostic_parser.parse_diagnostics.assert_called_once_with("Warnings here")


@pytest.mark.asyncio
async def test__process_compilation_result_failure_with_errors(compiler_instance, mock_diagnostic_parser, tmp_path):
    """Test processing a failed command result with parsed errors."""
    output_file = tmp_path / "build" / "main.o"
    # Don't simulate output file creation

    cmd_result = CommandResult(
        returncode=1, stdout="", stderr="Error: syntax error", success=False
    )
    command = ["mock_compiler", "main.cpp"]
    start_time = time.time() - 0.05 # Simulate 50ms duration

    mock_diagnostic_parser.parse_diagnostics.return_value = (
        ["Error: syntax error"], # errors
        [], # warnings
        [] # notes
    )

    result = await compiler_instance._process_compilation_result(
        cmd_result, output_file, command, start_time
    )

    assert result.success is False
    assert result.output_file is None
    assert result.duration_ms >= 50
    assert result.command_line == command
    assert result.errors == ["Error: syntax error"]
    assert result.warnings == []
    assert result.notes == []
    mock_diagnostic_parser.parse_diagnostics.assert_called_once_with("Error: syntax error")


@pytest.mark.asyncio
async def test__process_compilation_result_failure_no_parsed_errors(compiler_instance, mock_diagnostic_parser, tmp_path):
    """Test processing a failed command result with stderr but no parsed errors."""
    output_file = tmp_path / "build" / "main.o"
    # Don't simulate output file creation

    cmd_result = CommandResult(
        returncode=1, stdout="", stderr="Some unexpected output on stderr", success=False
    )
    command = ["mock_compiler", "main.cpp"]
    start_time = time.time() - 0.07 # Simulate 70ms duration

    mock_diagnostic_parser.parse_diagnostics.return_value = (
        [], # errors (parser failed to find known patterns)
        [], # warnings
        [] # notes
    )

    result = await compiler_instance._process_compilation_result(
        cmd_result, output_file, command, start_time
    )

    assert result.success is False
    assert result.output_file is None
    assert result.duration_ms >= 70
    assert result.command_line == command
    assert len(result.errors) == 1
    assert "Compilation failed: Some unexpected output on stderr" in result.errors
    assert result.warnings == []
    assert result.notes == []
    mock_diagnostic_parser.parse_diagnostics.assert_called_once_with("Some unexpected output on stderr")


@pytest.mark.asyncio
async def test_get_version_info_async_gcc(compiler_instance, mock_process_manager):
    """Test getting version info for GCC."""
    mock_process_manager.run_command_async.return_value = CommandResult(
        returncode=0, stdout="g++ (Ubuntu 11.3.0-1ubuntu1~22.04) 11.3.0\nCopyright (C) 2021 Free Software Foundation, Inc.", stderr="", success=True
    )
    compiler_instance.config.compiler_type = CompilerType.GCC
    compiler_instance.config.command = "/usr/bin/g++"

    version_info = await compiler_instance.get_version_info_async()

    assert version_info["version"] == "g++ (Ubuntu 11.3.0-1ubuntu1~22.04) 11.3.0"
    assert "Copyright (C) 2021" in version_info["full_output"]
    mock_process_manager.run_command_async.assert_called_once_with(["/usr/bin/g++", "--version"])


@pytest.mark.asyncio
async def test_get_version_info_async_msvc(compiler_instance_msvc, mock_process_manager):
    """Test getting version info for MSVC."""
    mock_process_manager.run_command_async.return_value = CommandResult(
        returncode=0, stdout="", stderr="Microsoft (R) C/C++ Optimizing Compiler Version 19.35.32215 for x64\n", success=True
    )
    compiler_instance_msvc.config.compiler_type = CompilerType.MSVC
    compiler_instance_msvc.config.command = "C:\\VC\\cl.exe"

    version_info = await compiler_instance_msvc.get_version_info_async()

    assert version_info["version"] == "Microsoft (R) C/C++ Optimizing Compiler Version 19.35.32215 for x64"
    assert "Version 19.35.32215" in version_info["full_output"] # MSVC puts output on stderr
    mock_process_manager.run_command_async.assert_called_once_with(["C:\\VC\\cl.exe", "/Bv"])


@pytest.mark.asyncio
async def test_get_version_info_async_failure(compiler_instance, mock_process_manager):
    """Test getting version info fails."""
    mock_process_manager.run_command_async.return_value = CommandResult(
        returncode=1, stdout="", stderr="command not found", success=False
    )
    compiler_instance.config.compiler_type = CompilerType.GCC
    compiler_instance.config.command = "/usr/bin/non_existent_compiler"

    version_info = await compiler_instance.get_version_info_async()

    assert version_info["version"] == "unknown"
    assert version_info["error"] == "command not found"
    mock_process_manager.run_command_async.assert_called_once_with(["/usr/bin/non_existent_compiler", "--version"])


def test_get_version_info_sync(compiler_instance, mocker):
    """Test synchronous version info wrapper."""
    mock_asyncio_run = mocker.patch('asyncio.run')
    mock_get_version_info_async = mocker.patch.object(compiler_instance, 'get_version_info_async', new_callable=AsyncMock)

    compiler_instance.get_version_info()

    mock_asyncio_run.assert_called_once()
    mock_get_version_info_async.assert_called_once()


def test_metrics_tracking(compiler_instance):
    """Test metrics recording and retrieval."""
    metrics = compiler_instance.metrics # Get the mock metrics object

    # Simulate successful compilation
    compiler_instance.metrics.record_compilation(True, 0.15, is_link=False)
    metrics.total_compilations += 1
    metrics.successful_compilations += 1
    metrics.total_compilation_time += 0.15

    # Simulate failed compilation
    compiler_instance.metrics.record_compilation(False, 0.08, is_link=False)
    metrics.total_compilations += 1
    metrics.total_compilation_time += 0.08 # Still add time even if failed

    # Simulate successful linking
    compiler_instance.metrics.record_compilation(True, 0.3, is_link=True)
    metrics.total_compilations += 1
    metrics.successful_compilations += 1
    metrics.total_link_time += 0.3

    # Simulate cache hit/miss
    compiler_instance.metrics.record_cache_hit()
    metrics.cache_hits += 1
    compiler_instance.metrics.record_cache_miss()
    metrics.cache_misses += 1

    # Check metrics object state (based on how the mock was called)
    assert metrics.record_compilation.call_count == 3
    assert metrics.record_cache_hit.call_count == 1
    assert metrics.record_cache_miss.call_count == 1

    # Test get_metrics calls the mock's to_dict
    compiler_instance.get_metrics()
    metrics.to_dict.assert_called_once()

    # Test reset_metrics
    compiler_instance.reset_metrics()
    # Check that a new metrics object was created (or the mock was reset)
    # Since we patched the class, a new mock instance is created
    assert compiler_instance.metrics != metrics # Should be a new mock object


def test_diagnostic_parser_gcc_clang():
    """Test DiagnosticParser for GCC/Clang format."""
    parser = DiagnosticParser(CompilerType.GCC)
    output = """
/path/to/file1.cpp:10:5: error: expected ';' after expression
/path/to/file2.cpp:25:10: warning: unused variable 'x' [-Wunused-variable]
/path/to/file1.cpp:11:6: note: in expansion of macro 'MY_MACRO'
another line of output
/path/to/file3.cpp:5:1: error: use of undeclared identifier 'y'
"""
    errors, warnings, notes = parser.parse_diagnostics(output)

    assert errors == [
        "/path/to/file1.cpp:10:5: error: expected ';' after expression",
        "/path/to/file3.cpp:5:1: error: use of undeclared identifier 'y'"
    ]
    assert warnings == [
        "/path/to/file2.cpp:25:10: warning: unused variable 'x' [-Wunused-variable]"
    ]
    assert notes == [
        "/path/to/file1.cpp:11:6: note: in expansion of macro 'MY_MACRO'"
    ]


def test_diagnostic_parser_msvc():
    """Test DiagnosticParser for MSVC format."""
    parser = DiagnosticParser(CompilerType.MSVC)
    output = """
Microsoft (R) C/C++ Optimizing Compiler Version 19.35.32215 for x64
Copyright (C) Microsoft Corporation. All rights reserved.

file1.cpp(10) : error C2059: syntax error: ';'
file2.cpp(25,10) : warning C4189: 'x': local variable is initialized but not referenced
file1.cpp(11) : note: see expansion of macro 'MY_MACRO'
file3.cpp(5) : error C3861: 'y': identifier not found
"""
    errors, warnings, notes = parser.parse_diagnostics(output)

    assert errors == [
        "file1.cpp(10) : error C2059: syntax error: ';'",
        "file3.cpp(5) : error C3861: 'y': identifier not found"
    ]
    assert warnings == [
        "file2.cpp(25,10) : warning C4189: 'x': local variable is initialized but not referenced"
    ]
    assert notes == [
        "file1.cpp(11) : note: see expansion of macro 'MY_MACRO'"
    ]