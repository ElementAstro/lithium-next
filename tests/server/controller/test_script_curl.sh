#!/bin/bash

# Test script for ScriptController endpoints
BASE_URL="http://localhost:8080"

# Test register script endpoint
echo "Testing /script/register"
curl -X POST $BASE_URL/script/register \
  -H "Content-Type: application/json" \
  -d '{
        "name": "test_script",
        "script": "echo Hello World"
      }'
echo -e "\n\n"

# Test delete script endpoint
echo "Testing /script/delete"
curl -X POST $BASE_URL/script/delete \
  -H "Content-Type: application/json" \
  -d '{
        "name": "test_script"
      }'
echo -e "\n\n"

# Test update script endpoint
echo "Testing /script/update"
curl -X POST $BASE_URL/script/update \
  -H "Content-Type: application/json" \
  -d '{
        "name": "test_script",
        "script": "echo Updated Script"
      }'
echo -e "\n\n"

# Test run script endpoint
echo "Testing /script/run"
curl -X POST $BASE_URL/script/run \
  -H "Content-Type: application/json" \
  -d '{
        "name": "test_script",
        "args": {
          "arg1": "value1",
          "arg2": "value2"
        }
      }'
echo -e "\n\n"

# Test run script async endpoint
echo "Testing /script/runAsync"
curl -X POST $BASE_URL/script/runAsync \
  -H "Content-Type: application/json" \
  -d '{
        "name": "test_script",
        "args": {
          "arg1": "value1",
          "arg2": "value2"
        }
      }'
echo -e "\n\n"

# Test get script output endpoint
echo "Testing /script/output"
curl -X POST $BASE_URL/script/output \
  -H "Content-Type: application/json" \
  -d '{
        "name": "test_script"
      }'
echo -e "\n\n"

# Test get script status endpoint
echo "Testing /script/status"
curl -X POST $BASE_URL/script/status \
  -H "Content-Type: application/json" \
  -d '{
        "name": "test_script"
      }'
echo -e "\n\n"

# Test get script logs endpoint
echo "Testing /script/logs"
curl -X POST $BASE_URL/script/logs \
  -H "Content-Type: application/json" \
  -d '{
        "name": "test_script"
      }'
echo -e "\n\n"

# Test list scripts endpoint
echo "Testing /script/list"
curl -X GET $BASE_URL/script/list
echo -e "\n\n"

# Test get script info endpoint
echo "Testing /script/info"
curl -X POST $BASE_URL/script/info \
  -H "Content-Type: application/json" \
  -d '{
        "name": "test_script"
      }'
echo -e "\n\n"

# =============================================================================
# Enhanced Script Management Endpoints
# =============================================================================

# Test discover scripts endpoint
echo "Testing /script/discover"
curl -X POST $BASE_URL/script/discover \
  -H "Content-Type: application/json" \
  -d '{
        "directory": "/tmp/scripts",
        "extensions": [".py", ".sh"],
        "recursive": true
      }'
echo -e "\n\n"

# Test get script statistics endpoint
echo "Testing /script/statistics"
curl -X POST $BASE_URL/script/statistics \
  -H "Content-Type: application/json" \
  -d '{
        "name": "test_script"
      }'
echo -e "\n\n"

# Test get global statistics endpoint
echo "Testing /script/globalStatistics"
curl -X GET $BASE_URL/script/globalStatistics
echo -e "\n\n"

# Test set resource limits endpoint
echo "Testing /script/resourceLimits"
curl -X POST $BASE_URL/script/resourceLimits \
  -H "Content-Type: application/json" \
  -d '{
        "maxMemoryMB": 1024,
        "maxCpuPercent": 80,
        "maxExecutionTimeSeconds": 3600,
        "maxConcurrentScripts": 4
      }'
echo -e "\n\n"

# Test get resource usage endpoint
echo "Testing /script/resourceUsage"
curl -X GET $BASE_URL/script/resourceUsage
echo -e "\n\n"

# Test execute with config endpoint
echo "Testing /script/executeWithConfig"
curl -X POST $BASE_URL/script/executeWithConfig \
  -H "Content-Type: application/json" \
  -d '{
        "name": "test_script",
        "args": {
          "param1": "value1"
        },
        "retryConfig": {
          "maxRetries": 3,
          "strategy": 1
        }
      }'
echo -e "\n\n"

# Test execute pipeline endpoint
echo "Testing /script/executePipeline"
curl -X POST $BASE_URL/script/executePipeline \
  -H "Content-Type: application/json" \
  -d '{
        "scripts": ["script1", "script2", "script3"],
        "context": {
          "shared_var": "shared_value"
        },
        "stopOnError": true
      }'
echo -e "\n\n"

# Test get script metadata endpoint
echo "Testing /script/metadata"
curl -X POST $BASE_URL/script/metadata \
  -H "Content-Type: application/json" \
  -d '{
        "name": "test_script"
      }'
echo -e "\n\n"

# Test Python availability endpoint
echo "Testing /script/pythonAvailable"
curl -X GET $BASE_URL/script/pythonAvailable
echo -e "\n"
