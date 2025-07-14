import asyncio
import json
import tempfile
from datetime import datetime, timedelta
from pathlib import Path
from unittest.mock import Mock, patch
import pytest
from .cache import LRUCache

#!/usr/bin/env python3
"""
Unit tests for the analytics module.
"""


from .analytics import (
    OperationMetric,
    PackageAnalytics,
    PackageUsageStats,
    SystemMetrics,
    async_create_analytics,
    create_analytics,
)


class TestOperationMetric:
    """Test cases for OperationMetric class."""

    def test_operation_metric_creation(self):
        """Test basic OperationMetric creation."""
        timestamp = datetime.now()
        metric = OperationMetric(
            operation="install",
            package_name="test-package",
            duration=1.5,
            success=True,
            timestamp=timestamp,
            memory_usage=100.0,
            cpu_usage=50.0,
        )

        assert metric.operation == "install"
        assert metric.package_name == "test-package"
        assert metric.duration == 1.5
        assert metric.success is True
        assert metric.timestamp == timestamp
        assert metric.memory_usage == 100.0
        assert metric.cpu_usage == 50.0

    def test_operation_metric_to_dict(self):
        """Test OperationMetric serialization to dictionary."""
        timestamp = datetime.now()
        metric = OperationMetric(
            operation="remove",
            package_name="test-pkg",
            duration=2.0,
            success=False,
            timestamp=timestamp,
        )

        result = metric.to_dict()
        expected = {
            'operation': 'remove',
            'package_name': 'test-pkg',
            'duration': 2.0,
            'success': False,
            'timestamp': timestamp.isoformat(),
            'memory_usage': None,
            'cpu_usage': None,
        }

        assert result == expected

    def test_operation_metric_from_dict(self):
        """Test OperationMetric deserialization from dictionary."""
        timestamp = datetime.now()
        data = {
            'operation': 'upgrade',
            'package_name': 'test-package',
            'duration': 3.5,
            'success': True,
            'timestamp': timestamp.isoformat(),
            'memory_usage': 200.0,
            'cpu_usage': 75.0,
        }

        metric = OperationMetric.from_dict(data)

        assert metric.operation == 'upgrade'
        assert metric.package_name == 'test-package'
        assert metric.duration == 3.5
        assert metric.success is True
        assert metric.timestamp == timestamp
        assert metric.memory_usage == 200.0
        assert metric.cpu_usage == 75.0

    def test_operation_metric_from_dict_minimal(self):
        """Test OperationMetric deserialization with minimal data."""
        timestamp = datetime.now()
        data = {
            'operation': 'search',
            'package_name': 'minimal-pkg',
            'duration': 0.5,
            'success': True,
            'timestamp': timestamp.isoformat(),
        }

        metric = OperationMetric.from_dict(data)

        assert metric.operation == 'search'
        assert metric.package_name == 'minimal-pkg'
        assert metric.duration == 0.5
        assert metric.success is True
        assert metric.timestamp == timestamp
        assert metric.memory_usage is None
        assert metric.cpu_usage is None


