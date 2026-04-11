#include <iostream>
#include <stdint.h>
#include <iomanip>

// Modbus CRC-16 calculation (Copied from modbus.cpp for isolation)
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

int main() {
    uint8_t data[] = {0x01, 0x03, 0x00, 0x15, 0x00, 0x01};
    uint16_t crc = calculateCRC(data, 6);
    std::cout << "CRC: 0x" << std::hex << std::uppercase << crc << std::endl;
    return 0;
}
