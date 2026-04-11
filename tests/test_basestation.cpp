#include <iostream>
#include <string>
#include <cassert>

// Mocking Arduino-like toFloat for std::string
float stringToFloat(std::string s) {
    try {
        return std::stof(s);
    } catch (...) {
        return 0.0;
    }
}

// Logic from basestation.ino
float extractValue(std::string payload, std::string key) {
    size_t keyIndex = payload.find(key);
    if (keyIndex == std::string::npos) return -9999.0;

    size_t valueStart = keyIndex + key.length();
    size_t valueEnd   = payload.find(',', valueStart);

    std::string valueStr = (valueEnd == std::string::npos)
                    ? payload.substr(valueStart)
                    : payload.substr(valueStart, valueEnd - valueStart);

    return stringToFloat(valueStr);
}

void testExtractAltitude() {
    std::string payload = "ALT:123.45,PRES:101325.00,TEMP:25.50";
    float val = extractValue(payload, "ALT:");
    assert(val > 123.4 && val < 123.5);
    std::cout << "testExtractAltitude: PASSED" << std::endl;
}

void testExtractPressure() {
    std::string payload = "ALT:123.45,PRES:101325.00,TEMP:25.50";
    float val = extractValue(payload, "PRES:");
    assert(val > 101324.0 && val < 101326.0);
    std::cout << "testExtractPressure: PASSED" << std::endl;
}

void testExtractTemperature() {
    std::string payload = "ALT:123.45,PRES:101325.00,TEMP:25.50";
    float val = extractValue(payload, "TEMP:");
    assert(val > 25.4 && val < 25.6);
    std::cout << "testExtractTemperature: PASSED" << std::endl;
}

void testExtractMissingKey() {
    std::string payload = "ALT:123.45,PRES:101325.00,TEMP:25.50";
    float val = extractValue(payload, "HUMID:");
    assert(val == -9999.0);
    std::cout << "testExtractMissingKey: PASSED" << std::endl;
}

void testExtractPartialKey() {
    std::string payload = "ALT:123.45,PRES:101325.00,TEMP:25.50";
    // If we search for "T:", it might find "ALT:" but our logic starts after "T:".
    // Wait, "T:" is in "ALT:". keyIndex will be 2. key.length() is 2. valueStart is 4.
    // valueStr will be "123.45". stringToFloat("123.45") is 123.45.
    // This highlights that keys should be unique and specific.
    float val = extractValue(payload, "T:");
    assert(val > 123.4 && val < 123.5);
    std::cout << "testExtractPartialKey (confirms behavior): PASSED" << std::endl;
}

int main() {
    testExtractAltitude();
    testExtractPressure();
    testExtractTemperature();
    testExtractMissingKey();
    testExtractPartialKey();
    std::cout << "All BaseStation tests PASSED!" << std::endl;
    return 0;
}
