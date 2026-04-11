/*
  nema_hw216.ino
  NEMA stepper controller with serial command interface for payload deployment.

  Wiring (ESP32 Dev Kit V1):
  - PIN_STEP  -> controller STEP pin  (GPIO 26)
  - PIN_DIR   -> controller DIR pin   (GPIO 27)
  - SLP + RST -> VIN directly         — driver always awake/out-of-reset, no GPIO needed

  Serial commands (115200 baud, sent from main.py):
    'D'  — full downward deployment (DIR = HIGH)
    'U'  — full upward deployment   (DIR = LOW)

  Response format on completion:
    DONE:<steps_executed>:<elapsed_ms>

  Pin selection rationale:
  - GPIO 6-11: reserved for flash SPI — never use
  - GPIO 0, 2, 12, 15: have boot-mode strapping requirements — avoided
  - GPIO 34, 35, 36, 39: input-only — cannot drive outputs
  - GPIO 25-27: clean general-purpose outputs with no boot-time concerns
*/

const int PIN_DIR            = 27;
const int PIN_STEP           = 26;
const unsigned int STEP_DELAY_US = 800;  // smaller -> faster; 800 µs ≈ 187 RPM on NEMA 17
const int DEPLOY_STEPS       = 4000;     // full deployment travel distance
const int STEPS_PER_REV      = 200;      // NEMA 17 full-step (1.8°/step)

void setup() {
  pinMode(PIN_DIR,  OUTPUT);
  pinMode(PIN_STEP, OUTPUT);
  digitalWrite(PIN_STEP, LOW);

  Serial.begin(115200);
  Serial.println("NEMA_READY");
}

void loop() {
  if (!Serial.available()) return;

  char cmd = Serial.read();

  if (cmd == 'D' || cmd == 'U') {
    bool dirDown = (cmd == 'D');
    unsigned long startMs = millis();
    stepMove(DEPLOY_STEPS, dirDown);
    unsigned long elapsed = millis() - startMs;

    Serial.print("DONE:");
    Serial.print(DEPLOY_STEPS);
    Serial.print(":");
    Serial.println(elapsed);
  }
  // Ignore '\n', '\r', and any other bytes
}

// Toggles STEP pin 'steps' times in the given direction
void stepMove(int steps, bool direction) {
  digitalWrite(PIN_DIR, direction ? HIGH : LOW);
  for (int i = 0; i < steps; ++i) {
    digitalWrite(PIN_STEP, HIGH);
    delayMicroseconds(STEP_DELAY_US);
    digitalWrite(PIN_STEP, LOW);
    delayMicroseconds(STEP_DELAY_US);
  }
}
