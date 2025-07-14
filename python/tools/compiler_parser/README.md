# Compiler Parser Widget

A modular, widget-based compiler output parser that supports multiple compilers and output formats.

## Features

- **Multi-compiler support**: GCC, Clang, MSVC, CMake
- **Multiple output formats**: JSON, CSV, XML
- **Concurrent file processing**: Process multiple files in parallel
- **Widget-based architecture**: Modular, reusable components
- **Filtering capabilities**: Filter by severity and file patterns
- **Statistics generation**: Comprehensive analysis of compiler output
- **Console formatting**: Colorized output for better readability

## Architecture

The parser is organized into a modular widget-based architecture:

```text
compiler_parser/
├── core/                    # Core data structures and enums
│   ├── enums.py            # CompilerType, OutputFormat, MessageSeverity
│   └── data_structures.py  # CompilerMessage, CompilerOutput
├── parsers/                 # Compiler-specific parsers
│   ├── base.py             # Parser protocol
│   ├── gcc_clang.py        # GCC/Clang parser
│   ├── msvc.py             # MSVC parser
│   ├── cmake.py            # CMake parser
│   └── factory.py          # Parser factory
├── writers/                 # Output format writers
│   ├── base.py             # Writer protocol
│   ├── json_writer.py      # JSON output
│   ├── csv_writer.py       # CSV output
│   ├── xml_writer.py       # XML output
│   └── factory.py          # Writer factory
├── widgets/                 # Main processing widgets
│   ├── formatter.py        # Console formatting
│   ├── processor.py        # File processing
│   └── main_widget.py      # Main orchestration
└── utils/                   # Utilities
    └── cli.py              # Command-line interface
```

## Usage

### Command Line

```bash
# Basic usage
python -m compiler_parser.main gcc build.log

# With filtering and custom output
python -m compiler_parser.main gcc build.log --output-format json --filter error warning

# Multiple files with statistics
python -m compiler_parser.main clang *.log --stats --concurrency 8
```

### Programmatic Usage

```python
from compiler_parser import CompilerParserWidget

# Create widget
widget = CompilerParserWidget()

# Parse from string
output = widget.parse_from_string('gcc', compiler_output_string)

# Parse from file
output = widget.parse_from_file('gcc', 'build.log')

# Parse multiple files
output = widget.parse_from_files('gcc', ['build1.log', 'build2.log'])

# Export to different formats
widget.write_output(output, 'json', 'output.json')
widget.write_output(output, 'csv', 'output.csv')
widget.write_output(output, 'xml', 'output.xml')

# Display formatted output
widget.display_output(output)
```

### Widget Components

Each widget has a specific responsibility:

- **CompilerParserWidget**: Main orchestration widget
- **CompilerProcessorWidget**: File processing and filtering
- **ConsoleFormatterWidget**: Console output formatting

## Backward Compatibility

The original `compiler_parser.py` file has been updated to provide backward compatibility while using the new widget architecture internally. Existing code should continue to work without modification.

## Benefits of Widget Architecture

1. **Modularity**: Each component has a single responsibility
2. **Testability**: Components can be tested independently
3. **Extensibility**: Easy to add new compilers or output formats
4. **Reusability**: Widgets can be used in different contexts
5. **Maintainability**: Clear separation of concerns

## Adding New Compilers

To add support for a new compiler:

1. Create a new parser in `parsers/`
2. Update the `ParserFactory` to include the new parser
3. Add the compiler type to the `CompilerType` enum

## Adding New Output Formats

To add a new output format:

1. Create a new writer in `writers/`
2. Update the `WriterFactory` to include the new writer
3. Add the format type to the `OutputFormat` enum
