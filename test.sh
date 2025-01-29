#!/bin/bash

curl -X POST http://localhost:18080/agents \
  -H "Content-Type: application/json" \
  -d '{
    "id": "test-agent2",
    "provider": "groq"
  }'



curl -X POST http://localhost:18080/accounts \
  -H "Content-Type: application/json" \
  -d '{
    "accountId": "1000",
    "domain": "127.0.0.1",
    "username": "1000",
    "password": "1000",
    "registrarUri": "sip:127.0.0.1",
    "agentId": "test-agent2"
  }'


curl -X POST http://localhost:18080/accounts \
  -H "Content-Type: application/json" \
  -d '{
    "accountId": "1001",
    "domain": "127.0.0.1",
    "username": "1001",
    "password": "1001",
    "registrarUri": "sip:127.0.0.1",
    "agentId": "test-agent2"
  }'

echo -e "\nWaiting for second account to register..."
sleep 2

curl -X POST http://localhost:18080/calls/make \
  -H "Content-Type: application/json" \
  -d '{
    "accountId": "1000",
    "destUri": "sip:1001@127.0.0.1"
  }'

# Add first account (1000)
# curl -X POST http://localhost:18080/accounts/add \
#   -H "Content-Type: application/json" \
#   -d '{
#     "accountId": "1000",
#     "domain": "127.0.0.1",
#     "username": "1000",
#     "password": "1000",
#     "registrarUri": "sip:127.0.0.1"
#   }'

# echo -e "\nWaiting for first account to register..."
# sleep 2

# #Add second account (1001)
# curl -X POST http://localhost:18080/accounts/add \
#   -H "Content-Type: application/json" \
#   -d '{
#     "accountId": "1001",
#     "domain": "127.0.0.1",
#     "username": "1001",
#     "password": "1001",
#     "registrarUri": "sip:127.0.0.1"
#   }'

# echo -e "\nWaiting for second account to register..."
# sleep 2

# Make call from 1000 to 1001
