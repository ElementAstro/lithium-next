#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
PyBind11 adapter for binary to header conversion utilities.

This module provides simplified interfaces for C++ bindings via pybind11.
It exposes the core functionality of the convert_to_header module in a way
that is optimized for C++ integration.
"""

import asyncio
from pathlib import Path
from typing import Dict, List, Any, Optional

from loguru import logger
from .converter import Converter, convert_to_header, convert_to_file, get_header_info
from .options import ConversionOptions
from .utils import HeaderInfo


class ConvertToHeaderPyBindAdapter:
    """
    Adapter class to expose convert_to_header functionality to C++ via pybind11.

    This class simplifies the interface and handles conversions between Python and C++.
    It provides both synchronous and asynchronous versions of key operations.
    """

    @staticmethod
    def to_header_sync(
        input_file: str,
        output_file: Optional[str] = None,
        array_name: str = "resource_data",
        array_type: str = "unsigned char",
        data_format: str = "hex",
        compression: str = "none",
        verify_checksum: bool = False,
        checksum_algorithm: str = "sha256",
        **kwargs
    ) -> Dict[str, Any]:
        """
        Synchronous conversion of binary file to C/C++ header (for C++ binding).

        Args:
            input_file: Path to the binary input file
            output_file: Path to the output header file (if None, derived from input_file)
            array_name: Name of the data array variable
            array_type: C++ data type for the array
            data_format: Format of data (hex, bin, dec, oct, char)
            compression: Compression algorithm (none, zlib, lzma, bz2, base64)
            verify_checksum: Whether to generate and verify checksums
            checksum_algorithm: Algorithm for checksum (md5, sha1, sha256, sha512, crc32)
            **kwargs: Additional options passed to ConversionOptions

        Returns:
            Dict with structure:
            {
                "success": bool,
                "output_files": [str],
                "error": str or None,
                "original_size": int,
                "compressed_size": int,
                "checksum": str or None,
                "message": str
            }
        """
        try:
            logger.info(
                f"C++ binding: Converting binary file {input_file} to header")

            # Build ConversionOptions
            options_dict = {
                "array_name": array_name,
                "array_type": array_type,
                "data_format": data_format,
                "compression": compression,
                "verify_checksum": verify_checksum,
                "checksum_algorithm": checksum_algorithm,
            }
            options_dict.update(kwargs)
            options = ConversionOptions(**{
                k: v for k, v in options_dict.items()
                if k in ConversionOptions.__dataclass_fields__
            })

            # Perform conversion
            output_paths = convert_to_header(input_file, output_file)
            output_files = [str(p) for p in output_paths]

            result = {
                "success": True,
                "output_files": output_files,
                "error": None,
                "message": f"Successfully converted {input_file} to {len(output_files)} header file(s)"
            }
            logger.info(result["message"])
            return result

        except Exception as e:
            logger.exception(f"Error in to_header_sync: {e}")
            return {
                "success": False,
                "output_files": [],
                "error": str(e),
                "message": f"Conversion failed: {str(e)}"
            }

    @staticmethod
    async def to_header_async(
        input_file: str,
        output_file: Optional[str] = None,
        array_name: str = "resource_data",
        array_type: str = "unsigned char",
        data_format: str = "hex",
        compression: str = "none",
        verify_checksum: bool = False,
        checksum_algorithm: str = "sha256",
        **kwargs
    ) -> Dict[str, Any]:
        """
        Asynchronous conversion of binary file to C/C++ header.

        Args:
            input_file: Path to the binary input file
            output_file: Path to the output header file (if None, derived from input_file)
            array_name: Name of the data array variable
            array_type: C++ data type for the array
            data_format: Format of data (hex, bin, dec, oct, char)
            compression: Compression algorithm (none, zlib, lzma, bz2, base64)
            verify_checksum: Whether to generate and verify checksums
            checksum_algorithm: Algorithm for checksum (md5, sha1, sha256, sha512, crc32)
            **kwargs: Additional options passed to ConversionOptions

        Returns:
            Dict with structure:
            {
                "success": bool,
                "output_files": [str],
                "error": str or None,
                "original_size": int,
                "compressed_size": int,
                "checksum": str or None,
                "message": str
            }
        """
        # Run the synchronous operation in a thread pool to avoid blocking
        return await asyncio.to_thread(
            ConvertToHeaderPyBindAdapter.to_header_sync,
            input_file, output_file, array_name, array_type,
            data_format, compression, verify_checksum, checksum_algorithm,
            **kwargs
        )

    @staticmethod
    def to_file_sync(
        input_header: str,
        output_file: Optional[str] = None,
        **kwargs
    ) -> Dict[str, Any]:
        """
        Synchronous conversion of C/C++ header back to binary file (for C++ binding).

        Args:
            input_header: Path to the header file
            output_file: Path to the output binary file (if None, derived from input_header)
            **kwargs: Additional options passed to ConversionOptions

        Returns:
            Dict with structure:
            {
                "success": bool,
                "output_file": str or None,
                "error": str or None,
                "original_size": int,
                "message": str
            }
        """
        try:
            logger.info(
                f"C++ binding: Converting header file {input_header} back to binary")

            # Perform conversion
            output_path = convert_to_file(input_header, output_file)

            result = {
                "success": True,
                "output_file": str(output_path),
                "error": None,
                "message": f"Successfully converted {input_header} to {output_path}"
            }
            logger.info(result["message"])
            return result

        except Exception as e:
            logger.exception(f"Error in to_file_sync: {e}")
            return {
                "success": False,
                "output_file": None,
                "error": str(e),
                "message": f"Conversion failed: {str(e)}"
            }

    @staticmethod
    async def to_file_async(
        input_header: str,
        output_file: Optional[str] = None,
        **kwargs
    ) -> Dict[str, Any]:
        """
        Asynchronous conversion of C/C++ header back to binary file.

        Args:
            input_header: Path to the header file
            output_file: Path to the output binary file (if None, derived from input_header)
            **kwargs: Additional options passed to ConversionOptions

        Returns:
            Dict with structure:
            {
                "success": bool,
                "output_file": str or None,
                "error": str or None,
                "original_size": int,
                "message": str
            }
        """
        # Run the synchronous operation in a thread pool to avoid blocking
        return await asyncio.to_thread(
            ConvertToHeaderPyBindAdapter.to_file_sync,
            input_header, output_file, **kwargs
        )

    @staticmethod
    def get_header_info_sync(header_file: str) -> Dict[str, Any]:
        """
        Extract metadata from a header file (synchronous version for C++ binding).

        Args:
            header_file: Path to the header file

        Returns:
            Dict with structure:
            {
                "success": bool,
                "info": {...} or None,
                "error": str or None,
                "message": str
            }
        """
        try:
            logger.info(f"C++ binding: Extracting info from header {header_file}")

            # Get header info
            info = get_header_info(header_file)

            # Convert Path objects to strings for pybind11 compatibility
            serializable_info = {}
            if info:
                for key, value in info.items():
                    if isinstance(value, Path):
                        serializable_info[key] = str(value)
                    else:
                        serializable_info[key] = value

            result = {
                "success": True,
                "info": serializable_info,
                "error": None,
                "message": f"Successfully extracted information from {header_file}"
            }
            logger.info(result["message"])
            return result

        except Exception as e:
            logger.exception(f"Error in get_header_info_sync: {e}")
            return {
                "success": False,
                "info": None,
                "error": str(e),
                "message": f"Failed to extract header info: {str(e)}"
            }

    @staticmethod
    async def get_header_info_async(header_file: str) -> Dict[str, Any]:
        """
        Extract metadata from a header file (asynchronous version).

        Args:
            header_file: Path to the header file

        Returns:
            Dict with structure:
            {
                "success": bool,
                "info": {...} or None,
                "error": str or None,
                "message": str
            }
        """
        # Run the synchronous operation in a thread pool to avoid blocking
        return await asyncio.to_thread(
            ConvertToHeaderPyBindAdapter.get_header_info_sync,
            header_file
        )

    @staticmethod
    def batch_convert_to_header_sync(
        input_files: List[str],
        output_dir: str,
        array_name_prefix: str = "resource",
        **kwargs
    ) -> Dict[str, Any]:
        """
        Batch convert multiple binary files to headers (synchronous version).

        Args:
            input_files: List of paths to binary input files
            output_dir: Directory to store output header files
            array_name_prefix: Prefix for array variable names
            **kwargs: Additional options passed to ConversionOptions

        Returns:
            Dict with structure:
            {
                "success": bool,
                "total_files": int,
                "converted_files": int,
                "failed_files": int,
                "results": [{"file": str, "success": bool, "output": str or None, "error": str or None}],
                "message": str
            }
        """
        try:
            logger.info(
                f"C++ binding: Batch converting {len(input_files)} binary files")

            results = []
            converted_count = 0
            failed_count = 0

            for input_file in input_files:
                try:
                    # Create output filename based on input
                    input_path = Path(input_file)
                    output_file = Path(output_dir) / f"{input_path.stem}.hpp"

                    # Perform conversion
                    output_paths = convert_to_header(input_file, str(output_file), **kwargs)
                    output_files = [str(p) for p in output_paths]

                    results.append({
                        "file": input_file,
                        "success": True,
                        "output": output_files,
                        "error": None
                    })
                    converted_count += 1

                except Exception as e:
                    logger.warning(f"Failed to convert {input_file}: {e}")
                    results.append({
                        "file": input_file,
                        "success": False,
                        "output": None,
                        "error": str(e)
                    })
                    failed_count += 1

            return {
                "success": failed_count == 0,
                "total_files": len(input_files),
                "converted_files": converted_count,
                "failed_files": failed_count,
                "results": results,
                "message": f"Batch conversion complete: {converted_count}/{len(input_files)} succeeded"
            }

        except Exception as e:
            logger.exception(f"Error in batch_convert_to_header_sync: {e}")
            return {
                "success": False,
                "total_files": len(input_files),
                "converted_files": 0,
                "failed_files": len(input_files),
                "results": [],
                "error": str(e),
                "message": f"Batch conversion failed: {str(e)}"
            }

    @staticmethod
    async def batch_convert_to_header_async(
        input_files: List[str],
        output_dir: str,
        array_name_prefix: str = "resource",
        **kwargs
    ) -> Dict[str, Any]:
        """
        Batch convert multiple binary files to headers (asynchronous version).

        Args:
            input_files: List of paths to binary input files
            output_dir: Directory to store output header files
            array_name_prefix: Prefix for array variable names
            **kwargs: Additional options passed to ConversionOptions

        Returns:
            Dict with structure:
            {
                "success": bool,
                "total_files": int,
                "converted_files": int,
                "failed_files": int,
                "results": [{"file": str, "success": bool, "output": str or None, "error": str or None}],
                "message": str
            }
        """
        # Run the synchronous operation in a thread pool to avoid blocking
        return await asyncio.to_thread(
            ConvertToHeaderPyBindAdapter.batch_convert_to_header_sync,
            input_files, output_dir, array_name_prefix,
            **kwargs
        )
