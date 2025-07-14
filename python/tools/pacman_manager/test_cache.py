import pytest
import tempfile
import shutil
import time
from pathlib import Path
from unittest.mock import Mock, patch
from datetime import datetime, timedelta
from .cache import LRUCache, PackageCache, CacheEntry, Serializable
from .models import PackageInfo
from .pacman_types import PackageName


# Mock PackageInfo for testing purposes
class MockPackageInfo(PackageInfo):
    def to_dict(self):
        return {
            "name": str(self.name),
            "version": str(self.version),
            "repository": str(self.repository),
            "installed": self.installed,
            "status": self.status.value,
            "description": self.description,
            "install_size": self.install_size,
            "dependencies": (
                [str(d.name) for d in self.dependencies] if self.dependencies else None
            ),
        }

    @classmethod
    def from_dict(cls, data):
        from .models import (
            PackageStatus,
            Dependency,
        )  # Import here to avoid circular dependency

        return cls(
            name=PackageName(data["name"]),
            version=data["version"],
            repository=data["repository"],
            installed=data["installed"],
            status=PackageStatus(data["status"]),
            description=data.get("description"),
            install_size=data.get("install_size"),
            dependencies=(
                [Dependency(name=PackageName(d)) for d in data["dependencies"]]
                if data.get("dependencies")
                else None
            ),
        )


@pytest.fixture
def temp_cache_dir():
    """Fixture to create and clean up a temporary cache directory."""
    with tempfile.TemporaryDirectory() as tmpdir:
        yield Path(tmpdir)


@pytest.fixture
def package_cache(temp_cache_dir):
    """Fixture for a PackageCache instance with a temporary disk cache."""
    config = {
        "max_size": 10,
        "ttl_seconds": 1,  # Short TTL for testing expiration
        "use_disk_cache": True,
        "cache_directory": str(temp_cache_dir),
    }
    cache = PackageCache(config)
    yield cache
    cache.clear_all()  # Ensure cleanup after each test


@pytest.fixture
def mock_package_info():
    """Fixture for a mock PackageInfo object."""
    return MockPackageInfo(
        name=PackageName("test-package"),
        version="1.0.0",
        repository="core",
        installed=True,
        status=MockPackageInfo.PackageStatus.INSTALLED,
        description="A test package",
        install_size=1024,
        dependencies=None,
    )


