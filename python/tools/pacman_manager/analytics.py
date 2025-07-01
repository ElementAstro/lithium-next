#!/usr/bin/env python3
"""
Package Analytics Module for Pacman Manager
Provides advanced analytics and insights for package management.
"""

from __future__ import annotations

import asyncio
import time
from collections import Counter, defaultdict
from dataclasses import dataclass, field
from datetime import datetime, timedelta
from pathlib import Path
from typing import Any, ClassVar, Dict, List, Optional, Tuple, TypedDict, Union

from .cache import LRUCache
from .exceptions import PacmanError
from .models import CommandResult, PackageInfo, PackageStatus
from .types import PackageName, RepositoryName


class PackageUsageStats(TypedDict):
    """Statistics about package usage."""

    install_count: int
    remove_count: int
    upgrade_count: int
    last_accessed: datetime
    avg_install_time: float
    total_install_time: float


class SystemMetrics(TypedDict):
    """System-wide package metrics."""

    total_packages: int
    installed_packages: int
    orphaned_packages: int
    outdated_packages: int
    disk_usage_mb: float
    cache_size_mb: float


@dataclass(slots=True, frozen=True)
class OperationMetric:
    """Metrics for a single package operation."""

    operation: str
    package_name: str
    duration: float
    success: bool
    timestamp: datetime
    memory_usage: Optional[float] = None
    cpu_usage: Optional[float] = None

    def to_dict(self) -> Dict[str, Any]:
        """Convert to dictionary for serialization."""
        return {
            'operation': self.operation,
            'package_name': self.package_name,
            'duration': self.duration,
            'success': self.success,
            'timestamp': self.timestamp.isoformat(),
            'memory_usage': self.memory_usage,
            'cpu_usage': self.cpu_usage,
        }

    @classmethod
    def from_dict(cls, data: Dict[str, Any]) -> OperationMetric:
        """Create from dictionary."""
        return cls(
            operation=data['operation'],
            package_name=data['package_name'],
            duration=data['duration'],
            success=data['success'],
            timestamp=datetime.fromisoformat(data['timestamp']),
            memory_usage=data.get('memory_usage'),
            cpu_usage=data.get('cpu_usage'),
        )