class TestPackageAnalytics:
    """Test cases for PackageAnalytics class."""

    def test_package_analytics_creation(self):
        """Test PackageAnalytics initialization."""
        cache = LRUCache(100, 1800)
        analytics = PackageAnalytics(cache=cache)

        assert analytics.cache is cache
        assert len(analytics._metrics) == 0
        assert len(analytics._usage_stats) == 0
        assert analytics._start_time is None

    def test_start_operation(self):
        """Test starting an operation."""
        analytics = PackageAnalytics()

        with patch('time.perf_counter', return_value=100.0):
            analytics.start_operation("install", "test-package")

        assert analytics._start_time == 100.0

    def test_end_operation_without_start(self):
        """Test ending an operation without starting."""
        analytics = PackageAnalytics()

        # Should not raise an exception
        analytics.end_operation("install", "test-package", True)

        assert len(analytics._metrics) == 0
        assert len(analytics._usage_stats) == 0

    @patch('time.perf_counter')
    @patch('datetime.datetime')
    def test_end_operation_success(self, mock_datetime, mock_perf_counter):
        """Test successfully ending an operation."""
        mock_now = datetime(2023, 1, 1, 12, 0, 0)
        mock_datetime.now.return_value = mock_now
        mock_perf_counter.side_effect = [100.0, 102.5]  # start, end

        analytics = PackageAnalytics()
        analytics.start_operation("install", "test-package")
        analytics.end_operation("install", "test-package", True)

        assert len(analytics._metrics) == 1
        metric = analytics._metrics[0]
        assert metric.operation == "install"
        assert metric.package_name == "test-package"
        assert metric.duration == 2.5
        assert metric.success is True
        assert metric.timestamp == mock_now

        # Check usage stats
        assert "test-package" in analytics._usage_stats
        stats = analytics._usage_stats["test-package"]
        assert stats['install_count'] == 1
        assert stats['total_install_time'] == 2.5
        assert stats['avg_install_time'] == 2.5

    def test_add_metric_max_size_limit(self):
        """Test that metrics are limited to MAX_METRICS size."""
        analytics = PackageAnalytics()
        original_max = PackageAnalytics.MAX_METRICS
        PackageAnalytics.MAX_METRICS = 5  # Temporarily set low limit

        try:
            # Add more metrics than the limit
            for i in range(10):
                metric = OperationMetric(
                    operation="test",
                    package_name=f"pkg-{i}",
                    duration=1.0,
                    success=True,
                    timestamp=datetime.now(),
                )
                analytics._add_metric(metric)

            # Should keep only half when limit exceeded
            assert len(analytics._metrics) == PackageAnalytics.MAX_METRICS // 2

        finally:
            PackageAnalytics.MAX_METRICS = original_max

    def test_update_usage_stats_install(self):
        """Test usage stats update for install operation."""
        analytics = PackageAnalytics()

        # First install
        analytics._update_usage_stats("install", "test-pkg", 2.0)
        stats = analytics._usage_stats["test-pkg"]
        assert stats['install_count'] == 1
        assert stats['total_install_time'] == 2.0
        assert stats['avg_install_time'] == 2.0

        # Second install
        analytics._update_usage_stats("install", "test-pkg", 3.0)
        stats = analytics._usage_stats["test-pkg"]
        assert stats['install_count'] == 2
        assert stats['total_install_time'] == 5.0
        assert stats['avg_install_time'] == 2.5

    def test_update_usage_stats_other_operations(self):
        """Test usage stats update for remove and upgrade operations."""
        analytics = PackageAnalytics()

        analytics._update_usage_stats("remove", "test-pkg", 1.0)
        analytics._update_usage_stats("upgrade", "test-pkg", 1.5)

        stats = analytics._usage_stats["test-pkg"]
        assert stats['remove_count'] == 1
        assert stats['upgrade_count'] == 1
        assert stats['install_count'] == 0

    def test_get_operation_stats_empty(self):
        """Test getting operation stats when no metrics exist."""
        analytics = PackageAnalytics()

        stats = analytics.get_operation_stats()
        assert stats == {}

        stats = analytics.get_operation_stats("install")
        assert stats == {}

    def test_get_operation_stats_with_data(self):
        """Test getting operation stats with existing metrics."""
        analytics = PackageAnalytics()

        # Add some test metrics
        metrics = [
            OperationMetric("install", "pkg1", 1.0, True, datetime.now()),
            OperationMetric("install", "pkg2", 2.0, True, datetime.now()),
            OperationMetric("install", "pkg1", 1.5, False, datetime.now()),
            OperationMetric("remove", "pkg1", 0.5, True, datetime.now()),
        ]
        analytics._metrics = metrics

        # Test overall stats
        stats = analytics.get_operation_stats()
        assert stats['total_operations'] == 4
        assert stats['success_rate'] == 0.75  # 3 out of 4 successful
        assert stats['avg_duration'] == 1.25  # (1.0 + 2.0 + 1.5 + 0.5) / 4
        assert stats['min_duration'] == 0.5
        assert stats['max_duration'] == 2.0
        assert stats['operations_by_package']['pkg1'] == 3
        assert stats['operations_by_package']['pkg2'] == 1

        # Test filtered stats
        install_stats = analytics.get_operation_stats("install")
        assert install_stats['total_operations'] == 3
        assert install_stats['success_rate'] == 2/3

    def test_get_package_usage(self):
        """Test getting usage statistics for a specific package."""
        analytics = PackageAnalytics()

        # Non-existent package
        assert analytics.get_package_usage("non-existent") is None

        # Create usage stats
        analytics._update_usage_stats("install", "test-pkg", 2.0)
        stats = analytics.get_package_usage("test-pkg")

        assert stats is not None
        assert stats['install_count'] == 1

    def test_get_most_used_packages(self):
        """Test getting most frequently used packages."""
        analytics = PackageAnalytics()

        # Create usage stats for multiple packages
        analytics._update_usage_stats("install", "pkg1", 1.0)
        analytics._update_usage_stats("install", "pkg1", 1.0)
        analytics._update_usage_stats("remove", "pkg1", 1.0)

        analytics._update_usage_stats("install", "pkg2", 1.0)
        analytics._update_usage_stats("upgrade", "pkg2", 1.0)

        analytics._update_usage_stats("install", "pkg3", 1.0)

        most_used = analytics.get_most_used_packages(limit=2)

        assert len(most_used) == 2
        assert most_used[0] == ("pkg1", 3)  # 2 installs + 1 remove
        assert most_used[1] == ("pkg2", 2)  # 1 install + 1 upgrade

    def test_get_slowest_operations(self):
        """Test getting slowest operations."""
        analytics = PackageAnalytics()

        metrics = [
            OperationMetric("install", "pkg1", 1.0, True, datetime.now()),
            OperationMetric("install", "pkg2", 3.0, True, datetime.now()),
            OperationMetric("install", "pkg3", 2.0, True, datetime.now()),
        ]
        analytics._metrics = metrics

        slowest = analytics.get_slowest_operations(limit=2)

        assert len(slowest) == 2
        assert slowest[0].duration == 3.0
        assert slowest[1].duration == 2.0

    def test_get_recent_failures(self):
        """Test getting recent failed operations."""
        analytics = PackageAnalytics()

        now = datetime.now()
        old_time = now - timedelta(hours=25)  # Older than 24 hours
        recent_time = now - timedelta(hours=12)  # Within 24 hours

        metrics = [
            OperationMetric("install", "pkg1", 1.0, False, old_time),
            OperationMetric("install", "pkg2", 1.0, False, recent_time),
            OperationMetric("install", "pkg3", 1.0, True,
                            recent_time),  # Success
        ]
        analytics._metrics = metrics

        failures = analytics.get_recent_failures(hours=24)

        assert len(failures) == 1
        assert failures[0].package_name == "pkg2"

    def test_get_system_metrics_cached(self):
        """Test getting system metrics from cache."""
        cache = Mock()
        cached_metrics = SystemMetrics(
            total_packages=100,
            installed_packages=80,
            orphaned_packages=5,
            outdated_packages=10,
            disk_usage_mb=500.0,
            cache_size_mb=50.0,
        )
        cache.get.return_value = cached_metrics

        analytics = PackageAnalytics(cache=cache)
        result = analytics.get_system_metrics()

        assert result == cached_metrics
        cache.get.assert_called_once_with("system_metrics")

    def test_get_system_metrics_not_cached(self):
        """Test getting system metrics when not cached."""
        cache = Mock()
        cache.get.return_value = None

        analytics = PackageAnalytics(cache=cache)
        result = analytics.get_system_metrics()

        # Should return mock data and cache it
        assert isinstance(result, dict)
        assert 'total_packages' in result
        cache.put.assert_called_once()

    def test_generate_report_basic(self):
        """Test basic report generation."""
        analytics = PackageAnalytics()

        # Add some test data
        analytics._metrics = [
            OperationMetric("install", "pkg1", 1.0, True, datetime.now())
        ]
        analytics._usage_stats = {"pkg1": PackageUsageStats(
            install_count=1, remove_count=0, upgrade_count=0,
            last_accessed=datetime.now(), avg_install_time=1.0, total_install_time=1.0
        )}

        report = analytics.generate_report(include_details=False)

        assert 'generated_at' in report
        assert report['metrics_count'] == 1
        assert report['tracked_packages'] == 1
        assert 'overall_stats' in report
        assert 'most_used_packages' in report
        assert 'system_metrics' in report

        # Should not include details
        assert 'slowest_operations' not in report
        assert 'recent_failures' not in report

    def test_generate_report_detailed(self):
        """Test detailed report generation."""
        analytics = PackageAnalytics()

        # Add test data
        analytics._metrics = [
            OperationMetric("install", "pkg1", 1.0, True, datetime.now())
        ]

        report = analytics.generate_report(include_details=True)

        assert 'slowest_operations' in report
        assert 'recent_failures' in report
        assert 'operation_breakdown' in report

    def test_export_import_metrics(self):
        """Test exporting and importing metrics."""
        analytics = PackageAnalytics()

        # Add test data
        metric = OperationMetric("install", "pkg1", 1.0, True, datetime.now())
        analytics._metrics = [metric]
        analytics._usage_stats = {"pkg1": PackageUsageStats(
            install_count=1, remove_count=0, upgrade_count=0,
            last_accessed=datetime.now(), avg_install_time=1.0, total_install_time=1.0
        )}

        with tempfile.NamedTemporaryFile(mode='w', suffix='.json', delete=False) as f:
            temp_path = Path(f.name)

        try:
            # Export
            analytics.export_metrics(temp_path)

            # Clear analytics
            analytics.clear_metrics()
            assert len(analytics._metrics) == 0
            assert len(analytics._usage_stats) == 0

            # Import
            analytics.import_metrics(temp_path)

            assert len(analytics._metrics) == 1
            assert analytics._metrics[0].operation == "install"
            assert "pkg1" in analytics._usage_stats

        finally:
            temp_path.unlink(missing_ok=True)

    def test_clear_metrics(self):
        """Test clearing all metrics and statistics."""
        analytics = PackageAnalytics()

        # Add some data
        analytics._metrics = [
            OperationMetric("install", "pkg1", 1.0, True, datetime.now())
        ]
        analytics._usage_stats = {
            "pkg1": PackageUsageStats(
                install_count=0,
                remove_count=0,
                upgrade_count=0,
                last_accessed=datetime.now(),
                avg_install_time=0.0,
                total_install_time=0.0
            )
        }

        analytics.clear_metrics()

        assert len(analytics._metrics) == 0
        assert len(analytics._usage_stats) == 0

    @pytest.mark.asyncio
    async def test_async_generate_report(self):
        """Test asynchronous report generation."""
        analytics = PackageAnalytics()

        report = await analytics.async_generate_report()

        assert isinstance(report, dict)
        assert 'generated_at' in report

    def test_context_manager_sync(self):
        """Test synchronous context manager."""
        with PackageAnalytics() as analytics:
            assert isinstance(analytics, PackageAnalytics)

    @pytest.mark.asyncio
    async def test_context_manager_async(self):
        """Test asynchronous context manager."""
        async with PackageAnalytics() as analytics:
            assert isinstance(analytics, PackageAnalytics)


