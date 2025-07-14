#!/usr/bin/env python3
"""
Advanced caching system for the enhanced pacman manager.
Provides multi-level caching with TTL, LRU eviction, and persistence.
"""

from __future__ import annotations

import time
import pickle
import threading
from pathlib import Path
from typing import TypeVar, Generic, Optional, Dict, Any, Protocol
from collections import OrderedDict

from loguru import logger

from .pacman_types import PackageName, CacheConfig
from .models import PackageInfo

T = TypeVar("T")


class Serializable(Protocol):
    """Protocol for objects that can be serialized."""

    def to_dict(self) -> Dict[str, Any]: ...
    @classmethod
    def from_dict(cls, data: Dict[str, Any]) -> Any: ...


class CacheEntry(Generic[T]):
    """A cache entry with metadata."""

    def __init__(self, key: str, value: T, ttl: float = 3600.0):
        self.key = key
        self.value = value
        self.created_at = time.time()
        self.ttl = ttl
        self.access_count = 0
        self.last_accessed = self.created_at

    @property
    def is_expired(self) -> bool:
        """Check if the cache entry has expired."""
        return time.time() - self.created_at > self.ttl

    @property
    def age(self) -> float:
        """Get the age of the cache entry in seconds."""
        return time.time() - self.created_at

    def touch(self) -> None:
        """Update access statistics."""
        self.access_count += 1
        self.last_accessed = time.time()

    def to_dict(self) -> Dict[str, Any]:
        """Convert to dictionary for serialization."""
        # Handle serialization based on value type
        if hasattr(self.value, "to_dict") and callable(getattr(self.value, "to_dict")):
            value_data = self.value.to_dict()  # type: ignore
        else:
            value_data = self.value

        return {
            "key": self.key,
            "value": value_data,
            "created_at": self.created_at,
            "ttl": self.ttl,
            "access_count": self.access_count,
            "last_accessed": self.last_accessed,
        }


class LRUCache(Generic[T]):
    """
    Thread-safe LRU cache with TTL support.
    """

    def __init__(self, max_size: int = 1000, default_ttl: float = 3600.0):
        self.max_size = max_size
        self.default_ttl = default_ttl
        self._cache: OrderedDict[str, CacheEntry[T]] = OrderedDict()
        self._lock = threading.RLock()
        self._hits = 0
        self._misses = 0

    def get(self, key: str) -> Optional[T]:
        """Get a value from the cache."""
        with self._lock:
            if key not in self._cache:
                self._misses += 1
                return None

            entry = self._cache[key]

            # Check if expired
            if entry.is_expired:
                del self._cache[key]
                self._misses += 1
                return None

            # Move to end (most recently used)
            self._cache.move_to_end(key)
            entry.touch()
            self._hits += 1

            return entry.value

    def put(self, key: str, value: T, ttl: Optional[float] = None) -> None:
        """Put a value into the cache."""
        with self._lock:
            ttl = ttl or self.default_ttl

            if key in self._cache:
                # Update existing entry
                self._cache[key] = CacheEntry(key, value, ttl)
                self._cache.move_to_end(key)
            else:
                # Add new entry
                if len(self._cache) >= self.max_size:
                    # Remove least recently used
                    self._cache.popitem(last=False)

                self._cache[key] = CacheEntry(key, value, ttl)

    def delete(self, key: str) -> bool:
        """Delete a key from the cache."""
        with self._lock:
            if key in self._cache:
                del self._cache[key]
                return True
            return False

    def clear(self) -> None:
        """Clear all cache entries."""
        with self._lock:
            self._cache.clear()
            self._hits = 0
            self._misses = 0

    def cleanup_expired(self) -> int:
        """Remove expired entries and return count removed."""
        with self._lock:
            expired_keys = [
                key for key, entry in self._cache.items() if entry.is_expired
            ]

            for key in expired_keys:
                del self._cache[key]

            return len(expired_keys)

    @property
    def size(self) -> int:
        """Get current cache size."""
        return len(self._cache)

    @property
    def hit_rate(self) -> float:
        """Get cache hit rate."""
        total = self._hits + self._misses
        return self._hits / total if total > 0 else 0.0

    @property
    def stats(self) -> Dict[str, Any]:
        """Get cache statistics."""
        return {
            "size": self.size,
            "max_size": self.max_size,
            "hits": self._hits,
            "misses": self._misses,
            "hit_rate": self.hit_rate,
            "total_requests": self._hits + self._misses,
        }


