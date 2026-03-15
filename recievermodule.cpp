/*
 * Libre Computer Le Potato + RYLR998 - RECEIVER
 * Compile with: g++ recievermodule.cpp -o recievermodule
 */

#include <iostream>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <string>
#include <cstring>

// --- CONFIGURATION ---
const int LOCAL_ADDRESS = 4;
const int NETWORK_ID = 5;
// Update this path based on your specific UART configuration on the Le Potato
const char* SERIAL_PORT = "/dev/ttyAML6"; 

// --- HELPER FUNCTIONS ---
void sendATCommand(int fd, const std::string& cmd) {
    std::string full_cmd = cmd + "\r\n";
    write(fd, full_cmd.c_str(), full_cmd.length());
    usleep(100000); // 100ms delay
}

std::string readLine(int fd) {
    std::string result = "";
    char c;
    while (read(fd, &c, 1) > 0) {
        if (c == '\n') break;
        if (c != '\r') result += c; // Ignore carriage return
    }
    return result;
}

int main() {
    // 1. Open Serial Port
    int serial_fd = open(SERIAL_PORT, O_RDWR | O_NOCTTY | O_SYNC);
    if (serial_fd < 0) {
        std::cerr << "Error: Unable to open serial port " << SERIAL_PORT << "\n";
        return 1;
    }

    // 2. Configure UART (115200 8N1)
    struct termios tty;
    if (tcgetattr(serial_fd, &tty) != 0) {
        std::cerr << "Error: tcgetattr failed\n";
        return 1;
    }

    cfsetospeed(&tty, B115200);
    cfsetispeed(&tty, B115200);

    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8; // 8-bit chars
    tty.c_iflag &= ~IGNBRK;                     // disable break processing
    tty.c_lflag = 0;                            // no signaling chars, no echo, no canonical processing
    tty.c_oflag = 0;                            // no remapping, no delays
    tty.c_cc[VMIN]  = 0;                        // read doesn't block
    tty.c_cc[VTIME] = 5;                        // 0.5 seconds read timeout

    tty.c_iflag &= ~(IXON | IXOFF | IXANY);     // shut off xon/xoff ctrl
    tty.c_cflag |= (CLOCAL | CREAD);            // ignore modem controls, enable reading
    tty.c_cflag &= ~(PARENB | PARODD);          // shut off parity
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CRTSCTS;

    if (tcsetattr(serial_fd, TCSANOW, &tty) != 0) {
        std::cerr << "Error: tcsetattr failed\n";
        return 1;
    }

    // Note: Hardware Reset for Linux GPIO is omitted here for brevity. 
    // It is recommended to handle the RST pin via the sysfs or libgpiod interface.

    // 3. Configure LoRa
    sendATCommand(serial_fd, "AT");
    usleep(500000);
    sendATCommand(serial_fd, "AT+ADDRESS=" + std::to_string(LOCAL_ADDRESS));
    usleep(500000);
    sendATCommand(serial_fd, "AT+NETWORKID=" + std::to_string(NETWORK_ID));
    usleep(500000);
    sendATCommand(serial_fd, "AT+BAND=914500000"); 
    usleep(500000);

    // Clear initial buffer setup responses
    while(true) {
        std::string junk = readLine(serial_fd);
        if (junk.empty()) break;
    }

    // 4. Main Reception Loop
    while (true) {
        std::string incoming = readLine(serial_fd);
        
        if (incoming.length() > 0) {
            if (incoming.find("+RCV=") == 0) {
                // Parse the string: +RCV=<Address>,<Length>,<Data>,<RSSI>,<SNR>
                size_t first_comma = incoming.find(',');
                if (first_comma != std::string::npos) {
                    size_t second_comma = incoming.find(',', first_comma + 1);
                    if (second_comma != std::string::npos) {
                        size_t third_comma = incoming.find(',', second_comma + 1);
                        if (third_comma != std::string::npos) {
                            // Extract the payload located between the second and third commas
                            std::string payload = incoming.substr(second_comma + 1, third_comma - second_comma - 1);
                            
                            // Output only the payload for Python to capture
                            std::cout << payload << std::endl;
                        }
                    }
                }
            }
        }
        usleep(10000); // 10ms delay to prevent excessive CPU utilization
    }

    close(serial_fd);
    return 0;
}