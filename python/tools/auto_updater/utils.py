# utils.py
from pathlib import Path
import hashlib
from typing import Tuple

from .types import HashType
from .logger import logger


def parse_version(version_str: str) -> Tuple[int, ...]:
    """
    Parse a version string into a tuple of integers for comparison.

    Args:
        version_str: Version string (e.g., "1.2.3")

    Returns:
        Tuple of integers representing the version components

    Example:
        >>> parse_version("1.2.3")
        (1, 2, 3)
    """
    # Extract numeric components while handling non-numeric parts
    components = []
    for part in version_str.split('.'):
        # Extract digits from the beginning of each part
        digits = ''
        for char in part:
            if char.isdigit():
                digits += char
            else:
                break

        # Convert to integer, default to 0 if empty
        components.append(int(digits) if digits else 0)

    return tuple(components)


def compare_versions(current: str, latest: str) -> int:
    """
    Compare two version strings.

    Args:
        current: Current version string
        latest: Latest version string

    Returns:
        -1 if current < latest, 0 if equal, 1 if current > latest
    """
    current_tuple = parse_version(current)
    latest_tuple = parse_version(latest)

    if current_tuple < latest_tuple:
        return -1
    elif current_tuple > latest_tuple:
        return 1
    else:
        return 0


def calculate_file_hash(file_path: Path, algorithm: HashType = "sha256") -> str:
    """
    Calculate the hash of a file.

    Args:
        file_path: Path to the file
        algorithm: Hash algorithm to use

    Returns:
        Hexadecimal hash string
    """
    hash_func = getattr(hashlib, algorithm)()

    with open(file_path, 'rb') as f:
        for chunk in iter(lambda: f.read(4096), b""):
            hash_func.update(chunk)

    return hash_func.hexdigest()