class PackageCache:
    """
    Specialized cache for package information with persistence support.
    """

    def __init__(self, config: CacheConfig | None = None):
        self.config = config or {}
        self.max_size = self.config.get("max_size", 10000)
        self.ttl = self.config.get("ttl_seconds", 3600)
        self.use_disk_cache = self.config.get("use_disk_cache", True)
        self.cache_dir = Path(
            self.config.get(
                "cache_directory", Path.home() / ".cache" / "pacman_manager"
            )
        )

        # Create cache directory
        if self.use_disk_cache:
            self.cache_dir.mkdir(parents=True, exist_ok=True)

        # In-memory cache
        self._memory_cache: LRUCache[PackageInfo] = LRUCache(self.max_size, self.ttl)
        self._lock = threading.RLock()

        # Load from disk if enabled
        if self.use_disk_cache:
            self._load_from_disk()

    def get_package(self, package_name: PackageName) -> Optional[PackageInfo]:
        """Get package information from cache."""
        cache_key = f"package:{package_name}"

        # Try memory cache first
        result = self._memory_cache.get(cache_key)
        if result:
            logger.debug(f"Memory cache hit for {package_name}")
            return result

        # Try disk cache if enabled
        if self.use_disk_cache:
            result = self._get_from_disk(cache_key)
            if result:
                logger.debug(f"Disk cache hit for {package_name}")
                # Promote to memory cache
                self._memory_cache.put(cache_key, result)
                return result

        logger.debug(f"Cache miss for {package_name}")
        return None

    def put_package(self, package_info: PackageInfo) -> None:
        """Store package information in cache."""
        cache_key = f"package:{package_info.name}"

        # Store in memory cache
        self._memory_cache.put(cache_key, package_info)

        # Store on disk if enabled
        if self.use_disk_cache:
            self._put_to_disk(cache_key, package_info)

        logger.debug(f"Cached package {package_info.name}")

    def invalidate_package(self, package_name: PackageName) -> bool:
        """Remove package from cache."""
        cache_key = f"package:{package_name}"

        # Remove from memory
        memory_deleted = self._memory_cache.delete(cache_key)

        # Remove from disk
        disk_deleted = False
        if self.use_disk_cache:
            disk_deleted = self._delete_from_disk(cache_key)

        return memory_deleted or disk_deleted

    def clear_all(self) -> None:
        """Clear all cached data."""
        self._memory_cache.clear()

        if self.use_disk_cache:
            for cache_file in self.cache_dir.glob("*.cache"):
                try:
                    cache_file.unlink()
                except OSError:
                    pass

    def cleanup_expired(self) -> int:
        """Remove expired entries from all cache levels."""
        memory_cleaned = self._memory_cache.cleanup_expired()
        disk_cleaned = 0

        if self.use_disk_cache:
            disk_cleaned = self._cleanup_disk_expired()

        total_cleaned = memory_cleaned + disk_cleaned
        if total_cleaned > 0:
            logger.info(f"Cleaned up {total_cleaned} expired cache entries")

        return total_cleaned

    def get_stats(self) -> Dict[str, Any]:
        """Get comprehensive cache statistics."""
        memory_stats = self._memory_cache.stats

        disk_stats = {}
        if self.use_disk_cache:
            cache_files = list(self.cache_dir.glob("*.cache"))
            disk_stats = {
                "disk_files": len(cache_files),
                "disk_size_bytes": sum(f.stat().st_size for f in cache_files),
            }

        return {
            **memory_stats,
            **disk_stats,
            "ttl_seconds": self.ttl,
            "use_disk_cache": self.use_disk_cache,
        }

    def _get_from_disk(self, key: str) -> Optional[PackageInfo]:
        """Get entry from disk cache."""
        cache_file = self.cache_dir / f"{self._safe_filename(key)}.cache"

        if not cache_file.exists():
            return None

        try:
            with open(cache_file, "rb") as f:
                entry_data = pickle.load(f)

            # Check if expired
            if time.time() - entry_data["created_at"] > entry_data["ttl"]:
                cache_file.unlink()
                return None

            # Reconstruct PackageInfo
            if isinstance(entry_data["value"], dict):
                return PackageInfo.from_dict(entry_data["value"])

            return entry_data["value"]

        except (OSError, pickle.UnpicklingError, KeyError) as e:
            logger.warning(f"Failed to load cache file {cache_file}: {e}")
            try:
                cache_file.unlink()
            except OSError:
                pass
            return None

    def _put_to_disk(self, key: str, value: PackageInfo) -> None:
        """Store entry to disk cache."""
        cache_file = self.cache_dir / f"{self._safe_filename(key)}.cache"

        entry_data = {
            "key": key,
            "value": value.to_dict(),
            "created_at": time.time(),
            "ttl": self.ttl,
        }

        try:
            with open(cache_file, "wb") as f:
                pickle.dump(entry_data, f)
        except OSError as e:
            logger.warning(f"Failed to write cache file {cache_file}: {e}")

    def _delete_from_disk(self, key: str) -> bool:
        """Delete entry from disk cache."""
        cache_file = self.cache_dir / f"{self._safe_filename(key)}.cache"

        try:
            cache_file.unlink()
            return True
        except OSError:
            return False

    def _cleanup_disk_expired(self) -> int:
        """Remove expired files from disk cache."""
        current_time = time.time()
        cleaned_count = 0

        for cache_file in self.cache_dir.glob("*.cache"):
            try:
                with open(cache_file, "rb") as f:
                    entry_data = pickle.load(f)

                if current_time - entry_data["created_at"] > entry_data["ttl"]:
                    cache_file.unlink()
                    cleaned_count += 1

            except (OSError, pickle.UnpicklingError):
                # Remove corrupted files
                try:
                    cache_file.unlink()
                    cleaned_count += 1
                except OSError:
                    pass

        return cleaned_count

    def _load_from_disk(self) -> None:
        """Load cache entries from disk to memory on startup."""
        if not self.cache_dir.exists():
            return

        loaded_count = 0
        for cache_file in self.cache_dir.glob("*.cache"):
            try:
                with open(cache_file, "rb") as f:
                    entry_data = pickle.load(f)

                # Check if not expired
                if time.time() - entry_data["created_at"] <= entry_data["ttl"]:
                    if isinstance(entry_data["value"], dict):
                        package_info = PackageInfo.from_dict(entry_data["value"])
                        self._memory_cache.put(entry_data["key"], package_info)
                        loaded_count += 1
                else:
                    # Remove expired file
                    cache_file.unlink()

            except (OSError, pickle.UnpicklingError, KeyError):
                # Remove corrupted files
                try:
                    cache_file.unlink()
                except OSError:
                    pass

        if loaded_count > 0:
            logger.info(f"Loaded {loaded_count} cache entries from disk")

    def _safe_filename(self, key: str) -> str:
        """Convert cache key to safe filename."""
        # Replace problematic characters
        safe_key = key.replace(":", "_").replace("/", "_").replace("\\", "_")
        # Limit length
        if len(safe_key) > 100:
            safe_key = safe_key[:100]
        return safe_key


# Export all cache classes
__all__ = [
    "CacheEntry",
    "LRUCache",
    "PackageCache",
    "Serializable",
]
