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
    "domain": "145.249.249.29",
    "username": "1000",
    "password": "1000pass",
    "registrarUri": "sip:145.249.249.29",
    "agentId": "test-agent2"
  }'


# curl -X POST http://localhost:18080/accounts \
#   -H "Content-Type: application/json" \
#   -d '{
#     "accountId": "1001",
#     "domain": "145.249.249.29",
#     "username": "1001",
#     "password": "1001pass",
#     "registrarUri": "sip:145.249.249.29",
#     "agentId": "test-agent2"
#   }'

# echo -e "\nWaiting for second account to register..."
# sleep 2

# curl -X POST http://localhost:18080/calls/make \
#   -H "Content-Type: application/json" \
#   -d '{
#     "accountId": "1000",
#     "destUri": "sip:1001@145.249.249.29"
#   }'