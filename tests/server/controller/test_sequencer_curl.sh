#!/bin/bash

# Test script for SequenceController API endpoints
# Base URL - replace with actual server URL
BASE_URL="http://localhost:8080/exposure_sequence"

# Test addTarget endpoint
echo "Testing addTarget..."
curl -X POST $BASE_URL/addTarget \
  -H "Content-Type: application/json" \
  -d '{"name": "test_target"}'
echo -e "\n"

# Test removeTarget endpoint  
echo "Testing removeTarget..."
curl -X POST $BASE_URL/removeTarget \
  -H "Content-Type: application/json" \
  -d '{"name": "test_target"}'
echo -e "\n"

# Test modifyTarget endpoint
echo "Testing modifyTarget..."
curl -X POST $BASE_URL/modifyTarget \
  -H "Content-Type: application/json" \
  -d '{"name": "test_target"}'
echo -e "\n"

# Test executeAll endpoint
echo "Testing executeAll..."
curl -X POST $BASE_URL/executeAll \
  -H "Content-Type: application/json"
echo -e "\n"

# Test stop endpoint
echo "Testing stop..."
curl -X POST $BASE_URL/stop \
  -H "Content-Type: application/json"
echo -e "\n"

# Test pause endpoint
echo "Testing pause..."
curl -X POST $BASE_URL/pause \
  -H "Content-Type: application/json"
echo -e "\n"

# Test resume endpoint
echo "Testing resume..."
curl -X POST $BASE_URL/resume \
  -H "Content-Type: application/json"
echo -e "\n"

# Test saveSequence endpoint
echo "Testing saveSequence..."
curl -X POST $BASE_URL/saveSequence \
  -H "Content-Type: application/json" \
  -d '{"filename": "test_sequence.json"}'
echo -e "\n"

# Test loadSequence endpoint
echo "Testing loadSequence..."
curl -X POST $BASE_URL/loadSequence \
  -H "Content-Type: application/json" \
  -d '{"filename": "test_sequence.json"}'
echo -e "\n"

# Test getTargetNames endpoint
echo "Testing getTargetNames..."
curl -X GET $BASE_URL/getTargetNames
echo -e "\n"

# Test getTargetStatus endpoint
echo "Testing getTargetStatus..."
curl -X POST $BASE_URL/getTargetStatus \
  -H "Content-Type: application/json" \
  -d '{"name": "test_target"}'
echo -e "\n"

# Test getProgress endpoint
echo "Testing getProgress..."
curl -X GET $BASE_URL/getProgress
echo -e "\n"

# Test setSchedulingStrategy endpoint
echo "Testing setSchedulingStrategy..."
curl -X POST $BASE_URL/setSchedulingStrategy \
  -H "Content-Type: application/json" \
  -d '{"strategy": 1}'
echo -e "\n"

# Test setRecoveryStrategy endpoint
echo "Testing setRecoveryStrategy..."
curl -X POST $BASE_URL/setRecoveryStrategy \
  -H "Content-Type: application/json" \
  -d '{"strategy": 1}'
echo -e "\n"

# Test addAlternativeTarget endpoint
echo "Testing addAlternativeTarget..."
curl -X POST $BASE_URL/addAlternativeTarget \
  -H "Content-Type: application/json" \
  -d '{"targetName": "test_target", "alternativeName": "alt_target"}'
echo -e "\n"

# Test setMaxConcurrentTargets endpoint
echo "Testing setMaxConcurrentTargets..."
curl -X POST $BASE_URL/setMaxConcurrentTargets \
  -H "Content-Type: application/json" \
  -d '{"max": 5}'
echo -e "\n"

# Test setGlobalTimeout endpoint
echo "Testing setGlobalTimeout..."
curl -X POST $BASE_URL/setGlobalTimeout \
  -H "Content-Type: application/json" \
  -d '{"timeout": 60}'
echo -e "\n"

# Test getFailedTargets endpoint
echo "Testing getFailedTargets..."
curl -X GET $BASE_URL/getFailedTargets
echo -e "\n"

# Test retryFailedTargets endpoint
echo "Testing retryFailedTargets..."
curl -X POST $BASE_URL/retryFailedTargets \
  -H "Content-Type: application/json"
echo -e "\n"

# Test getExecutionStats endpoint
echo "Testing getExecutionStats..."
curl -X GET $BASE_URL/getExecutionStats
echo -e "\n"

# Test setTargetTaskParams endpoint
echo "Testing setTargetTaskParams..."
curl -X POST $BASE_URL/setTargetTaskParams \
  -H "Content-Type: application/json" \
  -d '{"targetName": "test_target", "taskUUID": "1234", "params": {"key": "value"}}'
echo -e "\n"

# Test getTargetTaskParams endpoint
echo "Testing getTargetTaskParams..."
curl -X POST $BASE_URL/getTargetTaskParams \
  -H "Content-Type: application/json" \
  -d '{"targetName": "test_target", "taskUUID": "1234"}'
echo -e "\n"

# Test setTargetParams endpoint
echo "Testing setTargetParams..."
curl -X POST $BASE_URL/setTargetParams \
  -H "Content-Type: application/json" \
  -d '{"targetName": "test_target", "params": {"key": "value"}}'
echo -e "\n"

# Test getTargetParams endpoint
echo "Testing getTargetParams..."
curl -X POST $BASE_URL/getTargetParams \
  -H "Content-Type: application/json" \
  -d '{"targetName": "test_target"}'
echo -e "\n"
