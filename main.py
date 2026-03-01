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