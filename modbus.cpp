#include <iostream>
#include <iomanip>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <cstring>
#include <stdint.h>

// Modbus CRC-16 calculation
uint16_t calculateCRC(uint8_t *buffer, int length) {
    uint16_t crc = 0xFFFF;
    for (int pos = 0; pos < length; pos++) {
        crc ^= (uint16_t)buffer[pos];
        for (int i = 8; i != 0; i--) {
            if ((crc & 0x0001) != 0) {
                crc >>= 1;
                crc ^= 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

int main(int argc, char *argv[]) {
    // Default to /dev/ttyUSB0 if no argument is provided
    const char *portname = (argc > 1) ? argv[1] : "/dev/ttyUSB0";
    
    // Open the serial port
    int fd = open(portname, O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0) {
        std::cerr << "Error opening " << portname << std::endl;
        return 1;
    }

    // Configure the serial port (9600 baud, 8 data bits, No parity, 1 stop bit)
    struct termios tty;
    if (tcgetattr(fd, &tty) != 0) {
        std::cerr << "Error from tcgetattr" << std::endl;
        return 1;
    }

    cfsetospeed(&tty, B9600);
    cfsetispeed(&tty, B9600);

    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8; 
    tty.c_iflag &= ~IGNBRK; 
    tty.c_lflag = 0; 
    tty.c_oflag = 0; 
    tty.c_cc[VMIN]  = 0; 
    tty.c_cc[VTIME] = 5; 
    tty.c_iflag &= ~(IXON | IXOFF | IXANY); 
    tty.c_cflag |= (CLOCAL | CREAD); 
    tty.c_cflag &= ~(PARENB | PARODD); 
    tty.c_cflag &= ~CSTOPB; 
    tty.c_cflag &= ~CRTSCTS;

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        std::cerr << "Error from tcsetattr" << std::endl;
        return 1;
    }

    // Prepare the interrogation frame
    // Address: 0x01, Function: 0x03 (Read Holding Register)
    // Register Address: 0x0015, Register Length: 0x0001
    uint8_t request[8];
    request[0] = 0x01; 
    request[1] = 0x03; 
    request[2] = 0x00; 
    request[3] = 0x15; 
    request[4] = 0x00; 
    request[5] = 0x01; 

    // Append CRC (Low bit first, High bit second)
    uint16_t crc = calculateCRC(request, 6);
    request[6] = crc & 0xFF;        
    request[7] = (crc >> 8) & 0xFF; 

    // Initial structure >= 4-byte time (At 9600 baud, 1 byte is ~1.04ms. 5000us = 5ms)
    usleep(5000);

    // Send the request
    int written = write(fd, request, sizeof(request));
    if (written != sizeof(request)) {
        std::cerr << "Error writing to serial port." << std::endl;
        close(fd);
        return 1;
    }

    // Read the reply frame
    // Expected size for 1 register read: 1+1+1+2+2 = 7 bytes
    uint8_t response[256];
    int total_read = 0;
    int bytes_read = 0;

    // Small delay to allow sensor to process and respond
    usleep(50000); 

    do {
        bytes_read = read(fd, response + total_read, sizeof(response) - total_read);
        if (bytes_read > 0) {
            total_read += bytes_read;
        }
    } while (bytes_read > 0 && total_read < 7);

    // End structure >= 4-byte time
    usleep(5000);
    close(fd);

    // Output the response in hexadecimal for Python to capture
    if (total_read > 0) {
        for (int i = 0; i < total_read; i++) {
            std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)response[i];
        }
        std::cout << std::endl;
    } else {
        std::cerr << "No response received from sensor." << std::endl;
        return 1;
    }

    return 0;
}