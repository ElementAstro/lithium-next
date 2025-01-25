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

## New Shooting Functionalities

The project has been enhanced to include specific project requirements for shooting functions, ensuring efficient invocation and execution. The following new functionalities have been added:

1. **Script Module Integration**: A new task class for the script module has been created by extending the `Task` class. The task logic has been implemented in the `execute` method, and the new task class has been added to the appropriate targets in the `ExposureSequence` class.

2. **Celestial Search Module Integration**: A new task class for the celestial search module has been created by extending the `Task` class. The task logic has been implemented in the `execute` method, and the new task class has been added to the appropriate targets in the `ExposureSequence` class.

3. **Configuration Management Integration**: A new task class for the configuration management module has been created by extending the `Task` class. The task logic has been implemented in the `execute` method, and the new task class has been added to the appropriate targets in the `ExposureSequence` class.

4. **Utility Functions Integration**: New task classes for each utility function module have been created by extending the `Task` class. The task logic has been implemented in the `execute` method, and the new task classes have been added to the appropriate targets in the `ExposureSequence` class.

5. **Combined Script and Celestial Search Modules**: A new task class that combines the script and celestial search modules has been created by extending the `Task` class. The combined task logic has been implemented in the `execute` method, and the new task class has been added to the appropriate targets in the `ExposureSequence` class.

6. **Combined Configuration Management and Utility Functions**: A new task class that combines the configuration management and utility function modules has been created by extending the `Task` class. The combined task logic has been implemented in the `execute` method, and the new task class has been added to the appropriate targets in the `ExposureSequence` class.

## Task Generation Processes

The task generation processes have been updated to reflect the new shooting functionalities. The following changes have been made:

1. **Enhanced `src/task/task_camera.cpp`**: The file has been updated to include specific project requirements for shooting functions, ensuring efficient invocation and execution.

2. **Updated `src/device/indi/camera.cpp`**: The file has been updated to support new shooting tasks and goals, optimizing camera control based on project needs.

3. **Modified `src/task/sequencer.cpp`**: The file has been updated to manage and sequence new shooting tasks, ensuring efficient execution.

4. **New Functions in `modules/image/src/binning.cpp`**: New functions have been added to handle specific image processing requirements for the project.

5. **Updated Documentation**: The documentation has been updated to reflect the new shooting functionalities and task generation processes.
