#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
File: exceptions.py
Author: Max Qian <astro_air@126.com>
Enhanced: 2025-07-12
Version: 2.2

Description:
------------
Custom exceptions with enhanced error handling for the convert_to_header package.
"""

from typing import Optional, Any
from pathlib import Path


class ConversionError(Exception):
    """Base exception for conversion errors with enhanced context."""

    def __init__(
        self,
        message: str,
        *,
        file_path: Optional[Path] = None,
        error_code: Optional[str] = None,
        original_error: Optional[Exception] = None,
    ) -> None:
        super().__init__(message)
        self.file_path = file_path
        self.error_code = error_code
        self.original_error = original_error

    def __str__(self) -> str:
        parts = [super().__str__()]
        if self.file_path:
            parts.append(f"File: {self.file_path}")
        if self.error_code:
            parts.append(f"Code: {self.error_code}")
        if self.original_error:
            parts.append(f"Cause: {self.original_error}")
        return " | ".join(parts)

    def to_dict(self) -> dict[str, Any]:
        """Convert exception to dictionary for structured logging."""
        return {
            "message": str(self.args[0]) if self.args else "",
            "file_path": str(self.file_path) if self.file_path else None,
            "error_code": self.error_code,
            "original_error": str(self.original_error) if self.original_error else None,
            "exception_type": self.__class__.__name__,
        }


class FileFormatError(ConversionError):
    """Exception raised for file format errors."""

    def __init__(
        self,
        message: str,
        *,
        file_path: Optional[Path] = None,
        line_number: Optional[int] = None,
        expected_format: Optional[str] = None,
        actual_format: Optional[str] = None,
    ) -> None:
        super().__init__(message, file_path=file_path, error_code="FORMAT_ERROR")
        self.line_number = line_number
        self.expected_format = expected_format
        self.actual_format = actual_format


class CompressionError(ConversionError):
    """Exception raised for compression/decompression errors."""

    def __init__(
        self,
        message: str,
        *,
        compression_type: Optional[str] = None,
        data_size: Optional[int] = None,
        original_error: Optional[Exception] = None,
    ) -> None:
        super().__init__(
            message, error_code="COMPRESSION_ERROR", original_error=original_error
        )
        self.compression_type = compression_type
        self.data_size = data_size


class ChecksumError(ConversionError):
    """Exception raised for checksum verification errors."""

    def __init__(
        self,
        message: str,
        *,
        expected_checksum: Optional[str] = None,
        actual_checksum: Optional[str] = None,
        algorithm: Optional[str] = None,
    ) -> None:
        super().__init__(message, error_code="CHECKSUM_ERROR")
        self.expected_checksum = expected_checksum
        self.actual_checksum = actual_checksum
        self.algorithm = algorithm


class ValidationError(ConversionError):
    """Exception raised for input validation errors."""

    def __init__(
        self,
        message: str,
        *,
        field_name: Optional[str] = None,
        invalid_value: Optional[Any] = None,
        valid_values: Optional[list] = None,
    ) -> None:
        super().__init__(message, error_code="VALIDATION_ERROR")
        self.field_name = field_name
        self.invalid_value = invalid_value
        self.valid_values = valid_values
