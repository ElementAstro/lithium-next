#!/bin/bash

# Configuration
API_URL="http://localhost:8080"
TEST_CONFIG_PATH="test.config"
TEST_VALUE="test_value"

# Test GET config
echo "Testing GET config..."
curl -X POST $API_URL/config/get \
  -H "Content-Type: application/json" \
  -d '{"path":"'$TEST_CONFIG_PATH'"}'
echo -e "\n\n"

# Test SET config
echo "Testing SET config..."
curl -X POST $API_URL/config/set \
  -H "Content-Type: application/json" \
  -d '{"path":"'$TEST_CONFIG_PATH'","value":"'$TEST_VALUE'"}'
echo -e "\n\n"

# Test DELETE config
echo "Testing DELETE config..."
curl -X POST $API_URL/config/delete \
  -H "Content-Type: application/json" \
  -d '{"path":"'$TEST_CONFIG_PATH'"}'
echo -e "\n\n"

# Test LOAD config
echo "Testing LOAD config..."
curl -X POST $API_URL/config/load \
  -H "Content-Type: application/json" \
  -d '{"path":"'$TEST_CONFIG_PATH'"}'
echo -e "\n\n"

# Test RELOAD config
echo "Testing RELOAD config..."
curl -X POST $API_URL/config/reload \
  -H "Content-Type: application/json" \
  -d '{}'
echo -e "\n\n"

# Test SAVE config
echo "Testing SAVE config..."
curl -X POST $API_URL/config/save \
  -H "Content-Type: application/json" \
  -d '{"path":"'$TEST_CONFIG_PATH'"}'
echo -e "\n\n"

# Test APPEND config
echo "Testing APPEND config..."
curl -X POST $API_URL/config/append \
  -H "Content-Type: application/json" \
  -d '{"path":"'$TEST_CONFIG_PATH'","value":"'$TEST_VALUE'"}'
echo -e "\n\n"

# Test HAS config
echo "Testing HAS config..."
curl -X POST $API_URL/config/has \
  -H "Content-Type: application/json" \
  -d '{"path":"'$TEST_CONFIG_PATH'"}'
echo -e "\n\n"

# Test LIST config keys
echo "Testing LIST config keys..."
curl -X GET $API_URL/config/keys
echo -e "\n\n"

# Test LIST config paths
echo "Testing LIST config paths..."
curl -X GET $API_URL/config/paths
echo -e "\n\n"

# Test TIDY config
echo "Testing TIDY config..."
curl -X POST $API_URL/config/tidy \
  -H "Content-Type: application/json" \
  -d '{}'
echo -e "\n\n"

# Test CLEAR config
echo "Testing CLEAR config..."
curl -X POST $API_URL/config/clear \
  -H "Content-Type: application/json" \
  -d '{}'
echo -e "\n\n"
