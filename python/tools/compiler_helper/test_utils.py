import asyncio
import json
import os
import platform
import shutil
import subprocess
import tempfile
from contextlib import asynccontextmanager, contextmanager
from pathlib import Path
from unittest.mock import AsyncMock, MagicMock, patch
import pytest
from .core_types import CommandResult, CompilerException
from pydantic import BaseModel, ValidationError


# Use relative imports as the directory is a package
from .utils import (
    ConfigurationManager,
    FileManager,
    ProcessManager,
    SystemInfo,
    FileOperationError,
    load_json,
    save_json,
)


# --- Fixtures ---


@pytest.fixture
def process_manager():
    """Fixture for a ProcessManager instance."""
    return ProcessManager()


@pytest.fixture
def file_manager():
    """Fixture for a FileManager instance."""
    return FileManager()


@pytest.fixture
def config_manager(tmp_path):
    """Fixture for a ConfigurationManager instance with a temporary config directory."""
    config_dir = tmp_path / "config"
    return ConfigurationManager(config_dir=config_dir)


@pytest.fixture
def mock_subprocess_run(mocker):
    """Fixture to mock subprocess.run."""
    mock_run = mocker.patch("subprocess.run")
    # Default successful result
    mock_run.return_value = MagicMock(
        returncode=0, stdout=b"mock stdout", stderr=b"mock stderr"
    )
    return mock_run


@pytest.fixture
def mock_asyncio_subprocess_exec(mocker):
    """Fixture to mock asyncio.create_subprocess_exec."""
    mock_exec = mocker.patch("asyncio.create_subprocess_exec", new_callable=AsyncMock)
    # Default successful process mock
    mock_process = AsyncMock()
    mock_process.returncode = 0
    mock_process.communicate.return_value = (b"mock stdout async", b"mock stderr async")
    mock_exec.return_value = mock_process
    return mock_exec


# --- Tests for ProcessManager ---


@pytest.mark.asyncio
async def test_run_command_async_success(process_manager, mock_asyncio_subprocess_exec):
    """Test successful asynchronous command execution."""
    command = ["echo", "hello"]
    result = await process_manager.run_command_async(command)

    mock_asyncio_subprocess_exec.assert_called_once_with(
        *command,
        stdout=asyncio.subprocess.PIPE,
        stderr=asyncio.subprocess.PIPE,
        stdin=None,
        cwd=None,
        env=os.environ.copy(),  # Check default env is used
    )
    mock_asyncio_subprocess_exec.return_value.communicate.assert_called_once_with(
        input=None
    )

    assert result.success is True
    assert result.return_code == 0
    assert result.stdout == "mock stdout async"
    assert result.stderr == "mock stderr async"
    assert result.command == command
    assert result.execution_time > 0
    assert result.output == "mock stdout async\nmock stderr async"
    assert result.failed is False


@pytest.mark.asyncio
async def test_run_command_async_failure(process_manager, mock_asyncio_subprocess_exec):
    """Test asynchronous command execution failure."""
    command = ["false"]
    mock_asyncio_subprocess_exec.return_value.returncode = 1
    mock_asyncio_subprocess_exec.return_value.communicate.return_value = (
        b"",
        b"mock error output",
    )

    result = await process_manager.run_command_async(command)

    assert result.success is False
    assert result.return_code == 1
    assert result.stdout == ""
    assert result.stderr == "mock error output"
    assert result.command == command
    assert result.execution_time > 0
    assert result.failed is True


@pytest.mark.asyncio
async def test_run_command_async_timeout(process_manager, mock_asyncio_subprocess_exec):
    """Test asynchronous command timeout."""
    command = ["sleep", "10"]
    mock_asyncio_subprocess_exec.return_value.communicate.side_effect = (
        asyncio.TimeoutError
    )

    result = await process_manager.run_command_async(command, timeout=1)

    mock_asyncio_subprocess_exec.return_value.kill.assert_called_once()
    mock_asyncio_subprocess_exec.return_value.wait.assert_called_once()

    assert result.success is False
    assert (
        result.return_code == -1
    )  # Or whatever the killed process returns, but -1 is a safe mock
    assert "Command timed out after 1s" in result.stderr
    assert result.command == command
    assert result.execution_time > 0
    assert result.failed is True


