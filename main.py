import subprocess
import threading
import queue
import time
import sys
import json
from datetime import datetime, timezone

import gpiod
import serial  # pyserial — TMC2209 UART configuration

# --- Process executables ---
LORA_RECEIVER_EXEC = "./receivermodule"
LORA_SENDER_EXEC   = "./transmittermodule"
MODBUS_READER_EXEC = "./modbus_reader"

# --- Serial ports ---
LORA_PORT   = "/dev/ttyAML6"   # UART_AO_B: physical 24 (TX, GPIOAO_4) / 26 (RX, GPIOAO_5)
SENSOR_PORT = "/dev/ttyUSB0"   # Modbus RTU soil conductivity sensor

# --- TMC2209 GPIO (gpiochip0 line offsets on AML-S905X periphs-banks) ---
# GPIOX bank starts at offset 46 within gpiochip0.
GPIOX_BASE = 46

PIN_STEP = GPIOX_BASE + 6   # GPIOX_6, physical 11 — STEP pulse output
PIN_DIR  = GPIOX_BASE + 7   # GPIOX_7, physical 13 — DIR (LOW=down, HIGH=up)
PIN_EN   = GPIOX_BASE + 4   # GPIOX_4, physical 29 — EN (active LOW to enable driver)
# GPIOX_5, offset 51, physical 31: unassigned — available for DIAG or future use

# --- TMC2209 UART (UART_AO_A) ---
# Physical 8 (TX, GPIOAO_0) / 10 (RX, GPIOAO_1) → /dev/ttyAML0
# PREREQUISITE: disable the serial console on ttyAML0 before use:
#   sudo systemctl disable serial-getty@ttyAML0.service
#   remove "console=ttyAML0,115200n8" from the kernel command line (e.g. /boot/armbianEnv.txt)
#   then reboot
TMC_UART_PORT   = "/dev/ttyAML0"
TMC_UART_BAUD   = 115200
TMC_DRIVER_ADDR = 0b00   # UART node address: determined by MS1/MS2 pins (both GND → addr 0)

# --- Motor parameters ---
# MRES=8 (full-step GPIO input) + intpol=1 (TMC2209 interpolates to 256 μsteps).
# step_delay = 60 / (RPM × STEPS_PER_REV) = 5 ms at 60 RPM → reliable with time.sleep().
STEPS_PER_REV = 200          # NEMA 17: 1.8° × 200 = 360°
DEPLOY_STEPS  = 4000         # 20 revolutions of linear travel (tune to mechanism)
DEFAULT_RPM   = 60

# --- TMC2209 register addresses ---
_REG_GCONF      = 0x00
_REG_IHOLD_IRUN = 0x10
_REG_CHOPCONF   = 0x6C
_REG_PWMCONF    = 0x70

# --- Motor state (updated by deploy_nema, read by build_telemetry) ---
nema_step_count = 0
motor_rpm       = 0.0

# --- Command queue: receiver thread → main loop ---
command_queue = queue.Queue()


# ---------------------------------------------------------------------------
# Receiver thread
# ---------------------------------------------------------------------------

def monitor_receiver(process):
    """
    Reads stdout from the persistent LoRa receiver subprocess.
    Enqueues '2' when the base station sends the Initialize Payload command
    (basestation.ino: AT+SEND to Address 4, payload '2').
    """
    for line in iter(process.stdout.readline, ''):
        data = line.strip()
        if data == '2':
            print(f"[Receiver] Trigger received: {data}")
            command_queue.put(data)


# ---------------------------------------------------------------------------
# TMC2209 UART register access
# ---------------------------------------------------------------------------

def _tmc_crc8(data: bytes) -> int:
    # CRC-8 per TMC2209 datasheet: poly=0x07, init=0x00, data bits processed LSB-first.
    crc = 0
    for byte in data:
        for _ in range(8):
            if (crc >> 7) ^ (byte & 0x01):
                crc = ((crc << 1) ^ 0x07) & 0xFF
            else:
                crc = (crc << 1) & 0xFF
            byte >>= 1
    return crc


