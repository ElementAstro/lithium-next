/*
 * exceptions.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: Device Manager Exceptions

**************************************************/

#pragma once

#include "atom/error/exception.hpp"

namespace lithium {

/**
 * @brief Exception thrown when a device is not found
 */
class DeviceNotFoundException : public atom::error::Exception {
    using Exception::Exception;
};

#define THROW_DEVICE_NOT_FOUND(...)                               \
    throw DeviceNotFoundException(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                  ATOM_FUNC_NAME, __VA_ARGS__)

/**
 * @brief Exception thrown when a device type is not found
 */
class DeviceTypeNotFoundException : public atom::error::Exception {
    using Exception::Exception;
};

#define THROW_DEVICE_TYPE_NOT_FOUND(...)                              \
    throw DeviceTypeNotFoundException(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                      ATOM_FUNC_NAME, __VA_ARGS__)

/**
 * @brief Exception thrown when a device operation fails
 */
class DeviceOperationException : public atom::error::Exception {
    using Exception::Exception;
};

#define THROW_DEVICE_OPERATION_ERROR(...)                              \
    throw DeviceOperationException(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                   ATOM_FUNC_NAME, __VA_ARGS__)

/**
 * @brief Exception thrown when a backend is not found
 */
class BackendNotFoundException : public atom::error::Exception {
    using Exception::Exception;
};

#define THROW_BACKEND_NOT_FOUND(...)                               \
    throw BackendNotFoundException(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                   ATOM_FUNC_NAME, __VA_ARGS__)

}  // namespace lithium