@pytest.mark.asyncio
async def test_run_command_async_command_not_found(
    process_manager, mock_asyncio_subprocess_exec
):
    """Test asynchronous command not found error."""
    command = ["non_existent_command"]
    mock_asyncio_subprocess_exec.side_effect = FileNotFoundError

    result = await process_manager.run_command_async(command)

    assert result.success is False
    assert result.return_code == -1
    assert "Command not found: non_existent_command" in result.stderr
    assert result.command == command
    assert result.execution_time > 0
    assert result.failed is True


@pytest.mark.asyncio
async def test_run_command_async_unexpected_exception(
    process_manager, mock_asyncio_subprocess_exec
):
    """Test asynchronous command execution with an unexpected exception."""
    command = ["echo", "hello"]
    mock_asyncio_subprocess_exec.side_effect = Exception("Something went wrong")

    result = await process_manager.run_command_async(command)

    assert result.success is False
    assert result.return_code == -1
    assert "Unexpected error: Something went wrong" in result.stderr
    assert result.command == command
    assert result.execution_time > 0
    assert result.failed is True


@pytest.mark.asyncio
async def test_run_command_async_with_cwd(
    process_manager, mock_asyncio_subprocess_exec, tmp_path
):
    """Test asynchronous command execution with a specified working directory."""
    command = ["ls"]
    cwd = tmp_path / "test_dir"
    cwd.mkdir()

    await process_manager.run_command_async(command, cwd=cwd)

    mock_asyncio_subprocess_exec.assert_called_once_with(
        *command,
        stdout=asyncio.subprocess.PIPE,
        stderr=asyncio.subprocess.PIPE,
        stdin=None,
        cwd=str(cwd),  # cwd is passed as string
        env=os.environ.copy(),
    )


@pytest.mark.asyncio
async def test_run_command_async_with_env(
    process_manager, mock_asyncio_subprocess_exec
):
    """Test asynchronous command execution with custom environment variables."""
    command = ["printenv", "MY_VAR"]
    custom_env = {"MY_VAR": "my_value"}

    await process_manager.run_command_async(command, env=custom_env)

    expected_env = os.environ.copy()
    expected_env.update(custom_env)

    mock_asyncio_subprocess_exec.assert_called_once_with(
        *command,
        stdout=asyncio.subprocess.PIPE,
        stderr=asyncio.subprocess.PIPE,
        stdin=None,
        cwd=None,
        env=expected_env,
    )


@pytest.mark.asyncio
async def test_run_command_async_with_input(
    process_manager, mock_asyncio_subprocess_exec
):
    """Test asynchronous command execution with input data."""
    command = ["cat"]
    input_data = b"input data"

    await process_manager.run_command_async(command, input_data=input_data)

    mock_asyncio_subprocess_exec.assert_called_once_with(
        *command,
        stdout=asyncio.subprocess.PIPE,
        stderr=asyncio.subprocess.PIPE,
        stdin=asyncio.subprocess.PIPE,  # stdin should be PIPE
        cwd=None,
        env=os.environ.copy(),
    )
    mock_asyncio_subprocess_exec.return_value.communicate.assert_called_once_with(
        input=input_data
    )


def test_run_command_sync_success(process_manager, mock_subprocess_run):
    """Test successful synchronous command execution."""
    command = ["echo", "hello"]
    result = process_manager.run_command(command)

    mock_subprocess_run.assert_called_once_with(
        command,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        input=None,
        timeout=None,
        cwd=None,
        env=os.environ.copy(),  # Check default env is used
        text=False,  # Should be False as per implementation
    )

    assert result.success is True
    assert result.return_code == 0
    assert result.stdout == "mock stdout"
    assert result.stderr == "mock stderr"
    assert result.command == command
    assert result.execution_time > 0
    assert result.output == "mock stdout\nmock stderr"
    assert result.failed is False


def test_run_command_sync_failure(process_manager, mock_subprocess_run):
    """Test synchronous command execution failure."""
    command = ["false"]
    mock_subprocess_run.return_value.returncode = 1
    mock_subprocess_run.return_value.stdout = b""
    mock_subprocess_run.return_value.stderr = b"mock error output sync"

    result = process_manager.run_command(command)

    assert result.success is False
    assert result.return_code == 1
    assert result.stdout == ""
    assert result.stderr == "mock error output sync"
    assert result.command == command
    assert result.execution_time > 0
    assert result.failed is True


