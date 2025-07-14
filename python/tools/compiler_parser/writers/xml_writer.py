"""
XML output writer.

This module provides functionality to write compiler output to XML format.
"""

import logging
from pathlib import Path
import xml.etree.ElementTree as ET

from ..core.data_structures import CompilerOutput

logger = logging.getLogger(__name__)


class XmlWriter:
    """Writer for XML output format."""
    
    def write(self, compiler_output: CompilerOutput, output_path: Path) -> None:
        """Write compiler output to an XML file."""
        root = ET.Element("CompilerOutput")
        # Add metadata
        metadata = ET.SubElement(root, "Metadata")
        ET.SubElement(metadata, "Compiler").text = compiler_output.compiler.name
        ET.SubElement(metadata, "Version").text = compiler_output.version
        ET.SubElement(metadata, "MessageCount").text = str(len(compiler_output.messages))
        
        # Add messages
        messages_elem = ET.SubElement(root, "Messages")
        for msg in compiler_output.messages:
            msg_elem = ET.SubElement(messages_elem, "Message")
            for key, value in msg.to_dict().items():
                if value is not None:  # Skip None values
                    ET.SubElement(msg_elem, key).text = str(value)
                    
        # Write XML to file
        tree = ET.ElementTree(root)
        tree.write(output_path, encoding="utf-8", xml_declaration=True)
        logger.info(f"XML output written to {output_path}")
