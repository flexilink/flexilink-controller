# Flexilink Controller Repository

This repository contains Windows controller binaries, controller source code, and a Wireshark plugin for Flexilink frame analysis.

## Project Context
Flexilink is a non-IP Layer2/3 networking architecture focused on deterministic low-latency transport, security-by-design, and low protocol overhead.

## Repository Structure

### 1) `Controller*` folders
Folders such as `Controller2.1.2`, `Controller3.0.2`, and `Controller3.1.0c_BETA_*` contain Windows controller executable builds (`.exe`).

### 2) `ControllerSRC`
`ControllerSRC` contains the controller source code.

Build requirements:
- Microsoft Visual Studio 2022 or later
- MFC (Microsoft Foundation Class) library support installed

### 3) `WiresharkPlugin`
`WiresharkPlugin` contains `Flexilink.lua`, a Wireshark Lua dissector used to interpret captured Flexilink frames.

## Notes
- Use a controller binary folder for ready-to-run Windows builds.
- Use `ControllerSRC` if you need to modify and recompile the controller.
- Install/load `Flexilink.lua` in Wireshark to decode Flexilink traffic captures.
