import pytest
import tempfile
import shutil
import platform
from pathlib import Path
from unittest.mock import patch, mock_open
from datetime import datetime
from python.tools.pacman_manager.config import PacmanConfig, ConfigError, ConfigSection, PacmanConfigState

# Fixtures for temporary config files


@pytest.fixture
def temp_config_file():
    """Creates a temporary pacman.conf file for testing."""
    content = """
# General options
[options]
Architecture = auto
SigLevel = Required DatabaseOptional
LocalFileSigLevel = Optional
# SomeCommentedOption = value
HoldPkg = pacman glibc
SyncFirst = pacman
# Misc options
Color
TotalDownloadProgress
CheckSpace
VerbosePkgLists

[core]
Include = /etc/pacman.d/mirrorlist

[extra]
Include = /etc/pacman.d/mirrorlist

#[community]
#Include = /etc/pacman.d/mirrorlist

[multilib]
#Include = /etc/pacman.d/mirrorlist
"""
    with tempfile.NamedTemporaryFile(mode='w+', delete=False, encoding='utf-8') as f:
        f.write(content)
        temp_path = Path(f.name)
    yield temp_path
    temp_path.unlink(missing_ok=True)


@pytest.fixture
def empty_config_file():
    """Creates an empty temporary pacman.conf file."""
    with tempfile.NamedTemporaryFile(mode='w+', delete=False, encoding='utf-8') as f:
        temp_path = Path(f.name)
    yield temp_path
    temp_path.unlink(missing_ok=True)


@pytest.fixture
def pacman_config(temp_config_file):
    """Provides a PacmanConfig instance initialized with a temporary file."""
    return PacmanConfig(config_path=temp_config_file)