def _tmc_write_reg(ser: serial.Serial, reg: int, value: int) -> None:
    # 8-byte write datagram: [0x05, addr, reg|0x80, data[31:24..7:0], CRC]
    # Single-wire UART echoes TX bytes back on RX — discard them after write.
    payload = bytes([
        0x05,
        TMC_DRIVER_ADDR,
        reg | 0x80,
        (value >> 24) & 0xFF,
        (value >> 16) & 0xFF,
        (value >>  8) & 0xFF,
        value         & 0xFF,
    ])
    ser.write(payload + bytes([_tmc_crc8(payload)]))
    ser.flush()
    ser.read(8)  # discard echo


def _tmc_read_reg(ser: serial.Serial, reg: int) -> int | None:
    # 4-byte read request; TMC2209 replies with 8-byte datagram after the echo.
    req = bytes([0x05, TMC_DRIVER_ADDR, reg])
    ser.write(req + bytes([_tmc_crc8(req)]))
    ser.flush()
    ser.read(4)  # discard echo of request
    resp = ser.read(8)
    if len(resp) < 8 or resp[7] != _tmc_crc8(resp[:7]):
        return None
    return (resp[3] << 24) | (resp[4] << 16) | (resp[5] << 8) | resp[6]


def init_tmc2209() -> bool:
    """
    Configure the TMC2209 via UART on ttyAML0.

    GCONF      0x000000C0 — StealthChop, PDN_UART disabled, MRES from CHOPCONF
    IHOLD_IRUN 0x00060804 — IHOLDDELAY=6, IRUN=8, IHOLD=4
                            I_RMS(run)  = 9/32 × 0.325 V / (√2 × 0.11 Ω) ≈ 587 mA
                            I_RMS(hold) = 5/32 × 0.325 V / (√2 × 0.11 Ω) ≈ 326 mA
                            (assumes R_SENSE = 0.11 Ω; adjust IRUN/IHOLD for other boards)
    CHOPCONF   0x18000005 — TOFF=5 (enable), MRES=8 (full-step input), intpol=1
    PWMCONF    0xC10D0024 — StealthChop PWM defaults (Trinamic recommended)

    Returns True on success.  On UART failure the driver runs with power-on
    defaults (SpreadCycle, 1/8 μsteps) and motor operation continues.
    """
    try:
        with serial.Serial(
            TMC_UART_PORT,
            TMC_UART_BAUD,
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            timeout=0.1,
        ) as ser:
            _tmc_write_reg(ser, _REG_GCONF,      0x000000C0)
            _tmc_write_reg(ser, _REG_IHOLD_IRUN, 0x00060804)
            _tmc_write_reg(ser, _REG_CHOPCONF,   0x18000005)
            _tmc_write_reg(ser, _REG_PWMCONF,    0xC10D0024)

        print("[TMC2209] Driver configured via UART.")
        return True

    except serial.SerialException as e:
        print(f"[TMC2209] UART init failed ({TMC_UART_PORT}): {e}")
        print("[TMC2209] Running with power-on defaults. "
              "Disable the serial console on ttyAML0 (see TMC_UART_PORT comment).")
        return False


# ---------------------------------------------------------------------------
# NEMA motor deployment (TMC2209 STEP/DIR via gpiod)
# ---------------------------------------------------------------------------

