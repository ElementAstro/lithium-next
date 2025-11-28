/*
 * exception.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-6-4

Description: Configuration Exception Types

**************************************************/

#ifndef LITHIUM_CONFIG_CORE_EXCEPTION_HPP
#define LITHIUM_CONFIG_CORE_EXCEPTION_HPP

#include "atom/error/exception.hpp"

namespace lithium::config {

/**
 * @brief Base exception for configuration errors
 */
class BadConfigException : public atom::error::Exception {
    using atom::error::Exception::Exception;
};

#define THROW_BAD_CONFIG_EXCEPTION(...)                                        \
    throw lithium::config::BadConfigException(ATOM_FILE_NAME, ATOM_FILE_LINE,  \
                                              ATOM_FUNC_NAME, __VA_ARGS__)

/**
 * @brief Exception for invalid configuration values
 */
class InvalidConfigException : public BadConfigException {
    using BadConfigException::BadConfigException;
};

#define THROW_INVALID_CONFIG_EXCEPTION(...)                                    \
    throw lithium::config::InvalidConfigException(                             \
        ATOM_FILE_NAME, ATOM_FILE_LINE, ATOM_FUNC_NAME, __VA_ARGS__)

/**
 * @brief Exception for configuration not found
 */
class ConfigNotFoundException : public BadConfigException {
    using BadConfigException::BadConfigException;
};

#define THROW_CONFIG_NOT_FOUND_EXCEPTION(...)                                  \
    throw lithium::config::ConfigNotFoundException(                            \
        ATOM_FILE_NAME, ATOM_FILE_LINE, ATOM_FUNC_NAME, __VA_ARGS__)

/**
 * @brief Exception for configuration file I/O errors
 */
class ConfigIOException : public BadConfigException {
    using BadConfigException::BadConfigException;
};

#define THROW_CONFIG_IO_EXCEPTION(...)                                         \
    throw lithium::config::ConfigIOException(ATOM_FILE_NAME, ATOM_FILE_LINE,   \
                                             ATOM_FUNC_NAME, __VA_ARGS__)

}  // namespace lithium::config

// Backward compatibility aliases in global namespace
using BadConfigException = lithium::config::BadConfigException;
using InvalidConfigException = lithium::config::InvalidConfigException;

#endif  // LITHIUM_CONFIG_CORE_EXCEPTION_HPP
