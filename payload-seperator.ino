/*
 * ESP32 + RYLR998 - RECEIVER
 * REUSED PINS: RX=16, TX=17, RST=4
 * * BEHAVIOR:
 * 1. Listens for incoming LoRa packets.
 * 2. When data arrives, prints "RECEIVED" to Serial Monitor.
 */

#include <HardwareSerial.h>

// --- CONFIGURATION ---
const int LOCAL_ADDRESS = 3;   // This board (Receiver)
const int NETWORK_ID = 5;      // Must match Sender

// --- PIN DEFINITIONS (UNCHANGED) ---
#define RX_PIN 16 
#define TX_PIN 17 
#define RST_PIN 4 
#define TriggerPin 25

HardwareSerial LoRaSerial(2);

void setup() {
  // 1. Start PC Serial
  Serial.begin(115200);
  pinMode(TriggerPin, OUTPUT);
  digitalWrite(TriggerPin, LOW);
  // 2. Startup Delay
  delay(2000);
  Serial.println();
  Serial.println("----------------------------------------");
  Serial.println("   RECEIVER LISTENING...");
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
  Serial.println("   SYSTEM READY: Waiting for Signal...");
  Serial.println("----------------------------------------");
}

void loop() {
  // --- RECEIVING LOGIC ---
  if (LoRaSerial.available()) {
    // Read the incoming line from the LoRa module
    String incoming = LoRaSerial.readStringUntil('\n');
    incoming.trim();
    
    // Check if valid data
    if (incoming.length() > 0) {
      
      // The RYLR998 indicates a message with "+RCV="
      if (incoming.startsWith("+RCV=")) {
         Serial.println("RECEIVED");
         digitalWrite(TriggerPin, HIGH); //triggers the ematch because fuck this entire code base and what it stands for
         
         // Optional: If you want to see the specific data (e.g., "1")
         // parseAndPrintDebug(incoming); 
      } 
      else {
         // This handles debug messages like "+OK" or "+ERR"
         // Uncomment the line below if you want to see module status
         // Serial.println(incoming);
      }
    }
  }
}

// --- HELPER FUNCTIONS ---

void sendATCommand(String cmd) {
  LoRaSerial.println(cmd);
  delay(100); 
  while (LoRaSerial.available()) {
    // Flush responses during setup so they don't clog the loop later
    LoRaSerial.readStringUntil('\n');
  }
}

// Optional: Prints details if you want to verify it was actually "1"
void parseAndPrintDebug(String raw) {
  // Format: +RCV=SenderID,Length,Data,RSSI,SNR
  String data = raw.substring(5);
  int secondComma = data.indexOf(',', data.indexOf(',') + 1);
  int thirdComma = data.lastIndexOf(',', data.lastIndexOf(',') - 1);
  
  String msgContent = data.substring(secondComma + 1, thirdComma);
  Serial.print("   [Content]: ");
  Serial.println(msgContent);
}