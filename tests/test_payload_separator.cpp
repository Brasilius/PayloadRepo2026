#include <iostream>
#include <string>
#include <cassert>
#include <sstream>

// Mock Serial
struct MockSerial {
    std::stringstream buffer;
    void print(const std::string& s) { buffer << s; }
    void println(const std::string& s) { buffer << s << "\n"; }
    void clear() { buffer.str(""); buffer.clear(); }
    std::string str() { return buffer.str(); }
};

MockSerial Serial;

// Logic from payload-seperator.ino
void parseAndPrintDebug(std::string raw) {
  // Format: +RCV=SenderID,Length,Data,RSSI,SNR
  if (raw.length() < 5) return;
  std::string data = raw.substr(5);
  size_t firstComma = data.find(',');
  if (firstComma == std::string::npos) return;
  size_t secondComma = data.find(',', firstComma + 1);
  if (secondComma == std::string::npos) return;
  
  // The original code uses:
  // int thirdComma  = data.lastIndexOf(',', data.lastIndexOf(',') - 1);
  // This is a bit complex in std::string. Let's adapt it.
  
  size_t lastComma = data.find_last_of(',');
  if (lastComma == std::string::npos) return;
  size_t secondLastComma = data.find_last_of(',', lastComma - 1);
  if (secondLastComma == std::string::npos) return;

  std::string msgContent = data.substr(secondComma + 1, secondLastComma - secondComma - 1);
  Serial.print("   [Content]: ");
  Serial.println(msgContent);
}

void testParseDebug() {
    Serial.clear();
    parseAndPrintDebug("+RCV=2,10,MYDATA,-60,12");
    // Expected: "   [Content]: MYDATA\n"
    assert(Serial.str() == "   [Content]: MYDATA\n");
    std::cout << "testParseDebug: PASSED" << std::endl;
}

void testParseDebugWithCommas() {
    Serial.clear();
    // +RCV=2,15,DATA1,DATA2,-60,12
    // SenderID: 2, Length: 15, Data: DATA1,DATA2, RSSI: -60, SNR: 12
    // secondComma points to the comma after 15.
    // secondLastComma points to the comma after DATA2.
    parseAndPrintDebug("+RCV=2,15,DATA1,DATA2,-60,12");
    assert(Serial.str() == "   [Content]: DATA1,DATA2\n");
    std::cout << "testParseDebugWithCommas: PASSED" << std::endl;
}

int main() {
    testParseDebug();
    testParseDebugWithCommas();
    std::cout << "All PayloadSeparator tests PASSED!" << std::endl;
    return 0;
}
