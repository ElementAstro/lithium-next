#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
File: exceptions.py
Author: Max Qian <astro_air@126.com>
Enhanced: 2025-06-08
Version: 2.0

Description:
------------
Custom exceptions for the convert_to_header package.
"""


class ConversionError(Exception):
    """Base exception for conversion errors."""

    pass


class FileFormatError(ConversionError):
    """Exception raised for file format errors."""

    pass


class CompressionError(ConversionError):
    """Exception raised for compression/decompression errors."""

    pass


class ChecksumError(ConversionError):
    """Exception raised for checksum verification errors."""

    pass
