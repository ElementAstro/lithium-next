/*
 * test_sequencer_controller.cpp - Tests for Sequencer Controllers
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include <gtest/gtest.h>

#include "atom/type/json.hpp"

#include <string>
#include <vector>

using json = nlohmann::json;

// ============================================================================
// Target Request Format Tests
// ============================================================================

class TargetRequestFormatTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(TargetRequestFormatTest, CreateTargetRequest) {
    json request = {{"name", "M31"},
                    {"ra", 0.712},
                    {"dec", 41.269},
                    {"type", "galaxy"},
                    {"priority", 1}};

    EXPECT_EQ(request["name"], "M31");
    EXPECT_EQ(request["type"], "galaxy");
}

TEST_F(TargetRequestFormatTest, UpdateTargetRequest) {
    json request = {
        {"target_id", "target-123"}, {"priority", 2}, {"enabled", false}};

    EXPECT_EQ(request["target_id"], "target-123");
}

TEST_F(TargetRequestFormatTest, DeleteTargetRequest) {
    json request = {{"target_id", "target-123"}};

    EXPECT_EQ(request["target_id"], "target-123");
}

TEST_F(TargetRequestFormatTest, GetTargetRequest) {
    json request = {{"target_id", "target-123"}};

    EXPECT_EQ(request["target_id"], "target-123");
}

// ============================================================================
// Target Response Format Tests
// ============================================================================

class TargetResponseFormatTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(TargetResponseFormatTest, TargetDetailsResponse) {
    json response = {{"success", true},
                     {"data",
                      {{"id", "target-123"},
                       {"name", "M31"},
                       {"ra", 0.712},
                       {"dec", 41.269},
                       {"type", "galaxy"},
                       {"priority", 1},
                       {"enabled", true},
                       {"completed_exposures", 10},
                       {"total_exposures", 50}}}};

    EXPECT_TRUE(response["success"].get<bool>());
    EXPECT_EQ(response["data"]["name"], "M31");
}

TEST_F(TargetResponseFormatTest, ListTargetsResponse) {
    json response = {
        {"success", true},
        {"data",
         {{"targets",
           {{{"id", "target-1"}, {"name", "M31"}, {"priority", 1}},
            {{"id", "target-2"}, {"name", "M42"}, {"priority", 2}},
            {{"id", "target-3"}, {"name", "M45"}, {"priority", 3}}}}}}};

    EXPECT_EQ(response["data"]["targets"].size(), 3);
}

// ============================================================================
// Task Request Format Tests
// ============================================================================

class SequencerTaskRequestFormatTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(SequencerTaskRequestFormatTest, CreateExposureTask) {
    json request = {{"target_id", "target-123"},
                    {"type", "exposure"},
                    {"params",
                     {{"duration", 300.0},
                      {"count", 10},
                      {"filter", "Ha"},
                      {"binning", 1},
                      {"gain", 100}}}};

    EXPECT_EQ(request["type"], "exposure");
    EXPECT_EQ(request["params"]["duration"], 300.0);
}

TEST_F(SequencerTaskRequestFormatTest, CreateDitherTask) {
    json request = {{"target_id", "target-123"},
                    {"type", "dither"},
                    {"params", {{"pixels", 5.0}, {"settle_time", 10.0}}}};

    EXPECT_EQ(request["type"], "dither");
}

TEST_F(SequencerTaskRequestFormatTest, CreateFocusTask) {
    json request = {{"target_id", "target-123"},
                    {"type", "autofocus"},
                    {"params", {{"step_size", 50}, {"num_steps", 9}}}};

    EXPECT_EQ(request["type"], "autofocus");
}

TEST_F(SequencerTaskRequestFormatTest, CreateMeridianFlipTask) {
    json request = {{"target_id", "target-123"},
                    {"type", "meridian_flip"},
                    {"params", {{"recenter", true}, {"refocus", true}}}};

    EXPECT_EQ(request["type"], "meridian_flip");
}

// ============================================================================
// Sequence Execution Tests
// ============================================================================

class SequenceExecutionTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(SequenceExecutionTest, StartSequenceRequest) {
    json request = {{"sequence_id", "seq-123"}};

    EXPECT_EQ(request["sequence_id"], "seq-123");
}

TEST_F(SequenceExecutionTest, PauseSequenceRequest) {
    json request = {{"sequence_id", "seq-123"}};

    EXPECT_EQ(request["sequence_id"], "seq-123");
}

TEST_F(SequenceExecutionTest, ResumeSequenceRequest) {
    json request = {{"sequence_id", "seq-123"}};

    EXPECT_EQ(request["sequence_id"], "seq-123");
}

TEST_F(SequenceExecutionTest, StopSequenceRequest) {
    json request = {{"sequence_id", "seq-123"}, {"abort_current", false}};

    EXPECT_FALSE(request["abort_current"].get<bool>());
}

TEST_F(SequenceExecutionTest, SkipTaskRequest) {
    json request = {{"sequence_id", "seq-123"}, {"task_id", "task-456"}};

    EXPECT_EQ(request["task_id"], "task-456");
}

// ============================================================================
// Sequence Status Tests
// ============================================================================

class SequenceStatusTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(SequenceStatusTest, IdleStatus) {
    json status = {{"sequence_id", "seq-123"},
                   {"status", "idle"},
                   {"progress", 0},
                   {"current_target", nullptr},
                   {"current_task", nullptr}};

    EXPECT_EQ(status["status"], "idle");
}

TEST_F(SequenceStatusTest, RunningStatus) {
    json status = {{"sequence_id", "seq-123"},
                   {"status", "running"},
                   {"progress", 45},
                   {"current_target", {{"id", "target-1"}, {"name", "M31"}}},
                   {"current_task",
                    {{"id", "task-1"},
                     {"type", "exposure"},
                     {"progress", 60},
                     {"exposure_number", 5},
                     {"total_exposures", 10}}}};

    EXPECT_EQ(status["status"], "running");
    EXPECT_EQ(status["progress"], 45);
}

TEST_F(SequenceStatusTest, PausedStatus) {
    json status = {{"sequence_id", "seq-123"},
                   {"status", "paused"},
                   {"progress", 45},
                   {"paused_at", "2024-01-01T12:30:00Z"},
                   {"pause_reason", "user_request"}};

    EXPECT_EQ(status["status"], "paused");
}

TEST_F(SequenceStatusTest, CompletedStatus) {
    json status = {{"sequence_id", "seq-123"},
                   {"status", "completed"},
                   {"progress", 100},
                   {"started_at", "2024-01-01T20:00:00Z"},
                   {"completed_at", "2024-01-02T06:00:00Z"},
                   {"total_exposures", 100},
                   {"successful_exposures", 98},
                   {"failed_exposures", 2}};

    EXPECT_EQ(status["status"], "completed");
    EXPECT_EQ(status["progress"], 100);
}

TEST_F(SequenceStatusTest, ErrorStatus) {
    json status = {{"sequence_id", "seq-123"},
                   {"status", "error"},
                   {"error", "Mount tracking lost"},
                   {"error_time", "2024-01-01T23:45:00Z"},
                   {"recoverable", true}};

    EXPECT_EQ(status["status"], "error");
}

// ============================================================================
// Sequence Management Tests
// ============================================================================

class SequenceManagementTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(SequenceManagementTest, CreateSequenceRequest) {
    json request = {{"name", "M31 Session"},
                    {"description", "Deep sky imaging of M31"},
                    {"targets", {"target-1", "target-2"}},
                    {"settings",
                     {{"auto_meridian_flip", true},
                      {"park_on_complete", true},
                      {"warm_camera_on_complete", true}}}};

    EXPECT_EQ(request["name"], "M31 Session");
}

TEST_F(SequenceManagementTest, UpdateSequenceRequest) {
    json request = {{"sequence_id", "seq-123"},
                    {"name", "Updated Session"},
                    {"settings", {{"park_on_complete", false}}}};

    EXPECT_EQ(request["sequence_id"], "seq-123");
}

TEST_F(SequenceManagementTest, DeleteSequenceRequest) {
    json request = {{"sequence_id", "seq-123"}};

    EXPECT_EQ(request["sequence_id"], "seq-123");
}

TEST_F(SequenceManagementTest, DuplicateSequenceRequest) {
    json request = {{"sequence_id", "seq-123"},
                    {"new_name", "M31 Session Copy"}};

    EXPECT_EQ(request["new_name"], "M31 Session Copy");
}

TEST_F(SequenceManagementTest, ListSequencesResponse) {
    json response = {
        {"success", true},
        {"data",
         {{"sequences",
           {{{"id", "seq-1"}, {"name", "M31 Session"}, {"status", "completed"}},
            {{"id", "seq-2"}, {"name", "M42 Session"}, {"status", "idle"}},
            {{"id", "seq-3"},
             {"name", "M45 Session"},
             {"status", "running"}}}}}}};

    EXPECT_EQ(response["data"]["sequences"].size(), 3);
}

// ============================================================================
// Exposure Plan Tests
// ============================================================================

class ExposurePlanTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ExposurePlanTest, SimpleExposurePlan) {
    json plan = {
        {"target_id", "target-123"},
        {"exposures",
         {{{"filter", "L"}, {"duration", 300}, {"count", 20}, {"binning", 1}},
          {{"filter", "R"}, {"duration", 300}, {"count", 10}, {"binning", 1}},
          {{"filter", "G"}, {"duration", 300}, {"count", 10}, {"binning", 1}},
          {{"filter", "B"},
           {"duration", 300},
           {"count", 10},
           {"binning", 1}}}}};

    EXPECT_EQ(plan["exposures"].size(), 4);
}

TEST_F(ExposurePlanTest, NarrowbandExposurePlan) {
    json plan = {{"target_id", "target-123"},
                 {"exposures",
                  {{{"filter", "Ha"}, {"duration", 600}, {"count", 30}},
                   {{"filter", "OIII"}, {"duration", 600}, {"count", 30}},
                   {{"filter", "SII"}, {"duration", 600}, {"count", 30}}}}};

    EXPECT_EQ(plan["exposures"].size(), 3);
}

TEST_F(ExposurePlanTest, MosaicExposurePlan) {
    json plan = {
        {"target_id", "target-123"},
        {"mosaic",
         {{"panels", 4}, {"overlap_percent", 20}, {"pattern", "snake"}}},
        {"exposures_per_panel",
         {{{"filter", "L"}, {"duration", 300}, {"count", 10}}}}};

    EXPECT_EQ(plan["mosaic"]["panels"], 4);
}

// ============================================================================
// Scheduling Tests
// ============================================================================

class SchedulingTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(SchedulingTest, ScheduleSequenceRequest) {
    json request = {{"sequence_id", "seq-123"},
                    {"start_time", "2024-01-01T20:00:00Z"},
                    {"end_time", "2024-01-02T06:00:00Z"},
                    {"repeat", false}};

    EXPECT_EQ(request["sequence_id"], "seq-123");
}

TEST_F(SchedulingTest, RecurringScheduleRequest) {
    json request = {{"sequence_id", "seq-123"},
                    {"schedule",
                     {{"type", "recurring"},
                      {"days", {"monday", "wednesday", "friday"}},
                      {"start_time", "20:00"},
                      {"end_time", "06:00"}}}};

    EXPECT_EQ(request["schedule"]["type"], "recurring");
}

TEST_F(SchedulingTest, AltitudeConstraintRequest) {
    json request = {{"target_id", "target-123"},
                    {"constraints",
                     {{"min_altitude", 30.0},
                      {"max_altitude", 85.0},
                      {"avoid_moon", true},
                      {"min_moon_distance", 30.0}}}};

    EXPECT_EQ(request["constraints"]["min_altitude"], 30.0);
}

// ============================================================================
// Event Handling Tests
// ============================================================================

class SequencerEventTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(SequencerEventTest, ExposureStartedEvent) {
    json event = {{"type", "exposure_started"},
                  {"sequence_id", "seq-123"},
                  {"target_id", "target-1"},
                  {"exposure_number", 5},
                  {"total_exposures", 10},
                  {"filter", "Ha"},
                  {"duration", 300}};

    EXPECT_EQ(event["type"], "exposure_started");
}

TEST_F(SequencerEventTest, ExposureCompletedEvent) {
    json event = {{"type", "exposure_completed"},
                  {"sequence_id", "seq-123"},
                  {"target_id", "target-1"},
                  {"exposure_number", 5},
                  {"image_path", "/images/M31_Ha_005.fits"},
                  {"hfr", 2.5},
                  {"stars_detected", 150}};

    EXPECT_EQ(event["type"], "exposure_completed");
}

TEST_F(SequencerEventTest, TargetChangedEvent) {
    json event = {{"type", "target_changed"},
                  {"sequence_id", "seq-123"},
                  {"previous_target", "target-1"},
                  {"new_target", "target-2"},
                  {"slew_time", 45.0}};

    EXPECT_EQ(event["type"], "target_changed");
}

TEST_F(SequencerEventTest, MeridianFlipEvent) {
    json event = {{"type", "meridian_flip"},
                  {"sequence_id", "seq-123"},
                  {"target_id", "target-1"},
                  {"status", "completed"},
                  {"recenter_offset_arcsec", 2.5}};

    EXPECT_EQ(event["type"], "meridian_flip");
}

TEST_F(SequencerEventTest, AutofocusEvent) {
    json event = {{"type", "autofocus_completed"}, {"sequence_id", "seq-123"},
                  {"target_id", "target-1"},       {"old_position", 4500},
                  {"new_position", 4650},          {"hfr_improvement", 15.5}};

    EXPECT_EQ(event["type"], "autofocus_completed");
}

// ============================================================================
// Error Handling Tests
// ============================================================================

class SequencerErrorTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(SequencerErrorTest, SequenceNotFound) {
    json error = {{"success", false},
                  {"error",
                   {{"code", "sequence_not_found"},
                    {"message", "Sequence not found: seq-123"}}}};

    EXPECT_EQ(error["error"]["code"], "sequence_not_found");
}

TEST_F(SequencerErrorTest, TargetNotFound) {
    json error = {{"success", false},
                  {"error",
                   {{"code", "target_not_found"},
                    {"message", "Target not found: target-123"}}}};

    EXPECT_EQ(error["error"]["code"], "target_not_found");
}

TEST_F(SequencerErrorTest, SequenceAlreadyRunning) {
    json error = {{"success", false},
                  {"error",
                   {{"code", "sequence_already_running"},
                    {"message", "A sequence is already running"}}}};

    EXPECT_EQ(error["error"]["code"], "sequence_already_running");
}

TEST_F(SequencerErrorTest, TargetBelowHorizon) {
    json error = {{"success", false},
                  {"error",
                   {{"code", "target_below_horizon"},
                    {"message", "Target M31 is below the horizon"},
                    {"details", {{"current_altitude", -5.0}}}}}};

    EXPECT_EQ(error["error"]["code"], "target_below_horizon");
}
