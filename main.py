import subprocess
import threading
import queue
import time
import sys

# Global queue for thread-safe data transfer
data_queue = queue.Queue()

def read_soil_conductivity(port="/dev/ttyUSB0", executable_path="./modbus_reader"): 
    """
    Executes the C++ Modbus reader program and parses the electrical conductivity.
    """
    try:
        # Execute the C++ compiled program
        result = subprocess.run(
            [executable_path, port],
            capture_output=True,
            text=True,
            check=True
        )

        # Remove trailing whitespace
        hex_output = result.stdout.strip()

        if not hex_output:
            print("Error: No output received from the C++ program.")
            return None

        # A valid 1-register read response is exactly 7 bytes (14 hexadecimal characters)
        if len(hex_output) < 14:
            print(f"Error: Incomplete response received: {hex_output}")
            return None

        # Parse the Modbus RTU response frame
        # Address (chars 0-1)
        # Function Code (chars 2-3)
        # Byte Count (chars 4-5)
        # Data (chars 6-9) -> Electrical Conductivity
        # CRC (chars 10-13)
        data_hex = hex_output[6:10]

        # Convert the 2-byte hexadecimal data to a base-10 integer
        conductivity = int(data_hex, 16)

        return conductivity

    except subprocess.CalledProcessError as e:
        print(f"Subprocess failed with return code {e.returncode}")
        print(f"Standard Error: {e.stderr}")
        return None
    except FileNotFoundError:
        print(f"Error: The executable '{executable_path}' was not found.")
        return None

def read_lora_data(process):
    """
    Executes in a separate thread. Reads output from the C++ executable.
    Transmits valid integers to the main thread via the queue.
    """
    for line in iter(process.stdout.readline, ''):
        data = line.strip()
        if data in ['1', '2', '3']:
            print(f"[LoRa Thread] Valid integer received from /dev/ttyAML6: {data}")
            # Insert data into the queue for the main thread to process
            data_queue.put(data)
        elif data:
            print(f"[LoRa Thread] Unrecognized output: {data}")

def transmit_lora_message(payload, executable_path="./lora_sender", port="/dev/ttyAML6"):
    """
    Executes the C++ LoRa sender program to transmit an integer via the RYLR998.
    
    Args:
        payload (int or str): The integer value to transmit.
        executable_path (str): The file path to the compiled C++ sender executable.
        port (str): The serial port the RYLR998 is connected to.
        
    Returns:
        bool: True if the transmission was successful and acknowledged, False otherwise.
    """
    try:
        # Validate and convert the payload to a string format for command-line execution
        payload_str = str(int(payload))
        
        # Execute the C++ compiled program
        result = subprocess.run(
            [executable_path, payload_str, port],
            capture_output=True,
            text=True,
            check=True
        )

        # Parse the standard output for the expected +OK acknowledgment from the module
        output = result.stdout.strip()
        
        if "+OK" in output:
            print(f"[LoRa TX] Successfully transmitted payload '{payload_str}' on {port}.")
            return True
        else:
            print(f"[LoRa TX] Command executed, but unexpected response received: {output}")
            if result.stderr:
                print(f"[LoRa TX] Error details: {result.stderr.strip()}")
            return False

    except ValueError:
        print("Error: The payload must be an integer or a string representing an integer.")
        return False
    except subprocess.CalledProcessError as e:
        print(f"Subprocess failed with return code {e.returncode}")
        print(f"Standard Error: {e.stderr.strip()}")
        return False
    except FileNotFoundError:
        print(f"Error: The executable '{executable_path}' was not found. Please compile the C++ code.")
        return False

def main():
    print("Initializing system...")
    
    # 1. Initialize the continuous LoRa C++ subprocess
    lora_executable = "./lora_reader"
    try:
        lora_process = subprocess.Popen(
            [lora_executable],
            stdout=subprocess.PIPE,
            stderr=None, 
            text=True,
            bufsize=1
        )
    except FileNotFoundError:
        print(f"Error: {lora_executable} not found. Ensure the C++ code is compiled.")
        sys.exit(1)

    # 2. Start the LoRa receiver thread
    rx_thread = threading.Thread(
        target=read_lora_data, 
        args=(lora_process,), 
        daemon=True
    )
    rx_thread.start()
    
    print("Receiver thread active. Waiting for LoRa commands...")
    
    # 3. Main execution loop
    try:
        while True: 
            # Check if the LoRa thread passed data to the queue
            try:
                message = data_queue.get_nowait()
                print(f"[Main Logic] Event Initiated by Command: {message}")
                
                # Execute specific logic based on the received integer
                if message == '1':
                    print("Executing Command 1: Reading Soil Conductivity...")
                    sensor_port = "/dev/ttyUSB0"
                    reader_executable = "./modbus_reader"
                    
                    conductivity_value = read_soil_conductivity(
                        port=sensor_port, 
                        executable_path=reader_executable
                    )
                    
                    if conductivity_value is not None:
                        print(f"--> Electrical Conductivity Result: {conductivity_value}")
                
                elif message == '2':
                    print("Executing Command 2: [Assign function here]")
                    
                elif message == '3':
                    print("Executing Command 3: [Assign function here]")

            except queue.Empty:
                pass
                
            time.sleep(0.01) # Delay to prevent excessive CPU utilization
            
    except KeyboardInterrupt:
        print("\nTermination signal received. Closing subprocess.")
        lora_process.terminate()
        lora_process.wait()
        print("Process terminated.")

if __name__ == "__main__":
    main()