# PayloadRepo2026

Boomer Rocket Team's codebase for the Payload system - 2025-2026 NASA USLI Competition.

## System Overview

This system automates an in-field soil conductivity experiment deployed from a rocket payload. It spans three physical nodes communicating over LoRa at 914.5 MHz (per NASA frequency requirements):

```
[Base Station ESP32]  <--LoRa-->  [Separator/Sensor ESP32]  <--LoRa-->  [Payload SBC (Le Potato)]
   (Board Address 2)                  (Board Address 3)                     (Board Address 4)
```

**Mission flow:**
1. Ground operator sends **Separation Command** (cmd `1`) → Board 3 fires the ematch to deploy the payload.
2. Board 3 continuously streams **altimeter telemetry** (altitude, pressure, temperature) back to the base station.
3. Once the payload lands, operator sends **Initialize Payload** (cmd `2`) → Board 4 (Le Potato) triggers the soil test sequence.
4. The Le Potato drives the NEMA stepper **fully down then fully up** via the TMC2209 stepper driver, then reads soil **electrical conductivity** via Modbus RTU and transmits JSON telemetry back over LoRa **continuously**.
4. The Le Potato reads soil **electrical conductivity** via Modbus RTU and transmits the results back over LoRa - 20 iterations total.

---

## Repository Structure

| File | Node | Description |
|---|---|---|
| `basestation.ino` | ESP32 - Board 2 | Ground operator interface: sends commands, receives telemetry |
| `payload-seperator.ino` | ESP32 - Board 3 | Fires ematch on separation command; streams altimeter telemetry |
| `recievermodule.cpp` | Le Potato - Board 4 | LoRa receiver; outputs payload to stdout for Python |
| `transmittermodule.cpp` | Le Potato - Board 4 | LoRa transmitter; sends JSON telemetry to base station |
| `modbus.cpp` | Le Potato - Board 4 | Reads soil electrical conductivity via Modbus RTU over serial |
| `nema_tmc2209.cpp` | Le Potato - Board 4 | Standalone NEMA stepper utility (retained for bench testing; motor logic is now in `main.py`) |
| `main.py` | Le Potato - Board 4 | Orchestrator: waits for initialize command, drives NEMA motor via gpiod, reads soil |
| `nema_hw216.ino` | Standalone | Legacy NEMA test sketch (HW216 on ESP32, not used in flight) |

---

## Hardware Requirements

### Base Station (Board 2)
- ESP32 Dev Kit V1
- RYLR998 LoRa Transceiver
  - RX → GPIO 16, TX → GPIO 17, RST → GPIO 4

### Separator / Sensor Node (Board 3)
- ESP32 Dev Kit V1
- RYLR998 LoRa Transceiver (same wiring as above)
- MPL3115A2 Altimeter (I2C)
  - SDA → GPIO 21, SCL → GPIO 22
- Ematch trigger circuit → GPIO 25

### Payload SBC (Board 4)
- AML-S905X (Libre Computer Le Potato) or equivalent SBC

**RYLR998 LoRa Transceiver** (UART_AO_B — `gpiochip1`, separate from GPIOX):

| Signal | Physical pin | GPIO      | Notes               |
|--------|-------------|-----------|---------------------|
| TX     | 24          | GPIOAO_4  | Le Potato → RYLR998 |
| RX     | 26          | GPIOAO_5  | RYLR998 → Le Potato |
| VCC    | any 3.3V    | —         | Module power        |
| GND    | any GND     | —         | Common ground       |

Serial port: `/dev/ttyAML6` at 115200 8N1.

**TMC2209 stepper driver + NEMA 17** (STEP/DIR controlled by `main.py` and `nema_tmc2209` via gpiod on `gpiochip0`; configured at startup via UART_AO_A):

