
# Lithium-Next Project Architecture

This document provides a comprehensive overview of the architecture of the Lithium-Next project, a modular and extensible open-source platform for astrophotography.

## 1. High-Level Architecture

Lithium-Next follows a modular, component-based architecture that promotes separation of concerns and high cohesion. The project is organized into several distinct layers, each with a specific responsibility:

- **Application Layer**: The main entry point of the application, responsible for initializing the system and managing the main event loop.
- **Core Layer**: Provides fundamental services and utilities, such as logging, configuration management, and a tasking system.
- **Component Layer**: Contains the various components that make up the application's functionality, such as camera control, telescope control, and image processing.
- **Library Layer**: Includes third-party libraries and internal libraries that provide specialized functionality.
- **Module Layer**: A collection of self-contained packages that provide additional features and can be easily added or removed from the project.

## 2. Directory Structure

The project's directory structure reflects its modular architecture:

```
/
├── src/                # Source code for the core application and components
│   ├── app/            # Main application entry point
│   ├── client/         # Client-side components
│   ├── components/     # Reusable application components
│   ├── config/         # Configuration management
│   ├── constant/       # Application-wide constants
│   ├── database/       # Database interface
│   ├── debug/          # Debugging utilities
│   ├── device/         # Device control components (camera, telescope, etc.)
│   ├── exception/      # Custom exception types
│   ├── script/         # Scripting engine
│   ├── server/         # Server-side components
│   ├── target/         # Target management
│   ├── task/           # Tasking system
│   ├── tools/          # Command-line tools
│   └── utils/          # Utility functions
├── modules/            # Self-contained modules
├── libs/               # Third-party and internal libraries
├── example/            # Example applications and usage demonstrations
├── tests/              # Unit and integration tests
├── docs/               # Project documentation
├── cmake/              # CMake modules and scripts
└── build/              # Build output
```

## 3. Build System

The project uses CMake as its build system. The main `CMakeLists.txt` file in the project root orchestrates the build process, while each component and module has its own `CMakeLists.txt` file that defines how it should be built and linked.

The build system is designed to be highly modular and configurable. Components and modules can be easily added or removed by simply adding or removing their corresponding subdirectories and updating the `CMakeLists.txt` files.

## 4. Core Components

### 4.1. Task System

The task system is a key component of the Lithium-Next architecture. It provides a flexible and powerful way to execute and manage long-running operations, such as image exposures, calibration sequences, and automated workflows.

The task system is based on a producer-consumer pattern, where tasks are added to a queue and executed by a pool of worker threads. Tasks can be chained together to create complex workflows, and they can be monitored and controlled through a simple and intuitive API.

### 4.2. Device Control

The device control system provides a unified interface for controlling a wide range of astronomical devices, including cameras, telescopes, focusers, and filter wheels. It is designed to be extensible, allowing new devices to be added with minimal effort.

The device control system is based on a driver model, where each device has a corresponding driver that implements a common set of interfaces. This allows the application to interact with different devices in a consistent and predictable way.

### 4.3. Configuration Management

The configuration management system provides a centralized way to manage the application's settings and preferences. It supports a variety of configuration sources, including command-line arguments, environment variables, and configuration files.

The configuration system is designed to be type-safe and easy to use. It provides a simple API for accessing and modifying configuration values, and it supports a variety of data types, including strings, numbers, and booleans.

## 5. Modularity and Extensibility

Lithium-Next is designed to be highly modular and extensible. New features and functionality can be easily added by creating new components or modules.

Components are reusable building blocks that can be combined to create complex applications. They are designed to be self-contained and have a well-defined interface, which makes them easy to test and maintain.

Modules are self-contained packages that provide additional features and can be easily added or removed from the project. They are designed to be independent of the core application, which allows them to be developed and maintained separately.

## 6. Conclusion

The Lithium-Next project has a well-designed and documented architecture that promotes modularity, extensibility, and maintainability. The project's clear separation of concerns, component-based design, and powerful tasking system make it a flexible and robust platform for astrophotography.
