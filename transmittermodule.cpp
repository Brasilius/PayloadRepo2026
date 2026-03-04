#include <iostream>
#include <string>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <cstring>

// Adjust this to match the LOCAL_ADDRESS of your ESP32 or receiving node.
// Based on the provided .ino file, the ESP32 is address 2.
#define TARGET_ADDRESS "2" 

int main(int argc, char *argv[]) {
    // Require at least the integer payload as an argument
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <integer_to_send> [portname]" << std::endl;
        return 1;
    }

    std::string payload = argv[1];
    const char *portname = (argc > 2) ? argv[2] : "/dev/ttyUSB0";

    // Open the serial port
    int fd = open(portname, O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0) {
        std::cerr << "Error opening " << portname << std::endl;
        return 1;
    }

    // Configure the serial port for the RYLR998
    struct termios tty;
    if (tcgetattr(fd, &tty) != 0) {
        std::cerr << "Error from tcgetattr" << std::endl;
        close(fd);
        return 1;
    }

    // The .ino file uses 115200 baud for the LoRa module
    cfsetospeed(&tty, B115200);
    cfsetispeed(&tty, B115200);

    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8; // 8 data bits
    tty.c_iflag &= ~IGNBRK; 
    tty.c_lflag = 0; 
    tty.c_oflag = 0; 
    tty.c_cc[VMIN]  = 0; 
    tty.c_cc[VTIME] = 5; // 0.5 seconds read timeout
    tty.c_iflag &= ~(IXON | IXOFF | IXANY); 
    tty.c_cflag |= (CLOCAL | CREAD); 
    tty.c_cflag &= ~(PARENB | PARODD); // No parity
    tty.c_cflag &= ~CSTOPB; // 1 stop bit
    tty.c_cflag &= ~CRTSCTS; // No hardware flow control

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        std::cerr << "Error from tcsetattr" << std::endl;
        close(fd);
        return 1;
    }

    // Construct the RYLR998 AT command
    // Format: AT+SEND=<Address>,<Payload Length>,<Payload>\r\n
    std::string cmd = "AT+SEND=" + std::string(TARGET_ADDRESS) + "," + 
                      std::to_string(payload.length()) + "," + payload + "\r\n";

    // Transmit the command
    int written = write(fd, cmd.c_str(), cmd.length());
    if (written != cmd.length()) {
        std::cerr << "Error writing to serial port." << std::endl;
        close(fd);
        return 1;
    }

    // Read the acknowledgment from the RYLR998 module (+OK)
    char response[256];
    memset(response, '\0', sizeof(response));
    
    // 50ms delay to allow the module to process the AT command and respond
    usleep(50000); 
    
    int bytes_read = read(fd, response, sizeof(response) - 1);
    if (bytes_read > 0) {
        std::cout << "[LoRa TX] " << response;
    } else {
        std::cerr << "[LoRa TX] No acknowledgment received from module." << std::endl;
        close(fd);
        return 1;
    }

    close(fd);
    return 0;
}