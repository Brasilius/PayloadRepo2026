import subprocess
import threading
import queue
import time
import sys
import json
from datetime import datetime, timezone

# --- Configuration ---
LORA_RECEIVER_EXEC = "./receivermodule"
LORA_SENDER_EXEC   = "./transmittermodule"
MODBUS_READER_EXEC = "./modbus_reader"
NEMA_EXEC          = "./nema_l298n"      # L298N stepper controller (nema_l298n.cpp)

LORA_PORT   = "/dev/ttyAML6"
SENSOR_PORT = "/dev/ttyUSB0"

# NEMA 17 full-step: 200 steps per revolution.
# Used to convert the DONE:<steps>:<elapsed_ms> report into an RPM value
# for the webUI telemetry payload.
STEPS_PER_REV = 200

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
# NEMA motor deployment (L298N, nema_l298n.cpp)
# ---------------------------------------------------------------------------

def deploy_nema(direction):
    """
    Invokes ./nema_l298n with 'D' (down) or 'U' (up).
    The C++ program drives the L298N via Le Potato GPIO sysfs, blocks until the
    full deployment move is complete, then prints DONE:<steps>:<elapsed_ms>.

    Updates the global nema_step_count and motor_rpm used in telemetry.
    """
    global nema_step_count, motor_rpm

    label = "DOWN" if direction == 'D' else "UP"
    print(f"[NEMA] Deploying {label}...")

    try:
        result = subprocess.run(
            [NEMA_EXEC, direction],
            capture_output=True,
            text=True,
            timeout=120     # Hard ceiling: 4000 steps × 2 ms = ~8 s in practice
        )

        if result.returncode != 0:
            print(f"[NEMA] Controller exited with error ({result.returncode}).")
            if result.stderr:
                print(f"[NEMA] {result.stderr.strip()}")
            return

        output = result.stdout.strip()
        if output.startswith('DONE:'):
            parts = output.split(':')
            if len(parts) == 3:
                steps      = int(parts[1])
                elapsed_ms = int(parts[2])
                nema_step_count += steps
                if elapsed_ms > 0:
                    revolutions = steps / STEPS_PER_REV
                    motor_rpm   = revolutions / (elapsed_ms / 60_000.0)
                print(f"[NEMA] {label} complete - "
                      f"steps: {steps}, elapsed: {elapsed_ms} ms, RPM: {motor_rpm:.1f}")
        else:
            print(f"[NEMA] Unexpected output: {output!r}")

    except subprocess.TimeoutExpired:
        print(f"[NEMA] Timeout waiting for {label} deployment to finish.")
    except FileNotFoundError:
        print(f"[NEMA] Executable '{NEMA_EXEC}' not found. "
              f"Compile with: g++ nema_l298n.cpp -o nema_l298n")


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
      1. Deploy NEMA motor fully downward via L298N (nema_l298n.cpp).
      2. Deploy NEMA motor fully upward  via L298N (nema_l298n.cpp).
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
            [LORA_RECEIVER_EXEC],
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
