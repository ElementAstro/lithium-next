/*
 * exceptions.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-1-4

Description: Component Manager Exceptions

**************************************************/

#pragma once

#include "atom/error/exception.hpp"

namespace lithium {

class FailToLoadComponent : public atom::error::Exception {
public:
    using Exception::Exception;
};

#define THROW_FAIL_TO_LOAD_COMPONENT(...)                              \
    throw lithium::FailToLoadComponent(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                       ATOM_FUNC_NAME, __VA_ARGS__)

}  // namespace lithium
