#!/usr/bin/env python3
import asyncio
import websockets

async def test_websocket():
    uri = "ws://37.151.89.206:8765"
    
    try:
        async with websockets.connect(uri) as websocket:
            print(f"Connected to {uri}")
            
            # Send test message
            message = "Hello, Server!"
            await websocket.send(message)
            print(f"Sent: {message}")
            
            # Receive response
            response = await websocket.recv()
            print(f"Received: {response}")
            
    except ConnectionRefusedError:
        print("Connection refused - server may be down or unreachable")
    except websockets.exceptions.InvalidURI:
        print("Invalid URI format")
    except websockets.exceptions.WebSocketException as e:
        print(f"WebSocket error: {str(e)}")
    except Exception as e:
        print(f"Unexpected error: {str(e)}")

if __name__ == "__main__":
    asyncio.run(test_websocket())