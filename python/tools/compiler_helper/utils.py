#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Utility functions for the compiler helper module.
"""
import json
from pathlib import Path
from typing import Dict, Any

from loguru import logger

from .core_types import PathLike


def load_json(file_path: PathLike) -> Dict[str, Any]:
    """
    Load and parse a JSON file.
    """
    path = Path(file_path)
    if not path.exists():
        raise FileNotFoundError(f"JSON file not found: {path}")

    with open(path, 'r', encoding='utf-8') as f:
        return json.load(f)


def save_json(file_path: PathLike, data: Dict[str, Any], indent: int = 2) -> None:
    """
    Save data to a JSON file.
    """
    path = Path(file_path)
    path.parent.mkdir(parents=True, exist_ok=True)

    with open(path, 'w', encoding='utf-8') as f:
        json.dump(data, f, indent=indent)
