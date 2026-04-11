#include <iostream>
#include <string>
#include <cassert>

// Logic from transmittermodule.cpp
std::string formatCommand(const std::string& target, const std::string& payload) {
    return "AT+SEND=" + target + "," + std::to_string(payload.length()) + "," + payload + "\r\n";
}

void testFormatCommand() {
    std::string result = formatCommand("2", "123");
    assert(result == "AT+SEND=2,3,123\r\n");
    std::cout << "testFormatCommand: PASSED" << std::endl;
}

void testFormatCommandLong() {
    std::string result = formatCommand("10", "HELLO WORLD");
    assert(result == "AT+SEND=10,11,HELLO WORLD\r\n");
    std::cout << "testFormatCommandLong: PASSED" << std::endl;
}

int main() {
    testFormatCommand();
    testFormatCommandLong();
    std::cout << "All Transmitter logic tests PASSED!" << std::endl;
    return 0;
}
