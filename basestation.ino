/*
 * ESP32 + RYLR998 - TRIGGER SENDER / LISTENER
 * REUSED PINS: RX=16, TX=17, RST=4
 * * BEHAVIOR:
 * 1. Waits for user to type '1' or '2' in Serial Monitor.
 * 2. '1' - Separation Command: Sends '1' to Target Address 3.
 * 3. '2' - Initialize Payload: Sends '2' to Target Address 4 and shifts to Listener Mode indefinitely.
 */

#include <HardwareSerial.h>

// --- CONFIGURATION ---
const int LOCAL_ADDRESS = 2;    // This board (Sender)
const int TARGET_ADDRESS_1 = 3; // The other board (Separation target)
const int TARGET_ADDRESS_2 = 4; // The payload board (Initialize target)
const int NETWORK_ID = 5;       // Must match the receivers

// --- PIN DEFINITIONS ---
#define RX_PIN 16 
#define TX_PIN 17 
#define RST_PIN 4 

HardwareSerial LoRaSerial(2);

// --- STATE VARIABLE ---
bool isListenerMode = false; 

void setup() {
  // 1. Start PC Serial
  Serial.begin(115200);
  
  // 2. SAFETY DELAY
  delay(3000); 
  Serial.println();
  Serial.println("----------------------------------------");
  Serial.println("   SENDER STARTING...");
  Serial.println("----------------------------------------");

  // 3. Start LoRa Serial
  LoRaSerial.begin(115200, SERIAL_8N1, RX_PIN, TX_PIN);

  // 4. Reset LoRa Module
  pinMode(RST_PIN, OUTPUT);
  digitalWrite(RST_PIN, HIGH); delay(100);
  digitalWrite(RST_PIN, LOW);  delay(100);
  digitalWrite(RST_PIN, HIGH); delay(1000); 

  // 5. Configure LoRa
  sendATCommand("AT"); 
  delay(500);

  sendATCommand("AT+BAND=914500000"); //change band to 914.5 Mhz per NASA requirements
  delay(500);
  
  String addressCmd = "AT+ADDRESS=" + String(LOCAL_ADDRESS);
  sendATCommand(addressCmd);
  delay(500);

  String networkCmd = "AT+NETWORKID=" + String(NETWORK_ID);
  sendATCommand(networkCmd);
  delay(500);

  Serial.println("----------------------------------------");
  Serial.println("   SYSTEM READY");
  Serial.println("   Type '1' for Separation Command (Board 3)");
  Serial.println("   Type '2' for Initialize Payload (Board 4)");
  Serial.println("----------------------------------------");
}

void loop() {
  // --- SENDING LOGIC ---
  // This block only executes if the board has not been shifted into Listener Mode
  if (!isListenerMode && Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim(); // Remove whitespace/newlines
    
    if (input == "1") {
      Serial.println();
      Serial.println("[COMMAND 1] Separation Command Detected. Transmitting...");

      // Construct command: AT+SEND=<Address>,<Length>,<Data>
      String cmd = "AT+SEND=" + String(TARGET_ADDRESS_1) + ",1,1";
      LoRaSerial.println(cmd);
      
    } else if (input == "2") {
      Serial.println();
      Serial.println("[COMMAND 2] Initialize Payload Detected. Transmitting...");

      // Construct command for hardware ID 4
      String cmd = "AT+SEND=" + String(TARGET_ADDRESS_2) + ",1,2";
      LoRaSerial.println(cmd);

      // Lock the system into listener mode
      Serial.println("[STATE CHANGE] Shifting to indefinite Listener Mode...");
      isListenerMode = true;

    } else if (input.length() > 0) {
      Serial.println("[IGNORED] Invalid input. Type '1' or '2'.");
    }
  }

  // --- RECEIVING LOGIC ---
  // The RYLR998 is always receiving. This outputs all incoming data to the laptop.
  if (LoRaSerial.available()) {
    String incoming = LoRaSerial.readStringUntil('\n');
    incoming.trim();
    if (incoming.length() > 0) {
      if (isListenerMode) {
        // Formats output specifically for payload data monitoring
        Serial.print("[INCOMING DATA]: ");
        Serial.println(incoming);
      } else {
        // Standard debug output for module responses (e.g., +OK)
        Serial.print("[RYLR998]: ");
        Serial.println(incoming);
      }
    }
  }
}

// --- HELPER FUNCTION ---
void sendATCommand(String cmd) {
  LoRaSerial.println(cmd);
  delay(100); 
  while (LoRaSerial.available()) {
    String response = LoRaSerial.readStringUntil('\n');
    response.trim();
    if (response.length() > 0) {
      Serial.print("[CFG]: ");
      Serial.println(response);
    }
  }
}