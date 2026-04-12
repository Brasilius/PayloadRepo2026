#include <iostream>
#include <string>
#include <cassert>
#include <vector>

// Function logic copied from recievermodule.cpp
std::string extractPayload(const std::string& incoming) {
    if (incoming.length() > 0) {
        if (incoming.find("+RCV=") == 0) {
            // Parse the string: +RCV=<Address>,<Length>,<Data>,<RSSI>,<SNR>
            size_t first_comma = incoming.find(',');
            if (first_comma != std::string::npos) {
                size_t second_comma = incoming.find(',', first_comma + 1);
                if (second_comma != std::string::npos) {
                    size_t third_comma = incoming.find(',', second_comma + 1);
                    if (third_comma != std::string::npos && third_comma > second_comma) {
                        // Extract the payload located between the second and third commas
                        return incoming.substr(second_comma + 1, third_comma - second_comma - 1);
                    }
                }
            }
        }
    }
    return "";
}

void testBasicPayload() {
    std::string input = "+RCV=4,5,HELLO,-50,10";
    std::string result = extractPayload(input);
    assert(result == "HELLO");
    std::cout << "testBasicPayload: PASSED" << std::endl;
}

void testPayloadWithCommas() {
    // Edge case: what if payload contains commas?
    // Current implementation: uses second and third commas.
    // +RCV=<Address>,<Length>,<Data>,<RSSI>,<SNR>
    // If Data contains a comma, third_comma will point to the comma in Data.
    // This might be a bug in the original code if Data can contain commas.
    std::string input = "+RCV=4,11,HELLO,WORLD,-50,10";
    std::string result = extractPayload(input);
    // Based on current logic: first_comma at 6, second_comma at 8, third_comma at 14.
    // substr(8+1, 14-8-1) -> substr(9, 5) -> "HELLO"
    // The logic expects Data NOT to have commas or it only takes the part before the first comma in Data.
    assert(result == "HELLO");
    std::cout << "testPayloadWithCommas (current logic): PASSED" << std::endl;
}

void testEmptyInput() {
    assert(extractPayload("") == "");
    std::cout << "testEmptyInput: PASSED" << std::endl;
}

void testInvalidPrefix() {
    assert(extractPayload("SOMETHING ELSE") == "");
    std::cout << "testInvalidPrefix: PASSED" << std::endl;
}

void testMalformedRCV() {
    assert(extractPayload("+RCV=4,5") == "");
    std::cout << "testMalformedRCV: PASSED" << std::endl;
}

void testMalformedRCVNotEnoughCommas() {
    assert(extractPayload("+RCV=4,5,HELLO") == "");
    std::cout << "testMalformedRCVNotEnoughCommas: PASSED" << std::endl;
}

void testMalformedRCVConsecutiveCommas() {
    assert(extractPayload("+RCV=4,5,,-50,10") == "");
    std::cout << "testMalformedRCVConsecutiveCommas: PASSED" << std::endl;
}

int main() {
    testBasicPayload();
    testPayloadWithCommas();
    testEmptyInput();
    testInvalidPrefix();
    testMalformedRCV();
    testMalformedRCVNotEnoughCommas();
    testMalformedRCVConsecutiveCommas();
    std::cout << "All ReceiverModule tests PASSED!" << std::endl;
    return 0;
}