class TestPacmanConfig:
    """Tests for the PacmanConfig class."""

    @patch('platform.system', return_value='Linux')
    def test_init_linux_default_path(self, mock_system, temp_config_file):
        """Test initialization on Linux with default path."""
        with patch('pathlib.Path.exists', side_effect=lambda p: p == temp_config_file):
            with patch('python.tools.pacman_manager.config.PacmanConfig._default_paths', [temp_config_file]):
                config = PacmanConfig(config_path=None)
                assert config.config_path == temp_config_file
                assert not config.is_windows

    @patch('platform.system', return_value='Windows')
    def test_init_windows_default_path(self, mock_system, temp_config_file):
        """Test initialization on Windows with default path."""
        with patch('pathlib.Path.exists', side_effect=lambda p: p == temp_config_file):
            with patch('python.tools.pacman_manager.config.PacmanConfig._default_paths', [temp_config_file]):
                config = PacmanConfig(config_path=None)
                assert config.config_path == temp_config_file
                assert config.is_windows

    def test_init_explicit_path(self, temp_config_file):
        """Test initialization with an explicitly provided path."""
        config = PacmanConfig(config_path=temp_config_file)
        assert config.config_path == temp_config_file

    def test_init_explicit_path_not_found(self):
        """Test initialization with an explicit path that does not exist."""
        non_existent_path = Path("/tmp/non_existent_pacman.conf")
        with pytest.raises(ConfigError, match="Specified config path does not exist"):
            PacmanConfig(config_path=non_existent_path)

    @patch('platform.system', return_value='Linux')
    def test_init_no_default_path_found_linux(self, mock_system):
        """Test initialization when no default path is found on Linux."""
        with patch('pathlib.Path.exists', return_value=False):
            with patch('python.tools.pacman_manager.config.PacmanConfig._default_paths', [Path('/nonexistent/path')]):
                with pytest.raises(ConfigError, match="Pacman configuration file not found"):
                    PacmanConfig(config_path=None)

    @patch('platform.system', return_value='Windows')
    def test_init_no_default_path_found_windows(self, mock_system):
        """Test initialization when no default path is found on Windows."""
        with patch('pathlib.Path.exists', return_value=False):
            with patch('python.tools.pacman_manager.config.PacmanConfig._default_paths', [Path('C:\\nonexistent\\path')]):
                with pytest.raises(ConfigError, match="MSYS2 pacman configuration not found"):
                    PacmanConfig(config_path=None)

    def test_validate_config_file_unreadable(self, temp_config_file):
        """Test validation with an unreadable config file."""
        with patch.object(Path, 'open', side_effect=PermissionError):
            with pytest.raises(ConfigError, match="Cannot read pacman configuration file"):
                PacmanConfig(config_path=temp_config_file)

    def test_file_operation_read_error(self, pacman_config):
        """Test _file_operation context manager for read errors."""
        with patch.object(Path, 'open', side_effect=OSError("Read error")):
            with pytest.raises(ConfigError, match="Failed reading config file"):
                with pacman_config._file_operation('r') as f:
                    f.read()

    def test_file_operation_write_error(self, pacman_config):
        """Test _file_operation context manager for write errors."""
        with patch.object(Path, 'open', side_effect=OSError("Write error")):
            with pytest.raises(ConfigError, match="Failed writing config file"):
                with pacman_config._file_operation('w') as f:
                    f.write("test")

    def test_parse_config_initial(self, pacman_config):
        """Test initial parsing of the config file."""
        config_state = pacman_config._parse_config()
        assert config_state.options.get_option("Architecture") == "auto"
        assert config_state.options.get_option(
            "SigLevel") == "Required DatabaseOptional"
        assert config_state.options.get_option(
            "Color") == ""  # Option with no value
        assert "core" in config_state.repositories
        assert "extra" in config_state.repositories
        assert "community" not in config_state.repositories  # Commented out
        assert "multilib" in config_state.repositories
        assert not config_state.is_dirty()

    def test_parse_config_cached(self, pacman_config):
        """Test that _parse_config uses cached data when not dirty."""
        initial_state = pacman_config._parse_config()
        # Modify internal state without marking dirty
        initial_state.options.options["Architecture"] = "x86_64"

        # Re-parse, should return the same object if not dirty
        reparsed_state = pacman_config._parse_config()
        assert reparsed_state.options.get_option(
            "Architecture") == "x86_64"  # Still the modified value
        assert reparsed_state is initial_state  # Should be the same object

    def test_parse_config_dirty_reparse(self, pacman_config):
        """Test that _parse_config reparses when dirty."""
        initial_state = pacman_config._parse_config()
        initial_state.mark_dirty()
        # This change will be overwritten by re-parsing
        initial_state.options.options["Architecture"] = "x86_64"

        reparsed_state = pacman_config._parse_config()
        assert reparsed_state.options.get_option(
            "Architecture") == "auto"  # Original value from file
        assert reparsed_state is not initial_state  # Should be a new object

    def test_parse_config_empty_file(self, empty_config_file):
        """Test parsing an empty config file."""
        config = PacmanConfig(config_path=empty_config_file)
        config_state = config._parse_config()
        assert not config_state.options.options
        assert not config_state.repositories

    def test_parse_config_malformed_lines(self, temp_config_file):
        """Test parsing with malformed lines."""
        content = """
[options]
Key1 = Value1
MalformedLine
Key2: Value2
[repo]
RepoKey = RepoValue
"""
        temp_config_file.write_text(content)
        config = PacmanConfig(config_path=temp_config_file)

        with patch('loguru.logger.warning') as mock_warning:
            config_state = config._parse_config()
            assert config_state.options.get_option("Key1") == "Value1"
            assert "repo" in config_state.repositories
            assert config_state.repositories["repo"].get_option(
                "RepoKey") == "RepoValue"

            # Check warnings for malformed lines
            mock_warning.assert_any_call(
                f"Orphaned option 'MalformedLine' at line 4")
            mock_warning.assert_any_call(
                f"Orphaned option 'Key2: Value2' at line 5")

    def test_get_option_exists(self, pacman_config):
        """Test getting an existing option."""
        assert pacman_config.get_option("Architecture") == "auto"
        assert pacman_config.get_option("Color") == ""

    def test_get_option_not_exists(self, pacman_config):
        """Test getting a non-existent option."""
        assert pacman_config.get_option("NonExistentOption") is None

    def test_get_option_with_default(self, pacman_config):
        """Test getting a non-existent option with a default value."""
        assert pacman_config.get_option(
            "NonExistentOption", "default_value") == "default_value"

    def test_set_option_modify_existing(self, pacman_config, temp_config_file):
        """Test modifying an existing option."""
        original_content = temp_config_file.read_text()
        assert pacman_config.set_option("Architecture", "x86_64") is True

        new_content = temp_config_file.read_text()
        assert "Architecture = x86_64" in new_content
        assert "Architecture = auto" not in new_content
        assert pacman_config.get_option("Architecture") == "x86_64"
        assert pacman_config._state.is_dirty()

    def test_set_option_add_new(self, pacman_config, temp_config_file):
        """Test adding a new option."""
        assert pacman_config.set_option("NewOption", "NewValue") is True

        new_content = temp_config_file.read_text()
        assert "NewOption = NewValue" in new_content
        assert pacman_config.get_option("NewOption") == "NewValue"
        assert pacman_config._state.is_dirty()

    def test_set_option_add_new_no_options_section(self, empty_config_file):
        """Test adding a new option when no [options] section exists."""
        config = PacmanConfig(config_path=empty_config_file)
        assert config.set_option("NewOption", "NewValue") is True

        new_content = empty_config_file.read_text()
        assert "[options]" in new_content
        assert "NewOption = NewValue" in new_content
        assert config.get_option("NewOption") == "NewValue"

    def test_set_option_modify_commented(self, pacman_config, temp_config_file):
        """Test modifying a commented-out option."""
        assert pacman_config.set_option(
            "SomeCommentedOption", "newValue") is True

        new_content = temp_config_file.read_text()
        assert "SomeCommentedOption = newValue" in new_content
        assert "# SomeCommentedOption = value" not in new_content
        assert pacman_config.get_option("SomeCommentedOption") == "newValue"

    def test_set_option_invalid_option_name(self, pacman_config):
        """Test setting an option with an invalid name."""
        with pytest.raises(ConfigError, match="Option name must be a non-empty string"):
            pacman_config.set_option("", "value")
        with pytest.raises(ConfigError, match="Option name must be a non-empty string"):
            pacman_config.set_option(None, "value")  # type: ignore

    def test_set_option_invalid_value_type(self, pacman_config):
        """Test setting an option with an invalid value type."""
        with pytest.raises(ConfigError, match="Option value must be a string"):
            pacman_config.set_option("TestOption", 123)  # type: ignore

    @patch('shutil.copy2')
    @patch('datetime.datetime')
    def test_create_backup(self, mock_datetime, mock_copy2, pacman_config, temp_config_file):
        """Test creating a backup of the config file."""
        mock_datetime.now.return_value = datetime(2023, 1, 1, 12, 30, 0)

        backup_path = pacman_config._create_backup()
        expected_backup_path = temp_config_file.with_suffix(
            ".20230101_123000.backup")

        assert backup_path == expected_backup_path
        mock_copy2.assert_called_once_with(
            temp_config_file, expected_backup_path)

    @patch('shutil.copy2', side_effect=OSError("Backup error"))
    def test_create_backup_failure(self, mock_copy2, pacman_config):
        """Test backup creation failure."""
        with pytest.raises(ConfigError, match="Failed to create configuration backup"):
            pacman_config._create_backup()

    def test_get_enabled_repos(self, pacman_config):
        """Test getting a list of enabled repositories."""
        enabled_repos = pacman_config.get_enabled_repos()
        assert "core" in enabled_repos
        assert "extra" in enabled_repos
        assert "multilib" in enabled_repos
        assert "community" not in enabled_repos  # It's commented out in the fixture

    def test_enable_repo_existing_commented(self, pacman_config, temp_config_file):
        """Test enabling an existing, commented-out repository."""
        assert pacman_config.enable_repo("community") is True

        new_content = temp_config_file.read_text()
        assert "[community]" in new_content
        assert "#[community]" not in new_content
        assert "community" in pacman_config.get_enabled_repos()
        assert pacman_config._state.is_dirty()

    def test_enable_repo_non_existent(self, pacman_config):
        """Test enabling a non-existent repository."""
        assert pacman_config.enable_repo("nonexistent_repo") is False
        assert "nonexistent_repo" not in pacman_config.get_enabled_repos()

    def test_enable_repo_already_enabled(self, pacman_config):
        """Test enabling an already enabled repository."""
        assert pacman_config.enable_repo(
            "core") is False  # No change in file, so returns False
        assert "core" in pacman_config.get_enabled_repos()

    def test_enable_repo_invalid_name(self, pacman_config):
        """Test enabling a repository with an invalid name."""
        with pytest.raises(ConfigError, match="Repository name must be a non-empty string"):
            pacman_config.enable_repo("")
        with pytest.raises(ConfigError, match="Repository name must be a non-empty string"):
            pacman_config.enable_repo(None)  # type: ignore

    def test_repository_count(self, pacman_config):
        """Test the repository_count property."""
        assert pacman_config.repository_count == 3  # core, extra, multilib (community is commented)

    def test_enabled_repository_count(self, pacman_config):
        """Test the enabled_repository_count property."""
        assert pacman_config.enabled_repository_count == 3  # core, extra, multilib

    def test_get_config_summary(self, pacman_config):
        """Test getting a summary of the configuration."""
        summary = pacman_config.get_config_summary()
        assert summary['config_path'] == str(pacman_config.config_path)
        assert summary['total_options'] > 0
        assert summary['total_repositories'] == 3
        assert summary['enabled_repositories'] == 3
        assert isinstance(summary['is_windows'], bool)
        assert summary['is_dirty'] is False

    def test_validate_configuration_no_issues(self, pacman_config):
        """Test configuration validation with no issues."""
        issues = pacman_config.validate_configuration()
        assert not issues

    def test_validate_configuration_missing_option(self, temp_config_file):
        """Test validation with a missing required option."""
        temp_config_file.write_text("""
[options]
# Architecture = auto
SigLevel = Required DatabaseOptional
""")
        config = PacmanConfig(config_path=temp_config_file)
        issues = config.validate_configuration()
        assert "Missing required option: Architecture" in issues

    def test_validate_configuration_no_enabled_repos(self, temp_config_file):
        """Test validation with no enabled repositories."""
        temp_config_file.write_text("""
[options]
Architecture = auto
SigLevel = Required DatabaseOptional

#[core]
#Include = /etc/pacman.d/mirrorlist
""")
        config = PacmanConfig(config_path=temp_config_file)
        issues = config.validate_configuration()
        assert "No enabled repositories found" in issues

    def test_validate_configuration_invalid_architecture(self, temp_config_file):
        """Test validation with an invalid architecture."""
        temp_config_file.write_text("""
[options]
Architecture = invalid_arch
SigLevel = Required DatabaseOptional

[core]
Include = /etc/pacman.d/mirrorlist
""")
        config = PacmanConfig(config_path=temp_config_file)
        issues = config.validate_configuration()
        assert "Unknown architecture: invalid_arch" in issues

    def test_validate_configuration_parsing_error(self, temp_config_file):
        """Test validation when parsing itself causes an error."""
        temp_config_file.write_text("""
[options]
Architecture = auto
Malformed Line =
""")
        config = PacmanConfig(config_path=temp_config_file)
        issues = config.validate_configuration()
        assert any("Configuration parsing error" in issue for issue in issues)
