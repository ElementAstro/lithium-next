/*
 * SequenceManagementController.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "target.hpp"

// Static member definition
std::weak_ptr<lithium::task::ExposureSequence>
    TargetController::mExposureSequence;
