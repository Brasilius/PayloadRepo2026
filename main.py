import subprocess
import threading
import queue
import time
import sys

# --- Configuration ---
LORA_RECEIVER_EXEC = "./receiver_module"
LORA_SENDER_EXEC = "./sender_module"
MODBUS_READER_EXEC = "./modbus_reader"

LORA_PORT = "/dev/ttyAML6"
SENSOR_PORT = "/dev/ttyUSB0"

# Queue for transferring data between the receiver thread and main process
command_queue = queue.Queue()

def monitor_receiver(process):
    """
    Reads output from the continuous C++ receiver process.
    Places detected integers into the command queue.
    """
    for line in iter(process.stdout.readline, ''):
        data = line.strip()
        # Note: The provided C++ receiver currently only outputs "RECEIVED".
        # Ensure your C++ receiver outputs the actual payload (e.g., "2") to stdout.
        if data == '2':
            print(f"[Receiver Thread] Valid trigger received: {data}")
            command_queue.put(data)

def read_soil_conductivity(): 
    """
    Executes the C++ Modbus program and parses the electrical conductivity.
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

        # Extract the data segment and convert to base-10 integer
        data_hex = hex_output[6:10]
        return int(data_hex, 16)

    except subprocess.CalledProcessError as e:
        print(f"[Modbus] Process failed (Code {e.returncode}): {e.stderr}")
        return None
    except FileNotFoundError:
        print(f"[Modbus] Executable '{MODBUS_READER_EXEC}' not found.")
        return None

def transmit_data(payload):
    """
    Executes the C++ LoRa sender program to transmit data.
    """
    try:
        payload_str = str(int(payload))
        result = subprocess.run(
            [LORA_SENDER_EXEC, payload_str, LORA_PORT],
            capture_output=True,
            text=True,
            check=True
        )

        output = result.stdout.strip()
        
        if "+OK" in output:
            print(f"[Transmitter] Sent payload: {payload_str}")
            return True
        else:
            print(f"[Transmitter] Unexpected module response: {output}")
            return False

    except subprocess.CalledProcessError as e:
        print(f"[Transmitter] Process failed (Code {e.returncode}): {e.stderr.strip()}")
        return False
    except FileNotFoundError:
        print(f"[Transmitter] Executable '{LORA_SENDER_EXEC}' not found.")
        return False

def execute_soil_test_sequence():
    """
    Runs the soil analysis 20 times, transmitting the result during each iteration.
    """
    print("\n--- Initiating Soil Test Sequence ---")
    for iteration in range(1, 21):
        print(f"Iteration {iteration}/20")
        
        # 1. Read Data
        conductivity = read_soil_conductivity()
        
        # 2. Transmit Data
        if conductivity is not None:
            transmit_data(conductivity)
        else:
            print("[Sequence] Read failed, skipping transmission for this iteration.")
            
        # Delay between iterations to allow hardware components to reset
        time.sleep(1)
        
    print("--- Soil Test Sequence Complete ---\n")

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
        print(f"Error: {LORA_RECEIVER_EXEC} not found. Terminating.")
        sys.exit(1)

    # Start the receiver thread
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
                # Check for incoming commands without blocking the main loop
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