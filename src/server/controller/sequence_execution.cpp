/*
 * SequenceExecutionController.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "sequence_execution.hpp"

// Static member definition
std::weak_ptr<lithium::sequencer::ExposureSequence> SequenceExecutionController::mExposureSequence;