def test_run_command_sync_timeout(process_manager, mock_subprocess_run):
    """Test synchronous command timeout."""
    command = ["sleep", "10"]
    # Fix: Remove stdout and stderr args from TimeoutExpired
    mock_subprocess_run.side_effect = subprocess.TimeoutExpired(cmd=command, timeout=1)

    result = process_manager.run_command(command, timeout=1)

    assert result.success is False
    assert result.return_code == -1
    assert "Command timed out after 1s" in result.stderr
    assert result.command == command
    assert result.execution_time > 0
    assert result.failed is True


def test_run_command_sync_command_not_found(process_manager, mock_subprocess_run):
    """Test synchronous command not found error."""
    command = ["non_existent_command"]
    mock_subprocess_run.side_effect = FileNotFoundError

    result = process_manager.run_command(command)

    assert result.success is False
    assert result.return_code == -1
    assert "Command not found: non_existent_command" in result.stderr
    assert result.command == command
    assert result.execution_time > 0
    assert result.failed is True


def test_run_command_sync_unexpected_exception(process_manager, mock_subprocess_run):
    """Test synchronous command execution with an unexpected exception."""
    command = ["echo", "hello"]
    mock_subprocess_run.side_effect = Exception("Something went wrong sync")

    result = process_manager.run_command(command)

    assert result.success is False
    assert result.return_code == -1
    assert "Unexpected error: Something went wrong sync" in result.stderr
    assert result.command == command
    assert result.execution_time > 0
    assert result.failed is True


def test_run_command_sync_with_cwd(process_manager, mock_subprocess_run, tmp_path):
    """Test synchronous command execution with a specified working directory."""
    command = ["ls"]
    cwd = tmp_path / "test_dir_sync"
    cwd.mkdir()

    process_manager.run_command(command, cwd=cwd)

    mock_subprocess_run.assert_called_once_with(
        command,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        input=None,
        timeout=None,
        cwd=str(cwd),  # cwd is passed as string
        env=os.environ.copy(),
        text=False,
    )


def test_run_command_sync_with_env(process_manager, mock_subprocess_run):
    """Test synchronous command execution with custom environment variables."""
    command = ["printenv", "MY_VAR_SYNC"]
    custom_env = {"MY_VAR_SYNC": "my_value_sync"}

    process_manager.run_command(command, env=custom_env)

    expected_env = os.environ.copy()
    expected_env.update(custom_env)

    mock_subprocess_run.assert_called_once_with(
        command,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        input=None,
        timeout=None,
        cwd=None,
        env=expected_env,
        text=False,
    )


def test_run_command_sync_with_input(process_manager, mock_subprocess_run):
    """Test synchronous command execution with input data."""
    command = ["cat"]
    input_data = b"input data sync"

    process_manager.run_command(command, input_data=input_data)

    mock_subprocess_run.assert_called_once_with(
        command,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        input=input_data,  # input data is passed directly
        timeout=None,
        cwd=None,
        env=os.environ.copy(),
        text=False,
    )


# --- Tests for FileManager ---


def test_temporary_directory_context_manager(file_manager):
    """Test synchronous temporary_directory context manager."""
    initial_temp_dir_count = len(os.listdir(tempfile.gettempdir()))
    temp_dir_path = None
    with file_manager.temporary_directory() as temp_dir:
        temp_dir_path = temp_dir
        assert temp_dir.is_dir()
        assert temp_dir.name.startswith("compiler_helper_")
        # Create a file inside
        (temp_dir / "test_file.txt").touch()
        assert (temp_dir / "test_file.txt").exists()

    # After exiting the context, the directory should be removed
    assert temp_dir_path is not None
    assert not temp_dir_path.exists()
    # Check that the number of items in the temp dir is back to normal (approx)
    # This is not a perfect check due to other processes, but gives some confidence
    assert (
        len(os.listdir(tempfile.gettempdir())) <= initial_temp_dir_count + 1
    )  # Allow for slight variations


