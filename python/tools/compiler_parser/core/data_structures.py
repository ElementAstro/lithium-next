"""
Data structures for compiler parser.

This module contains the core data structures used to represent compiler messages
and compiler output.
"""

from dataclasses import dataclass, field
from typing import Dict, List, Optional, Any

from .enums import CompilerType, MessageSeverity


@dataclass
class CompilerMessage:
    """Data class representing a compiler message (error, warning, or info)."""

    file: str
    line: int
    message: str
    severity: MessageSeverity
    column: Optional[int] = None
    code: Optional[str] = None

    def to_dict(self) -> Dict[str, Any]:
        """Convert the CompilerMessage to a dictionary."""
        result = {
            "file": self.file,
            "line": self.line,
            "message": self.message,
            "severity": self.severity.value,
        }
        if self.column is not None:
            result["column"] = self.column
        if self.code is not None:
            result["code"] = self.code
        return result


@dataclass
class CompilerOutput:
    """Data class representing the structured output from a compiler."""

    compiler: CompilerType
    version: str
    messages: List[CompilerMessage] = field(default_factory=list)

    def add_message(self, message: CompilerMessage) -> None:
        """Add a message to the compiler output."""
        self.messages.append(message)

    def get_messages_by_severity(
        self, severity: MessageSeverity
    ) -> List[CompilerMessage]:
        """Get all messages with the specified severity."""
        return [msg for msg in self.messages if msg.severity == severity]

    @property
    def errors(self) -> List[CompilerMessage]:
        """Get all error messages."""
        return self.get_messages_by_severity(MessageSeverity.ERROR)

    @property
    def warnings(self) -> List[CompilerMessage]:
        """Get all warning messages."""
        return self.get_messages_by_severity(MessageSeverity.WARNING)

    @property
    def infos(self) -> List[CompilerMessage]:
        """Get all info messages."""
        return self.get_messages_by_severity(MessageSeverity.INFO)

    def to_dict(self) -> Dict[str, Any]:
        """Convert the CompilerOutput to a dictionary."""
        return {
            "compiler": self.compiler.name,
            "version": self.version,
            "messages": [msg.to_dict() for msg in self.messages],
        }
