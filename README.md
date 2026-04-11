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
4. The Le Potato reads soil **electrical conductivity** via Modbus RTU and transmits the results back over LoRa - 20 iterations total.

---

## Repository Structure

| File | Node | Description |
|---|---|---|
| `basestation.ino` | ESP32 - Board 2 | Ground operator interface: sends commands, receives telemetry |
| `payload-seperator.ino` | ESP32 - Board 3 | Fires ematch on separation command; streams altimeter telemetry |
| `recievermodule.cpp` | Le Potato - Board 4 | LoRa receiver; outputs payload to stdout for Python |
| `transmittermodule.cpp` | Le Potato - Board 4 | LoRa transmitter; sends soil conductivity readings to base |
| `modbus.cpp` | Le Potato - Board 4 | Reads soil electrical conductivity via Modbus RTU over serial |
| `main.py` | Le Potato - Board 4 | Orchestrator: waits for initialize command, runs soil test sequence |
| `nema_hw216.ino` | Standalone | NEMA motor driver test sketch for HW216 controller |

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
- RYLR998 LoRa Transceiver → `/dev/ttyAML6`
- Soil Sensor (Modbus RTU, 9600 baud) → `/dev/ttyUSB0`

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

On the Le Potato, compile all three C++ modules:

```bash
g++ -o modbus_reader modbus.cpp
g++ -o receivermodule recievermodule.cpp
g++ -o transmittermodule transmittermodule.cpp
```

### 3. Run the Payload Orchestrator

```bash
uv run main.py
```

The program will start the LoRa receiver in a background thread and wait for a command `2` from the base station to trigger the soil test sequence.

---

## Component Details

### `basestation.ino` - Base Station (Board 2)

Operator-facing Serial Monitor interface. On startup, presents a menu:

- **`1`** - Sends separation command to Board 3 (fires ematch)
- **`2`** - Sends initialize command to Board 4 (starts soil test on Le Potato)
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

Called by `main.py` as a subprocess. Takes an integer payload and port as arguments:

```bash
./transmittermodule <value> [portname]
```

Sends the value to the base station (Address `2`) via `AT+SEND`. Returns `+OK` on success.

---

### `modbus.cpp` - Soil Sensor Reader (Le Potato, Board 4)

Reads register `0x0015` (electrical conductivity) from a Modbus RTU soil sensor at 9600 baud. Accepts a serial port as an argument (defaults to `/dev/ttyUSB0`):

```bash
./modbus_reader [portname]
```

Outputs the raw hex response to `stdout`. `main.py` extracts bytes `[6:10]` and converts to a decimal conductivity value.

---

### `main.py` - Payload Orchestrator (Le Potato, Board 4)

The top-level controller for the SBC:

1. Launches `./receivermodule` as a persistent subprocess and monitors its output in a background thread.
2. When the string `2` is received (Initialize Payload command from base station), triggers `execute_soil_test_sequence()`.
3. The sequence runs 20 iterations: reads soil conductivity via `./modbus_reader`, then transmits the value via `./transmittermodule`. 1-second delay between iterations.

---

### `nema_hw216.ino` - Motor Driver Test Sketch

Standalone test sketch for a NEMA motor with an HW216-style controller. Supports two modes set via `MODE_STEPPER`:

- **Stepper mode** (`true`): Uses STEP/DIR signals - runs 400 steps forward, pause, 400 steps back, repeat.
- **DC mode** (`false`): Uses PWM/DIR signals - ramps up then down, repeat.

Default pins: DIR → 4, STEP/PWM → 5, ENABLE → 6. Adjust `STEP_DELAY_US` and `STEPS_PER_MOVE` to tune speed and travel.
