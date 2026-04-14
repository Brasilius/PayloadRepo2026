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
4. The Le Potato drives the NEMA stepper **fully down then fully up** via the L298N motor controller, then reads soil **electrical conductivity** via Modbus RTU and transmits JSON telemetry back over LoRa **continuously**.
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
| `nema_l298n.cpp` | Le Potato - Board 4 | NEMA stepper controller via L298N wired directly to Le Potato GPIO |
| `main.py` | Le Potato - Board 4 | Orchestrator: waits for initialize command, deploys motor, reads soil |
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
- RYLR998 LoRa Transceiver → `/dev/ttyAML6` (UART_AO_B, physical pins 24/26)
- Soil Sensor (Modbus RTU, 9600 baud) → `/dev/ttyUSB0`
- L298N H-bridge motor controller wired directly to Le Potato GPIO (see `nema_l298n.cpp`):
  - IN1 → physical pin 7  (GPIOX_6)
  - IN2 → physical pin 11 (GPIOX_17)
  - IN3 → physical pin 13 (GPIOX_18)
  - IN4 → physical pin 15 (GPIOX_19)
  - ENA / ENB → 5V (jumper, always enabled)
  - VS → external 12V supply; GND shared with Le Potato

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

On the Le Potato, compile all C++ modules:

```bash
g++ -o modbus_reader     modbus.cpp
g++ -o receivermodule    recievermodule.cpp
g++ -o transmittermodule transmittermodule.cpp
g++ -o nema_l298n        nema_l298n.cpp
```

> `nema_l298n` writes to `/sys/class/gpio` - run as root or add the user to the `gpio` group:
> ```bash
> sudo usermod -aG gpio $USER   # then log out and back in
> ```
> If the default `GPIOCHIP0_BASE` (410) does not match your kernel, find the correct value with:
> ```bash
> cat /sys/class/gpio/gpiochip*/base | sort -n
> # Lowest number = gpiochip0 base (periphs-banks)
> ```
> Then update `GPIOCHIP0_BASE` at the top of `nema_l298n.cpp` and recompile.

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

### `nema_l298n.cpp` - NEMA Stepper Controller (Le Potato, Board 4)

Drives a bipolar NEMA 17 stepper motor through an L298N dual H-bridge wired directly to the Le Potato GPIO header. No intermediate microcontroller - the Le Potato controls the motor itself via Linux sysfs GPIO.

```bash
./nema_l298n D   # full downward deployment
./nema_l298n U   # full upward deployment
```

On completion, prints `DONE:<steps>:<elapsed_ms>` to `stdout`. `main.py` parses this to calculate and track `motorRPM` and `stepCount` for the telemetry payload.

**Full-step sequence (4-phase, bipolar):**

| Phase | IN1 | IN2 | IN3 | IN4 |
|-------|-----|-----|-----|-----|
| 0     | 1   | 0   | 1   | 0   |
| 1     | 0   | 1   | 1   | 0   |
| 2     | 0   | 1   | 0   | 1   |
| 3     | 1   | 0   | 0   | 1   |

Down = phases 0→1→2→3. Up = phases 3→2→1→0. Coils are de-energised after each move to prevent heat buildup.

**GPIO pin mapping (Le Potato 40-pin header):**

| L298N | Physical pin | GPIO     | gpiochip0 offset |
|-------|-------------|----------|-----------------|
| IN1   | 7           | GPIOX_6  | 52              |
| IN2   | 11          | GPIOX_17 | 63              |
| IN3   | 13          | GPIOX_18 | 64              |
| IN4   | 15          | GPIOX_19 | 65              |

These pins are in the GPIOX bank (`gpiochip0`) - entirely separate from the GPIOAO bank (`gpiochip1`) used by `/dev/ttyAML6` (UART_AO_B, physical 24/26). No conflict.

---

### `main.py` - Payload Orchestrator (Le Potato, Board 4)

The top-level controller for the SBC:

1. Launches `./receivermodule` as a persistent subprocess and monitors its stdout in a background thread.
2. When string `2` is received (Initialize Payload command from the base station), calls `execute_soil_test_sequence()`.
3. The sequence:
   - Invokes `./nema_l298n D` - blocks until the NEMA motor completes full downward deployment.
   - Invokes `./nema_l298n U` - blocks until the NEMA motor completes full upward deployment.
   - Enters a continuous loop: reads soil conductivity via `./modbus_reader`, builds a JSON telemetry packet, and transmits it via `./transmittermodule`. Repeats every second indefinitely.
4. Each telemetry packet includes `pingLatency`, `motorRPM`, `stepCount`, `soilConductivity`, and `timestamp` - matching the hardware JSON format expected by the dashboard.

---

### `nema_hw216.ino` - Legacy Motor Test Sketch

Standalone test sketch for a NEMA motor with an HW216-style STEP/DIR controller on an ESP32. **Not used in flight.** Retained for bench-testing motor mechanics independent of the payload stack.

Accepts serial commands (`D` = deploy down, `U` = deploy up) and responds with `DONE:<steps>:<elapsed_ms>`.
