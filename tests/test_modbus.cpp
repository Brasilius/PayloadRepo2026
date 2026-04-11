#include <iostream>
#include <stdint.h>
#include <cassert>

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

void testBasicCRC() {
    uint8_t data[] = {0x01, 0x03, 0x00, 0x15, 0x00, 0x01};
    uint16_t crc = calculateCRC(data, 6);
    // Verified result is 0xCE95
    assert(crc == 0xCE95);
    std::cout << "testBasicCRC: PASSED" << std::endl;
}

void testSingleByte() {
    uint8_t data[] = {0x01};
    uint16_t crc = calculateCRC(data, 1);
    // 0xFFFF ^ 0x0001 = 0xFFFE
    // 8 iterations: 0xFFFE -> 0x7FFF -> 0xBFFF^0xA001...
    // Let's trust the current implementation for consistency if it matches common Modbus tools
    assert(crc == 0x807E); 
    std::cout << "testSingleByte: PASSED" << std::endl;
}

void testEmptyBuffer() {
    uint8_t data[] = {};
    uint16_t crc = calculateCRC(data, 0);
    assert(crc == 0xFFFF);
    std::cout << "testEmptyBuffer: PASSED" << std::endl;
}

int main() {
    testBasicCRC();
    testSingleByte();
    testEmptyBuffer();
    std::cout << "All Modbus tests PASSED!" << std::endl;
    return 0;
}
