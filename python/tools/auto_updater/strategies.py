# strategies.py
"""Defines strategies for checking for updates from various sources."""

from typing import Optional
import aiohttp
from pydantic import HttpUrl

from .models import UpdateInfo, NetworkError
from .utils import compare_versions


class JsonUpdateStrategy:
    """Checks for updates by fetching a JSON file from a URL."""

    def __init__(self, url: HttpUrl):
        self.url = url

    async def check_for_updates(self, current_version: str) -> Optional[UpdateInfo]:
        """Fetches update information and compares versions."""
        try:
            async with aiohttp.ClientSession() as session:
                async with session.get(str(self.url)) as response:
                    response.raise_for_status()
                    data = await response.json()
                    update_info = UpdateInfo(**data)

                    if compare_versions(current_version, update_info.version) < 0:
                        return update_info
                    return None
        except aiohttp.ClientError as e:
            raise NetworkError(
                f"Failed to fetch update info from {self.url}: {e}"
            ) from e
        except Exception as e:
            raise NetworkError(
                f"An unexpected error occurred while checking for updates: {e}"
            ) from e
