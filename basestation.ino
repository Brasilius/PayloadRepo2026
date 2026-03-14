/*
 * ESP32 + RYLR998 - TRIGGER SENDER
 * REUSED PINS: RX=16, TX=17, RST=4
 * * BEHAVIOR:
 * 1. Waits for user to type "seperate" in Serial Monitor.
 * 2. Sends the integer "1" to the Target Address.
 */

#include <HardwareSerial.h>

// --- CONFIGURATION ---
const int LOCAL_ADDRESS = 2;   // This board (Sender)
const int TARGET_ADDRESS = 3;  // The other board (Receiver from your previous code)
const int NETWORK_ID = 5;      // Must match the receiver

// --- PIN DEFINITIONS (UNCHANGED) ---
#define RX_PIN 16 
#define TX_PIN 17 
#define RST_PIN 4 

HardwareSerial LoRaSerial(2);

void setup() {
  // 1. Start PC Serial
  Serial.begin(115200);
  
  // 2. SAFETY DELAY
  delay(3000); // Shortened slightly for faster testing
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
  
  String addressCmd = "AT+ADDRESS=" + String(LOCAL_ADDRESS);
  sendATCommand(addressCmd);
  delay(500);

  String networkCmd = "AT+NETWORKID=" + String(NETWORK_ID);
  sendATCommand(networkCmd);
  delay(500);

  Serial.println("----------------------------------------");
  Serial.println("   SYSTEM READY");
  Serial.println("   Type 'seperate' to send signal '1'");
  Serial.println("----------------------------------------");
}

void loop() {
  // --- SENDING LOGIC ---
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim(); // Remove whitespace/newlines
    
    if (input.length() > 0) {
      // Check for the specific keyword "seperate" (Case insensitive)
      if (input.equalsIgnoreCase("seperate")) {
        
        Serial.println();
        Serial.println("[TRIGGER DETECTED] Sending signal...");

        // Construct command: AT+SEND=<Address>,<Length>,<Data>
        // Data is "1", so Length is 1.
        String cmd = "AT+SEND=" + String(TARGET_ADDRESS) + ",1,1";
        LoRaSerial.println(cmd);
        
      } else {
        Serial.println("[IGNORED] Type 'seperate' to trigger.");
      }
    }
  }

  // --- RECEIVING / DEBUG LOGIC ---
  // We keep this to see "OK" or "+ERR" responses from the module
  if (LoRaSerial.available()) {
    String incoming = LoRaSerial.readStringUntil('\n');
    incoming.trim();
    if (incoming.length() > 0) {
      Serial.print("[RYLR998]: ");
      Serial.println(incoming);
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