#include <iostream>
#include <vector>
#include <iomanip>
#include <cstdint>
#include <chrono>
#include <sstream>

// Constants for the specific soil sensor request
const uint8_t DEFAULT_ADDRESS = 0x01;
const uint8_t FUNC_READ_HOLDING = 0x03; // Function code for reading registers
const uint16_t EC_REGISTER_ADDR = 0x0015;
const uint16_t EC_REGISTER_LEN = 0x0001;

// Modbus CRC-16 calculation (Polynomial: 0xA001)
uint16_t calculate_crc16(const std::vector<uint8_t>& data) {
    uint16_t crc = 0xFFFF;
    for (uint8_t byte : data) {
        crc ^= byte;
        for (int i = 0; i < 8; ++i) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

// Retrieves the current timestamp as a 4-byte array
std::vector<uint8_t> get_4byte_timestamp() {
    auto now = std::chrono::system_clock::now();
    uint32_t epoch_seconds = static_cast<uint32_t>(std::chrono::system_clock::to_time_t(now));
    
    std::vector<uint8_t> time_bytes(4);
    time_bytes[0] = (epoch_seconds >> 24) & 0xFF;
    time_bytes[1] = (epoch_seconds >> 16) & 0xFF;
    time_bytes[2] = (epoch_seconds >> 8) & 0xFF;
    time_bytes[3] = epoch_seconds & 0xFF;
    return time_bytes;
}

// Formats a byte vector to a hexadecimal string
std::string to_hex_string(const std::vector<uint8_t>& data) {
    std::ostringstream oss;
    for (uint8_t byte : data) {
        oss << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
    }
    return oss.str();
}

// Constructs the Interrogation Frame
void generate_interrogation_frame() {
    std::vector<uint8_t> full_frame;
    std::vector<uint8_t> modbus_payload;

    // 1. Initial structure >_ 4-byte time
    std::vector<uint8_t> start_time = get_4byte_timestamp();
    full_frame.insert(full_frame.end(), start_time.begin(), start_time.end());

    // 2. Modbus Payload (Interrogation)
    modbus_payload.push_back(DEFAULT_ADDRESS);                  // Address code
    modbus_payload.push_back(FUNC_READ_HOLDING);                // Function code
    modbus_payload.push_back((EC_REGISTER_ADDR >> 8) & 0xFF);   // Register start address (High)
    modbus_payload.push_back(EC_REGISTER_ADDR & 0xFF);          // Register start address (Low)
    modbus_payload.push_back((EC_REGISTER_LEN >> 8) & 0xFF);    // Register length (High)
    modbus_payload.push_back(EC_REGISTER_LEN & 0xFF);           // Register length (Low)

    // 3. Error Check (CRC-16 on the Modbus payload)
    uint16_t crc = calculate_crc16(modbus_payload);
    modbus_payload.push_back(crc & 0xFF);                       // Check code low bit
    modbus_payload.push_back((crc >> 8) & 0xFF);                // Check code high bit

    full_frame.insert(full_frame.end(), modbus_payload.begin(), modbus_payload.end());

    // 4. End structure >_ 4-byte time
    std::vector<uint8_t> end_time = get_4byte_timestamp();
    full_frame.insert(full_frame.end(), end_time.begin(), end_time.end());

    // Output in hexadecimal
    std::cout << to_hex_string(full_frame) << std::endl;
}

// Parses the Reply Frame (Placeholder for Python input)
// Expects hex string input from standard input or arguments
void parse_reply_frame(const std::string& hex_input) {
    // In a complete implementation, this function will:
    // 1. Strip the initial 4-byte time.
    // 2. Extract Address (1 byte), Function (1 byte), Quantity of valid bytes (1 byte).
    // 3. Extract Data area (N bytes). For EC, this will be 2 bytes representing the conductivity value.
    // 4. Extract Check code (2 bytes) and validate against the calculated CRC.
    // 5. Strip the end 4-byte time.
    // 6. Output the decimal parsed value for Python to consume.
}

int main(int argc, char* argv[]) {
    // Basic argument routing for Python subprocess execution
    if (argc > 1) {
        std::string command = argv[1];
        if (command == "request") {
            generate_interrogation_frame();
        } else if (command == "parse" && argc > 2) {
            parse_reply_frame(argv[2]);
        } else {
            std::cerr << "Usage: " << argv[0] << " [request | parse <hex_string>]" << std::endl;
            return 1;
        }
    } else {
        // Default behavior: output the interrogation frame
        generate_interrogation_frame();
    }

    return 0;
}