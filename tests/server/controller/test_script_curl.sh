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
echo -e "\n"
