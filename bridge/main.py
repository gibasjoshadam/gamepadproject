import asyncio
import json
import websockets
import hid
import requests
import sys
import time
import config

# Global Variables
input_prev_st = 0
input_cur_st = 0

last_trigger_time = {}
DEBOUNCE = 0.1

def get_dataref_ids():
    try:
        response = requests.get(f"{config.BASE_URL}/datarefs", timeout=5)
        all_refs = response.json().get("data", [])
        return {ref["name"]: ref["id"] for ref in all_refs if ref["name"] in config.DATAREFS}
    except Exception as e:
        print(f"Connection Error: {e}")
        return {}

def get_command_ids():
    try:
        response = requests.get(f"{config.BASE_URL}/commands", timeout=5)
        all_cmds = response.json().get("data", [])

        cmd_list = set()
        for each_cmd in config.CMD_MAP:
            data = config.CMD_MAP.get(each_cmd)
            cmd_list.add(data.get("name"))
            if "altName" in data:
                cmd_list.add(data.get("altName"))
        print(cmd_list)

        return {c["name"]: c["id"] for c in all_cmds if c["name"] in cmd_list}
    except Exception as e:
        print(f"Command Lookup Error: {e}")
        return {}

async def execute_cmd(bit_list, current_state, cmd_list, websocket):
    global last_trigger_time

    try:
        for each_bit in bit_list:
            now = time.time()

            if now - last_trigger_time.get(each_bit, 0) > DEBOUNCE:
                cmd = config.CMD_MAP.get(each_bit)
                if cmd:
                    cmd_id = cmd_list.get(cmd.get("name"))
                    
                    if cmd_id is not None:
                        cmd_flag = cmd.get("isButton")
                        match cmd_flag:
                            case True: #if its a button
                                if (current_state >> each_bit) & 1:
                                    await websocket.send(json.dumps(cmd_format(cmd_id)))
                                    print(f"Sent {cmd.get("name")}")
                            case False: #toggle switches
                                if (current_state >> each_bit) & 1:
                                    await websocket.send(json.dumps(cmd_format(cmd_id)))
                                    print(f"Sent {cmd.get("name")}")
                                else:
                                    cmd_alt = cmd_list.get(cmd.get("altName"))
                                    await websocket.send(json.dumps(cmd_format(cmd_alt)))
                                    print(f"Sent {cmd.get("altName")}")
                            #case _:
                                #print(f"idk u tell me; isButton: {cmd_flag}")
                    else:
                        print(f"execute id error: cmd_id is {cmd_id}")
            else:
                print(f"too fast! {now}; {last_trigger_time.get(each_bit, 0)}")
            last_trigger_time[each_bit] = now
    except Exception as e:
        print(f"execute_cmd: {e}")

def cmd_format(cmd_id):
    return  {
            "req_id": 1,
            "type": "command_set_is_active",
            "params": {"commands": [{"id": cmd_id, "is_active": True, "duration":0}]}
            }

def get_changed_states(prev, new):
    state = prev ^ new
    state_list = []

    for bit in range(8):
        if (state >> bit) & 1:
            state_list.append(bit)
    #print(state_list)
    return state_list

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
                    device.write([config.REPORT_ID, current_state])
                    print(f">>> XP12 -> ESP: Gear is {'DOWN' if current_state else 'UP'}")
                    last_state = current_state

async def esp2xp(websocket, device, command_list):
    global input_prev_st, input_cur_st
    loop = asyncio.get_event_loop()

    while True:
        try:
            report = await loop.run_in_executor(None, device.read, config.ESP_BYTE)
            if report:
                input_cur_st = report[1]
                #print(f"{input_cur_st}, {input_prev_st}")
                if input_cur_st != input_prev_st:
                    await execute_cmd(get_changed_states(input_prev_st, input_cur_st), input_cur_st, command_list, websocket)
                    input_prev_st = input_cur_st
            await asyncio.sleep(0.01) # 100Hz polling, saves CPU
        except Exception as e:
            print(f"esp2xp: {e}")
            await asyncio.sleep(1)

async def esp_debug():
    try:
        print(f"DEBUG MODE")
        esp32 = hid.device()
        esp32.open(config.VID, config.PID)
        esp32.set_nonblocking(True)

        await asyncio.gather(
            esp_tx(esp32),
            esp_rx(esp32)
        )
    except Exception as e:
        print(f"Error: {e}")
    finally:
        esp32.close()
    
async def console_usr_input():
    return (await asyncio.to_thread(sys.stdin.readline)).strip()

async def esp_tx(device):
    print(f"1 or 0; q to quit.")
    while True:
        usr_input = await console_usr_input()
        if usr_input == '1':
            device.write([config.REPORT_ID, 1])
        elif usr_input == '0':
            device.write([config.REPORT_ID, 0])
        elif usr_input == 'q':
            print(f"Exiting!")
            sys.exit()

async def esp_rx(device):
    while True:
        data = device.read(config.ESP_BYTE)
        if data:
            print(f"got {data}")
            input_cur_st = data[1]
            if input_cur_st != input_prev_st:
                print(f"{input_prev_st} > {input_cur_st}")

                input_prev_st = input_cur_st
    
        await asyncio.sleep(0.01) # 100Hz polling, saves CPU

async def heartbeat(websocket):
    while True:
        try:
            if websocket.open:
                await websocket.send(json.dumps({"req_id": 999, "type": "ping"}))
            await asyncio.sleep(5)
        except:
            break

async def xplane_bridge():
    commands = get_command_ids()
    mapping = get_dataref_ids()
    if not mapping:
        print("Waiting for X-Plane... (Make sure an aircraft is loaded)")
        await esp_debug()
        return

    lever_id = mapping.get("sim/cockpit/switches/gear_handle_status")
    print(f"Targeting Landing Gear Status ID: {lever_id}")

    try:
        esp32 = hid.device()
        esp32.open(config.VID, config.PID)
        esp32.set_nonblocking(True)

        async with websockets.connect(config.WS_URL) as ws:
            # Subscribe
            await ws.send(json.dumps({
                "req_id": 1,
                "type": "dataref_subscribe_values",
                "params": {"datarefs": [{"id": lever_id}]}
            }))

            await asyncio.gather(
                #xp2esp(ws, esp32, lever_id),
                esp2xp(ws, esp32, commands),
                heartbeat(ws)
            )
                
    except Exception as e:
        print(f"Bridge Error: {e}")
    finally:
        esp32.close()

if __name__ == "__main__":
    asyncio.run(xplane_bridge())