def deploy_nema(direction: str) -> None:
    """
    Drive the NEMA 17 stepper via TMC2209 STEP/DIR GPIO pulses (gpiochip0).

    direction='D' → DIR=LOW  (downward deployment)
    direction='U' → DIR=HIGH (upward deployment)

    The TMC2209 CHOPCONF intpol=1 setting causes the driver to internally
    interpolate each full-step GPIO pulse to a 256-μstep waveform, giving
    smooth, quiet motion without requiring high-rate pulse generation from Python.

    EN is held LOW (active) during movement and deasserted HIGH afterward so
    the driver can power down its output stage and reduce heat at rest.
    """
    global nema_step_count, motor_rpm

    label      = "DOWN" if direction == 'D' else "UP"
    step_delay = 60.0 / (DEFAULT_RPM * STEPS_PER_REV)

    print(f"[NEMA] Deploying {label} ({DEPLOY_STEPS} steps at {DEFAULT_RPM} RPM)...")

    try:
        chip      = gpiod.Chip('0')
        step_line = chip.get_line(PIN_STEP)
        dir_line  = chip.get_line(PIN_DIR)
        en_line   = chip.get_line(PIN_EN)

        step_line.request(consumer='nema_step', type=gpiod.LINE_REQ_DIR_OUT, default_vals=[0])
        dir_line.request( consumer='nema_dir',  type=gpiod.LINE_REQ_DIR_OUT, default_vals=[0])
        en_line.request(  consumer='nema_en',   type=gpiod.LINE_REQ_DIR_OUT, default_vals=[1])

    except Exception as e:
        print(f"[NEMA] GPIO setup failed: {e}")
        print("[NEMA] Ensure gpiod is installed and user is in the gpio group.")
        return

    t_start = time.monotonic()

    try:
        dir_line.set_value(0 if direction == 'D' else 1)
        en_line.set_value(0)      # Enable driver (active LOW)
        time.sleep(0.001)         # 1 ms EN→STEP setup time per TMC2209 datasheet

        half_delay = step_delay / 2
        for _ in range(DEPLOY_STEPS):
            step_line.set_value(1)
            time.sleep(half_delay)
            step_line.set_value(0)
            time.sleep(half_delay)

    finally:
        en_line.set_value(1)      # Disable driver (reduces heat at rest)
        step_line.set_value(0)
        step_line.release()
        dir_line.release()
        en_line.release()
        chip.close()

    elapsed_ms = int((time.monotonic() - t_start) * 1000)
    nema_step_count += DEPLOY_STEPS
    if elapsed_ms > 0:
        revolutions = DEPLOY_STEPS / STEPS_PER_REV
        motor_rpm   = revolutions / (elapsed_ms / 60_000.0)

    print(f"[NEMA] {label} complete — "
          f"steps: {DEPLOY_STEPS}, elapsed: {elapsed_ms} ms, RPM: {motor_rpm:.1f}")


# ---------------------------------------------------------------------------
# Modbus / soil sensor
# ---------------------------------------------------------------------------

def read_soil_conductivity():
    """
    Calls ./modbus_reader, parses the raw hex Modbus RTU response, and returns
    electrical conductivity as an integer (µS/cm), or None on failure.
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
# Telemetry — JSON format required by the web dashboard
# ---------------------------------------------------------------------------

def build_telemetry(conductivity, ping_ms):
    """
    Returns a dict matching the hardware JSON format consumed by server.js
    and rendered by the React dashboard (useSerialData.ts → TelemetryData):

      { "pingLatency": <int ms>, "motorRPM": <float>, "stepCount": <int>,
        "soilConductivity": <float µS/cm>, "timestamp": <ISO-8601 UTC> }
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
    JSON-serialises the telemetry dict and passes it to ./transmittermodule.
    Returns the measured transmit latency in ms, or 0 on failure.
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
            print(f"[TX] {len(payload_str)} B — "
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
      1. Deploy NEMA motor fully down via TMC2209 STEP/DIR GPIO.
      2. Deploy NEMA motor fully up.
      3. Read soil conductivity via Modbus continuously, transmitting each
         reading as a JSON telemetry packet over LoRa.
    """
    print("\n--- NEMA Motor Deployment ---")
    deploy_nema('D')
    deploy_nema('U')
    print("--- Deployment Complete ---\n")

    print("--- Starting Continuous Soil Sensing ---")
    ping_ms = 0
    while True:
        conductivity = read_soil_conductivity()
        if conductivity is not None:
            payload = build_telemetry(conductivity, ping_ms)
            ping_ms = transmit_data(payload)
        else:
            print("[Sequence] Modbus read failed — skipping transmission.")
        time.sleep(1)


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def main():
    print("Initializing system...")

    init_tmc2209()

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
