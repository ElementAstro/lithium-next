"""
Lithium export declarations for compiler_helper module.

This module declares functions that can be exposed as HTTP controllers
or commands in the Lithium server.
"""

from typing import List

from lithium_bridge import expose_command, expose_controller

from .api import build_project, compile_file, get_compiler


@expose_controller(
    endpoint="/api/compiler/compile",
    method="POST",
    description="Compile a single source file",
    tags=["compiler", "build"],
    version="1.0.0",
)
def compile_source(
    source_file: str,
    output_file: str,
    compiler_name: str = None,
    cpp_version: str = "c++17",
    include_dirs: List[str] = None,
    defines: List[str] = None,
    optimization: str = "O2",
) -> dict:
    """
    Compile a single C++ source file.

    Args:
        source_file: Path to the source file
        output_file: Path for the output file
        compiler_name: Compiler to use (auto-detect if None)
        cpp_version: C++ standard version (e.g., "c++17")
        include_dirs: Additional include directories
        defines: Preprocessor definitions
        optimization: Optimization level (O0, O1, O2, O3, Os)

    Returns:
        Dictionary with compilation result
    """
    from .core_types import CompileOptions

    options = CompileOptions(
        include_dirs=include_dirs or [],
        defines=defines or [],
        optimization=optimization,
    )

    result = compile_file(
        source_file=source_file,
        output_file=output_file,
        compiler_name=compiler_name,
        cpp_version=cpp_version,
        options=options,
    )

    return {
        "success": result.success,
        "output_file": str(result.output_file) if result.output_file else "",
        "stdout": result.stdout,
        "stderr": result.stderr,
        "return_code": result.return_code,
    }


@expose_controller(
    endpoint="/api/compiler/build",
    method="POST",
    description="Build a project from multiple source files",
    tags=["compiler", "build"],
    version="1.0.0",
)
def build_sources(
    source_files: List[str],
    output_file: str,
    compiler_name: str = None,
    cpp_version: str = "c++17",
    include_dirs: List[str] = None,
    defines: List[str] = None,
    libraries: List[str] = None,
    library_dirs: List[str] = None,
    incremental: bool = True,
) -> dict:
    """
    Build a project from multiple source files.

    Args:
        source_files: List of source file paths
        output_file: Path for the output executable/library
        compiler_name: Compiler to use (auto-detect if None)
        cpp_version: C++ standard version
        include_dirs: Additional include directories
        defines: Preprocessor definitions
        libraries: Libraries to link
        library_dirs: Library search directories
        incremental: Enable incremental builds

    Returns:
        Dictionary with build result
    """
    from .core_types import CompileOptions, LinkOptions

    compile_options = CompileOptions(
        include_dirs=include_dirs or [],
        defines=defines or [],
    )

    link_options = LinkOptions(
        libraries=libraries or [],
        library_dirs=library_dirs or [],
    )

    result = build_project(
        source_files=source_files,
        output_file=output_file,
        compiler_name=compiler_name,
        cpp_version=cpp_version,
        compile_options=compile_options,
        link_options=link_options,
        incremental=incremental,
    )

    return {
        "success": result.success,
        "output_file": str(result.output_file) if result.output_file else "",
        "stdout": result.stdout,
        "stderr": result.stderr,
        "return_code": result.return_code,
    }


@expose_controller(
    endpoint="/api/compiler/info",
    method="GET",
    description="Get compiler information",
    tags=["compiler"],
    version="1.0.0",
)
def get_compiler_info(compiler_name: str = None) -> dict:
    """
    Get information about a compiler.

    Args:
        compiler_name: Compiler name (auto-detect if None)

    Returns:
        Dictionary with compiler information
    """
    try:
        compiler = get_compiler(compiler_name)
        return {
            "name": compiler.name,
            "path": str(compiler.path),
            "version": compiler.version,
            "available": True,
        }
    except Exception as e:
        return {
            "name": compiler_name or "default",
            "available": False,
            "error": str(e),
        }


@expose_command(
    command_id="compiler.compile",
    description="Compile source file (command)",
    priority=5,
    timeout_ms=60000,
    tags=["compiler"],
)
def cmd_compile(
    source_file: str,
    output_file: str,
    cpp_version: str = "c++17",
) -> dict:
    """Compile source file via command dispatcher."""
    result = compile_file(
        source_file=source_file,
        output_file=output_file,
        cpp_version=cpp_version,
    )
    return {
        "success": result.success,
        "output_file": str(result.output_file) if result.output_file else "",
    }
