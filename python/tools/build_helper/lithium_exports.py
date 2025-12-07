"""
Lithium export declarations for build_helper module.

This module declares functions that can be exposed as HTTP controllers
or commands in the Lithium server.
"""

from typing import List

from lithium_bridge import expose_command, expose_controller


@expose_controller(
    endpoint="/api/build/cmake/configure",
    method="POST",
    description="Configure a CMake project",
    tags=["build", "cmake"],
    version="1.0.0",
)
def cmake_configure(
    source_dir: str,
    build_dir: str,
    generator: str = None,
    build_type: str = "Release",
    cmake_args: List[str] = None,
) -> dict:
    """
    Configure a CMake project.

    Args:
        source_dir: Path to the source directory
        build_dir: Path to the build directory
        generator: CMake generator (e.g., "Ninja", "Unix Makefiles")
        build_type: Build type (Debug, Release, RelWithDebInfo, MinSizeRel)
        cmake_args: Additional CMake arguments

    Returns:
        Dictionary with configuration result
    """
    from .builders.cmake import CMakeBuilder

    try:
        builder = CMakeBuilder(
            source_dir=source_dir,
            build_dir=build_dir,
            generator=generator,
            build_type=build_type,
        )
        success = builder.configure(extra_args=cmake_args or [])
        return {
            "source_dir": source_dir,
            "build_dir": build_dir,
            "success": success,
        }
    except Exception as e:
        return {
            "source_dir": source_dir,
            "build_dir": build_dir,
            "success": False,
            "error": str(e),
        }


@expose_controller(
    endpoint="/api/build/cmake/build",
    method="POST",
    description="Build a CMake project",
    tags=["build", "cmake"],
    version="1.0.0",
)
def cmake_build(
    build_dir: str,
    target: str = None,
    parallel: int = None,
) -> dict:
    """
    Build a CMake project.

    Args:
        build_dir: Path to the build directory
        target: Specific target to build (all if None)
        parallel: Number of parallel jobs

    Returns:
        Dictionary with build result
    """
    from .builders.cmake import CMakeBuilder

    try:
        builder = CMakeBuilder(build_dir=build_dir)
        success = builder.build(target=target, parallel=parallel)
        return {
            "build_dir": build_dir,
            "target": target,
            "success": success,
        }
    except Exception as e:
        return {
            "build_dir": build_dir,
            "success": False,
            "error": str(e),
        }


@expose_controller(
    endpoint="/api/build/meson/setup",
    method="POST",
    description="Setup a Meson project",
    tags=["build", "meson"],
    version="1.0.0",
)
def meson_setup(
    source_dir: str,
    build_dir: str,
    build_type: str = "release",
    meson_args: List[str] = None,
) -> dict:
    """
    Setup a Meson project.

    Args:
        source_dir: Path to the source directory
        build_dir: Path to the build directory
        build_type: Build type (debug, release, debugoptimized, minsize)
        meson_args: Additional Meson arguments

    Returns:
        Dictionary with setup result
    """
    from .builders.meson import MesonBuilder

    try:
        builder = MesonBuilder(
            source_dir=source_dir,
            build_dir=build_dir,
            build_type=build_type,
        )
        success = builder.setup(extra_args=meson_args or [])
        return {
            "source_dir": source_dir,
            "build_dir": build_dir,
            "success": success,
        }
    except Exception as e:
        return {
            "source_dir": source_dir,
            "build_dir": build_dir,
            "success": False,
            "error": str(e),
        }


@expose_controller(
    endpoint="/api/build/meson/compile",
    method="POST",
    description="Compile a Meson project",
    tags=["build", "meson"],
    version="1.0.0",
)
def meson_compile(
    build_dir: str,
    target: str = None,
    parallel: int = None,
) -> dict:
    """
    Compile a Meson project.

    Args:
        build_dir: Path to the build directory
        target: Specific target to build
        parallel: Number of parallel jobs

    Returns:
        Dictionary with compile result
    """
    from .builders.meson import MesonBuilder

    try:
        builder = MesonBuilder(build_dir=build_dir)
        success = builder.compile(target=target, parallel=parallel)
        return {
            "build_dir": build_dir,
            "target": target,
            "success": success,
        }
    except Exception as e:
        return {
            "build_dir": build_dir,
            "success": False,
            "error": str(e),
        }


@expose_command(
    command_id="build.cmake.configure",
    description="Configure CMake project (command)",
    priority=5,
    timeout_ms=120000,
    tags=["build", "cmake"],
)
def cmd_cmake_configure(source_dir: str, build_dir: str) -> dict:
    """Configure CMake project via command dispatcher."""
    return cmake_configure(source_dir, build_dir)


@expose_command(
    command_id="build.cmake.build",
    description="Build CMake project (command)",
    priority=5,
    timeout_ms=300000,
    tags=["build", "cmake"],
)
def cmd_cmake_build(build_dir: str) -> dict:
    """Build CMake project via command dispatcher."""
    return cmake_build(build_dir)
