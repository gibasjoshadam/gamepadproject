VID, PID = 0x303A, 0x4002
REPORT_ID = 0x01
BASE_URL = "http://localhost:8086/api/v3"
WS_URL = "ws://localhost:8086/api/v3"
ESP_BYTE = 64

DATAREFS = ["sim/cockpit/switches/gear_handle_status"]

CMD_MAP = {
    3: {"name": "sim/flight_controls/landing_gear_up", "isButton": False, "altName": "sim/flight_controls/landing_gear_down"},
    4: {"name": "sim/flight_controls/brakes_toggle_max", "isButton": True}
}