@dataclass(slots=True)
class PackageAnalytics:
    """Advanced analytics for package management operations."""

    cache: LRUCache[Any] = field(default_factory=lambda: LRUCache(1000, 3600))
    _metrics: List[OperationMetric] = field(default_factory=list, init=False)
    _usage_stats: Dict[str, PackageUsageStats] = field(
        default_factory=dict, init=False)
    _start_time: Optional[float] = field(default=None, init=False)

    # Class-level constants
    MAX_METRICS: ClassVar[int] = 10000
    ANALYTICS_CACHE_TTL: ClassVar[int] = 3600  # 1 hour

    def start_operation(self, operation: str, package_name: str) -> None:
        """Start tracking an operation."""
        self._start_time = time.perf_counter()

    def end_operation(self, operation: str, package_name: str, success: bool = True) -> None:
        """End tracking an operation and record metrics."""
        if self._start_time is None:
            return

        duration = time.perf_counter() - self._start_time
        metric = OperationMetric(
            operation=operation,
            package_name=package_name,
            duration=duration,
            success=success,
            timestamp=datetime.now(),
        )

        self._add_metric(metric)
        self._update_usage_stats(operation, package_name, duration)
        self._start_time = None

    def _add_metric(self, metric: OperationMetric) -> None:
        """Add a metric, maintaining max size."""
        self._metrics.append(metric)
        if len(self._metrics) > self.MAX_METRICS:
            # Remove oldest metrics when exceeding limit
            self._metrics = self._metrics[-self.MAX_METRICS // 2:]

    def _update_usage_stats(self, operation: str, package_name: str, duration: float) -> None:
        """Update usage statistics for a package."""
        if package_name not in self._usage_stats:
            self._usage_stats[package_name] = PackageUsageStats(
                install_count=0,
                remove_count=0,
                upgrade_count=0,
                last_accessed=datetime.now(),
                avg_install_time=0.0,
                total_install_time=0.0,
            )

        stats = self._usage_stats[package_name]
        stats['last_accessed'] = datetime.now()

        if operation == 'install':
            stats['install_count'] += 1
            stats['total_install_time'] += duration
            stats['avg_install_time'] = stats['total_install_time'] / \
                stats['install_count']
        elif operation == 'remove':
            stats['remove_count'] += 1
        elif operation == 'upgrade':
            stats['upgrade_count'] += 1

    def get_operation_stats(self, operation: Optional[str] = None) -> Dict[str, Any]:
        """Get statistics for operations."""
        metrics = self._metrics
        if operation:
            metrics = [m for m in metrics if m.operation == operation]

        if not metrics:
            return {}

        durations = [m.duration for m in metrics]
        success_count = sum(1 for m in metrics if m.success)

        return {
            'total_operations': len(metrics),
            'success_rate': success_count / len(metrics) if metrics else 0,
            'avg_duration': sum(durations) / len(durations) if durations else 0,
            'min_duration': min(durations) if durations else 0,
            'max_duration': max(durations) if durations else 0,
            'operations_by_package': Counter(m.package_name for m in metrics),
        }

    def get_package_usage(self, package_name: str) -> Optional[PackageUsageStats]:
        """Get usage statistics for a specific package."""
        return self._usage_stats.get(package_name)

    def get_most_used_packages(self, limit: int = 10) -> List[Tuple[str, int]]:
        """Get most frequently used packages."""
        package_counts = {
            name: stats['install_count'] +
            stats['remove_count'] + stats['upgrade_count']
            for name, stats in self._usage_stats.items()
        }
        return sorted(package_counts.items(), key=lambda x: x[1], reverse=True)[:limit]

    def get_slowest_operations(self, limit: int = 10) -> List[OperationMetric]:
        """Get slowest operations."""
        return sorted(self._metrics, key=lambda m: m.duration, reverse=True)[:limit]

    def get_recent_failures(self, hours: int = 24) -> List[OperationMetric]:
        """Get recent failed operations."""
        cutoff = datetime.now() - timedelta(hours=hours)
        return [
            m for m in self._metrics
            if not m.success and m.timestamp >= cutoff
        ]

    def get_system_metrics(self) -> SystemMetrics:
        """Get system-wide package metrics."""
        cache_key = "system_metrics"

        # Try to get from cache
        cached = self.cache.get(cache_key)

        if cached is not None:
            return cached

        # This would typically interface with the actual pacman manager
        # For now, we'll return mock data
        metrics = SystemMetrics(
            total_packages=0,
            installed_packages=0,
            orphaned_packages=0,
            outdated_packages=0,
            disk_usage_mb=0.0,
            cache_size_mb=0.0,
        )

        # Store in cache
        self.cache.put(cache_key, metrics, self.ANALYTICS_CACHE_TTL)
        return metrics

    def generate_report(self, include_details: bool = False) -> Dict[str, Any]:
        """Generate a comprehensive analytics report."""
        report = {
            'generated_at': datetime.now().isoformat(),
            'metrics_count': len(self._metrics),
            'tracked_packages': len(self._usage_stats),
            'overall_stats': self.get_operation_stats(),
            'most_used_packages': self.get_most_used_packages(),
            'system_metrics': self.get_system_metrics(),
        }

        if include_details:
            report.update({
                'slowest_operations': [m.to_dict() for m in self.get_slowest_operations()],
                'recent_failures': [m.to_dict() for m in self.get_recent_failures()],
                'operation_breakdown': {
                    op: self.get_operation_stats(op)
                    for op in {'install', 'remove', 'upgrade', 'search'}
                },
            })

        return report

    def export_metrics(self, file_path: Path) -> None:
        """Export metrics to a file."""
        import json

        data = {
            'metrics': [m.to_dict() for m in self._metrics],
            'usage_stats': self._usage_stats,
            'exported_at': datetime.now().isoformat(),
        }

        with open(file_path, 'w', encoding='utf-8') as f:
            json.dump(data, f, indent=2, default=str)

    def import_metrics(self, file_path: Path) -> None:
        """Import metrics from a file."""
        import json

        with open(file_path, 'r', encoding='utf-8') as f:
            data = json.load(f)

        self._metrics = [
            OperationMetric.from_dict(m) for m in data.get('metrics', [])
        ]
        self._usage_stats = data.get('usage_stats', {})

    def clear_metrics(self) -> None:
        """Clear all stored metrics and statistics."""
        self._metrics.clear()
        self._usage_stats.clear()

    async def async_generate_report(self, include_details: bool = False) -> Dict[str, Any]:
        """Asynchronously generate analytics report."""
        loop = asyncio.get_event_loop()
        return await loop.run_in_executor(None, self.generate_report, include_details)

    def __enter__(self) -> PackageAnalytics:
        """Context manager entry."""
        return self

    def __exit__(self, exc_type, exc_val, exc_tb) -> None:
        """Context manager exit."""
        pass  # Analytics don't need cleanup

    async def __aenter__(self) -> PackageAnalytics:
        """Async context manager entry."""
        return self

    async def __aexit__(self, exc_type, exc_val, exc_tb) -> None:
        """Async context manager exit."""
        pass  # Analytics don't need cleanup


# Module-level convenience functions
def create_analytics(cache: Optional[LRUCache[Any]] = None) -> PackageAnalytics:
    """Create a new analytics instance."""
    return PackageAnalytics(cache=cache or LRUCache(1000, 3600))


async def async_create_analytics(cache: Optional[LRUCache[Any]] = None) -> PackageAnalytics:
    """Asynchronously create analytics instance."""
    return create_analytics(cache)