| TMC2209 | Physical pin | GPIO     | gpiochip0 offset | Function                      |
|---------|-------------|----------|------------------|-------------------------------|
| STEP    | 11          | GPIOX_6  | 52               | Step pulse output             |
| DIR     | 13          | GPIOX_7  | 53               | Direction (LOW=down, HIGH=up) |
| EN      | 29          | GPIOX_4  | 50               | Enable (active LOW)           |
| (spare) | 31          | GPIOX_5  | 51               | Available (DIAG or future use)|
| VM      | ext. 12V    | —        | —                | Motor power                   |
| GND     | any GND     | —        | —                | Common ground                 |

TMC2209 UART configuration (single-wire PDN_UART pin → UART_AO_A):

| Signal   | Physical pin | GPIO     | Notes                        |
|----------|-------------|----------|------------------------------|
| TX (AO_A)| 8           | GPIOAO_0 | Le Potato → TMC2209 PDN_UART |
| RX (AO_A)| 10          | GPIOAO_1 | TMC2209 PDN_UART → Le Potato |

Serial port: `/dev/ttyAML0` at 115200 8N1. Driver node address: `0b00` (MS1=GND, MS2=GND).

> **Prerequisite**: disable the kernel serial console on `ttyAML0` before use:
> ```
> sudo systemctl disable serial-getty@ttyAML0.service
> # Remove "console=ttyAML0,115200n8" from /boot/armbianEnv.txt, then reboot.
> ```

> The GPIOX lines (gpiochip0, offsets 50–53) are entirely separate from the GPIOAO lines (gpiochip1) used by `/dev/ttyAML0` (TMC2209 UART) and `/dev/ttyAML6` (LoRa) — no conflict.

**Soil Sensor** (Modbus RTU): → `/dev/ttyUSB0` at 9600 baud.

---

## Software Requirements