class TestPackageCache:
    """Unit tests for the PackageCache class."""

    def test_initialization(self, temp_cache_dir):
        """Test PackageCache initialization."""
        config = {
            "max_size": 5,
            "ttl_seconds": 60,
            "use_disk_cache": True,
            "cache_directory": str(temp_cache_dir),
        }
        cache = PackageCache(config)
        assert cache.max_size == 5
        assert cache.ttl == 60
        assert cache.use_disk_cache is True
        assert cache.cache_dir == temp_cache_dir
        assert cache.cache_dir.is_dir()

        # Test with no disk cache
        config["use_disk_cache"] = False
        cache_no_disk = PackageCache(config)
        assert cache_no_disk.use_disk_cache is False
        # Ensure no directory is created if use_disk_cache is False
        assert not (Path(config["cache_directory"]) / "pacman_manager").exists()

    def test_get_package_memory_hit(self, package_cache, mock_package_info):
        """Test getting a package from memory cache."""
        package_cache._memory_cache.put(
            f"package:{mock_package_info.name}", mock_package_info
        )
        retrieved_package = package_cache.get_package(mock_package_info.name)
        assert retrieved_package == mock_package_info

    def test_get_package_disk_hit(self, package_cache, mock_package_info):
        """Test getting a package from disk cache (and promoting to memory)."""
        # Ensure it's not in memory cache initially
        package_cache._memory_cache.clear()

        # Manually put to disk
        package_cache._put_to_disk(
            f"package:{mock_package_info.name}", mock_package_info
        )

        retrieved_package = package_cache.get_package(mock_package_info.name)
        assert retrieved_package is not None
        assert retrieved_package.name == mock_package_info.name
        assert retrieved_package.version == mock_package_info.version
        # Verify it's now in memory cache
        assert (
            package_cache._memory_cache.get(f"package:{mock_package_info.name}")
            == retrieved_package
        )

    def test_get_package_miss(self, package_cache, mock_package_info):
        """Test getting a package that is not in cache."""
        retrieved_package = package_cache.get_package(mock_package_info.name)
        assert retrieved_package is None

    def test_put_package(self, package_cache, mock_package_info):
        """Test putting a package into cache."""
        package_cache.put_package(mock_package_info)

        # Verify in memory cache
        assert (
            package_cache._memory_cache.get(f"package:{mock_package_info.name}")
            == mock_package_info
        )

        # Verify on disk
        cache_file = package_cache.cache_dir / package_cache._safe_filename(
            f"package:{mock_package_info.name}.cache"
        )
        assert cache_file.exists()

    def test_invalidate_package(self, package_cache, mock_package_info):
        """Test invalidating a package from cache."""
        package_cache.put_package(mock_package_info)
        assert package_cache.get_package(mock_package_info.name) is not None

        invalidated = package_cache.invalidate_package(mock_package_info.name)
        assert invalidated is True
        assert package_cache.get_package(mock_package_info.name) is None

        # Verify disk file is removed
        cache_file = package_cache.cache_dir / package_cache._safe_filename(
            f"package:{mock_package_info.name}.cache"
        )
        assert not cache_file.exists()

        # Invalidate non-existent package
        invalidated_non_existent = package_cache.invalidate_package(
            PackageName("non-existent")
        )
        assert invalidated_non_existent is False

    def test_clear_all(self, package_cache, mock_package_info):
        """Test clearing all cache entries."""
        package_cache.put_package(mock_package_info)
        package_cache.put_package(
            MockPackageInfo(
                name=PackageName("another-pkg"),
                version="1.0.0",
                repository="extra",
                installed=True,
                status=MockPackageInfo.PackageStatus.INSTALLED,
            )
        )

        assert package_cache._memory_cache.size == 2
        assert len(list(package_cache.cache_dir.iterdir())) == 2

        package_cache.clear_all()

        assert package_cache._memory_cache.size == 0
        assert len(list(package_cache.cache_dir.iterdir())) == 0

    def test_cleanup_expired_memory(self, package_cache, mock_package_info):
        """Test cleaning up expired entries from memory cache."""
        # Put an entry with a very short TTL that expires immediately
        package_cache._memory_cache.put(
            f"package:{mock_package_info.name}", mock_package_info, ttl=-1
        )
        package_cache._memory_cache.put(
            "package:valid-pkg", mock_package_info, ttl=100
        )  # Valid entry

        cleaned_count = package_cache.cleanup_expired()
        assert cleaned_count == 1  # Only the expired one
        assert package_cache._memory_cache.size == 1
        assert package_cache._memory_cache.get("package:valid-pkg") is not None

    def test_cleanup_expired_disk(self, package_cache, mock_package_info):
        """Test cleaning up expired entries from disk cache."""
        # Manually put an expired entry to disk
        expired_key = "package:expired-disk-pkg"
        expired_file = package_cache.cache_dir / package_cache._safe_filename(
            f"{expired_key}.cache"
        )

        expired_data = {
            "key": expired_key,
            "value": mock_package_info.to_dict(),
            "created_at": time.time() - package_cache.ttl - 10,  # Older than TTL
            "ttl": package_cache.ttl,
        }
        with open(expired_file, "wb") as f:
            pickle.dump(expired_data, f)

        # Put a valid entry to disk
        valid_key = "package:valid-disk-pkg"
        package_cache._put_to_disk(valid_key, mock_package_info)

        assert expired_file.exists()
        assert (
            package_cache.cache_dir / package_cache._safe_filename(f"{valid_key}.cache")
        ).exists()

        cleaned_count = package_cache.cleanup_expired()
        assert cleaned_count == 1  # Only the expired disk entry
        assert not expired_file.exists()
        assert (
            package_cache.cache_dir / package_cache._safe_filename(f"{valid_key}.cache")
        ).exists()

    def test_get_stats(self, package_cache, mock_package_info):
        """Test getting comprehensive cache statistics."""
        package_cache.put_package(mock_package_info)
        package_cache.get_package(mock_package_info.name)  # Hit
        package_cache.get_package(PackageName("non-existent"))  # Miss

        stats = package_cache.get_stats()

        assert stats["size"] == 1
        assert stats["hits"] == 1
        assert stats["misses"] == 1
        assert stats["hit_rate"] == 0.5
        assert stats["total_requests"] == 2
        assert stats["ttl_seconds"] == package_cache.ttl
        assert stats["use_disk_cache"] is True
        assert "disk_files" in stats
        assert "disk_size_bytes" in stats
        assert stats["disk_files"] == 1  # One file on disk

    def test_safe_filename(self, package_cache):
        """Test _safe_filename method."""
        assert (
            package_cache._safe_filename("package:name/with:slash\\colon")
            == "package_name_with_slash_colon"
        )
        long_key = "a" * 200
        assert len(package_cache._safe_filename(long_key)) == 100
        assert package_cache._safe_filename("simple_key") == "simple_key"

    def test_load_from_disk_on_startup(self, temp_cache_dir, mock_package_info):
        """Test loading valid entries from disk on startup."""
        # Manually put some entries to disk
        valid_key = "package:startup-valid"
        expired_key = "package:startup-expired"
        corrupted_key = "package:startup-corrupted"

        # Valid entry
        valid_data = {
            "key": valid_key,
            "value": mock_package_info.to_dict(),
            "created_at": time.time(),
            "ttl": 100,
        }
        with open(
            temp_cache_dir / package_cache._safe_filename(f"{valid_key}.cache"), "wb"
        ) as f:
            pickle.dump(valid_data, f)

        # Expired entry
        expired_data = {
            "key": expired_key,
            "value": mock_package_info.to_dict(),
            "created_at": time.time() - 200,  # Expired
            "ttl": 100,
        }
        with open(
            temp_cache_dir / package_cache._safe_filename(f"{expired_key}.cache"), "wb"
        ) as f:
            pickle.dump(expired_data, f)

        # Corrupted entry
        with open(
            temp_cache_dir / package_cache._safe_filename(f"{corrupted_key}.cache"), "w"
        ) as f:
            f.write("this is not a pickle")

        # Initialize PackageCache, which should load from disk
        cache = PackageCache(
            {"use_disk_cache": True, "cache_directory": str(temp_cache_dir)}
        )

        assert cache._memory_cache.size == 1
        assert cache._memory_cache.get(valid_key) is not None
        assert (
            cache._memory_cache.get(expired_key) is None
        )  # Expired should not be loaded

        # Verify expired and corrupted files are removed from disk
        assert not (
            temp_cache_dir / package_cache._safe_filename(f"{expired_key}.cache")
        ).exists()
        assert not (
            temp_cache_dir / package_cache._safe_filename(f"{corrupted_key}.cache")
        ).exists()
        assert (
            temp_cache_dir / package_cache._safe_filename(f"{valid_key}.cache")
        ).exists()

    def test_disk_cache_disabled(self, temp_cache_dir, mock_package_info):
        """Test behavior when disk cache is disabled."""
        config = {
            "max_size": 10,
            "ttl_seconds": 1,
            "use_disk_cache": False,
            "cache_directory": str(temp_cache_dir),
        }
        cache = PackageCache(config)

        cache.put_package(mock_package_info)
        assert cache._memory_cache.size == 1
        assert not list(temp_cache_dir.iterdir())  # No files should be written to disk

        retrieved = cache.get_package(mock_package_info.name)
        assert retrieved == mock_package_info

        cache.invalidate_package(mock_package_info.name)
        assert cache._memory_cache.size == 0

        cache.clear_all()
        assert cache._memory_cache.size == 0
        assert not list(temp_cache_dir.iterdir())
