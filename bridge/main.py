#THIS IS JUST A TEST TO GET VALUES FROM XP12 TO THE ESP32

import asyncio
import json
import websockets
import hid
import requests

# Configuration
VID, PID = 0x303A, 0x4002
REPORT_ID = 0x01
BASE_URL = "http://localhost:8086/api/v3"
WS_URL = "ws://localhost:8086/api/v3"

TARGETS = ["sim/cockpit2/controls/parking_brake_ratio"]

def get_dataref_ids():
    try:
        response = requests.get(f"{BASE_URL}/datarefs", timeout=5)
        all_refs = response.json().get("data", [])
        return {ref["name"]: ref["id"] for ref in all_refs if ref["name"] in TARGETS}
    except Exception as e:
        print(f"Connection Error: {e}")
        return {}

async def xplane_bridge():
    mapping = get_dataref_ids()
    if not mapping:
        print("Waiting for X-Plane... (Make sure an aircraft is loaded)")
        return

    brake_id = mapping.get("sim/cockpit2/controls/parking_brake_ratio")
    print(f"Targeting Brake ID: {brake_id}")

    try:
        esp32 = hid.device()
        esp32.open(VID, PID)
        esp32.set_nonblocking(True)

        async with websockets.connect(WS_URL) as ws:
            # Subscribe
            await ws.send(json.dumps({
                "req_id": 1,
                "type": "dataref_subscribe_values",
                "params": {"datarefs": [{"id": brake_id}]}
            }))

            last_state = None
            while True:
                msg = await ws.recv()
                data = json.loads(msg)
                
                print(f"DEBUG RECEIVE: {data}")

                for dr_id, val in data.get("data", {}).items():

                    print(f"{data.get("type")}, {dr_id}, {val}")

                    if data.get("type") == "dataref_update_values" and dr_id == str(brake_id):
                        # Some XP12 versions send a list of updates
                        print(f"CATCH: ID {dr_id} is now {val}")

                        current_state = 1 if val > 0.5 else 0

                        if current_state != last_state:
                            # Send to ESP32
                            esp32.send_feature_report([REPORT_ID, current_state])
                            print(f">>> BRAKE CHANGED: {current_state}")
                            last_state = current_state
                
    except Exception as e:
        print(f"Bridge Error: {e}")
    finally:
        esp32.close()

if __name__ == "__main__":
    asyncio.run(xplane_bridge())