"""
Compiler processor widget.

This module provides functionality to process compiler output files with
filtering, statistics generation, and concurrent processing capabilities.
"""

import re
import logging
from pathlib import Path
from typing import Dict, List, Optional, Union, Any
from concurrent.futures import ThreadPoolExecutor, as_completed
from functools import partial

from ..core.enums import CompilerType, MessageSeverity
from ..core.data_structures import CompilerOutput
from ..parsers.factory import ParserFactory

logger = logging.getLogger(__name__)


class CompilerProcessorWidget:
    """Widget for processing compiler output files."""
    
    def __init__(self, config: Optional[Dict[str, Any]] = None):
        """Initialize the processor widget with optional configuration."""
        self.config = config or {}
        
    def process_file(self, compiler_type: Union[CompilerType, str], file_path: Union[str, Path]) -> CompilerOutput:
        """Process a single file containing compiler output."""
        # Ensure file_path is a Path object
        file_path = Path(file_path)
        
        logger.info(f"Processing file: {file_path}")
        
        # Create parser based on compiler type
        parser = ParserFactory.create_parser(compiler_type)
        
        # Read and parse the file
        try:
            with file_path.open('r', encoding="utf-8") as file:
                output = file.read()
            return parser.parse(output)
        except FileNotFoundError:
            logger.error(f"File not found: {file_path}")
            raise
        except Exception as e:
            logger.error(f"Error processing file {file_path}: {e}")
            raise
    
    def process_files(
        self, 
        compiler_type: Union[CompilerType, str],
        file_paths: List[Union[str, Path]],
        concurrency: int = 4
    ) -> List[CompilerOutput]:
        """Process multiple files concurrently and return all compiler outputs."""
        results = []
        
        # Convert strings to Path objects
        file_paths = [Path(p) for p in file_paths]
        
        # Use ThreadPoolExecutor for concurrent processing
        with ThreadPoolExecutor(max_workers=concurrency) as executor:
            # Submit all file processing tasks
            futures = {executor.submit(self.process_file, compiler_type, file_path): file_path 
                      for file_path in file_paths}
            
            # Collect results as they complete
            for future in as_completed(futures):
                file_path = futures[future]
                try:
                    result = future.result()
                    results.append(result)
                    logger.info(f"Successfully processed {file_path}")
                except Exception as e:
                    logger.error(f"Failed to process {file_path}: {e}")
                    
        return results
    
    def process_string(self, compiler_type: Union[CompilerType, str], output: str) -> CompilerOutput:
        """Process a string containing compiler output."""
        parser = ParserFactory.create_parser(compiler_type)
        return parser.parse(output)
    
    def filter_messages(
        self,
        compiler_output: CompilerOutput,
        severities: Optional[List[MessageSeverity]] = None,
        file_pattern: Optional[str] = None
    ) -> CompilerOutput:
        """Filter messages by severity and/or file pattern."""
        if not severities and not file_pattern:
            return compiler_output
            
        # Create a new output with the same metadata
        filtered = CompilerOutput(
            compiler=compiler_output.compiler,
            version=compiler_output.version
        )
        
        # Filter messages based on criteria
        for msg in compiler_output.messages:
            # Check severity filter
            severity_match = not severities or msg.severity in severities
            
            # Check file pattern filter
            file_match = not file_pattern or re.search(file_pattern, msg.file)
            
            # Add message if it matches all filters
            if severity_match and file_match:
                filtered.add_message(msg)
                
        return filtered
    
    def combine_outputs(self, compiler_outputs: List[CompilerOutput]) -> Optional[CompilerOutput]:
        """Combine multiple compiler outputs into a single output."""
        if not compiler_outputs:
            return None
            
        # Use the first compiler type and version for the combined output
        combined_output = CompilerOutput(
            compiler=compiler_outputs[0].compiler,
            version=compiler_outputs[0].version
        )
        
        # Add all messages from all outputs
        for output in compiler_outputs:
            for msg in output.messages:
                combined_output.add_message(msg)
                
        return combined_output
    
    def generate_statistics(self, compiler_outputs: List[CompilerOutput]) -> Dict[str, Any]:
        """Generate statistics from a list of compiler outputs."""
        stats = {
            "total_files": len(compiler_outputs),
            "total_messages": 0,
            "by_severity": {
                "error": 0,
                "warning": 0,
                "info": 0
            },
            "by_compiler": {},
            "files_with_errors": 0
        }
        
        for output in compiler_outputs:
            # Count messages by severity
            errors = len(output.errors)
            warnings = len(output.warnings)
            infos = len(output.infos)
            
            # Update counts
            stats["total_messages"] += errors + warnings + infos
            stats["by_severity"]["error"] += errors
            stats["by_severity"]["warning"] += warnings
            stats["by_severity"]["info"] += infos
            
            # Count files with errors
            if errors > 0:
                stats["files_with_errors"] += 1
                
            # Count by compiler
            compiler_name = output.compiler.name
            if compiler_name not in stats["by_compiler"]:
                stats["by_compiler"][compiler_name] = 0
            stats["by_compiler"][compiler_name] += 1
            
        return stats
