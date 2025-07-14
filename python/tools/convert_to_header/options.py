#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
File: options.py
Author: Max Qian <astro_air@126.com>
Enhanced: 2025-07-12
Version: 2.2

Description:
------------
Enhanced configuration classes with validation and modern Python features.
"""

from __future__ import annotations
import json
from dataclasses import dataclass, field, asdict
from typing import Optional, Any, ClassVar
from pathlib import Path

from loguru import logger
from .utils import (
    PathLike, DataFormat, CommentStyle, CompressionType, ChecksumAlgo,
    validate_data_format, validate_compression_type, validate_checksum_algorithm,
    sanitize_identifier
)
from .exceptions import ValidationError


@dataclass
class ConversionOptions:
    """
    Enhanced data class for storing conversion options with validation.
    
    This class provides comprehensive configuration options for the conversion
    process with built-in validation and type safety.
    """
    
    # Class-level constants for validation
    MIN_ITEMS_PER_LINE: ClassVar[int] = 1
    MAX_ITEMS_PER_LINE: ClassVar[int] = 50
    MIN_LINE_WIDTH: ClassVar[int] = 40
    MAX_LINE_WIDTH: ClassVar[int] = 200
    MIN_INDENT_SIZE: ClassVar[int] = 0
    MAX_INDENT_SIZE: ClassVar[int] = 16
    
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
    include_original_filename: bool = True
    cpp_namespace: Optional[str] = None
    cpp_class: bool = False
    cpp_class_name: Optional[str] = None
    split_size: Optional[int] = None

    # Advanced options
    extra_includes: list[str] = field(default_factory=list)
    custom_header: Optional[str] = None
    custom_footer: Optional[str] = None
    
    def __post_init__(self) -> None:
        """Validate options after initialization."""
        self._validate_all()
    
    def _validate_all(self) -> None:
        """Perform comprehensive validation of all options."""
        try:
            # Validate basic types and ranges
            self._validate_names()
            self._validate_numeric_ranges()
            self._validate_enum_types()
            self._validate_offsets()
            self._validate_cpp_options()
            
        except (ValueError, TypeError) as e:
            raise ValidationError(f"Invalid configuration: {e}") from e
    
    def _validate_names(self) -> None:
        """Validate array and variable names."""
        if not self.array_name or not self.array_name.strip():
            raise ValidationError("array_name cannot be empty")
        
        if not self.size_name or not self.size_name.strip():
            raise ValidationError("size_name cannot be empty")
        
        # Sanitize names to ensure they're valid C identifiers
        self.array_name = sanitize_identifier(self.array_name.strip())
        self.size_name = sanitize_identifier(self.size_name.strip())
        
        if self.array_name == self.size_name:
            raise ValidationError("array_name and size_name must be different")
    
    def _validate_numeric_ranges(self) -> None:
        """Validate numeric parameters are within acceptable ranges."""
        if not self.MIN_ITEMS_PER_LINE <= self.items_per_line <= self.MAX_ITEMS_PER_LINE:
            raise ValidationError(
                f"items_per_line must be between {self.MIN_ITEMS_PER_LINE} and {self.MAX_ITEMS_PER_LINE}"
            )
        
        if not self.MIN_LINE_WIDTH <= self.line_width <= self.MAX_LINE_WIDTH:
            raise ValidationError(
                f"line_width must be between {self.MIN_LINE_WIDTH} and {self.MAX_LINE_WIDTH}"
            )
        
        if not self.MIN_INDENT_SIZE <= self.indent_size <= self.MAX_INDENT_SIZE:
            raise ValidationError(
                f"indent_size must be between {self.MIN_INDENT_SIZE} and {self.MAX_INDENT_SIZE}"
            )
        
        if self.start_offset < 0:
            raise ValidationError("start_offset cannot be negative")
        
        if self.split_size is not None and self.split_size <= 0:
            raise ValidationError("split_size must be positive if specified")
    
    def _validate_enum_types(self) -> None:
        """Validate enum-like string parameters."""
        # These will raise ValueError if invalid, which gets caught by _validate_all
        self.data_format = validate_data_format(self.data_format)
        self.compression = validate_compression_type(self.compression)
        self.checksum_algorithm = validate_checksum_algorithm(self.checksum_algorithm)
        
        if self.comment_style not in ("C", "CPP"):
            raise ValidationError(f"Invalid comment_style: {self.comment_style}")
    
    def _validate_offsets(self) -> None:
        """Validate offset parameters."""
        if self.end_offset is not None:
            if self.end_offset <= self.start_offset:
                raise ValidationError("end_offset must be greater than start_offset")
    
    def _validate_cpp_options(self) -> None:
        """Validate C++ specific options."""
        if self.cpp_namespace is not None:
            self.cpp_namespace = sanitize_identifier(self.cpp_namespace.strip())
            if not self.cpp_namespace:
                raise ValidationError("cpp_namespace cannot be empty if specified")
        
        if self.cpp_class_name is not None:
            self.cpp_class_name = sanitize_identifier(self.cpp_class_name.strip())
            if not self.cpp_class_name:
                raise ValidationError("cpp_class_name cannot be empty if specified")

    def to_dict(self) -> dict[str, Any]:
        """Convert options to dictionary with proper type handling."""
        result = asdict(self)
        
        # Convert Path objects to strings if any
        for key, value in result.items():
            if isinstance(value, Path):
                result[key] = str(value)
        
        return result

    @classmethod
    def from_dict(cls, options_dict: dict[str, Any]) -> ConversionOptions:
        """
        Create ConversionOptions from dictionary with validation.
        
        Args:
            options_dict: Dictionary containing option values
            
        Returns:
            Validated ConversionOptions instance
            
        Raises:
            ValidationError: If any option values are invalid
        """
        try:
            # Filter to only include valid fields
            valid_keys = {f.name for f in cls.__dataclass_fields__.values()}
            filtered_dict = {
                k: v for k, v in options_dict.items() 
                if k in valid_keys and v is not None
            }
            
            return cls(**filtered_dict)
        
        except (TypeError, ValueError) as e:
            raise ValidationError(f"Failed to create options from dictionary: {e}") from e

    @classmethod
    def from_json(cls, json_file: PathLike) -> ConversionOptions:
        """
        Load options from JSON file with enhanced error handling.
        
        Args:
            json_file: Path to JSON configuration file
            
        Returns:
            ConversionOptions instance
            
        Raises:
            ValidationError: If file cannot be read or contains invalid data
        """
        json_path = Path(json_file)
        
        try:
            if not json_path.exists():
                raise FileNotFoundError(f"Configuration file not found: {json_path}")
            
            if not json_path.is_file():
                raise ValueError(f"Path is not a file: {json_path}")
            
            with open(json_path, 'r', encoding='utf-8') as f:
                options_dict = json.load(f)
            
            if not isinstance(options_dict, dict):
                raise ValueError("JSON file must contain a dictionary")
            
            logger.info(f"Loaded configuration from {json_path}")
            return cls.from_dict(options_dict)
            
        except json.JSONDecodeError as e:
            raise ValidationError(
                f"Invalid JSON in configuration file: {e}",
                file_path=json_path
            ) from e
        except (OSError, IOError) as e:
            raise ValidationError(
                f"Failed to read configuration file: {e}",
                file_path=json_path
            ) from e

    @classmethod
    def from_yaml(cls, yaml_file: PathLike) -> ConversionOptions:
        """
        Load options from YAML file with enhanced error handling.
        
        Args:
            yaml_file: Path to YAML configuration file
            
        Returns:
            ConversionOptions instance
            
        Raises:
            ValidationError: If file cannot be read or contains invalid data
        """
        yaml_path = Path(yaml_file)
        
        try:
            import yaml
        except ImportError as e:
            raise ValidationError(
                "YAML support requires PyYAML. Install with 'pip install convert_to_header[yaml]'"
            ) from e
        
        try:
            if not yaml_path.exists():
                raise FileNotFoundError(f"Configuration file not found: {yaml_path}")
            
            if not yaml_path.is_file():
                raise ValueError(f"Path is not a file: {yaml_path}")
            
            with open(yaml_path, 'r', encoding='utf-8') as f:
                options_dict = yaml.safe_load(f)
            
            if not isinstance(options_dict, dict):
                raise ValueError("YAML file must contain a dictionary")
            
            logger.info(f"Loaded configuration from {yaml_path}")
            return cls.from_dict(options_dict)
            
        except yaml.YAMLError as e:
            raise ValidationError(
                f"Invalid YAML in configuration file: {e}",
                file_path=yaml_path
            ) from e
        except (OSError, IOError) as e:
            raise ValidationError(
                f"Failed to read configuration file: {e}",
                file_path=yaml_path
            ) from e
    
    def save_to_json(self, json_file: PathLike) -> None:
        """
        Save current options to JSON file.
        
        Args:
            json_file: Path to output JSON file
            
        Raises:
            ValidationError: If file cannot be written
        """
        json_path = Path(json_file)
        
        try:
            # Create parent directories if needed
            json_path.parent.mkdir(parents=True, exist_ok=True)
            
            with open(json_path, 'w', encoding='utf-8') as f:
                json.dump(self.to_dict(), f, indent=2, sort_keys=True)
            
            logger.info(f"Saved configuration to {json_path}")
            
        except (OSError, IOError) as e:
            raise ValidationError(
                f"Failed to save configuration file: {e}",
                file_path=json_path
            ) from e
    
    def copy(self, **changes: Any) -> ConversionOptions:
        """
        Create a copy of the options with specified changes.
        
        Args:
            **changes: Field values to change in the copy
            
        Returns:
            New ConversionOptions instance with changes applied
        """
        current_dict = self.to_dict()
        current_dict.update(changes)
        return self.__class__.from_dict(current_dict)
