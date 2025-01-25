# lithium-next
Next Generation of Lithium

## Project Description

Lithium-Next is an open-source astrophotography terminal designed to provide a comprehensive solution for managing and automating astrophotography tasks. The project aims to offer a user-friendly interface, robust features, and seamless integration with various astrophotography equipment and software.

### Features

- Automated build process using GitHub Actions
- Support for multiple compilers (GCC and Clang)
- Detailed project configuration using CMake
- Pre-commit hooks for code quality checks
- Integration with CodeQL for code analysis
- Comprehensive logging and debugging support
- Modular architecture for easy extension and customization

## GitHub Actions Workflows

### Build Workflow

The `build.yml` workflow automates the build process for the project. It includes steps for checking out the repository, setting up the build environment, building the project, running tests, and packaging the project.

#### Triggering the Workflow

The `build.yml` workflow is triggered on push and pull request events to the `master` branch. This ensures that the project is built, tested, and packaged automatically on every push and pull request to the `master` branch.

## Development Environment Setup

To set up the development environment for Lithium-Next, follow these steps:

1. Clone the repository:
   ```bash
   git clone https://github.com/ElementAstro/lithium-next.git
   cd lithium-next
   ```

2. Install the required dependencies:
   ```bash
   sudo apt-get update
   sudo apt-get install -y gcc g++ cmake libcfitsio-dev zlib1g-dev libssl-dev libzip-dev libnova-dev libfmt-dev gettext
   ```

3. Set up the pre-commit hooks:
   ```bash
   pre-commit install
   ```

4. Build the project:
   ```bash
   mkdir build
   cd build
   cmake ..
   make
   ```

5. Run the tests:
   ```bash
   ctest
   ```

6. Package the project:
   ```bash
   make package
   ```

7. To build with Clang, ensure Clang 18 or higher is installed and set the compiler:
   ```bash
   sudo apt-get install -y clang-18
   sudo update-alternatives --install /usr/bin/clang clang /usr/bin/clang-18 60
   sudo update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-18 60
   mkdir build-clang
   cd build-clang
   cmake -DCMAKE_CXX_COMPILER=clang++ ..
   make
   ```

8. To build with GCC 13, ensure GCC 13 is installed and set the compiler:
   ```bash
   sudo add-apt-repository ppa:ubuntu-toolchain-r/test -y
   sudo apt-get update
   sudo apt-get install -y gcc-13 g++-13
   sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-13 60
   sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-13 60
   mkdir build-gcc
   cd build-gcc
   cmake -DCMAKE_CXX_COMPILER=g++ ..
   make
   ```
