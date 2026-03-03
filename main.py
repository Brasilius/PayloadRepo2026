import subprocess
import threading
import queue
import time

data_queue = queue.Queue()

def reciever():
    # Start the compiled C++ program
    proc = subprocess.Popen(['./recievermodule'], stdout=subprocess.PIPE, text=True, bufsize=1)
    
    # Read the C++ output line by line
    for line in iter(proc.stdout.readline, ''):
        clean_line = line.strip()
        if clean_line:
            data_queue.put(clean_line)
            
    proc.stdout.close()
    proc.wait()

def transmit():
    # Example placeholder for transmission logic
    proc = subprocess.Popen(['./example_program2'], stdin=subprocess.PIPE, text=True, bufsize=1)
    # Write to proc.stdin.write("data\n") when necessary
    pass

def get_electrical_conductivity(cpp_executable_path: str, simulated_sensor_reply: str = None) -> int:
    """
    Executes the C++ Modbus program to get the request frame, 
    communicates with the sensor, and parses the reply to an integer.
    """
    # 1. Execute the C++ program to get the interrogation frame
    try:
        process = subprocess.run(
            [cpp_executable_path, "request"], 
            capture_output=True, 
            text=True, 
            check=True
        )
        interrogation_frame = process.stdout.strip()
        print(f"Transmitting to sensor: {interrogation_frame}")
    except subprocess.CalledProcessError as e:
        print(f"Error executing C++ program: {e}")
        return -1

    # 2. Transmit the frame via Serial (Placeholder for actual serial.write)
    # In a live environment, you would send bytes.fromhex(interrogation_frame) via PySerial
    
    # 3. Receive the Reply Frame
    # For demonstration, we use a simulated valid reply if none is provided.
    # Structure of simulated reply:
    # Time(8) + Addr(2) + Func(2) + ValidBytes(2) + Data(4) + CRC(4) + Time(8)
    # Example Data = 01F4 (hex) = 500 (decimal)
    reply_hex = simulated_sensor_reply or "65E5A1B201030201F4B85365E5A1B2"
    print(f"Received from sensor:   {reply_hex}")

    # 4. Parse the Reply Frame
    # Each byte is represented by 2 hexadecimal characters.
    # Index 0-7: Initial 4-byte time (8 chars)
    # Index 8-9: Address code (2 chars)
    # Index 10-11: Function code (2 chars)
    # Index 12-13: Quantity of valid bytes (2 chars)
    # Index 14-17: Data area for register 0015H (4 chars)
    
    try:
        # Extract the 4 characters corresponding to the 2-byte data area
        data_hex = reply_hex[14:18]
        
        # Convert the hexadecimal string to a base-10 integer
        electrical_conductivity = int(data_hex, 16)
        
        return electrical_conductivity
        
    except (ValueError, IndexError) as e:
        print(f"Failed to parse the reply frame: {e}")
        return -1





def main():
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

if __name__ == "__main__":
    main()