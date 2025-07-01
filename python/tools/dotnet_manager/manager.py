"""Core manager class for .NET Framework installations."""

import asyncio
import hashlib
import platform
import re
import subprocess
import tempfile
import winreg
from pathlib import Path
from typing import List, Optional, Dict

import aiohttp
import aiofiles
from loguru import logger
from tqdm import tqdm

from .models import DotNetVersion, HashAlgorithm, SystemInfo


class DotNetManager:
    """Core class for managing .NET Framework installations."""
    VERSIONS: Dict[str, DotNetVersion] = {
        "v4.8": DotNetVersion(
            key="v4.8",
            name=".NET Framework 4.8",
            release=528040,
            installer_url="https://go.microsoft.com/fwlink/?LinkId=2085155",
            installer_sha256="72398a77fb2c2c00c38c30e34f301e631ec9e745a35c082e3e87cce597d0fcf5",
            min_windows_version="10.0.17134" # Windows 10 April 2018 Update
        ),
        # Add other versions as needed
    }

    NET_FRAMEWORK_REGISTRY_PATH = r"SOFTWARE\Microsoft\NET Framework Setup\NDP"

    def __init__(self, download_dir: Optional[Path] = None):
        if platform.system() != "Windows":
            raise NotImplementedError("This module is designed for Windows systems only")

        self.download_dir = download_dir or Path(tempfile.gettempdir()) / "dotnet_manager"
        self.download_dir.mkdir(parents=True, exist_ok=True)

    def get_system_info(self) -> SystemInfo:
        """Gathers detailed information about the current system and installed .NET versions."""
        system = platform.uname()
        return SystemInfo(
            os_name=system.system,
            os_version=system.version,
            os_build=system.release,
            architecture=system.machine,
            installed_versions=self.list_installed_versions()
        )

    def _query_registry_value(self, key_path: str, value_name: str) -> Optional[any]:
        try:
            with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, key_path) as key:
                value, _ = winreg.QueryValueEx(key, value_name)
                return value
        except FileNotFoundError:
            return None
        except Exception as e:
            logger.warning(f"Failed to query registry value {value_name} at {key_path}: {e}")
            return None

    def check_installed(self, version_key: str) -> bool:
        """Checks if a specific .NET Framework version is installed using direct registry access."""
        version_info = self.VERSIONS.get(version_key)
        if not version_info or not version_info.release:
            logger.warning(f"Unknown or invalid version key: {version_key}")
            return False

        release_path = f"{self.NET_FRAMEWORK_REGISTRY_PATH}\\v4\\Full"
        installed_release = self._query_registry_value(release_path, "Release")

        return isinstance(installed_release, int) and installed_release >= version_info.release

    def list_installed_versions(self) -> List[DotNetVersion]:
        """Lists all installed .NET Framework versions detected in the registry."""
        installed_versions = []
        try:
            with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, self.NET_FRAMEWORK_REGISTRY_PATH) as ndp_key:
                for i in range(winreg.QueryInfoKey(ndp_key)[0]):
                    version_key_name = winreg.EnumKey(ndp_key, i)
                    if not version_key_name.startswith("v"): continue

                    with winreg.OpenKey(ndp_key, version_key_name) as version_key:
                        release = self._query_registry_value(f"{self.NET_FRAMEWORK_REGISTRY_PATH}\\{version_key_name}", "Release")
                        sp = self._query_registry_value(f"{self.NET_FRAMEWORK_REGISTRY_PATH}\\{version_key_name}", "SP")
                        version_name = self._query_registry_value(f"{self.NET_FRAMEWORK_REGISTRY_PATH}\\{version_key_name}", "Version")

                        installed_versions.append(DotNetVersion(
                            key=version_key_name,
                            name=f".NET Framework {version_name or version_key_name[1:]}",
                            release=release,
                            service_pack=sp
                        ))
        except FileNotFoundError:
            pass # No .NET Framework installed
        except Exception as e:
            logger.error(f"Failed to list installed .NET versions: {e}")
        
        return installed_versions

    async def verify_checksum_async(self, file_path: Path, expected_checksum: str, algorithm: HashAlgorithm = HashAlgorithm.SHA256) -> bool:
        """Asynchronously verifies a file's checksum."""
        if not file_path.exists(): return False
        hasher = hashlib.new(algorithm.value)
        async with aiofiles.open(file_path, "rb") as f:
            while chunk := await f.read(1024 * 1024):
                hasher.update(chunk)
        return hasher.hexdigest().lower() == expected_checksum.lower()

    async def download_file_async(self, url: str, output_path: Path, checksum: Optional[str] = None, show_progress: bool = True) -> Path:
        """Asynchronously downloads a file with checksum verification."""
        if output_path.exists() and checksum and await self.verify_checksum_async(output_path, checksum):
            logger.info(f"File {output_path} already exists with matching checksum.")
            return output_path

        logger.info(f"Downloading {url} to {output_path}")
        try:
            async with aiohttp.ClientSession() as session:
                async with session.get(url) as response:
                    response.raise_for_status()
                    total_size = int(response.headers.get("content-length", 0))
                    progress_bar = tqdm(total=total_size, unit="B", unit_scale=True, desc=output_path.name, disable=not show_progress)
                    async with aiofiles.open(output_path, 'wb') as f:
                        while True:
                            chunk = await response.content.read(8192)
                            if not chunk:
                                break
                            await f.write(chunk)
                            progress_bar.update(len(chunk))
                    progress_bar.close()

            if checksum and not await self.verify_checksum_async(output_path, checksum):
                output_path.unlink(missing_ok=True)
                raise ValueError("Downloaded file failed checksum verification")

            return output_path
        except Exception as e:
            output_path.unlink(missing_ok=True)
            raise RuntimeError(f"Failed to download {url}: {e}") from e

    def install_software(self, installer_path: Path, quiet: bool = False) -> bool:
        """Executes a software installer."""
        if not installer_path.exists():
            logger.error(f"Installer not found: {installer_path}")
            return False
        try:
            cmd = [str(installer_path)]
            if quiet:
                cmd.extend(["/q", "/norestart"])
            subprocess.Popen(cmd, creationflags=subprocess.CREATE_NO_WINDOW)
            return True
        except Exception as e:
            logger.error(f"Failed to start installer: {e}")
            return False

    def uninstall_dotnet(self, version_key: str) -> bool:
        """Attempts to uninstall a specific .NET Framework version."""
        logger.warning(".NET Framework is a system component and generally cannot be uninstalled directly.")
        logger.warning("Please use the 'Turn Windows features on or off' dialog to manage .NET Framework versions.")
        return False

    def get_latest_known_version(self) -> Optional[DotNetVersion]:
        """Returns the latest .NET version known to the manager."""
        if not self.VERSIONS:
            return None
        return max(self.VERSIONS.values(), key=lambda v: v.release or 0)