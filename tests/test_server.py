#!/usr/bin/env python3
import unittest
import requests
import json
import time
import threading
import subprocess
import os
from typing import Optional

class TestServer(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.base_url = "http://localhost:18080"

    def test_01_server_status(self):
        """Test server status endpoint"""
        response = requests.get(f"{self.base_url}/status")
        self.assertEqual(response.status_code, 200)
        data = response.json()
        self.assertEqual(data["status"], "OK")

    def test_02_create_agent_basic(self):
        """Test basic agent creation"""
        agent_data = {
            "id": "agent1",
            "type": "BaseAgent",
            "config": {
                "provider": "default",
                "stm_capacity": 15,
                "voice": {
                    "style": "neutral",
                    "temperature": 0.7
                }
            }
        }
        response = requests.post(
            f"{self.base_url}/agents",
            json=agent_data
        )
        self.assertEqual(response.status_code, 201)
        data = response.json()
        print(data)
        self.assertEqual(data["id"], "agent1")
        self.assertEqual(data["config"]["provider"], "default")
        self.assertEqual(data["config"]["stm_capacity"], 15)
        self.assertEqual(data["config"]["voice"]["style"], "neutral")
        self.assertEqual(data["config"]["voice"]["temperature"], 0.7)

    def test_03_create_agent_invalid_config(self):
        """Test agent creation with invalid configuration"""
        agent_data = {
            "id": "invalid_agent",
            "type": "BaseAgent",
            "config": {
                "voice": {
                    "temperature": "invalid"  # Should be float
                }
            }
        }
        response = requests.post(
            f"{self.base_url}/agents",
            json=agent_data
        )
        self.assertEqual(response.status_code, 422)
        data = response.json()
        self.assertIn("error", data)

    def test_04_update_agent_voice(self):
        """Test updating agent voice settings"""
        update_data = {
            "voice": {
                "style": "cheerful",
                "temperature": 0.9
            }
        }
        response = requests.patch(
            f"{self.base_url}/agents/agent1",
            json=update_data
        )
        self.assertEqual(response.status_code, 200)
        data = response.json()
        self.assertEqual(data["id"], "agent1")
        self.assertEqual(data["config"]["voice"]["style"], "cheerful")
        self.assertEqual(data["config"]["voice"]["temperature"], 0.9)

    def test_05_update_agent_provider(self):
        """Test updating agent provider"""
        update_data = {
            "provider": "openai"
        }
        response = requests.patch(
            f"{self.base_url}/agents/agent1",
            json=update_data
        )
        self.assertEqual(response.status_code, 200)
        data = response.json()
        self.assertEqual(data["id"], "agent1")
        self.assertEqual(data["config"]["provider"], "openai")

    def test_06_update_agent_stm_capacity(self):
        """Test updating agent short-term memory capacity"""
        update_data = {
            "stm_capacity": 20
        }
        response = requests.patch(
            f"{self.base_url}/agents/agent1",
            json=update_data
        )
        self.assertEqual(response.status_code, 200)
        data = response.json()
        self.assertEqual(data["id"], "agent1")
        self.assertEqual(data["config"]["stm_capacity"], 20)

    def test_07_update_agent_invalid_stm(self):
        """Test updating agent with invalid STM capacity"""
        update_data = {
            "stm_capacity": -1  # Cannot be negative
        }
        response = requests.patch(
            f"{self.base_url}/agents/agent1",
            json=update_data
        )
        self.assertEqual(response.status_code, 422)
        data = response.json()
        self.assertIn("error", data)

    def test_08_list_agents_with_config(self):
        """Test listing all agents with full configuration"""
        response = requests.get(f"{self.base_url}/agents")
        self.assertEqual(response.status_code, 200)
        data = response.json()
        self.assertIsInstance(data, list)
        self.assertTrue(len(data) > 0)
        
        # Verify first agent has all required config fields
        agent = data[0]
        self.assertIn("id", agent)
        self.assertIn("config", agent)
        self.assertIn("provider", agent["config"])
        self.assertIn("stm_capacity", agent["config"])
        self.assertIn("voice", agent["config"])
        self.assertIn("style", agent["config"]["voice"])
        self.assertIn("temperature", agent["config"]["voice"])

    def test_09_get_nonexistent_agent(self):
        """Test getting a non-existent agent"""
        response = requests.get(f"{self.base_url}/agents/nonexistent")
        self.assertEqual(response.status_code, 404)
        data = response.json()
        self.assertIn("error", data)

    def test_10_update_nonexistent_agent(self):
        """Test updating a non-existent agent"""
        update_data = {
            "provider": "openai"
        }
        response = requests.patch(
            f"{self.base_url}/agents/nonexistent",
            json=update_data
        )
        self.assertEqual(response.status_code, 404)
        data = response.json()
        self.assertIn("error", data)

    def test_11_agent_thinking(self):
        """Test agent thinking capability"""
        think_data = {
            "input": "Hello, how are you?"
        }
        response = requests.post(
            f"{self.base_url}/agents/agent1/think",
            json=think_data
        )
        self.assertEqual(response.status_code, 200)
        data = response.json()
        self.assertIn("response", data)
        self.assertIsInstance(data["response"], str)
        self.assertTrue(len(data["response"]) > 0)

    def test_12_bulk_agent_update(self):
        """Test bulk update of agent configuration"""
        update_data = {
            "provider": "groq",
            "stm_capacity": 25,
            "voice": {
                "style": "professional",
                "temperature": 0.5
            }
        }
        response = requests.patch(
            f"{self.base_url}/agents/agent1",
            json=update_data
        )
        self.assertEqual(response.status_code, 200)
        data = response.json()
        self.assertEqual(data["config"]["provider"], "groq")
        self.assertEqual(data["config"]["stm_capacity"], 25)
        self.assertEqual(data["config"]["voice"]["style"], "professional")
        self.assertEqual(data["config"]["voice"]["temperature"], 0.5)

    def test_13_delete_agent(self):
        """Test agent deletion"""
        response = requests.delete(f"{self.base_url}/agents/agent1")
        self.assertEqual(response.status_code, 204)
        
        # Verify agent is actually deleted
        response = requests.get(f"{self.base_url}/agents/agent1")
        self.assertEqual(response.status_code, 404)

if __name__ == '__main__':
    unittest.main(verbosity=2)
