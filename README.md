# Gamepad Flightsim Project

Testing environment for future gamepad/HID builds (mostly for flight sim purposes).

# Goal
The purpose is to recreate (mainly avionics) the A330 cockpit. I could build it for different aircrafts as well, but I am interested in Airbus for now.

# TO DO
- MCDU
- Autopilot
- Overhead
- Center Console

# Progress Report

2026 3 24
- will go down the generic HID hardware + supporting software route instead of relying on the gamepad/joystick HID

2026 3 25
- broke down main C file from gpio configs and HID-related descriptors
- updated python config to have fallback ESP-only testing defs
- used mapping/dictionary for commands/datarefs
- i learned more about bitwise AND/XOR operation + left/right shifts
- problem about the websocket connection dying when sending commands too fast even with debounce

2026 3 30
- temporarily created matrixkb.c to experiment on a matrix key input