- [Astral UV](https://docs.astral.sh/uv/) - Python package/runtime manager
- `g++` - C++ compiler
  ```
  sudo dnf install g++     # Fedora/RHEL
  sudo apt install g++     # Debian/Ubuntu
  ```
- Arduino IDE with ESP32 board support and the following libraries:
  - `Adafruit MPL3115A2` (for `payload-seperator.ino`)
- Linux on SBC (Raspbian or compatible; for Le Potato: [libre.computer/products/aml-s905x-cc](https://libre.computer/products/aml-s905x-cc/))

---

## Setup & Flashing

### 1. Flash Arduino Sketches (ESP32 boards)

Using Arduino IDE, flash the following:

- **Base Station (Board 2):** `basestation.ino`
- **Separator/Sensor Node (Board 3):** `payload-seperator.ino`

Both require the ESP32 board package installed in Arduino IDE.

### 2. Compile C++ Programs (Le Potato)

On the Le Potato, compile the C++ modules used at runtime:

```bash
g++ -o modbus_reader     modbus.cpp
g++ -o receivermodule    recievermodule.cpp
g++ -o transmittermodule transmittermodule.cpp
```

`main.py` drives the NEMA motor directly via Python `gpiod` — no need to compile `nema_tmc2209.cpp` for normal operation. Ensure the `gpiod` Python package is available and the user is in the `gpio` group:

```bash
sudo usermod -aG gpio $USER   # then log out and back in
```

To build `nema_tmc2209.cpp` as a standalone test utility:

```bash
g++ -o nema_tmc2209 nema_tmc2209.cpp -lgpiod
```

### 3. Run the Payload Orchestrator

```bash
uv run main.py
```

The program starts the LoRa receiver in a background thread and waits for command `2` from the base station. On receipt it deploys the NEMA motor then begins continuous soil sensing.

---

## Docker Usage

A `Dockerfile` is provided to run the Payload SBC environment (Le Potato) within a Fedora 42 container. This ensures all C++ modules are compiled and Python dependencies are managed consistently.

### 1. Build the Image
From the repository root:
```bash
docker build -t payload-repo .
```

### 2. Run the Orchestrator (Hardware Mode)
The orchestrator requires access to hardware serial ports and GPIO. Run the container with `--privileged` or map the specific devices and sysfs paths:

```bash
# Recommended for hardware access
docker run --privileged -it payload-repo
```

Alternatively, map specific devices:
```bash
docker run -it \
  --device /dev/ttyAML6:/dev/ttyAML6 \
  --device /dev/ttyUSB0:/dev/ttyUSB0 \
  -v /sys:/sys \
  payload-repo
```

### 3. Run the Dashboard (Simulation Mode)
To run the dashboard and its server within the container, expose the necessary ports (3001 for server, 5173 for web):

```bash
docker run -p 3001:3001 -p 5173:5173 -it payload-repo /bin/bash
```

Inside the container shell, start both the backend (with simulation) and the frontend:
```bash
# Start backend in simulation mode
npm --prefix dashboard/server run dev &

# Start frontend (must listen on 0.0.0.0 for host access)
npm --prefix dashboard/web run dev -- --host
```

Then visit `http://localhost:5173` on your host machine.

*Note: The default container command is `uv run main.py`. To access the dashboard or run individual modules, use `docker run -it payload-repo /bin/bash`.*

---

## Ground Station Dashboard (v2.6)

A real-time Web UI for visualizing LoRa telemetry, NEMA stepper positioning, and high-precision soil conductivity data.

### Features
- **3D Drill Visualization:** Real-time NEMA motor positioning (steps) and rotation (RPM).
- **Live Latency Charting:** Rolling history of LoRa signal ping times.
- **Soil Analysis:** High-precision readout for electrical conductivity (µS/cm).
- **Hardware Simulator:** Built-in mock data generator for testing without physical sensors.

### Initialization

#### 1. Backend (Data Bridge)
The backend pipes serial data to WebSockets.
```bash
cd dashboard/server
npm install
npm run dev      # Starts in SIMULATION mode (no hardware required)
# OR
npm start        # Starts in HARDWARE mode (reads JSON from serial port)
```
*Environment Variables:* `PORT` (default 3001), `SERIAL_PORT` (default `/dev/ttyUSB0`).

#### 2. Frontend (Web UI)
The dashboard is a React/Vite application.
```bash
cd dashboard/web
npm install
npm run dev
```
Open the provided URL (typically `http://localhost:5173`) in your browser.

### Hardware JSON Format
Each telemetry packet transmitted by `main.py` is a compact single-line JSON string. `server.js` reads it from the serial port, calls `JSON.parse()`, and forwards it to the React frontend via WebSocket:
```json
{
  "pingLatency": 25,
  "motorRPM": 150.0,
  "stepCount": 8000,
  "soilConductivity": 1850.25,
  "timestamp": "2026-04-11T12:00:00.000000+00:00"
}
```

---

## Component Details

### `basestation.ino` - Base Station (Board 2)

Operator-facing Serial Monitor interface. On startup, presents a menu:

- **`1`** - Sends separation command to Board 3 (fires ematch)
- **`2`** - Sends initialize command to Board 4 (triggers deployment + soil sensing on Le Potato)
- **`3`** - Enters telemetry listener mode; streams live altitude/pressure/temperature from Board 3. Type `stop` to exit.

LoRa config: Address `2`, Network ID `5`, 914.5 MHz.

---

### `payload-seperator.ino` - Separator / Sensor Node (Board 3)

Dual-purpose node:

1. **Listens** for incoming LoRa packets. On receipt, pulses GPIO 25 HIGH for 500ms to fire the ematch.
2. **Every 2 seconds**, reads the MPL3115A2 and transmits a telemetry packet to the base station in the format:
   ```
   ALT:<meters>,PRES:<pascals>,TEMP:<celsius>
   ```

LoRa config: Address `3`, Network ID `5`, 914.5 MHz.

---

### `recievermodule.cpp` - LoRa Receiver (Le Potato, Board 4)

Configures the RYLR998 on `/dev/ttyAML6` and enters a continuous receive loop. When a `+RCV=` packet arrives, it parses and prints only the payload string to `stdout` - consumed by `main.py` via subprocess pipe.

LoRa config: Address `4`, Network ID `5`, 914.5 MHz.

---

### `transmittermodule.cpp` - LoRa Transmitter (Le Potato, Board 4)

Called by `main.py` as a subprocess. Accepts a payload string and optional port:

```bash
./transmittermodule <payload_string> [portname]
```

Sends the payload to the base station (Address `2`) via `AT+SEND`. `main.py` passes a compact JSON string as the payload. Returns `+OK` on success.

---

### `modbus.cpp` - Soil Sensor Reader (Le Potato, Board 4)

Reads register `0x0015` (electrical conductivity) from a Modbus RTU soil sensor at 9600 baud. Accepts a serial port as an argument (defaults to `/dev/ttyUSB0`):

```bash
./modbus_reader [portname]
```

Outputs the raw hex response frame to `stdout`. `main.py` extracts bytes `[6:10]` and converts to a decimal conductivity value (µS/cm).

---

### `nema_tmc2209.cpp` - Standalone NEMA Test Utility

Retained for bench testing the motor independently of the payload stack. The flight-path motor logic now lives in `main.py` (`deploy_nema()`).

```bash
./nema_tmc2209 D   # full downward deployment
./nema_tmc2209 U   # full upward deployment
```

On completion, prints `DONE:<steps>:<elapsed_ms>` to `stdout`.

Configures the TMC2209 via UART (`/dev/ttyAML0`) at startup (StealthChop, ~587 mA run current, full-step input with 256-µstep interpolation), then steps the motor by pulsing the STEP line. EN is asserted LOW during movement and released HIGH afterward to reduce driver heat at rest.

**GPIO pin mapping (Le Potato 40-pin header):**

| TMC2209 | Physical pin | GPIO     | gpiochip0 offset | Function                      |
|---------|-------------|----------|------------------|-------------------------------|
| STEP    | 11          | GPIOX_6  | 52               | Step pulse                    |
| DIR     | 13          | GPIOX_7  | 53               | Direction (LOW=down, HIGH=up) |
| EN      | 29          | GPIOX_4  | 50               | Enable (active LOW)           |

These pins are in the GPIOX bank (`gpiochip0`) — entirely separate from the GPIOAO bank (`gpiochip1`) used by `/dev/ttyAML0` (TMC2209 UART, physical 8/10) and `/dev/ttyAML6` (LoRa, physical 24/26). No conflict.

---

### `main.py` - Payload Orchestrator (Le Potato, Board 4)

The top-level controller for the SBC:

1. Launches `./receivermodule` as a persistent subprocess and monitors its stdout in a background thread.
2. When string `2` is received (Initialize Payload command from the base station), calls `execute_soil_test_sequence()`.
3. The sequence:
   - Calls `deploy_nema('D')` — drives the stepper fully down by pulsing the TMC2209 STEP/DIR GPIO lines via Python `gpiod` on `gpiochip0`. Runs synchronously in the main thread (no subprocess, no extra thread).
   - Calls `deploy_nema('U')` — same as above but with DIR=HIGH for upward travel.
   - Enters a continuous loop: reads soil conductivity via `./modbus_reader`, builds a JSON telemetry packet, and transmits it via `./transmittermodule`. Repeats every second indefinitely.
4. Each telemetry packet includes `pingLatency`, `motorRPM`, `stepCount`, `soilConductivity`, and `timestamp` - matching the hardware JSON format expected by the dashboard.

---

### `nema_hw216.ino` - Legacy Motor Test Sketch

Standalone test sketch for a NEMA motor with an HW216-style STEP/DIR controller on an ESP32. **Not used in flight.** Retained for bench-testing motor mechanics independent of the payload stack.

Accepts serial commands (`D` = deploy down, `U` = deploy up) and responds with `DONE:<steps>:<elapsed_ms>`.
