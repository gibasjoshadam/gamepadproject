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

# ESP bit position (base 0)
ESP_LEVER = 3

TARGETS = ["sim/cockpit/switches/gear_handle_status"]
COMMANDS = ["sim/flight_controls/landing_gear_down", "sim/flight_controls/landing_gear_up"]

def get_dataref_ids():
    try:
        response = requests.get(f"{BASE_URL}/datarefs", timeout=5)
        all_refs = response.json().get("data", [])
        return {ref["name"]: ref["id"] for ref in all_refs if ref["name"] in TARGETS}
    except Exception as e:
        print(f"Connection Error: {e}")
        return {}

def get_command_ids():
    try:
        response = requests.get(f"{BASE_URL}/commands", timeout=5)
        all_cmds = response.json().get("data", [])
        
        # Returns a dictionary like {"sim/lights/landing_lights_on": 37555}
        return {c["name"]: c["id"] for c in all_cmds if c["name"] in COMMANDS}
    except Exception as e:
        print(f"Command Lookup Error: {e}")
        return {}


async def xp2esp(websocket, device, dataref_id):
    last_state = None
    async for msg in websocket:
        data = json.loads(msg)
        if data.get("type") == "dataref_update_values":
            updates = data.get("data", {})
            if str(dataref_id) in updates:
                val = updates[str(dataref_id)]
                current_state = 1 if val > 0.5 else 0
                if current_state != last_state:
                    device.send_feature_report([REPORT_ID, current_state])
                    print(f">>> XP12 -> ESP: Gear is {'DOWN' if current_state else 'UP'}")
                    last_state = current_state

async def esp2xp(websocket, device, command_list):
    last_btn_state = None
    while True:
        report = device.read(64)
        if report:
            btn_state = report[1]
            print(f"{btn_state}, {last_btn_state}, {bool(btn_state & (1 << 3))}")

            if btn_state != last_btn_state:


                
                print(f"state is: {bool(btn_state & (1 << 3))}")

                if bool(btn_state & (1 << 3)):
                    cmd = "sim/flight_controls/landing_gear_up"
                    cmd_id = command_list.get(cmd)

                    print(f"{cmd}, {cmd_id}")

                    payload = {
                        "req_id": 67,
                        "type": "command_set_is_active",
                        "params": {"commands": [{"id": cmd_id, "is_active": True, "duration": 0}]}
                    }
                    await websocket.send(json.dumps(payload))
                    print(f"<<< ESP -> XP12: Sent {cmd}")
                else:
                    cmd = "sim/flight_controls/landing_gear_down"
                    cmd_id = command_list.get(cmd)

                    print(f"{cmd}, {cmd_id}")

                    payload = {
                        "req_id": 67,
                        "type": "command_set_is_active",
                        "params": {"commands": [{"id": cmd_id, "is_active": True, "duration": 0}]}
                    }
                    await websocket.send(json.dumps(payload))
                    print(f"<<< ESP -> XP12: Sent {cmd}")
                    

                last_btn_state = btn_state
        await asyncio.sleep(0.01) # 100Hz polling, saves CPU

async def xplane_bridge():
    commands = get_command_ids()
    mapping = get_dataref_ids()
    if not mapping:
        print("Waiting for X-Plane... (Make sure an aircraft is loaded)")
        return

    lever_id = mapping.get("sim/cockpit/switches/gear_handle_status")
    print(f"Targeting Brake ID: {lever_id}")

    try:
        esp32 = hid.device()
        esp32.open(VID, PID)
        esp32.set_nonblocking(True)

        async with websockets.connect(WS_URL) as ws:
            # Subscribe
            await ws.send(json.dumps({
                "req_id": 1,
                "type": "dataref_subscribe_values",
                "params": {"datarefs": [{"id": lever_id}]}
            }))

            await asyncio.gather(
                xp2esp(ws, esp32, lever_id),
                esp2xp(ws, esp32, commands)
            )

            """
            esp_data = esp32.read(64)
            print(f"got {esp_data} from esp")

            msg = await ws.recv()
            data = json.loads(msg)
            
            #print(f"DEBUG RECEIVE: {data}")

            for dr_id, val in data.get("data", {}).items():

                #print(f"{data.get("type")}, {dr_id}, {val}")

                if data.get("type") == "dataref_update_values" and dr_id == str(lever_id):
                    # Some XP12 versions send a list of updates
                    #print(f"CATCH: ID {dr_id} is now {val}")

                    current_state = 1 if val > 0.5 else 0

                    if current_state != last_state:
                        # Send to ESP32
                        esp32.send_feature_report([REPORT_ID, current_state])
                        #print(f">>> BRAKE CHANGED: {current_state}")
                        last_state = current_state
            """
                
    except Exception as e:
        print(f"Bridge Error: {e}")
    finally:
        esp32.close()

if __name__ == "__main__":
    asyncio.run(xplane_bridge())