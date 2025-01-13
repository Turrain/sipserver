#!/bin/bash

# Test configuration
SERVER="http://127.0.0.1:18080"
CONTENT_TYPE="Content-Type: application/json"

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m'

# Helper function for printing
print_result() {
    if [ $1 -eq 0 ]; then
        echo -e "${GREEN}[✓] $2${NC}"
    else
        echo -e "${RED}[✗] $2${NC}"
    fi
}

echo "Starting API Tests..."

# Test 1: Create new account
echo -e "\nTesting Account Creation..."
ACCOUNT_RESPONSE=$(curl -s -X POST $SERVER/accounts \
    -H "$CONTENT_TYPE" \
    -d '{
        "domain": "example.com",
        "username": "testuser",
        "password": "testpass",
        "registrarUri": "sip:example.com",
        "agentId": "agent1"
    }')
print_result $? "Create Account"

# Test 2: Create new agent
echo -e "\nTesting Agent Creation..."
AGENT_RESPONSE=$(curl -s -X POST $SERVER/agents \
    -H "$CONTENT_TYPE" \
    -d '{
        "id": "agent1",
        "type": "basic",
        "config": {
            "parameter": "value"
        }
    }')
print_result $? "Create Agent"

# Test 3: Get all agents
echo -e "\nTesting Get Agents..."
curl -s -X GET $SERVER/agents
print_result $? "Get All Agents"

# Test 4: Make a call
echo -e "\nTesting Make Call..."
curl -s -X POST $SERVER/calls/make \
    -H "$CONTENT_TYPE" \
    -d '{
        "accountId": "testuser@example.com",
        "destUri": "sip:recipient@example.com"
    }'
print_result $? "Make Call"

# Test 5: Hangup call
echo -e "\nTesting Hangup Call..."
curl -s -X POST $SERVER/calls/hangup \
    -H "$CONTENT_TYPE" \
    -d '{
        "callId": 1
    }'
print_result $? "Hangup Call"

# Test 6: Update agent
echo -e "\nTesting Update Agent..."
curl -s -X PUT $SERVER/agents/agent1 \
    -H "$CONTENT_TYPE" \
    -d '{
        "config": {
            "parameter": "updated_value"
        }
    }'
print_result $? "Update Agent"

# Test 7: Delete account
echo -e "\nTesting Delete Account..."
curl -s -X DELETE "$SERVER/accounts/testuser@example.com"
print_result $? "Delete Account"

# Test 8: Delete agent
echo -e "\nTesting Delete Agent..."
curl -s -X DELETE "$SERVER/agents/agent1"
print_result $? "Delete Agent"

echo -e "\nAPI Tests Completed!"