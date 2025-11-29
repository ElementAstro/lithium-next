#!/bin/bash
# Test script for Logging Controller HTTP API
# Usage: ./test_logging_curl.sh [base_url]

BASE_URL="${1:-http://localhost:8080}"
API_URL="$BASE_URL/api/v1/logging"

echo "=========================================="
echo "Testing Logging Controller API"
echo "Base URL: $API_URL"
echo "=========================================="

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

pass_count=0
fail_count=0

test_endpoint() {
    local method=$1
    local endpoint=$2
    local data=$3
    local expected_status=$4
    local description=$5

    echo -e "\n${YELLOW}Testing: $description${NC}"
    echo "  $method $endpoint"

    if [ -z "$data" ]; then
        response=$(curl -s -w "\n%{http_code}" -X "$method" "$API_URL$endpoint" -H "Content-Type: application/json")
    else
        response=$(curl -s -w "\n%{http_code}" -X "$method" "$API_URL$endpoint" -H "Content-Type: application/json" -d "$data")
    fi

    status_code=$(echo "$response" | tail -n1)
    body=$(echo "$response" | sed '$d')

    if [ "$status_code" -eq "$expected_status" ]; then
        echo -e "  ${GREEN}✓ PASS${NC} (Status: $status_code)"
        ((pass_count++))
    else
        echo -e "  ${RED}✗ FAIL${NC} (Expected: $expected_status, Got: $status_code)"
        echo "  Response: $body"
        ((fail_count++))
    fi
}

# ============================================================================
# Logger Management Tests
# ============================================================================

echo -e "\n${YELLOW}=== Logger Management ===${NC}"

test_endpoint "GET" "/loggers" "" 200 "List all loggers"

test_endpoint "GET" "/loggers/default" "" 200 "Get default logger info"

test_endpoint "PUT" "/loggers/test_logger" '{"level": "debug", "pattern": "[%l] %v"}' 200 "Create/update logger"

test_endpoint "DELETE" "/loggers/test_logger" "" 200 "Delete logger"

# ============================================================================
# Level Management Tests
# ============================================================================

echo -e "\n${YELLOW}=== Level Management ===${NC}"

test_endpoint "GET" "/level" "" 200 "Get global log level"

test_endpoint "PUT" "/level" '{"level": "info"}' 200 "Set global log level"

test_endpoint "PUT" "/loggers/default/level" '{"level": "debug"}' 200 "Set logger level"

# ============================================================================
# Sink Management Tests
# ============================================================================

echo -e "\n${YELLOW}=== Sink Management ===${NC}"

test_endpoint "GET" "/sinks" "" 200 "List all sinks"

test_endpoint "POST" "/sinks" '{"name": "test_sink", "type": "console", "level": "info"}' 200 "Add new sink"

test_endpoint "DELETE" "/sinks/test_sink" "" 200 "Remove sink"

# ============================================================================
# Log Retrieval Tests
# ============================================================================

echo -e "\n${YELLOW}=== Log Retrieval ===${NC}"

test_endpoint "GET" "/logs" "" 200 "Get recent logs"

test_endpoint "GET" "/logs?limit=10" "" 200 "Get logs with limit"

test_endpoint "GET" "/logs?level=warn" "" 200 "Get logs filtered by level"

test_endpoint "GET" "/logs?logger=default" "" 200 "Get logs filtered by logger"

test_endpoint "GET" "/buffer/stats" "" 200 "Get buffer statistics"

test_endpoint "POST" "/buffer/clear" "" 200 "Clear log buffer"

# ============================================================================
# Operations Tests
# ============================================================================

echo -e "\n${YELLOW}=== Operations ===${NC}"

test_endpoint "POST" "/flush" "" 200 "Flush all loggers"

test_endpoint "POST" "/rotate" "" 200 "Trigger log rotation"

# ============================================================================
# Configuration Tests
# ============================================================================

echo -e "\n${YELLOW}=== Configuration ===${NC}"

test_endpoint "GET" "/config" "" 200 "Get logging configuration"

test_endpoint "PUT" "/config" '{"default_level": "info"}' 200 "Update logging configuration"

# ============================================================================
# Summary
# ============================================================================

echo -e "\n=========================================="
echo "Test Summary"
echo "=========================================="
echo -e "Passed: ${GREEN}$pass_count${NC}"
echo -e "Failed: ${RED}$fail_count${NC}"
echo "Total: $((pass_count + fail_count))"

if [ $fail_count -eq 0 ]; then
    echo -e "\n${GREEN}All tests passed!${NC}"
    exit 0
else
    echo -e "\n${RED}Some tests failed!${NC}"
    exit 1
fi
