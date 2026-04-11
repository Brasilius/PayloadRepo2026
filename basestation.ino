/*
 * ESP32 + RYLR998 - TRIGGER SENDER / LISTENER (BASE STATION)
 * REUSED PINS: RX=16, TX=17, RST=4
 * Hardware ID: 2, Network ID: 5
 *
 * BEHAVIOR:
 * 1. Waits for user to type '1', '2', or '3' in Serial Monitor.
 * 2. '1' - Separation Command : Sends '1' to Target Address 3.
 * 3. '2' - Initialize Payload : Sends '2' to Target Address 4.
 * 4. '3' - Listener Mode      : Enters telemetry stream from Board 3.
 *                               Type "stop" to exit and return to menu.
 */

#include <HardwareSerial.h>

// --- CONFIGURATION ---
const int LOCAL_ADDRESS    = 2;  // This board (Base Station)
const int TARGET_ADDRESS_1 = 3;  // Separation + Sensor Node
const int TARGET_ADDRESS_2 = 4;  // Payload board
const int NETWORK_ID       = 5;  // Must match all receivers

// --- PIN DEFINITIONS ---
#define RX_PIN  16
#define TX_PIN  17
#define RST_PIN  4

HardwareSerial LoRaSerial(2);

// --- STATE VARIABLE ---
bool isListenerMode = false;

// -----------------------------------------------------------------------
void printMenu() {
  Serial.println();
  Serial.println("----------------------------------------");
  Serial.println("   SYSTEM READY");
  Serial.println("   Type '1' for Separation Command (Board 3)");
  Serial.println("   Type '2' for Initialize Payload  (Board 4)");
  Serial.println("   Type '3' to Enter Telemetry Listener Mode");
  Serial.println("----------------------------------------");
}

// -----------------------------------------------------------------------
void setup() {
  Serial.begin(115200);

  delay(3000);
  Serial.println();
  Serial.println("----------------------------------------");
  Serial.println("   BASE STATION STARTING...");
  Serial.println("----------------------------------------");

  // Start LoRa Serial
  LoRaSerial.begin(115200, SERIAL_8N1, RX_PIN, TX_PIN);

  // Reset LoRa Module
  pinMode(RST_PIN, OUTPUT);
  digitalWrite(RST_PIN, HIGH); delay(100);
  digitalWrite(RST_PIN, LOW);  delay(100);
  digitalWrite(RST_PIN, HIGH); delay(1000);

  // Configure LoRa
  sendATCommand("AT");
  delay(500);
  sendATCommand("AT+BAND=914500000"); // 914.5 MHz per NASA requirements
  delay(500);
  sendATCommand("AT+ADDRESS=" + String(LOCAL_ADDRESS));
  delay(500);
  sendATCommand("AT+NETWORKID=" + String(NETWORK_ID));
  delay(500);

  printMenu();
}

// -----------------------------------------------------------------------
void loop() {

  // =====================================================================
  // LISTENER MODE
  // =====================================================================
  if (isListenerMode) {

    // Check for "stop" typed in Serial Monitor
    if (Serial.available()) {
      String input = Serial.readStringUntil('\n');
      input.trim();
      input.toLowerCase();

      if (input == "stop") {
        isListenerMode = false;
        Serial.println();
        Serial.println("[LISTENER] Stream stopped.");
        printMenu();
        return;
      }
      // Ignore anything else typed during listener mode
    }

    // Print any incoming LoRa packets
    if (LoRaSerial.available()) {
      String incoming = LoRaSerial.readStringUntil('\n');
      incoming.trim();

      if (incoming.length() == 0) return;

      if (incoming.startsWith("+RCV=")) {
        parseSensorPacket(incoming);
      } else {
        // Module status responses (+OK, +ERR, etc.)
        Serial.print("[RYLR998]: ");
        Serial.println(incoming);
      }
    }

    return; // Skip command block entirely while in listener mode
  }

  // =====================================================================
  // COMMAND MODE
  // =====================================================================
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();

    if (input == "1") {
      Serial.println();
      Serial.println("[COMMAND 1] Separation Command detected. Transmitting to Board 3...");
      LoRaSerial.println("AT+SEND=" + String(TARGET_ADDRESS_1) + ",1,1");

      // Wait briefly and print module response
      delay(300);
      while (LoRaSerial.available()) {
        String resp = LoRaSerial.readStringUntil('\n');
        resp.trim();
        if (resp.length() > 0) {
          Serial.print("[RYLR998]: ");
          Serial.println(resp);
        }
      }
      printMenu();

    } else if (input == "2") {
      Serial.println();
      Serial.println("[COMMAND 2] Initialize Payload detected. Transmitting to Board 4...");
      LoRaSerial.println("AT+SEND=" + String(TARGET_ADDRESS_2) + ",1,2");

      // Wait briefly and print module response
      delay(300);
      while (LoRaSerial.available()) {
        String resp = LoRaSerial.readStringUntil('\n');
        resp.trim();
        if (resp.length() > 0) {
          Serial.print("[RYLR998]: ");
          Serial.println(resp);
        }
      }
      printMenu();

    } else if (input == "3") {
      isListenerMode = true;
      Serial.println();
      Serial.println("[LISTENER] Entering Telemetry Listener Mode...");
      Serial.println("           Type 'stop' at any time to exit.");
      Serial.println("----------------------------------------");
      Serial.println("   Board 3 -> ALT (m) | PRES (Pa) | TEMP (C)");
      Serial.println("   Board 4 -> JSON telemetry (piped to dashboard)");
      Serial.println("----------------------------------------");

    } else if (input.length() > 0) {
      Serial.println("[IGNORED] Invalid input. Type '1', '2', or '3'.");
      printMenu();
    }
  }

  // Drain any unsolicited module noise in command mode (+OK bleed, etc.)
  if (LoRaSerial.available()) {
    String incoming = LoRaSerial.readStringUntil('\n');
    incoming.trim();
    if (incoming.length() > 0) {
      Serial.print("[RYLR998]: ");
      Serial.println(incoming);
    }
  }
}

