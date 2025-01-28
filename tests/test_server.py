#!/usr/bin/env python3
import unittest
import requests
import json
import time
from typing import Optional

class TestSipServer(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        """Set up test environment"""
        cls.base_url = "http://localhost:18080"
        cls.headers = {"Content-Type": "application/json"}

    def test_server_health(self):
        """Test server health check endpoint"""
        response = requests.get(f"{self.base_url}/status")
        self.assertEqual(response.status_code, 200)
        data = response.json()
        self.assertEqual(data["status"], "OK")

    def account_lifecycle(self):
        """Test complete account lifecycle: create, update, delete"""
        # Create account
        account_data = {
            "domain": "sip.test",
            "username": "testuser",
            "password": "testpass",
            "registrarUri": "sip:sip.test"
        }
        
        create_response = requests.post(
            f"{self.base_url}/accounts",
            headers=self.headers,
            json=account_data
        )
        self.assertEqual(create_response.status_code, 201)
        created = create_response.json()
        self.assertEqual(created["accountId"], "testuser@sip.test")
        self.assertEqual(created["status"], "registered")
        
        # Update account
        update_data = {
            "domain": "sip.test",
            "username": "testuser",
            "password": "newpass",
            "registrarUri": "sip:sip.test"
        }
        
        update_response = requests.put(
            f"{self.base_url}/accounts/testuser@sip.test",
            headers=self.headers,
            json=update_data
        )
        self.assertEqual(update_response.status_code, 200)
        
        # Delete account
        delete_response = requests.delete(
            f"{self.base_url}/accounts/testuser@sip.test"
        )
        self.assertEqual(delete_response.status_code, 204)

    def call_operations(self):
        """Test call operations: make call and hangup"""
        # Create test account first
        account_data = {
            "domain": "sip.test",
            "username": "caller",
            "password": "pass123",
            "registrarUri": "sip:sip.test"
        }
        requests.post(
            f"{self.base_url}/accounts",
            headers=self.headers,
            json=account_data
        )

        # Make call
        call_data = {
            "accountId": "caller@sip.test",
            "destUri": "sip:callee@sip.test"
        }
        make_response = requests.post(
            f"{self.base_url}/calls/make",
            headers=self.headers,
            json=call_data
        )
        self.assertEqual(make_response.status_code, 202)
        make_result = make_response.json()
        self.assertEqual(make_result["status"], "Call initiated")
        
        # Hangup call (assuming call ID 1)
        hangup_data = {"callId": 1}
        hangup_response = requests.post(
            f"{self.base_url}/calls/hangup",
            headers=self.headers,
            json=hangup_data
        )
        self.assertEqual(hangup_response.status_code, 200)
        hangup_result = hangup_response.json()
        self.assertEqual(hangup_result["status"], "Call terminated")

    def event_stream(self):
        """Test event stream endpoint"""
        response = requests.get(
            f"{self.base_url}/events",
            headers={"Accept": "text/event-stream"},
            stream=True
        )
        self.assertEqual(response.status_code, 200)
        self.assertEqual(response.headers["Content-Type"], "text/event-stream")

        # Read first event
        for line in response.iter_lines():
            if line:
                decoded = line.decode("utf-8")
                if decoded.startswith("data: "):
                    event_data = json.loads(decoded[6:])  # Skip "data: " prefix
                    self.assertIn("id", event_data)
                    self.assertIsInstance(event_data["id"], int)
                    break

    def test_agent_management(self):
        """Test basic agent operations"""
        # Create agent
        agent_data = {
            "id": "test-agent2",
            "provider": "dify",
        }
        create_response = requests.post(
            f"{self.base_url}/agents",
            headers=self.headers,
            json=agent_data
        )
        print(create_response.json())
        self.assertEqual(create_response.status_code, 201)
        
        # Get agent
        get_response = requests.get(
            f"{self.base_url}/agents/test-agent2"
        )
        print(get_response.json())
        self.assertEqual(get_response.status_code, 200)
        
        # Update agent
        update_data = {
            "provider": "dify"
        }
        update_response = requests.patch(
            f"{self.base_url}/agents/test-agent2",
            headers=self.headers,
            json=update_data
        )
        print(update_response.json())
        self.assertEqual(update_response.status_code, 200)
        
        print(requests.post(
            f"{self.base_url}/agents/test-agent2/think",
            json={"input": "Hello, world!"}
        ))
     
        # Delete agent
        delete_response = requests.delete(
            f"{self.base_url}/agents/test-agent2"
        )
        self.assertEqual(delete_response.status_code, 204)


if __name__ == "__main__":
    unittest.main(verbosity=2)