@pytest.mark.asyncio
async def test_temporary_directory_async_context_manager(file_manager):
    """Test asynchronous temporary_directory_async context manager."""
    initial_temp_dir_count = len(os.listdir(tempfile.gettempdir()))
    temp_dir_path = None
    async with file_manager.temporary_directory_async() as temp_dir:
        temp_dir_path = temp_dir
        assert temp_dir.is_dir()
        assert temp_dir.name.startswith("compiler_helper_")
        # Create a file inside
        (temp_dir / "test_file_async.txt").touch()
        assert (temp_dir / "test_file_async.txt").exists()

    # After exiting the context, the directory should be removed
    assert temp_dir_path is not None
    assert not temp_dir_path.exists()
    assert len(os.listdir(tempfile.gettempdir())) <= initial_temp_dir_count + 1


def test_ensure_directory_exists(file_manager, tmp_path):
    """Test ensure_directory when the directory already exists."""
    existing_dir = tmp_path / "existing"
    existing_dir.mkdir()
    assert existing_dir.is_dir()

    returned_path = file_manager.ensure_directory(existing_dir)

    assert returned_path == existing_dir
    assert returned_path.is_dir()  # Still exists
    # Check permissions if needed, but default is usually fine


def test_ensure_directory_creates_new(file_manager, tmp_path):
    """Test ensure_directory when the directory needs to be created."""
    new_dir = tmp_path / "new" / "subdir"
    assert not new_dir.exists()

    returned_path = file_manager.ensure_directory(new_dir)

    assert returned_path == new_dir
    assert returned_path.is_dir()
    assert new_dir.parent.is_dir()  # Parent should also be created


def test_safe_copy_success(file_manager, tmp_path):
    """Test safe_copy for successful file copy."""
    src_file = tmp_path / "source.txt"
    src_file.write_text("hello world")
    dst_file = tmp_path / "dest" / "copied_source.txt"

    assert src_file.exists()
    assert not dst_file.exists()

    file_manager.safe_copy(src_file, dst_file)

    assert dst_file.exists()
    assert dst_file.read_text() == "hello world"
    assert dst_file.parent.is_dir()  # Destination directory should be created


def test_safe_copy_source_not_found(file_manager, tmp_path):
    """Test safe_copy when the source file does not exist."""
    src_file = tmp_path / "non_existent_source.txt"
    dst_file = tmp_path / "dest" / "copied_source.txt"

    assert not src_file.exists()

    with pytest.raises(FileOperationError) as excinfo:
        file_manager.safe_copy(src_file, dst_file)

    assert "Source file does not exist:" in str(excinfo.value)
    assert excinfo.value.error_code == "SOURCE_NOT_FOUND"
    # Fix: Access context dictionary
    assert excinfo.value.context["source"] == str(src_file)
    assert not dst_file.exists()  # Destination should not be created


def test_safe_copy_os_error(file_manager, tmp_path, mocker):
    """Test safe_copy when an OSError occurs during copy."""
    src_file = tmp_path / "source.txt"
    src_file.write_text("hello world")
    dst_file = tmp_path / "dest" / "copied_source.txt"

    # Mock shutil.copy2 to raise an OSError
    mocker.patch("shutil.copy2", side_effect=OSError("Mock copy error"))

    with pytest.raises(FileOperationError) as excinfo:
        file_manager.safe_copy(src_file, dst_file)

    assert "Failed to copy" in str(excinfo.value)
    assert excinfo.value.error_code == "COPY_FAILED"
    # Fix: Access context dictionary
    assert excinfo.value.context["source"] == str(src_file)
    # Fix: Access context dictionary
    assert excinfo.value.context["destination"] == str(dst_file)
    # Fix: Access context dictionary
    assert excinfo.value.context["os_error"] == "Mock copy error"


def test_get_file_info_exists(file_manager, tmp_path):
    """Test get_file_info for an existing file."""
    test_file = tmp_path / "info_test.txt"
    test_file.write_text("some content")
    os.chmod(test_file, 0o755)  # Make it executable for the test

    info = file_manager.get_file_info(test_file)

    assert info["exists"] is True
    assert info["is_file"] is True
    assert info["is_dir"] is False
    assert info["is_symlink"] is False
    assert info["size"] == len("some content")
    assert info["permissions"] == "755"
    assert info["is_executable"] is True
    assert isinstance(info["modified_time"], (int, float))
    assert isinstance(info["created_time"], (int, float))


def test_get_file_info_not_exists(file_manager, tmp_path):
    """Test get_file_info for a non-existent file."""
    test_file = tmp_path / "non_existent_info.txt"

    info = file_manager.get_file_info(test_file)

    assert info["exists"] is False
    assert len(info) == 1  # Only 'exists' key should be present