// -----------------------------------------------------------------------
// Routes incoming LoRa packets by sender board.
//
// Board 3 → ALT/PRES/TEMP formatted telemetry for the Serial Monitor.
// Board 4 → JSON telemetry relayed as a raw single line so server.js
//            can JSON.parse() it and push it to the dashboard WebSocket.
//
// Full RCV format: +RCV=<SenderID>,<Len>,<Data>,<RSSI>,<SNR>
// -----------------------------------------------------------------------
void parseSensorPacket(String raw) {
  String body = raw.substring(5);

  int firstComma  = body.indexOf(',');
  int secondComma = body.indexOf(',', firstComma + 1);
  int lastComma   = body.lastIndexOf(',');
  int secondLast  = body.lastIndexOf(',', lastComma - 1);

  if (firstComma < 0 || secondComma < 0 || lastComma < 0 || secondLast < 0) {
    Serial.print("[RAW PACKET]: ");
    Serial.println(raw);
    return;
  }

  String senderID = body.substring(0, firstComma);
  String payload  = body.substring(secondComma + 1, secondLast);
  String rssi     = body.substring(secondLast + 1, lastComma);
  String snr      = body.substring(lastComma + 1);

  // ── Board 4 (Le Potato) ──────────────────────────────────────────────
  // Payload is a compact JSON string. Print it as a single line so the
  // dashboard server (server.js) can call JSON.parse() on it directly.
  // Any other Serial.print() here would corrupt the JSON line.
  if (senderID == String(TARGET_ADDRESS_2)) {
    Serial.println(payload);
    return;
  }

  // ── Board 3 (Separator / Sensor) ─────────────────────────────────────
  // Payload format: "ALT:xxx.xx,PRES:xxxxx.xx,TEMP:xx.xx"
  float altitude    = extractValue(payload, "ALT:");
  float pressure    = extractValue(payload, "PRES:");
  float temperature = extractValue(payload, "TEMP:");

  Serial.println();
  Serial.println("--- TELEMETRY FROM BOARD " + senderID + " ---");
  Serial.print("   Altitude    : "); Serial.print(altitude,    2); Serial.println(" m");
  Serial.print("   Pressure    : "); Serial.print(pressure,    2); Serial.println(" Pa");
  Serial.print("   Temperature : "); Serial.print(temperature, 2); Serial.println(" C");
  Serial.print("   RSSI        : "); Serial.print(rssi);           Serial.println(" dBm");
  Serial.print("   SNR         : "); Serial.print(snr);            Serial.println(" dB");
  Serial.println("-------------------------------");
}

// -----------------------------------------------------------------------
// Extracts a float value from a key:value CSV payload
// -----------------------------------------------------------------------
float extractValue(String payload, String key) {
  int keyIndex = payload.indexOf(key);
  if (keyIndex < 0) return -9999.0;

  int valueStart = keyIndex + key.length();
  int valueEnd   = payload.indexOf(',', valueStart);

  String valueStr = (valueEnd < 0)
                    ? payload.substring(valueStart)
                    : payload.substring(valueStart, valueEnd);

  return valueStr.toFloat();
}

// -----------------------------------------------------------------------
// HELPER FUNCTION - used only during setup configuration
// -----------------------------------------------------------------------
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