/*
 * TargetManagementController.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "target_management.hpp"

// Static member definition
std::weak_ptr<lithium::sequencer::ExposureSequence> TargetManagementController::mExposureSequence;