def test_get_file_info_directory(file_manager, tmp_path):
    """Test get_file_info for a directory."""
    test_dir = tmp_path / "info_dir"
    test_dir.mkdir()

    info = file_manager.get_file_info(test_dir)

    assert info["exists"] is True
    assert info["is_file"] is False
    assert info["is_dir"] is True
    # Other fields like size, times, permissions might vary or be zero depending on OS/FS
    assert "size" in info
    assert "permissions" in info
    assert "is_executable" in info  # Directories can be executable (searchable)


# --- Tests for ConfigurationManager ---


@pytest.mark.asyncio
async def test_load_json_async_success(config_manager, tmp_path):
    """Test asynchronous loading of a valid JSON file."""
    json_data = {"key": "value", "number": 123}
    json_file = tmp_path / "config.json"
    json_file.write_text(json.dumps(json_data))

    loaded_data = await config_manager.load_json_async(json_file)

    assert loaded_data == json_data


@pytest.mark.asyncio
async def test_load_json_async_file_not_found(config_manager, tmp_path):
    """Test asynchronous loading when JSON file is not found."""
    json_file = tmp_path / "non_existent_config.json"

    with pytest.raises(FileOperationError) as excinfo:
        await config_manager.load_json_async(json_file)

    assert "JSON file not found:" in str(excinfo.value)
    assert excinfo.value.error_code == "FILE_NOT_FOUND"
    # Fix: Access context dictionary
    assert excinfo.value.context["file_path"] == str(json_file)


@pytest.mark.asyncio
async def test_load_json_async_invalid_json(config_manager, tmp_path):
    """Test asynchronous loading when JSON file contains invalid JSON."""
    json_file = tmp_path / "invalid.json"
    json_file.write_text("this is not json")

    with pytest.raises(FileOperationError) as excinfo:
        await config_manager.load_json_async(json_file)

    assert "Invalid JSON in file" in str(excinfo.value)
    assert excinfo.value.error_code == "INVALID_JSON"
    # Fix: Access context dictionary
    assert excinfo.value.context["file_path"] == str(json_file)
    assert "json_error" in excinfo.value.context


@pytest.mark.asyncio
async def test_load_json_async_os_error(config_manager, tmp_path, mocker):
    """Test asynchronous loading when an OSError occurs during file read."""
    json_file = tmp_path / "readable.json"
    json_file.write_text("{}")

    # Mock aiofiles.open to raise an OSError
    mocker.patch("aiofiles.open", side_effect=OSError("Mock read error"))

    with pytest.raises(FileOperationError) as excinfo:
        await config_manager.load_json_async(json_file)

    assert "Failed to read file" in str(excinfo.value)
    assert excinfo.value.error_code == "FILE_READ_ERROR"
    # Fix: Access context dictionary
    assert excinfo.value.context["file_path"] == str(json_file)
    # Fix: Access context dictionary
    assert excinfo.value.context["os_error"] == "Mock read error"


def test_load_json_sync_success(config_manager, tmp_path):
    """Test synchronous loading of a valid JSON file."""
    json_data = {"sync_key": "sync_value"}
    json_file = tmp_path / "config_sync.json"
    json_file.write_text(json.dumps(json_data))

    loaded_data = config_manager.load_json(json_file)

    assert loaded_data == json_data


def test_load_json_sync_file_not_found(config_manager, tmp_path):
    """Test synchronous loading when JSON file is not found."""
    json_file = tmp_path / "non_existent_config_sync.json"

    with pytest.raises(FileOperationError) as excinfo:
        config_manager.load_json(json_file)

    assert "JSON file not found:" in str(excinfo.value)
    assert excinfo.value.error_code == "FILE_NOT_FOUND"
    # Fix: Access context dictionary
    assert excinfo.value.context["file_path"] == str(json_file)


def test_load_json_sync_invalid_json(config_manager, tmp_path):
    """Test synchronous loading when JSON file contains invalid JSON."""
    json_file = tmp_path / "invalid_sync.json"
    json_file.write_text("this is not json")

    with pytest.raises(FileOperationError) as excinfo:
        config_manager.load_json(json_file)

    assert "Invalid JSON in file" in str(excinfo.value)
    assert excinfo.value.error_code == "INVALID_JSON"
    # Fix: Access context dictionary
    assert excinfo.value.context["file_path"] == str(json_file)
    assert "json_error" in excinfo.value.context