class TestModuleFunctions:
    """Test module-level convenience functions."""

    def test_create_analytics_default(self):
        """Test creating analytics with default cache."""
        analytics = create_analytics()

        assert isinstance(analytics, PackageAnalytics)
        assert isinstance(analytics.cache, LRUCache)

    def test_create_analytics_custom_cache(self):
        """Test creating analytics with custom cache."""
        custom_cache = LRUCache(500, 1800)
        analytics = create_analytics(cache=custom_cache)

        assert analytics.cache is custom_cache

    @pytest.mark.asyncio
    async def test_async_create_analytics(self):
        """Test asynchronous analytics creation."""
        analytics = await async_create_analytics()

        assert isinstance(analytics, PackageAnalytics)
        assert isinstance(analytics.cache, LRUCache)

        @patch('time.perf_counter')
        @patch('datetime.datetime')
        def test_end_operation_failure(self, mock_datetime, mock_perf_counter):
            """Test ending an operation with failure."""
            mock_now = datetime(2023, 1, 1, 12, 0, 0)
            mock_datetime.now.return_value = mock_now
            mock_perf_counter.side_effect = [200.0, 203.0]  # start, end

            analytics = PackageAnalytics()
            analytics.start_operation("remove", "test-package-fail")
            analytics.end_operation("remove", "test-package-fail", False)

            assert len(analytics._metrics) == 1
            metric = analytics._metrics[0]
            assert metric.operation == "remove"
            assert metric.package_name == "test-package-fail"
            assert metric.duration == 3.0
            assert metric.success is False
            assert metric.timestamp == mock_now

            # Check usage stats - should still update counts even on failure
            assert "test-package-fail" in analytics._usage_stats
            stats = analytics._usage_stats["test-package-fail"]
            assert stats['remove_count'] == 1
            # Ensure install count is not affected
            assert stats['install_count'] == 0

        def test_add_metric_no_exceed_limit(self):
            """Test that metrics are added correctly when not exceeding MAX_METRICS."""
            analytics = PackageAnalytics()
            original_max = PackageAnalytics.MAX_METRICS
            PackageAnalytics.MAX_METRICS = 10  # Temporarily set a limit

            try:
                for i in range(5):
                    metric = OperationMetric(
                        operation="test",
                        package_name=f"pkg-{i}",
                        duration=1.0,
                        success=True,
                        timestamp=datetime.now(),
                    )
                    analytics._add_metric(metric)

                assert len(analytics._metrics) == 5
                assert analytics._metrics[0].package_name == "pkg-0"

            finally:
                PackageAnalytics.MAX_METRICS = original_max

        def test_add_metric_exceed_limit_multiple_times(self):
            """Test that metrics are trimmed correctly after exceeding limit multiple times."""
            analytics = PackageAnalytics()
            original_max = PackageAnalytics.MAX_METRICS
            PackageAnalytics.MAX_METRICS = 10  # Temporarily set a limit

            try:
                # First exceed
                for i in range(12):  # 12 > 10, should trim to 5
                    metric = OperationMetric(
                        operation="test",
                        package_name=f"pkg-{i}",
                        duration=1.0,
                        success=True,
                        timestamp=datetime.now(),
                    )
                    analytics._add_metric(metric)
                assert len(analytics._metrics) == 6  # 12 // 2 = 6, not 5

                # Second exceed
                for i in range(6, 15):  # 6 + 9 = 15 > 10, should trim again
                    metric = OperationMetric(
                        operation="test",
                        package_name=f"pkg-{i}",
                        duration=1.0,
                        success=True,
                        timestamp=datetime.now(),
                    )
                    analytics._add_metric(metric)
                assert len(analytics._metrics) == 7  # 15 // 2 = 7

            finally:
                PackageAnalytics.MAX_METRICS = original_max

        def test_update_usage_stats_new_package(self):
            """Test that a new package is correctly added to usage stats."""
            analytics = PackageAnalytics()
            analytics._update_usage_stats("install", "new-pkg", 5.0)
            stats = analytics._usage_stats["new-pkg"]
            assert stats['install_count'] == 1
            assert stats['remove_count'] == 0
            assert stats['upgrade_count'] == 0
            assert stats['total_install_time'] == 5.0
            assert stats['avg_install_time'] == 5.0
            assert isinstance(stats['last_accessed'], datetime)

        def test_update_usage_stats_last_accessed_update(self):
            """Test that last_accessed is always updated."""
            analytics = PackageAnalytics()
            analytics._update_usage_stats("install", "pkg-time", 1.0)
            first_access = analytics._usage_stats["pkg-time"]['last_accessed']

            # Simulate time passing
            with patch('datetime.datetime') as mock_dt:
                mock_dt.now.return_value = first_access + timedelta(minutes=5)
                analytics._update_usage_stats("remove", "pkg-time", 0.5)
                second_access = analytics._usage_stats["pkg-time"]['last_accessed']
                assert second_access > first_access

        def test_get_operation_stats_single_metric(self):
            """Test get_operation_stats with a single metric."""
            analytics = PackageAnalytics()
            metric = OperationMetric(
                "install", "pkg1", 1.0, True, datetime.now())
            analytics._metrics = [metric]

            stats = analytics.get_operation_stats()
            assert stats['total_operations'] == 1
            assert stats['success_rate'] == 1.0
            assert stats['avg_duration'] == 1.0
            assert stats['min_duration'] == 1.0
            assert stats['max_duration'] == 1.0
            assert stats['operations_by_package']['pkg1'] == 1

        def test_get_operation_stats_no_durations(self):
            """Test get_operation_stats when no durations are present (shouldn't happen with current logic)."""
            analytics = PackageAnalytics()
            # Manually create a metric list that would result in no durations
            analytics._metrics = []
            stats = analytics.get_operation_stats()
            assert stats == {}

        def test_get_package_usage_non_existent(self):
            """Test get_package_usage for a package that doesn't exist."""
            analytics = PackageAnalytics()
            assert analytics.get_package_usage("non-existent-package") is None

        def test_get_most_used_packages_empty(self):
            """Test get_most_used_packages with no usage data."""
            analytics = PackageAnalytics()
            assert analytics.get_most_used_packages() == []

        def test_get_most_used_packages_limit(self):
            """Test get_most_used_packages with a limit."""
            analytics = PackageAnalytics()
            analytics._update_usage_stats("install", "pkg1", 1.0)
            analytics._update_usage_stats("install", "pkg2", 1.0)
            analytics._update_usage_stats("install", "pkg3", 1.0)
            analytics._update_usage_stats("install", "pkg4", 1.0)

            most_used = analytics.get_most_used_packages(limit=2)
            assert len(most_used) == 2
            assert most_used[0][0] in ["pkg1", "pkg2", "pkg3",
                                       "pkg4"]  # Order might vary for equal counts
            assert most_used[1][0] in ["pkg1", "pkg2", "pkg3", "pkg4"]
            assert most_used[0][1] == 1
            assert most_used[1][1] == 1

        def test_get_slowest_operations_empty(self):
            """Test get_slowest_operations with no metrics."""
            analytics = PackageAnalytics()
            assert analytics.get_slowest_operations() == []

        def test_get_slowest_operations_limit(self):
            """Test get_slowest_operations with a limit."""
            analytics = PackageAnalytics()
            metrics = [
                OperationMetric("op", "p1", 5.0, True, datetime.now()),
                OperationMetric("op", "p2", 1.0, True, datetime.now()),
                OperationMetric("op", "p3", 8.0, True, datetime.now()),
                OperationMetric("op", "p4", 3.0, True, datetime.now()),
            ]
            analytics._metrics = metrics

            slowest = analytics.get_slowest_operations(limit=2)
            assert len(slowest) == 2
            assert slowest[0].duration == 8.0
            assert slowest[1].duration == 5.0

        def test_get_recent_failures_empty(self):
            """Test get_recent_failures with no failures."""
            analytics = PackageAnalytics()
            assert analytics.get_recent_failures() == []

        def test_get_recent_failures_no_recent(self):
            """Test get_recent_failures when failures are too old."""
            analytics = PackageAnalytics()
            old_time = datetime.now() - timedelta(days=5)
            analytics._metrics = [
                OperationMetric("op", "p1", 1.0, False, old_time),
            ]
            failures = analytics.get_recent_failures(hours=24)
            assert len(failures) == 0

        def test_get_system_metrics_cache_expiration(self):
            """Test that system metrics cache expires."""
            cache = Mock()
            analytics = PackageAnalytics(cache=cache)

            # Simulate cached data
            cached_metrics = SystemMetrics(
                total_packages=100, installed_packages=80, orphaned_packages=5,
                outdated_packages=10, disk_usage_mb=500.0, cache_size_mb=50.0,
            )
            cache.get.return_value = cached_metrics

            # First call, should hit cache
            result1 = analytics.get_system_metrics()
            assert result1 == cached_metrics
            cache.get.assert_called_once_with("system_metrics")

            # Simulate cache miss (e.g., TTL expired)
            cache.get.return_value = None
            cache.put.reset_mock()  # Reset put mock for next assertion

            result2 = analytics.get_system_metrics()
            assert result2 != cached_metrics  # Should be new mock data
            cache.put.assert_called_once()  # Should have put new data into cache

        def test_generate_report_empty_data(self):
            """Test report generation with no metrics or usage stats."""
            analytics = PackageAnalytics()
            report = analytics.generate_report()

            assert report['metrics_count'] == 0
            assert report['tracked_packages'] == 0
            assert report['overall_stats'] == {}
            assert report['most_used_packages'] == []
            assert 'system_metrics' in report  # System metrics always return mock data

        def test_export_import_metrics_empty(self):
            """Test exporting and importing when no metrics exist."""
            analytics = PackageAnalytics()

            with tempfile.NamedTemporaryFile(mode='w', suffix='.json', delete=False) as f:
                temp_path = Path(f.name)

            try:
                analytics.export_metrics(temp_path)

                analytics_new = PackageAnalytics()
                analytics_new.import_metrics(temp_path)

                assert len(analytics_new._metrics) == 0
                assert len(analytics_new._usage_stats) == 0

            finally:
                temp_path.unlink(missing_ok=True)

        def test_export_import_metrics_multiple_metrics(self):
            """Test exporting and importing multiple metrics."""
            analytics = PackageAnalytics()

            # Add multiple metrics
            analytics.end_operation("install", "pkg1", True)
            analytics.end_operation("remove", "pkg2", False)
            analytics.end_operation("upgrade", "pkg1", True)

            with tempfile.NamedTemporaryFile(mode='w', suffix='.json', delete=False) as f:
                temp_path = Path(f.name)

            try:
                analytics.export_metrics(temp_path)

                analytics_new = PackageAnalytics()
                analytics_new.import_metrics(temp_path)

                assert len(analytics_new._metrics) == 3
                assert "pkg1" in analytics_new._usage_stats
                assert "pkg2" in analytics_new._usage_stats

                # Verify specific metric data
                metric_ops = [m.operation for m in analytics_new._metrics]
                assert "install" in metric_ops
                assert "remove" in metric_ops
                assert "upgrade" in metric_ops

            finally:
                temp_path.unlink(missing_ok=True)

        def test_clear_metrics_empty(self):
            """Test clearing metrics when already empty."""
            analytics = PackageAnalytics()
            analytics.clear_metrics()
            assert len(analytics._metrics) == 0
            assert len(analytics._usage_stats) == 0

        @pytest.mark.asyncio
        async def test_async_generate_report_with_details(self):
            """Test asynchronous report generation with details."""
            analytics = PackageAnalytics()
            analytics.end_operation("install", "pkg_async", True)

            report = await analytics.async_generate_report(include_details=True)

            assert isinstance(report, dict)
            assert 'generated_at' in report
            assert 'slowest_operations' in report
            assert 'recent_failures' in report
            assert 'operation_breakdown' in report
            assert report['metrics_count'] == 1

        def test_create_analytics_cache_none(self):
            """Test create_analytics when cache is explicitly None."""
            analytics = create_analytics(cache=None)
            assert isinstance(analytics, PackageAnalytics)
            assert isinstance(analytics.cache, LRUCache)

        @pytest.mark.asyncio
        async def test_async_create_analytics_cache_none(self):
            """Test async_create_analytics when cache is explicitly None."""
            analytics = await async_create_analytics(cache=None)
            assert isinstance(analytics, PackageAnalytics)
            assert isinstance(analytics.cache, LRUCache)
