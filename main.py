import subprocess
import threading
import queue
import time
import sys
import json
from datetime import datetime, timezone

import gpiod

# --- Configuration ---
LORA_RECEIVER_EXEC = "./receivermodule"
LORA_SENDER_EXEC   = "./transmittermodule"
MODBUS_READER_EXEC = "./modbus_reader"

LORA_PORT   = "/dev/ttyAML6"
SENSOR_PORT = "/dev/ttyUSB0"

# --- NEMA 17 GPIO configuration (gpiochip0 line offsets) ---
# GPIOX bank starts at offset 46 within gpiochip0 on AML-S905X periphs-banks.
GPIOX_BASE = 46

PIN_IN1 = GPIOX_BASE + 6   # GPIOX_6,  physical 11 - Coil A+
PIN_IN2 = GPIOX_BASE + 7   # GPIOX_7,  physical 13 - Coil A-
PIN_IN3 = GPIOX_BASE + 4   # GPIOX_4,  physical 29 - Coil B+
PIN_IN4 = GPIOX_BASE + 5   # GPIOX_5,  physical 31 - Coil B-

NEMA_PIN_OFFSETS = [PIN_IN1, PIN_IN2, PIN_IN3, PIN_IN4]

# Full-step sequence for bipolar NEMA 17 through L298N dual H-bridge.
# Rows: (IN1, IN2, IN3, IN4).  Down = 0→1→2→3,  Up = 3→2→1→0.
STEP_SEQ = [
    (1, 0, 1, 0),   # phase 0: A+, B+
    (0, 1, 1, 0),   # phase 1: A-, B+
    (0, 1, 0, 1),   # phase 2: A-, B-
    (1, 0, 0, 1),   # phase 3: A+, B-
]

STEPS_PER_REV = 200    # NEMA 17 full-step: 200 steps/rev (1.8°/step)
DEPLOY_STEPS  = 4000   # Full travel distance - tune to your mechanism
DEFAULT_RPM   = 60

# --- Motor state - updated by deploy_nema() ---
nema_step_count = 0
motor_rpm       = 0.0

# Queue for passing LoRa commands from the receiver thread to the main loop
command_queue = queue.Queue()


# ---------------------------------------------------------------------------
# Receiver thread
# ---------------------------------------------------------------------------

def monitor_receiver(process):
    """
    Reads stdout from the persistent C++ receiver process.
    Enqueues '2' when the base station fires the Initialize Payload command
    (basestation.ino: AT+SEND to Address 4, payload '2').
    """
    for line in iter(process.stdout.readline, ''):
        data = line.strip()
        if data == '2':
            print(f"[Receiver] Trigger received: {data}")
            command_queue.put(data)


# ---------------------------------------------------------------------------
# NEMA motor deployment (L298N, direct GPIO via gpiod)
# ---------------------------------------------------------------------------

def deploy_nema(direction):
    """
    Drive the NEMA 17 stepper via the L298N H-bridge using gpiod directly.

    direction='D' → forward step sequence (downward deployment)
    direction='U' → reverse step sequence (upward deployment)

    Updates the global nema_step_count and motor_rpm used in telemetry.
    Coils are de-energised after each move to prevent heat buildup.
    """
    global nema_step_count, motor_rpm

    label     = "DOWN" if direction == 'D' else "UP"
    down      = (direction == 'D')
    step_delay = 60.0 / (DEFAULT_RPM * STEPS_PER_REV)  # seconds per step

    print(f"[NEMA] Deploying {label}...")

    try:
        chip  = gpiod.Chip('gpiochip1')
        lines = chip.get_lines(NEMA_PIN_OFFSETS)
        lines.request(consumer='nema_stepper',
                      type=gpiod.LINE_REQ_DIR_OUT,
                      default_vals=[0, 0, 0, 0])
    except Exception as e:
        print(f"[NEMA] GPIO setup failed: {e}")
        print("[NEMA] Ensure gpiod is installed and user is in the gpio group.")
        return

    t_start = time.monotonic()

    try:
        for i in range(DEPLOY_STEPS):
            phase = (i % 4) if down else (3 - (i % 4))
            lines.set_values(list(STEP_SEQ[phase]))
            time.sleep(step_delay)
    finally:
        lines.set_values([0, 0, 0, 0])  # de-energise all coils
        lines.release()
        chip.close()

    elapsed_ms = int((time.monotonic() - t_start) * 1000)
    nema_step_count += DEPLOY_STEPS
    if elapsed_ms > 0:
        revolutions = DEPLOY_STEPS / STEPS_PER_REV
        motor_rpm   = revolutions / (elapsed_ms / 60_000.0)

    print(f"[NEMA] {label} complete - "
          f"steps: {DEPLOY_STEPS}, elapsed: {elapsed_ms} ms, RPM: {motor_rpm:.1f}")


# ---------------------------------------------------------------------------
# Modbus / soil sensor
# ---------------------------------------------------------------------------

