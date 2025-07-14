#!/bin/bash

# Lithium Build Optimization Script
# This script provides optimized build configurations for different scenarios

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Default values
BUILD_TYPE="Release"
BUILD_DIR="build"
CLEAN_BUILD=false
USE_NINJA=false
USE_CCACHE=true
PARALLEL_JOBS=$(nproc)
UNITY_BUILD=false

# Function to print colored output
print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Help function
show_help() {
    cat << EOF
Lithium Build Optimization Script

Usage: $0 [OPTIONS]

OPTIONS:
    -t, --type TYPE         Build type (Debug, Release, RelWithDebInfo, MinSizeRel) [default: Release]
    -d, --dir DIR          Build directory [default: build]
    -c, --clean            Clean build directory before building
    -n, --ninja            Use Ninja generator instead of Make
    -j, --jobs JOBS        Number of parallel jobs [default: $(nproc)]
    -u, --unity            Enable unity builds for faster compilation
    --no-ccache            Disable ccache usage
    --profile              Enable profiling build (Release with debug info)
    --asan                 Enable AddressSanitizer (Debug build)
    --tsan                 Enable ThreadSanitizer (Debug build)
    --ubsan                Enable UndefinedBehaviorSanitizer
    --benchmarks           Build optimized for benchmarks
    --size                 Optimize for minimal size
    -h, --help             Show this help message

EXAMPLES:
    $0                                 # Standard Release build
    $0 -t Debug -c -n                 # Clean Debug build with Ninja
    $0 --profile --unity               # Profiling build with unity builds
    $0 --benchmarks -j 16              # Benchmark optimized build with 16 jobs
    $0 --asan -t Debug                 # Debug build with AddressSanitizer

EOF
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -t|--type)
            BUILD_TYPE="$2"
            shift 2
            ;;
        -d|--dir)
            BUILD_DIR="$2"
            shift 2
            ;;
        -c|--clean)
            CLEAN_BUILD=true
            shift
            ;;
        -n|--ninja)
            USE_NINJA=true
            shift
            ;;
        -j|--jobs)
            PARALLEL_JOBS="$2"
            shift 2
            ;;
        -u|--unity)
            UNITY_BUILD=true
            shift
            ;;
        --no-ccache)
            USE_CCACHE=false
            shift
            ;;
        --profile)
            BUILD_TYPE="RelWithDebInfo"
            PROFILE_BUILD=true
            shift
            ;;
        --asan)
            ASAN_BUILD=true
            BUILD_TYPE="Debug"
            shift
            ;;
        --tsan)
            TSAN_BUILD=true
            BUILD_TYPE="Debug"
            shift
            ;;
        --ubsan)
            UBSAN_BUILD=true
            shift
            ;;
        --benchmarks)
            BENCHMARK_BUILD=true
            BUILD_TYPE="Release"
            shift
            ;;
        --size)
            BUILD_TYPE="MinSizeRel"
            SIZE_OPT=true
            shift
            ;;
        -h|--help)
            show_help
            exit 0
            ;;
        *)
            print_error "Unknown option: $1"
            show_help
            exit 1
            ;;
    esac
done

# Validate build type
case $BUILD_TYPE in
    Debug|Release|RelWithDebInfo|MinSizeRel)
        ;;
    *)
        print_error "Invalid build type: $BUILD_TYPE"
        exit 1
        ;;
esac

print_status "Lithium Build Configuration:"
print_status "  Build Type: $BUILD_TYPE"
print_status "  Build Directory: $BUILD_DIR"
print_status "  Parallel Jobs: $PARALLEL_JOBS"
print_status "  Generator: $([ "$USE_NINJA" = true ] && echo "Ninja" || echo "Unix Makefiles")"
print_status "  Unity Build: $UNITY_BUILD"
print_status "  ccache: $USE_CCACHE"

# Clean build directory if requested
if [ "$CLEAN_BUILD" = true ]; then
    print_status "Cleaning build directory..."
    rm -rf "$BUILD_DIR"
fi

# Create build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Prepare CMake arguments
CMAKE_ARGS=(
    "-DCMAKE_BUILD_TYPE=$BUILD_TYPE"
    "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"
)

# Generator selection
if [ "$USE_NINJA" = true ]; then
    CMAKE_ARGS+=("-GNinja")
    if command -v ninja &> /dev/null; then
        print_status "Using Ninja generator"
    else
        print_error "Ninja not found, falling back to Make"
        USE_NINJA=false
    fi
fi

# Unity build option
if [ "$UNITY_BUILD" = true ]; then
    CMAKE_ARGS+=("-DCMAKE_UNITY_BUILD=ON")
    print_status "Unity builds enabled"
fi

# ccache configuration
if [ "$USE_CCACHE" = true ] && command -v ccache &> /dev/null; then
    print_status "Using ccache for faster rebuilds"
else
    CMAKE_ARGS+=("-DUSE_CCACHE=OFF")
fi

# Sanitizer configurations
if [ "$ASAN_BUILD" = true ]; then
    CMAKE_ARGS+=(
        "-DCMAKE_CXX_FLAGS=-fsanitize=address -fno-omit-frame-pointer"
        "-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address"
    )
    print_status "AddressSanitizer enabled"
fi

if [ "$TSAN_BUILD" = true ]; then
    CMAKE_ARGS+=(
        "-DCMAKE_CXX_FLAGS=-fsanitize=thread -fno-omit-frame-pointer"
        "-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=thread"
    )
    print_status "ThreadSanitizer enabled"
fi

if [ "$UBSAN_BUILD" = true ]; then
    CMAKE_ARGS+=(
        "-DCMAKE_CXX_FLAGS=-fsanitize=undefined -fno-omit-frame-pointer"
        "-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=undefined"
    )
    print_status "UndefinedBehaviorSanitizer enabled"
fi

# Benchmark optimizations
if [ "$BENCHMARK_BUILD" = true ]; then
    CMAKE_ARGS+=(
        "-DCMAKE_CXX_FLAGS=-O3 -DNDEBUG -march=native -mtune=native -flto"
        "-DCMAKE_EXE_LINKER_FLAGS=-flto"
    )
    print_status "Benchmark optimizations enabled"
fi

# Size optimizations
if [ "$SIZE_OPT" = true ]; then
    CMAKE_ARGS+=(
        "-DCMAKE_CXX_FLAGS=-Os -DNDEBUG -ffunction-sections -fdata-sections"
        "-DCMAKE_EXE_LINKER_FLAGS=-Wl,--gc-sections"
    )
    print_status "Size optimizations enabled"
fi

# Configure with CMake
print_status "Configuring with CMake..."
cmake "${CMAKE_ARGS[@]}" ..

if [ $? -ne 0 ]; then
    print_error "CMake configuration failed"
    exit 1
fi

print_success "Configuration completed successfully"

# Build the project
print_status "Building project..."
if [ "$USE_NINJA" = true ]; then
    ninja -j "$PARALLEL_JOBS"
else
    make -j "$PARALLEL_JOBS"
fi

if [ $? -eq 0 ]; then
    print_success "Build completed successfully!"
    print_status "Executable location: $BUILD_DIR/lithium-next"
else
    print_error "Build failed"
    exit 1
fi
