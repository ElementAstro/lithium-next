# Contributing to Lithium-Next

Thank you for your interest in contributing to Lithium-Next! This document provides guidelines and instructions for contributing.

## Table of Contents

- [Code of Conduct](#code-of-conduct)
- [Getting Started](#getting-started)
- [How to Contribute](#how-to-contribute)
- [Development Workflow](#development-workflow)
- [Coding Standards](#coding-standards)
- [Commit Guidelines](#commit-guidelines)
- [Pull Request Process](#pull-request-process)

## Code of Conduct

This project adheres to the [Contributor Covenant Code of Conduct](CODE_OF_CONDUCT.md). By participating, you are expected to uphold this code. Please report unacceptable behavior to the project maintainers.

## Getting Started

### Prerequisites

- GCC 13+ or Clang 18+
- CMake 3.20+
- Git
- Python 3.8+ (for pre-commit hooks)

### Setting Up the Development Environment

1. Fork the repository on GitHub
2. Clone your fork locally:
   ```bash
   git clone https://github.com/YOUR_USERNAME/lithium-next.git
   cd lithium-next
   git submodule update --init --recursive
   ```
3. Add the upstream repository:
   ```bash
   git remote add upstream https://github.com/ElementAstro/lithium-next.git
   ```
4. Install dependencies:
   ```bash
   sudo apt-get install -y gcc g++ cmake libcfitsio-dev zlib1g-dev libssl-dev libzip-dev libnova-dev libfmt-dev gettext
   ```
5. Set up pre-commit hooks:
   ```bash
   pip install pre-commit
   pre-commit install
   ```

## How to Contribute

### Reporting Bugs

- Use the [Bug Report template](.github/ISSUE_TEMPLATE/bug_report.md)
- Check existing issues to avoid duplicates
- Include detailed steps to reproduce
- Provide system information and logs

### Suggesting Features

- Use the [Feature Request template](.github/ISSUE_TEMPLATE/feature_request.md)
- Explain the use case and benefits
- Consider backward compatibility

### Contributing Code

1. Find an issue to work on or create one
2. Comment on the issue to let others know you're working on it
3. Create a feature branch
4. Write code and tests
5. Submit a pull request

## Development Workflow

### Building the Project

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### Running Tests

```bash
cd build
ctest --output-on-failure
```

### Code Formatting

We use `clang-format` for C++ code formatting. Run before committing:

```bash
# Format all files
find src tests -name "*.cpp" -o -name "*.hpp" | xargs clang-format -i
```

## Coding Standards

### C++ Style Guide

- Follow the existing code style in the project
- Use modern C++20 features where appropriate
- Prefer `std::` containers and algorithms
- Use RAII for resource management
- Document public APIs with Doxygen comments

### File Organization

- Headers in `src/` alongside implementation files
- Tests mirror the source structure in `tests/`
- Each component should have corresponding tests

### Naming Conventions

- **Classes/Structs**: `PascalCase`
- **Functions/Methods**: `camelCase`
- **Variables**: `snake_case`
- **Constants**: `UPPER_SNAKE_CASE`
- **Namespaces**: `lowercase`

## Commit Guidelines

We follow [Conventional Commits](https://www.conventionalcommits.org/):

```
<type>(<scope>): <description>

[optional body]

[optional footer(s)]
```

### Types

- `feat`: New feature
- `fix`: Bug fix
- `docs`: Documentation changes
- `style`: Code style changes (formatting, etc.)
- `refactor`: Code refactoring
- `perf`: Performance improvements
- `test`: Adding or updating tests
- `build`: Build system changes
- `ci`: CI/CD changes
- `chore`: Other changes

### Examples

```
feat(camera): add support for ASCOM camera binning

fix(sequencer): resolve race condition in task scheduling

docs(readme): update build instructions for Windows
```

## Pull Request Process

1. **Create a branch** from `master`:
   ```bash
   git checkout -b feat/your-feature-name
   ```

2. **Make your changes** following the coding standards

3. **Write/update tests** for your changes

4. **Run the test suite** to ensure nothing is broken

5. **Commit your changes** following commit guidelines

6. **Push to your fork**:
   ```bash
   git push origin feat/your-feature-name
   ```

7. **Open a Pull Request** using the PR template

8. **Address review feedback** promptly

### PR Requirements

- [ ] All tests pass
- [ ] Code follows project style guidelines
- [ ] Documentation is updated if needed
- [ ] Commit messages follow conventions
- [ ] PR description clearly explains the changes

## Questions?

If you have questions, feel free to:

- Open a [Discussion](https://github.com/ElementAstro/lithium-next/discussions)
- Check existing issues and documentation

Thank you for contributing to Lithium-Next! 🌟