def read_soil_conductivity():
    """
    Calls ./modbus_reader, parses the raw hex Modbus RTU response, and returns
    the electrical conductivity as an integer (µS/cm), or None on failure.
    """
    try:
        result = subprocess.run(
            [MODBUS_READER_EXEC, SENSOR_PORT],
            capture_output=True,
            text=True,
            check=True
        )
        hex_output = result.stdout.strip()
        if len(hex_output) < 14:
            print(f"[Modbus] Incomplete response: {hex_output}")
            return None
        return int(hex_output[6:10], 16)

    except subprocess.CalledProcessError as e:
        print(f"[Modbus] Process failed (code {e.returncode}): {e.stderr}")
        return None
    except FileNotFoundError:
        print(f"[Modbus] Executable '{MODBUS_READER_EXEC}' not found.")
        return None


# ---------------------------------------------------------------------------
# Telemetry - JSON format required by the webUI dashboard
# ---------------------------------------------------------------------------

def build_telemetry(conductivity, ping_ms):
    """
    Returns a dict matching the hardware JSON format consumed by server.js
    and rendered by the React dashboard (useSerialData.ts → TelemetryData):

      {
        "pingLatency":      <int ms>          - LoRa transmit round-trip
        "motorRPM":         <float>           - NEMA RPM during last deployment
        "stepCount":        <int>             - cumulative NEMA steps
        "soilConductivity": <float µS/cm>     - Modbus register 0x0015
        "timestamp":        <ISO-8601 UTC>
      }
    """
    return {
        "pingLatency":      ping_ms,
        "motorRPM":         round(motor_rpm, 2),
        "stepCount":        nema_step_count,
        "soilConductivity": float(conductivity),
        "timestamp":        datetime.now(timezone.utc).isoformat(),
    }


def transmit_data(payload_dict):
    """
    JSON-serialises the telemetry dict and passes it as a single string
    argument to ./transmittermodule, which encodes it in a LoRa AT+SEND frame
    and forwards it to the base station (Address 2).

    server.js on the base station calls JSON.parse() on each received line,
    so the payload must be a single compact JSON string.

    Returns the measured transmit latency in ms (used as next pingLatency),
    or 0 on failure.
    """
    try:
        payload_str = json.dumps(payload_dict, separators=(',', ':'))
        t_start = time.monotonic()
        result = subprocess.run(
            [LORA_SENDER_EXEC, payload_str, LORA_PORT],
            capture_output=True,
            text=True,
            check=True
        )
        ping_ms = int((time.monotonic() - t_start) * 1000)
        output  = result.stdout.strip()

        if "+OK" in output:
            print(f"[TX] {len(payload_str)} B - "
                  f"conductivity={payload_dict['soilConductivity']:.2f} µS/cm, "
                  f"ping={ping_ms} ms")
            return ping_ms
        else:
            print(f"[TX] Unexpected module response: {output}")
            return 0

    except subprocess.CalledProcessError as e:
        print(f"[TX] Process failed (code {e.returncode}): {e.stderr.strip()}")
        return 0
    except FileNotFoundError:
        print(f"[TX] Executable '{LORA_SENDER_EXEC}' not found.")
        return 0


# ---------------------------------------------------------------------------
# Deployment + sensing sequence
# ---------------------------------------------------------------------------

def execute_soil_test_sequence():
    """
    Triggered by command '2' from the base station:
      1. Deploy NEMA motor fully downward via L298N (direct GPIO).
      2. Deploy NEMA motor fully upward  via L298N (direct GPIO).
      3. Read soil conductivity via Modbus (modbus.cpp) continuously,
         transmitting each reading as a JSON telemetry packet over LoRa.
    """
    print("\n--- NEMA Motor Deployment ---")
    deploy_nema('D')    # Full downward travel
    deploy_nema('U')    # Full upward travel
    print("--- Deployment Complete ---\n")

    print("--- Starting Continuous Soil Sensing ---")
    ping_ms = 0
    while True:
        conductivity = read_soil_conductivity()
        if conductivity is not None:
            payload = build_telemetry(conductivity, ping_ms)
            ping_ms = transmit_data(payload)
        else:
            print("[Sequence] Modbus read failed - skipping transmission.")
        time.sleep(1)


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def main():
    print("Initializing system...")

    try:
        receiver_process = subprocess.Popen(
            [LORA_RECEIVER_EXEC, LORA_PORT],
            stdout=subprocess.PIPE,
            stderr=None,
            text=True,
            bufsize=1
        )
    except FileNotFoundError:
        print(f"Error: '{LORA_RECEIVER_EXEC}' not found. Terminating.")
        sys.exit(1)

    rx_thread = threading.Thread(
        target=monitor_receiver,
        args=(receiver_process,),
        daemon=True
    )
    rx_thread.start()

    print("System active. Monitoring for incoming commands...")

    try:
        while True:
            try:
                message = command_queue.get_nowait()
                if message == '2':
                    execute_soil_test_sequence()
            except queue.Empty:
                pass
            time.sleep(0.1)

    except KeyboardInterrupt:
        print("\nTermination signal received. Stopping processes.")
        receiver_process.terminate()
        receiver_process.wait()
        sys.exit(0)


if __name__ == "__main__":
    main()
