#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
File: options.py
Author: Max Qian <astro_air@126.com>
Enhanced: 2025-07-01
Version: 2.1

Description:
------------
Classes for handling conversion options in the convert_to_header package.
"""

import json
from dataclasses import dataclass, field, asdict
from typing import Optional, List, Dict, Any
from pathlib import Path

from loguru import logger
from .utils import PathLike, DataFormat, CommentStyle, CompressionType, ChecksumAlgo


@dataclass
class ConversionOptions:
    """Data class for storing conversion options."""
    # Content options
    array_name: str = "resource_data"
    size_name: str = "resource_size"
    array_type: str = "unsigned char"
    const_qualifier: str = "const"
    include_size_var: bool = True

    # Format options
    data_format: DataFormat = "hex"
    comment_style: CommentStyle = "C"
    line_width: int = 80
    indent_size: int = 4
    items_per_line: int = 12

    # Processing options
    compression: CompressionType = "none"
    start_offset: int = 0
    end_offset: Optional[int] = None
    include_checksum: bool = False
    verify_checksum: bool = False  # For to_file mode
    checksum_algorithm: ChecksumAlgo = "sha256"

    # Output structure options
    add_include_guard: bool = True
    add_header_comment: bool = True
    include_timestamp: bool = True
    cpp_namespace: Optional[str] = None
    cpp_class: bool = False
    cpp_class_name: Optional[str] = None
    split_size: Optional[int] = None

    # Advanced options
    extra_includes: List[str] = field(default_factory=list)
    custom_header: Optional[str] = None
    custom_footer: Optional[str] = None

    def to_dict(self) -> Dict[str, Any]:
        """Convert options to dictionary."""
        return asdict(self)

    @classmethod
    def from_dict(cls, options_dict: Dict[str, Any]) -> 'ConversionOptions':
        """Create ConversionOptions from dictionary."""
        valid_keys = {f.name for f in cls.__dataclass_fields__.values()}
        filtered_dict = {k: v for k, v in options_dict.items()
                         if k in valid_keys}
        return cls(**filtered_dict)

    @classmethod
    def from_json(cls, json_file: PathLike) -> 'ConversionOptions':
        """Load options from JSON file."""
        try:
            with open(json_file, 'r', encoding='utf-8') as f:
                options_dict = json.load(f)
            return cls.from_dict(options_dict)
        except Exception as e:
            logger.error(f"Failed to load options from JSON file: {e}")
            raise

    @classmethod
    def from_yaml(cls, yaml_file: PathLike) -> 'ConversionOptions':
        """Load options from YAML file."""
        try:
            import yaml
            with open(yaml_file, 'r', encoding='utf-8') as f:
                options_dict = yaml.safe_load(f)
            return cls.from_dict(options_dict)
        except ImportError:
            logger.error("YAML support requires PyYAML. Install with 'pip install "convert_to_header[yaml]"'")
            raise
        except Exception as e:
            logger.error(f"Failed to load options from YAML file: {e}")
            raise