def test_load_json_sync_os_error(config_manager, tmp_path, mocker):
    """Test synchronous loading when an OSError occurs during file read."""
    json_file = tmp_path / "readable_sync.json"
    json_file.write_text("{}")

    # Mock Path.open to raise an OSError
    mocker.patch.object(Path, "open", side_effect=OSError("Mock read error sync"))

    with pytest.raises(FileOperationError) as excinfo:
        config_manager.load_json(json_file)

    assert "Failed to read file" in str(excinfo.value)
    assert excinfo.value.error_code == "FILE_READ_ERROR"
    # Fix: Access context dictionary
    assert excinfo.value.context["file_path"] == str(json_file)
    # Fix: Access context dictionary
    assert excinfo.value.context["os_error"] == "Mock read error sync"


@pytest.mark.asyncio
async def test_save_json_async_success(config_manager, tmp_path):
    """Test asynchronous saving of JSON data."""
    json_data = {"save_key": "save_value"}
    json_file = tmp_path / "output" / "saved_config.json"

    assert not json_file.exists()

    await config_manager.save_json_async(json_file, json_data)

    assert json_file.exists()
    loaded_data = json.loads(json_file.read_text())
    assert loaded_data == json_data
    assert json_file.parent.is_dir()  # Directory should be created


@pytest.mark.asyncio
async def test_save_json_async_with_backup(config_manager, tmp_path):
    """Test asynchronous saving with backup enabled."""
    json_file = tmp_path / "output" / "config_with_backup.json"
    json_file.parent.mkdir()
    json_file.write_text(json.dumps({"initial": "data"}))
    backup_file = json_file.with_suffix(f"{json_file.suffix}.backup")

    assert json_file.exists()
    assert not backup_file.exists()

    new_data = {"updated": "data"}
    await config_manager.save_json_async(json_file, new_data, backup=True)

    assert json_file.exists()
    assert backup_file.exists()
    assert json.loads(json_file.read_text()) == new_data
    assert json.loads(backup_file.read_text()) == {"initial": "data"}


@pytest.mark.asyncio
async def test_save_json_async_os_error(config_manager, tmp_path, mocker):
    """Test asynchronous saving when an OSError occurs during file write."""
    json_file = tmp_path / "output" / "unwritable.json"
    json_data = {"data": "to_save"}

    # Mock aiofiles.open to raise an OSError
    mocker.patch("aiofiles.open", side_effect=OSError("Mock write error"))

    with pytest.raises(FileOperationError) as excinfo:
        await config_manager.save_json_async(json_file, json_data)

    assert "Failed to save JSON to" in str(excinfo.value)
    assert excinfo.value.error_code == "FILE_WRITE_ERROR"
    # Fix: Access context dictionary
    assert excinfo.value.context["file_path"] == str(json_file)
    # Fix: Access context dictionary
    assert excinfo.value.context["error"] == "Mock write error"


def test_save_json_sync_success(config_manager, tmp_path):
    """Test synchronous saving of JSON data."""
    json_data = {"save_key_sync": "save_value_sync"}
    json_file = tmp_path / "output_sync" / "saved_config_sync.json"

    assert not json_file.exists()

    config_manager.save_json(json_file, json_data)

    assert json_file.exists()
    loaded_data = json.loads(json_file.read_text())
    assert loaded_data == json_data
    assert json_file.parent.is_dir()  # Directory should be created


def test_save_json_sync_with_backup(config_manager, tmp_path):
    """Test synchronous saving with backup enabled."""
    json_file = tmp_path / "output_sync" / "config_with_backup_sync.json"
    json_file.parent.mkdir()
    json_file.write_text(json.dumps({"initial_sync": "data_sync"}))
    backup_file = json_file.with_suffix(f"{json_file.suffix}.backup")

    assert json_file.exists()
    assert not backup_file.exists()

    new_data = {"updated_sync": "data_sync"}
    config_manager.save_json(json_file, new_data, backup=True)

    assert json_file.exists()
    assert backup_file.exists()
    assert json.loads(json_file.read_text()) == new_data
    assert json.loads(backup_file.read_text()) == {"initial_sync": "data_sync"}


