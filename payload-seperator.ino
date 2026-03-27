/*
 * ESP32 + RYLR998 - RECEIVER + MPL3115A2 SENSOR NODE
 * REUSED PINS: RX=16, TX=17, RST=4
 * I2C PINS: SDA=21, SCL=22 (MPL3115A2)
 *
 * BEHAVIOR:
 * 1. Listens for incoming LoRa packets.
 * 2. When data arrives, prints "RECEIVED" and triggers ematch.
 * 3. Reads MPL3115A2 sensor data (pressure, altitude, temperature).
 * 4. Transmits sensor data to base station (Network ID=5, Hardware ID=2).
 */




#include <HardwareSerial.h>
#include <Wire.h>
#include <Adafruit_MPL3115A2.h>

// --- CONFIGURATION ---
const int LOCAL_ADDRESS  = 3;   // This board (Receiver/Sensor Node)
const int NETWORK_ID     = 5;   // Must match base station
const int BASE_STATION_ID = 2;  // Hardware ID of the base station

// How often to read + transmit sensor data (milliseconds)
const unsigned long SENSOR_INTERVAL = 2000;

// --- PIN DEFINITIONS (UNCHANGED) ---
#define RX_PIN      16
#define TX_PIN      17
#define RST_PIN      4
#define TriggerPin  25
#define SDA_PIN     21
#define SCL_PIN     22

HardwareSerial LoRaSerial(2);
Adafruit_MPL3115A2 baro;

unsigned long lastSensorTime = 0;
bool sensorReady = false;

// -----------------------------------------------------------------------
void setup() {
  // 1. Start PC Serial
  Serial.begin(115200);
  pinMode(TriggerPin, OUTPUT);
  digitalWrite(TriggerPin, LOW);

  // 2. Startup Delay
  delay(2000);
  Serial.println();
  Serial.println("----------------------------------------");
  Serial.println("   RECEIVER + SENSOR NODE STARTING...");
  Serial.println("----------------------------------------");

  // 3. Initialize I2C on GPIO 21 (SDA) and GPIO 22 (SCL)
  Wire.begin(SDA_PIN, SCL_PIN);

  // 4. Initialize MPL3115A2
  if (!baro.begin()) {
    Serial.println("[SENSOR] MPL3115A2 not found! Check wiring.");
    sensorReady = false;
  } else {
    Serial.println("[SENSOR] MPL3115A2 initialized OK.");
    baro.setMode(MPL3115A2_ALTIMETER); // Use altimeter mode (vs barometer)
    sensorReady = true;
  }

  // 5. Start LoRa Serial
  LoRaSerial.begin(115200, SERIAL_8N1, RX_PIN, TX_PIN);

  // 6. Reset LoRa Module
  pinMode(RST_PIN, OUTPUT);
  digitalWrite(RST_PIN, HIGH); delay(100);
  digitalWrite(RST_PIN, LOW);  delay(100);
  digitalWrite(RST_PIN, HIGH); delay(1000);

  // 7. Configure LoRa
  sendATCommand("AT");
  delay(500);
  sendATCommand("AT+ADDRESS=" + String(LOCAL_ADDRESS));
  delay(500);
  sendATCommand("AT+NETWORKID=" + String(NETWORK_ID));
  delay(500);
  //TO DO: Implement AT command for frequency at 914.5 Mhz.
  sendATCommand("AT+BAND=914500000"); // 914.5 MHz per NASA requirements
  delay(500);

  Serial.println("----------------------------------------");
  Serial.println("   SYSTEM READY: Listening + Sensing...");
  Serial.println("----------------------------------------");
}

// -----------------------------------------------------------------------
void loop() {
  // --- RECEIVING LOGIC (UNCHANGED BEHAVIOR) ---
  if (LoRaSerial.available()) {
    String incoming = LoRaSerial.readStringUntil('\n');
    incoming.trim();

    if (incoming.length() > 0) {
      if (incoming.startsWith("+RCV=")) {
        Serial.println("RECEIVED");
        digitalWrite(TriggerPin, HIGH);
        delay(500);
        digitalWrite(TriggerPin, LOW);
        parseAndPrintDebug(incoming); // Uncomment to debug content
      }
      // else { Serial.println(incoming); } // Uncomment to see +OK / +ERR
    }
  }

  // --- SENSOR READ + TRANSMIT LOGIC ---
  unsigned long now = millis();
  if (sensorReady && (now - lastSensorTime >= SENSOR_INTERVAL)) {
    lastSensorTime = now;
    readAndTransmitSensor();
  }
}

// -----------------------------------------------------------------------
// Reads MPL3115A2 and sends a formatted packet to the base station
// -----------------------------------------------------------------------
void readAndTransmitSensor() {
  float altitude    = baro.getAltitude();    // meters
  float temperature = baro.getTemperature(); // Celsius

  // Switch to barometer mode briefly to grab pressure, then back
  baro.setMode(MPL3115A2_BAROMETER);
  float pressure = baro.getPressure();       // Pascals
  baro.setMode(MPL3115A2_ALTIMETER);

  // Build a compact CSV payload: "ALT:xxx.xx,PRES:xxxxx.xx,TEMP:xx.xx"
  String payload = "ALT:"  + String(altitude,    2) +
                   ",PRES:" + String(pressure,    2) +
                   ",TEMP:" + String(temperature, 2);

  int payloadLen = payload.length();

  // RYLR998 send format: AT+SEND=<DestID>,<Length>,<Data>
  String cmd = "AT+SEND=" + String(BASE_STATION_ID) +
               "," + String(payloadLen) +
               "," + payload;

  Serial.print("[SENSOR] Sending -> ");
  Serial.println(payload);

  sendATCommand(cmd);
}

// -----------------------------------------------------------------------
// HELPER FUNCTIONS
// -----------------------------------------------------------------------
void sendATCommand(String cmd) {
  LoRaSerial.println(cmd);
  delay(100);
  while (LoRaSerial.available()) {
    LoRaSerial.readStringUntil('\n'); // flush responses
  }
}

// Optional: Prints content of a received packet for debugging
void parseAndPrintDebug(String raw) {
  // Format: +RCV=SenderID,Length,Data,RSSI,SNR
  String data = raw.substring(5);
  int secondComma = data.indexOf(',', data.indexOf(',') + 1);
  int thirdComma  = data.lastIndexOf(',', data.lastIndexOf(',') - 1);
  String msgContent = data.substring(secondComma + 1, thirdComma);
  Serial.print("   [Content]: ");
  Serial.println(msgContent);
}
