"""Enhanced core manager class for .NET Framework installations."""

from __future__ import annotations
import asyncio
import hashlib
import platform
import re
import subprocess
import tempfile
import time
import winreg
from contextlib import asynccontextmanager, contextmanager
from functools import lru_cache
from pathlib import Path
from typing import Optional, Protocol, runtime_checkable, AsyncContextManager

import aiohttp
import aiofiles
from loguru import logger
from tqdm import tqdm

from .models import (
    DotNetVersion, HashAlgorithm, SystemInfo, DownloadResult, InstallationResult,
    DotNetManagerError, UnsupportedPlatformError, RegistryAccessError,
    DownloadError, ChecksumError, InstallationError
)


@runtime_checkable
class ProgressCallback(Protocol):
    """Protocol for progress callbacks during downloads."""
    
    def __call__(self, downloaded: int, total: int) -> None: ...


class DotNetManager:
    """Enhanced core class for managing .NET Framework installations."""
    
    # Comprehensive version database with latest known versions
    VERSIONS: dict[str, DotNetVersion] = {
        "v4.8": DotNetVersion(
            key="v4.8",
            name=".NET Framework 4.8",
            release=528040,
            installer_url="https://go.microsoft.com/fwlink/?LinkId=2085155",
            installer_sha256="72398a77fb2c2c00c38c30e34f301e631ec9e745a35c082e3e87cce597d0fcf5",
            min_windows_version="10.0.17134"  # Windows 10 April 2018 Update
        ),
        "v4.7.2": DotNetVersion(
            key="v4.7.2",
            name=".NET Framework 4.7.2",
            release=461808,
            installer_url="https://go.microsoft.com/fwlink/?LinkId=863262",
            installer_sha256="41bc97274e31bd5b1aeaca26abad5fb7b1b99d7b0c654dac02ada6bf7e1a8b0d",
            min_windows_version="10.0.14393"  # Windows 10 Anniversary Update
        ),
        "v4.6.2": DotNetVersion(
            key="v4.6.2",
            name=".NET Framework 4.6.2",
            release=394802,
            installer_url="https://go.microsoft.com/fwlink/?LinkId=780597",
            installer_sha256="8bdf2e3c5ce6ad45f8c3b46b49c5e9b5b1ad4b3baed2b55b01c3e5c2d9b5e5e1",
            min_windows_version="6.1.7601"  # Windows 7 SP1
        ),
    }

    NET_FRAMEWORK_REGISTRY_PATH = r"SOFTWARE\Microsoft\NET Framework Setup\NDP"
    
    def __init__(self, download_dir: Optional[Path] = None) -> None:
        """
        Initialize the .NET Manager with enhanced platform checking.
        
        Args:
            download_dir: Optional custom download directory
            
        Raises:
            UnsupportedPlatformError: If not running on Windows
        """
        current_platform = platform.system()
        if current_platform != "Windows":
            raise UnsupportedPlatformError(current_platform)

        self.download_dir = download_dir or Path(tempfile.gettempdir()) / "dotnet_manager"
        self.download_dir.mkdir(parents=True, exist_ok=True)
        
        logger.info(
            f"Initialized .NET Manager",
            extra={
                "platform": current_platform,
                "download_dir": str(self.download_dir),
                "known_versions": len(self.VERSIONS)
            }
        )

    def get_system_info(self) -> SystemInfo:
        """
        Gather comprehensive information about the current system and installed .NET versions.
        
        Returns:
            SystemInfo object with detailed system and .NET information
        """
        logger.debug("Gathering system information")
        
        try:
            system = platform.uname()
            installed_versions = self.list_installed_versions()
            
            system_info = SystemInfo(
                os_name=system.system,
                os_version=system.version,
                os_build=system.release,
                architecture=system.machine,
                installed_versions=installed_versions
            )
            
            logger.info(
                f"System info gathered: {system_info.installed_version_count} .NET versions found",
                extra={
                    "platform_compatible": system_info.platform_compatible,
                    "architecture": system_info.architecture,
                    "latest_version": (
                        system_info.latest_installed_version.key 
                        if system_info.latest_installed_version else None
                    )
                }
            )
            
            return system_info
            
        except Exception as e:
            logger.error(f"Failed to gather system information: {e}")
            raise DotNetManagerError(
                "Failed to gather system information",
                error_code="SYSTEM_INFO_ERROR",
                original_error=e
            ) from e

    @contextmanager
    def _registry_key(self, key_path: str, access: int = winreg.KEY_READ):
        """
        Context manager for safe registry key access.
        
        Args:
            key_path: Registry key path
            access: Access permissions
            
        Yields:
            Registry key handle
            
        Raises:
            RegistryAccessError: If registry access fails
        """
        try:
            key = winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, key_path, 0, access)
            try:
                yield key
            finally:
                winreg.CloseKey(key)
        except FileNotFoundError as e:
            raise RegistryAccessError(
                f"Registry key not found: {key_path}",
                registry_path=key_path,
                original_error=e
            ) from e
        except OSError as e:
            raise RegistryAccessError(
                f"Failed to access registry key: {key_path}",
                registry_path=key_path,
                original_error=e
            ) from e

    @lru_cache(maxsize=128)
    def _query_registry_value(self, key_path: str, value_name: str) -> Optional[any]:
        """
        Query a registry value with caching for performance.
        
        Args:
            key_path: Registry key path
            value_name: Value name to query
            
        Returns:
            Registry value or None if not found
        """
        try:
            with self._registry_key(key_path) as key:
                value, _ = winreg.QueryValueEx(key, value_name)
                logger.debug(
                    f"Registry value retrieved: {value_name}={value}",
                    extra={"key_path": key_path, "value_name": value_name}
                )
                return value
        except RegistryAccessError:
            logger.debug(f"Registry value not found: {key_path}\\{value_name}")
            return None
        except Exception as e:
            logger.warning(
                f"Failed to query registry value {value_name} at {key_path}: {e}",
                extra={"key_path": key_path, "value_name": value_name}
            )
            return None

    def check_installed(self, version_key: str) -> bool:
        """
        Check if a specific .NET Framework version is installed using enhanced registry access.
        
        Args:
            version_key: Version key to check (e.g., "v4.8")
            
        Returns:
            True if version is installed, False otherwise
            
        Raises:
            DotNetManagerError: If version key is invalid
        """
        logger.debug(f"Checking if .NET version is installed: {version_key}")
        
        version_info = self.VERSIONS.get(version_key)
        if not version_info or not version_info.release:
            raise DotNetManagerError(
                f"Unknown or invalid version key: {version_key}",
                error_code="INVALID_VERSION_KEY",
                version_key=version_key
            )

        try:
            release_path = f"{self.NET_FRAMEWORK_REGISTRY_PATH}\\v4\\Full"
            installed_release = self._query_registry_value(release_path, "Release")
            
            is_installed = (
                isinstance(installed_release, int) and 
                installed_release >= version_info.release
            )
            
            logger.debug(
                f"Version check result: {version_key} = {is_installed}",
                extra={
                    "version_key": version_key,
                    "required_release": version_info.release,
                    "installed_release": installed_release,
                    "is_installed": is_installed
                }
            )
            
            return is_installed
            
        except Exception as e:
            logger.error(f"Failed to check installed version {version_key}: {e}")
            raise DotNetManagerError(
                f"Failed to check if version {version_key} is installed",
                error_code="VERSION_CHECK_ERROR",
                original_error=e,
                version_key=version_key
            ) from e

    def list_installed_versions(self) -> list[DotNetVersion]:
        """
        List all installed .NET Framework versions with enhanced error handling.
        
        Returns:
            List of installed DotNetVersion objects
        """
        logger.debug("Scanning for installed .NET Framework versions")
        
        installed_versions: list[DotNetVersion] = []
        
        try:
            with self._registry_key(self.NET_FRAMEWORK_REGISTRY_PATH) as ndp_key:
                key_count = winreg.QueryInfoKey(ndp_key)[0]
                
                for i in range(key_count):
                    try:
                        version_key_name = winreg.EnumKey(ndp_key, i)
                        if not version_key_name.startswith("v"):
                            continue

                        # Query version information
                        version_path = f"{self.NET_FRAMEWORK_REGISTRY_PATH}\\{version_key_name}"
                        
                        # Try different subkeys for different .NET versions
                        subkeys_to_check = ["", "\\Full", "\\Client"]
                        version_info = None
                        
                        for subkey in subkeys_to_check:
                            full_path = version_path + subkey
                            try:
                                release = self._query_registry_value(full_path, "Release")
                                version_str = self._query_registry_value(full_path, "Version")
                                sp = self._query_registry_value(full_path, "SP")
                                
                                if release or version_str:
                                    version_info = DotNetVersion(
                                        key=version_key_name,
                                        name=f".NET Framework {version_str or version_key_name[1:]}",
                                        release=release,
                                        service_pack=sp
                                    )
                                    break
                            except RegistryAccessError:
                                continue
                        
                        if version_info:
                            installed_versions.append(version_info)
                            logger.debug(f"Found installed version: {version_info}")
                            
                    except Exception as e:
                        logger.warning(f"Error processing registry key {i}: {e}")
                        continue
                        
        except RegistryAccessError:
            logger.info("No .NET Framework installations found in registry")
        except Exception as e:
            logger.error(f"Failed to list installed .NET versions: {e}")
            raise DotNetManagerError(
                "Failed to scan for installed .NET versions",
                error_code="VERSION_SCAN_ERROR",
                original_error=e
            ) from e
        
        # Sort versions by release number
        installed_versions.sort()
        
        logger.info(
            f"Found {len(installed_versions)} installed .NET Framework versions",
            extra={"version_count": len(installed_versions)}
        )
        
        return installed_versions

    async def verify_checksum_async(
        self, 
        file_path: Path, 
        expected_checksum: str, 
        algorithm: HashAlgorithm = HashAlgorithm.SHA256
    ) -> bool:
        """
        Asynchronously verify a file's checksum with enhanced error handling.
        
        Args:
            file_path: Path to file to verify
            expected_checksum: Expected checksum value
            algorithm: Hash algorithm to use
            
        Returns:
            True if checksum matches, False otherwise
            
        Raises:
            ChecksumError: If verification fails due to errors
        """
        logger.debug(
            f"Verifying checksum for {file_path} using {algorithm.value}",
            extra={
                "file_path": str(file_path),
                "algorithm": algorithm.value,
                "expected_checksum": expected_checksum[:16] + "..."  # Log partial checksum
            }
        )
        
        if not file_path.exists():
            raise ChecksumError(
                f"File not found for checksum verification: {file_path}",
                file_path=file_path,
                algorithm=algorithm
            )
        
        try:
            hasher = hashlib.new(algorithm.value)
            file_size = file_path.stat().st_size
            
            async with aiofiles.open(file_path, "rb") as f:
                processed = 0
                while chunk := await f.read(1024 * 1024):  # 1MB chunks
                    hasher.update(chunk)
                    processed += len(chunk)
                    
                    # Log progress for large files
                    if file_size > 50 * 1024 * 1024:  # 50MB
                        progress = (processed / file_size) * 100
                        if processed % (10 * 1024 * 1024) == 0:  # Every 10MB
                            logger.debug(f"Checksum progress: {progress:.1f}%")
            
            actual_checksum = hasher.hexdigest().lower()
            expected_normalized = expected_checksum.lower()
            
            matches = actual_checksum == expected_normalized
            
            logger.debug(
                f"Checksum verification {'passed' if matches else 'failed'}",
                extra={
                    "file_path": str(file_path),
                    "algorithm": algorithm.value,
                    "matches": matches,
                    "actual_checksum": actual_checksum[:16] + "...",
                    "file_size": file_size
                }
            )
            
            return matches
            
        except Exception as e:
            raise ChecksumError(
                f"Failed to verify checksum for {file_path}: {e}",
                file_path=file_path,
                algorithm=algorithm,
                original_error=e
            ) from e

    @asynccontextmanager
    async def _http_session(self) -> AsyncContextManager[aiohttp.ClientSession]:
        """Create HTTP session with appropriate timeouts and settings."""
        timeout = aiohttp.ClientTimeout(total=3600, connect=30)  # 1 hour total, 30s connect
        connector = aiohttp.TCPConnector(limit=10, limit_per_host=2)
        
        async with aiohttp.ClientSession(
            timeout=timeout,
            connector=connector,
            headers={"User-Agent": "dotnet-manager/3.0.0"}
        ) as session:
            yield session

    async def download_file_async(
        self, 
        url: str, 
        output_path: Path, 
        expected_checksum: Optional[str] = None,
        show_progress: bool = True,
        progress_callback: Optional[ProgressCallback] = None
    ) -> DownloadResult:
        """
        Asynchronously download a file with comprehensive error handling and progress tracking.
        
        Args:
            url: URL to download from
            output_path: Path where file should be saved
            expected_checksum: Optional checksum for verification
            show_progress: Whether to show progress bar
            progress_callback: Optional callback for progress updates
            
        Returns:
            DownloadResult with download metadata
            
        Raises:
            DownloadError: If download fails
            ChecksumError: If checksum verification fails
        """
        logger.info(
            f"Starting download: {url}",
            extra={"url": url, "output_path": str(output_path)}
        )
        
        # Check if file already exists with valid checksum
        if (output_path.exists() and expected_checksum and 
            await self.verify_checksum_async(output_path, expected_checksum)):
            logger.info(f"File already exists with matching checksum: {output_path}")
            return DownloadResult(
                path=str(output_path),
                size=output_path.stat().st_size,
                checksum_matched=True
            )
        
        start_time = time.time()
        
        try:
            async with self._http_session() as session:
                async with session.get(url) as response:
                    response.raise_for_status()
                    
                    total_size = int(response.headers.get("content-length", 0))
                    downloaded = 0
                    
                    # Setup progress tracking
                    progress_bar = None
                    if show_progress and total_size > 0:
                        progress_bar = tqdm(
                            total=total_size, 
                            unit="B", 
                            unit_scale=True, 
                            desc=output_path.name,
                            disable=False
                        )
                    
                    # Ensure output directory exists
                    output_path.parent.mkdir(parents=True, exist_ok=True)
                    
                    async with aiofiles.open(output_path, 'wb') as f:
                        async for chunk in response.content.iter_chunked(8192):
                            await f.write(chunk)
                            downloaded += len(chunk)
                            
                            if progress_bar:
                                progress_bar.update(len(chunk))
                            
                            if progress_callback:
                                progress_callback(downloaded, total_size)
                    
                    if progress_bar:
                        progress_bar.close()

            # Calculate download metrics
            end_time = time.time()
            download_time = end_time - start_time
            speed_mbps = (downloaded / (1024 * 1024)) / download_time if download_time > 0 else 0
            
            # Verify checksum if provided
            checksum_matched = None
            if expected_checksum:
                try:
                    checksum_matched = await self.verify_checksum_async(
                        output_path, expected_checksum
                    )
                    if not checksum_matched:
                        output_path.unlink(missing_ok=True)
                        raise ChecksumError(
                            "Downloaded file failed checksum verification",
                            file_path=output_path,
                            expected_checksum=expected_checksum
                        )
                except ChecksumError:
                    raise
                except Exception as e:
                    raise ChecksumError(
                        f"Checksum verification failed: {e}",
                        file_path=output_path,
                        expected_checksum=expected_checksum,
                        original_error=e
                    ) from e

            result = DownloadResult(
                path=str(output_path),
                size=downloaded,
                checksum_matched=checksum_matched,
                download_time_seconds=download_time,
                average_speed_mbps=speed_mbps
            )
            
            logger.info(
                f"Download completed successfully",
                extra={
                    "url": url,
                    "output_path": str(output_path),
                    "size_mb": result.size_mb,
                    "download_time": download_time,
                    "speed_mbps": speed_mbps,
                    "checksum_verified": checksum_matched
                }
            )
            
            return result
            
        except aiohttp.ClientError as e:
            output_path.unlink(missing_ok=True)
            raise DownloadError(
                f"HTTP error downloading {url}: {e}",
                url=url,
                file_path=output_path,
                original_error=e
            ) from e
        except OSError as e:
            output_path.unlink(missing_ok=True)
            raise DownloadError(
                f"File system error downloading to {output_path}: {e}",
                url=url,
                file_path=output_path,
                original_error=e
            ) from e
        except Exception as e:
            output_path.unlink(missing_ok=True)
            raise DownloadError(
                f"Unexpected error downloading {url}: {e}",
                url=url,
                file_path=output_path,
                original_error=e
            ) from e

    def install_software(
        self, 
        installer_path: Path, 
        quiet: bool = False,
        timeout_seconds: int = 3600
    ) -> InstallationResult:
        """
        Execute a software installer with enhanced monitoring and error handling.
        
        Args:
            installer_path: Path to installer executable
            quiet: Whether to run installer silently
            timeout_seconds: Maximum time to wait for installation
            
        Returns:
            InstallationResult with installation details
            
        Raises:
            InstallationError: If installation fails
        """
        logger.info(
            f"Starting installation: {installer_path}",
            extra={
                "installer_path": str(installer_path),
                "quiet": quiet,
                "timeout": timeout_seconds
            }
        )
        
        if not installer_path.exists():
            raise InstallationError(
                f"Installer not found: {installer_path}",
                installer_path=installer_path
            )
        
        try:
            cmd = [str(installer_path)]
            if quiet:
                cmd.extend(["/q", "/norestart"])
            
            # Start the installation process
            process = subprocess.Popen(
                cmd,
                creationflags=subprocess.CREATE_NO_WINDOW,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True
            )
            
            try:
                stdout, stderr = process.communicate(timeout=timeout_seconds)
                return_code = process.returncode
                
                success = return_code == 0
                
                result = InstallationResult(
                    success=success,
                    version_key="unknown",  # Could be determined from installer name
                    installer_path=installer_path,
                    return_code=return_code,
                    error_message=stderr if not success else None
                )
                
                logger.info(
                    f"Installation {'completed' if success else 'failed'}",
                    extra={
                        "installer_path": str(installer_path),
                        "return_code": return_code,
                        "success": success
                    }
                )
                
                return result
                
            except subprocess.TimeoutExpired:
                process.kill()
                raise InstallationError(
                    f"Installation timed out after {timeout_seconds} seconds",
                    installer_path=installer_path
                )
                
        except OSError as e:
            raise InstallationError(
                f"Failed to start installer: {e}",
                installer_path=installer_path,
                original_error=e
            ) from e
        except Exception as e:
            raise InstallationError(
                f"Unexpected error during installation: {e}",
                installer_path=installer_path,
                original_error=e
            ) from e

    def uninstall_dotnet(self, version_key: str) -> bool:
        """
        Attempt to uninstall a specific .NET Framework version.
        
        Note: .NET Framework is a system component and generally cannot be uninstalled directly.
        
        Args:
            version_key: Version to uninstall
            
        Returns:
            False (uninstallation not supported)
        """
        logger.warning(
            f"Uninstall requested for {version_key}, but .NET Framework cannot be uninstalled",
            extra={"version_key": version_key}
        )
        
        logger.warning(
            ".NET Framework is a system component and generally cannot be uninstalled directly."
        )
        logger.warning(
            "Please use the 'Turn Windows features on or off' dialog to manage .NET Framework versions."
        )
        
        return False

    def get_latest_known_version(self) -> Optional[DotNetVersion]:
        """
        Get the latest .NET version known to the manager.
        
        Returns:
            Latest known DotNetVersion or None if no versions are known
        """
        if not self.VERSIONS:
            logger.warning("No known .NET versions available")
            return None
        
        latest = max(self.VERSIONS.values(), key=lambda v: v.release or 0)
        
        logger.debug(
            f"Latest known version: {latest.key}",
            extra={"version_key": latest.key, "release": latest.release}
        )
        
        return latest

    def get_version_info(self, version_key: str) -> Optional[DotNetVersion]:
        """
        Get detailed information about a specific version.
        
        Args:
            version_key: Version key to look up
            
        Returns:
            DotNetVersion object or None if not found
        """
        return self.VERSIONS.get(version_key)

    def list_available_versions(self) -> list[DotNetVersion]:
        """
        List all versions available for download.
        
        Returns:
            List of available DotNetVersion objects
        """
        available = [v for v in self.VERSIONS.values() if v.is_downloadable]
        available.sort(reverse=True)  # Latest first
        
        logger.debug(
            f"Found {len(available)} downloadable versions",
            extra={"available_count": len(available)}
        )
        
        return available