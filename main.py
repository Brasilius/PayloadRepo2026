import subprocess
import threading
import queue
import time

data_queue = queue.Queue()

def reciever():
    proc = subprocess.Popen([`./example_program`], stdout = subprocess.PIPE, text=True, bufsize=1)
def transmit():
    proc = subprocess.Popen([`./example_program2`], stdin = subprocess.PIPE, text = True, bufsize=1)


def main():
    print("Hello from payloadrepo2026!")
    while True: 
        #logic placed here
        time.sleep(.01)#anti CPU death

if __name__ == "__main__":
    main()
