import subprocess
import threading
import queue
import time

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

        # Remove any trailing newlines or whitespace
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
        print(f"Error: The executable '{executable_path}' was not found. Ensure it is compiled and the path is correct.")
        return None



def main():
  # Define the serial port and the path to your compiled C++ executable
    sensor_port = "/dev/ttyUSB0"
    reader_executable = "./modbus_reader"
    
    print(f"Requesting data from {sensor_port}...")
    
    conductivity_value = read_soil_conductivity(
        port=sensor_port, 
        executable_path=reader_executable
    )
    
    if conductivity_value is not None:
        print(f"Electrical Conductivity: {conductivity_value}")

    """
  Actually FUCK this main function
    print("Hello from payloadrepo2026!")
    
    # Start the receiver thread
    rx_thread = threading.Thread(target=reciever, daemon=True)
    rx_thread.start()
    
    while True: 
        # Check if the C++ program passed data to the queue
        try:
            message = data_queue.get_nowait()
            print(f"[Python Logic] Event Triggered: {message}")
            # Insert additional handling logic here
        except queue.Empty:
            pass
            
        time.sleep(.01) # anti CPU death
    """

if __name__ == "__main__":
    main()