# Convert to Header

A powerful and flexible Python tool to convert binary files into C/C++ header files and back.

This modernized version is built for performance, flexibility, and ease of use, featuring a new Typer-based CLI, enhanced modularity, and more customization options.

## Features

- **Modern CLI**: Fast, intuitive command-line interface powered by Typer.
- **Multiple Conversion Modes**:
  - `to-header`: Convert binary data to a C/C++ header.
  - `to-file`: Convert a header back to a binary file.
  - `info`: Display metadata from a generated header.
- **Data Compression**: Supports `zlib`, `gzip`, `lzma`, `bz2`, and `base64` encoding.
- **Checksum Verification**: Embed checksums (`md5`, `sha1`, `sha256`, `sha512`, `crc32`) to ensure data integrity.
- **Highly Customizable Output**:
  - Control array and variable names, data types, and `const` qualifiers.
  - Choose data format (`hex`, `dec`, `oct`, etc.).
  - Generate C or C++ style comments.
  - Wrap output in C++ namespaces or classes.
  - Split large files into multiple smaller headers.
- **Configuration Files**: Define complex configurations in JSON or YAML files for easy reuse.
- **Progress Bars**: Visual feedback for long-running operations with `tqdm`.
- **Extensible API**: A clean, modular API for programmatic use.

## Installation

```bash
# Install from the local directory
pip install .

# Install with YAML support
pip install ".[yaml]"
```

## Usage

The tool is available via the `convert-to-header` command.

```
Usage: convert-to-header [OPTIONS] COMMAND [ARGS]...

Options:
  --verbose, -v  Enable verbose logging.
  --help         Show this message and exit.

Commands:
  info        Show information about a header file.
  to-file     Convert C/C++ header back to binary file.
  to-header   Convert binary file to C/C++ header.
```

### Examples

**1. Basic Conversion**

```bash
# Convert a binary file to a header
convert-to-header to-header my_data.bin
```
This creates `my_data.h` in the same directory.

**2. Conversion with Compression and Checksum**

```bash
# Use zlib compression and add a SHA-256 checksum
convert-to-header to-header my_firmware.bin --compression zlib --include-checksum
```

**3. Convert Header Back to Binary**

```bash
# Decompression is handled automatically
convert-to-header to-file my_firmware.h --output my_firmware_restored.bin

# Verify checksum during conversion
convert-to-header to-file my_firmware.h --verify-checksum
```

**4. Generate a C++ Class**

```bash
convert-to-header to-header icon.png --cpp-class --cpp-class-name IconData
```

**5. Using a Configuration File**

Create a `config.json`:
```json
{
  "compression": "lzma",
  "checksum_algorithm": "sha512",
  "cpp_namespace": "Assets",
  "items_per_line": 16
}
```

Run the tool with the config:
```bash
convert-to-header to-header level_data.bin --config config.json
```
