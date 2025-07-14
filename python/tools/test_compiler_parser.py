#!/usr/bin/env python3
"""
Simple test script to verify the compiler parser widget functionality.
"""

import sys
from pathlib import Path

# Add the tools directory to the path
sys.path.insert(0, str(Path(__file__).parent))

from compiler_parser import CompilerParserWidget


def test_widget_creation():
    """Test that widget can be created."""
    widget = CompilerParserWidget()
    print("✓ Widget created successfully")
    return widget


def test_parse_from_string():
    """Test parsing from string."""
    widget = CompilerParserWidget()

    # Sample GCC output
    gcc_output = """
gcc version 9.4.0 (Ubuntu 9.4.0-1ubuntu1~20.04.2)
test.c:10:15: error: 'undeclared' undeclared (first use in this function)
test.c:20:5: warning: unused variable 'x' [-Wunused-variable]
    """

    result = widget.parse_from_string("gcc", gcc_output)
    print(f"✓ Parsed GCC output: {len(result.messages)} messages")
    print(f"  - Errors: {len(result.errors)}")
    print(f"  - Warnings: {len(result.warnings)}")

    return result


def test_console_formatting():
    """Test console formatting."""
    widget = CompilerParserWidget()

    # Sample output
    gcc_output = """
gcc version 9.4.0 (Ubuntu 9.4.0-1ubuntu1~20.04.2)
test.c:10:15: error: 'undeclared' undeclared (first use in this function)
test.c:20:5: warning: unused variable 'x' [-Wunused-variable]
    """

    result = widget.parse_from_string("gcc", gcc_output)
    print("✓ Console formatting test:")
    widget.display_output(result, colorize=False)

    return result


def main():
    """Run all tests."""
    print("Testing Compiler Parser Widget...")
    print("=" * 50)

    try:
        # Test widget creation
        widget = test_widget_creation()

        # Test parsing
        result = test_parse_from_string()

        # Test formatting
        test_console_formatting()

        print("=" * 50)
        print("✓ All tests passed!")

    except Exception as e:
        print(f"✗ Test failed: {e}")
        import traceback

        traceback.print_exc()
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