def test_save_json_sync_os_error(config_manager, tmp_path, mocker):
    """Test synchronous saving when an OSError occurs during file write."""
    json_file = tmp_path / "output_sync" / "unwritable_sync.json"
    json_data = {"data": "to_save"}

    # Mock Path.open to raise an OSError
    mocker.patch.object(Path, "open", side_effect=OSError("Mock write error sync"))

    with pytest.raises(FileOperationError) as excinfo:
        config_manager.save_json(json_file, json_data)

    assert "Failed to save JSON to" in str(excinfo.value)
    assert excinfo.value.error_code == "FILE_WRITE_ERROR"
    # Fix: Access context dictionary
    assert excinfo.value.context["file_path"] == str(json_file)
    # Fix: Access context dictionary
    assert excinfo.value.context["error"] == "Mock write error sync"


def test_load_config_with_model_success(config_manager, tmp_path):
    """Test loading and validating config with a Pydantic model."""

    class TestModel(BaseModel):
        name: str
        value: int

    config_data = {"name": "test", "value": 123}
    config_file = tmp_path / "valid_config.json"
    config_file.write_text(json.dumps(config_data))

    loaded_model = config_manager.load_config_with_model(config_file, TestModel)

    assert isinstance(loaded_model, TestModel)
    assert loaded_model.name == "test"
    assert loaded_model.value == 123


def test_load_config_with_model_validation_error(config_manager, tmp_path):
    """Test loading config with a Pydantic model when validation fails."""

    class TestModel(BaseModel):
        name: str
        value: int

    # Invalid data: value is string instead of int
    config_data = {"name": "test", "value": "not a number"}
    config_file = tmp_path / "invalid_config.json"
    config_file.write_text(json.dumps(config_data))

    with pytest.raises(FileOperationError) as excinfo:
        config_manager.load_config_with_model(config_file, TestModel)

    assert "Invalid configuration in" in str(excinfo.value)
    assert excinfo.value.error_code == "INVALID_CONFIGURATION"
    # Fix: Access context dictionary
    assert excinfo.value.context["file_path"] == str(config_file)
    assert "validation_errors" in excinfo.value.context
    assert isinstance(excinfo.value.__cause__, ValidationError)


def test_load_config_with_model_file_not_found(config_manager, tmp_path):
    """Test loading config with a Pydantic model when file is not found."""

    class TestModel(BaseModel):
        name: str

    config_file = tmp_path / "non_existent_config_model.json"

    with pytest.raises(FileOperationError) as excinfo:
        config_manager.load_config_with_model(config_file, TestModel)

    assert "JSON file not found:" in str(excinfo.value)
    assert excinfo.value.error_code == "FILE_NOT_FOUND"
    # Fix: Access context dictionary
    assert excinfo.value.context["file_path"] == str(config_file)


# --- Tests for SystemInfo ---


def test_get_platform_info(mocker):
    """Test get_platform_info."""
    # Mock platform functions to return predictable values
    mocker.patch("platform.system", return_value="MockOS")
    mocker.patch("platform.machine", return_value="MockMachine")
    mocker.patch("platform.architecture", return_value=("64bit", "ELF"))
    mocker.patch("platform.processor", return_value="MockProcessor")
    mocker.patch("platform.python_version", return_value="3.9.7")
    mocker.patch("platform.platform", return_value="MockPlatform-1.0")
    mocker.patch("platform.release", return_value="1.0")
    mocker.patch("platform.version", return_value="#1 MockVersion")

    info = SystemInfo.get_platform_info()

    assert info == {
        "system": "MockOS",
        "machine": "MockMachine",
        "architecture": "64bit",
        "processor": "MockProcessor",
        "python_version": "3.9.7",
        "platform": "MockPlatform-1.0",
        "release": "1.0",
        "version": "#1 MockVersion",
    }


def test_get_cpu_count(mocker):
    """Test get_cpu_count."""
    mocker.patch("os.cpu_count", return_value=8)
    assert SystemInfo.get_cpu_count() == 8

    mocker.patch("os.cpu_count", return_value=None)
    assert SystemInfo.get_cpu_count() == 1  # Fallback to 1


def test_get_memory_info_available(mocker):
    """Test get_memory_info when psutil is available."""
    mock_psutil = MagicMock()
    mock_psutil.virtual_memory.return_value = MagicMock(
        total=16 * 1024**3, available=8 * 1024**3, percent=50.0  # 16GB  # 8GB
    )
    mocker.patch('sys.modules["psutil"]', mock_psutil)  # Simulate psutil being imported

    info = SystemInfo.get_memory_info()

    assert info == {
        "total": 16 * 1024**3,
        "available": 8 * 1024**3,
        "percent_used": 50.0,
    }


def test_get_memory_info_not_available(mocker):
    """Test get_memory_info when psutil is not available."""
    # Simulate psutil not being importable
    mocker.patch(
        "builtins.__import__", side_effect=ImportError("No module named 'psutil'")
    )

    info = SystemInfo.get_memory_info()

    assert info == {}  # Should return empty dict


def test_find_executable_in_path(mocker):
    """Test find_executable when executable is in system PATH."""
    mocker.patch("shutil.which", return_value="/usr/bin/mock_exe")
    mocker.patch("pathlib.Path.is_file", return_value=True)  # Mock Path methods too
    mocker.patch("os.access", return_value=True)

    found_path = SystemInfo.find_executable("mock_exe")

    assert found_path == Path("/usr/bin/mock_exe")
    mocker.patch("shutil.which").assert_called_once_with("mock_exe")


def test_find_executable_in_additional_paths(mocker, tmp_path):
    """Test find_executable when executable is in additional paths."""
    mocker.patch("shutil.which", return_value=None)  # Not in PATH

    additional_path = tmp_path / "custom_bin"
    additional_path.mkdir()
    exe_path = additional_path / "custom_exe"
    exe_path.touch()  # Create the dummy file

    # Mock Path methods for the additional path check
    mocker.patch.object(Path, "is_dir", return_value=True)
    mocker.patch.object(Path, "is_file", return_value=True)
    mocker.patch("os.access", return_value=True)

    found_path = SystemInfo.find_executable("custom_exe", paths=[str(additional_path)])

    assert found_path == exe_path
    mocker.patch("shutil.which").assert_called_once_with("custom_exe")
    # Check Path.is_dir and os.access were called for the additional path


def test_find_executable_not_found(mocker, tmp_path):
    """Test find_executable when executable is not found anywhere."""
    mocker.patch("shutil.which", return_value=None)
    mocker.patch.object(
        Path, "is_dir", return_value=False
    )  # Simulate additional path is not a dir

    found_path = SystemInfo.find_executable(
        "non_existent_exe", paths=[str(tmp_path / "fake_bin")]
    )

    assert found_path is None
    mocker.patch("shutil.which").assert_called_once_with("non_existent_exe")


def test_get_environment_info(mocker):
    """Test get_environment_info."""
    # Mock os.environ
    mock_environ = {
        "PATH": "/bin:/usr/bin",
        "CC": "gcc",
        "CXX": "g++",
        "MY_CUSTOM_VAR": "ignore_me",  # Should be ignored
    }
    mocker.patch("os.environ", mock_environ)

    info = SystemInfo.get_environment_info()

    assert info == {
        "PATH": "/bin:/usr/bin",
        "CC": "gcc",
        "CXX": "g++",
        # Other relevant vars should be included if they were in mock_environ,
        # but since they weren't, they are correctly omitted.
    }


# --- Tests for Convenience Functions ---


def test_load_json_convenience(mocker, tmp_path):
    """Test the top-level load_json convenience function."""
    mock_config_manager_instance = MagicMock(spec=ConfigurationManager)
    mocker.patch(
        "tools.compiler_helper.utils.ConfigurationManager",
        return_value=mock_config_manager_instance,
    )

    file_path = tmp_path / "convenience.json"
    load_json(file_path)

    mock_config_manager_instance.load_json.assert_called_once_with(file_path)


def test_save_json_convenience(mocker, tmp_path):
    """Test the top-level save_json convenience function."""
    mock_config_manager_instance = MagicMock(spec=ConfigurationManager)
    mocker.patch(
        "tools.compiler_helper.utils.ConfigurationManager",
        return_value=mock_config_manager_instance,
    )

    file_path = tmp_path / "convenience_save.json"
    data = {"a": 1}
    save_json(file_path, data, indent=4)

    mock_config_manager_instance.save_json.assert_called_once_with(
        file_path, data, indent=4
    )
