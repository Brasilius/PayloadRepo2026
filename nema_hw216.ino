 /*
  nema_hw216.ino
  Quick Arduino sketch to drive a NEMA motor via an HW216-style controller.
  Set MODE_STEPPER to true for a stepper using STEP/DIR, false for a DC motor using PWM/DIR.

  Wiring (example):
  - Stepper mode: DIR -> controller DIR pin, STEP -> controller STEP pin, ENABLE -> controller enable
  - DC mode: DIR -> controller DIR pin, PWM -> controller PWM/enable pin

  Adjust pins and timings to match your controller and motor.
*/

const bool MODE_STEPPER = true; // set to false for DC motor mode

// Common pins (change as needed)
const int PIN_DIR = 4;
const int PIN_ENABLE = 6;

// Stepper-specific
const int PIN_STEP = 5;
const unsigned int STEP_DELAY_US = 800; // smaller -> faster
const int STEPS_PER_MOVE = 400; // adjust to suit your motor

// DC-specific
const int PIN_PWM = 5; // shared with STEP if you change wiring for DC

void setup() {
  pinMode(PIN_DIR, OUTPUT);
  pinMode(PIN_ENABLE, OUTPUT);
  digitalWrite(PIN_ENABLE, LOW); // enable controller (check HW216 wiring: LOW/ HIGH may differ)

  if (MODE_STEPPER) {
    pinMode(PIN_STEP, OUTPUT);
    digitalWrite(PIN_STEP, LOW);
  } else {
    pinMode(PIN_PWM, OUTPUT);
    analogWrite(PIN_PWM, 0);
  }

  Serial.begin(115200);
  Serial.println("nema_hw216: starting");
}

void loop() {
  if (MODE_STEPPER) {
    stepMove(STEPS_PER_MOVE, true);
    delay(500);
    stepMove(STEPS_PER_MOVE, false);
    delay(1000);
  } else {
    // Simple DC ramp test
    digitalWrite(PIN_DIR, HIGH);
    rampPWM(0, 255, 10);
    delay(500);
    digitalWrite(PIN_DIR, LOW);
    rampPWM(255, 0, 10);
    delay(1000);
  }
}

// Stepping helper: toggles STEP pin 'steps' times
void stepMove(int steps, bool direction) {
  digitalWrite(PIN_DIR, direction ? HIGH : LOW);
  for (int i = 0; i < steps; ++i) {
    digitalWrite(PIN_STEP, HIGH);
    delayMicroseconds(STEP_DELAY_US);
    digitalWrite(PIN_STEP, LOW);
    delayMicroseconds(STEP_DELAY_US);
  }
}

// Ramp PWM from start to end (inclusive) with step delay in ms
void rampPWM(int startVal, int endVal, unsigned int stepDelayMs) {
  if (startVal < endVal) {
    for (int v = startVal; v <= endVal; v += 5) {
      analogWrite(PIN_PWM, v);
      delay(stepDelayMs);
    }
  } else {
    for (int v = startVal; v >= endVal; v -= 5) {
      analogWrite(PIN_PWM, v);
      delay(stepDelayMs);
    }
  }
}
/*
  nema_hw216.ino
  Quick Arduino sketch to drive a NEMA motor via an HW216-style controller.
  Set MODE_STEPPER to true for a stepper using STEP/DIR, false for a DC motor using PWM/DIR.

  Wiring (example):
  - Stepper mode: DIR -> controller DIR pin, STEP -> controller STEP pin, ENABLE -> controller enable
  - DC mode: DIR -> controller DIR pin, PWM -> controller PWM/enable pin

  Adjust pins and timings to match your controller and motor.
*/

const bool MODE_STEPPER = true; // set to false for DC motor mode

// Common pins (change as needed)
const int PIN_DIR = 4;
const int PIN_ENABLE = 6;

// Stepper-specific
const int PIN_STEP = 5;
const unsigned int STEP_DELAY_US = 800; // smaller -> faster
const int STEPS_PER_MOVE = 400; // adjust to suit your motor

// DC-specific
const int PIN_PWM = 5; // shared with STEP if you change wiring for DC

void setup() {
  pinMode(PIN_DIR, OUTPUT);
  pinMode(PIN_ENABLE, OUTPUT);
  digitalWrite(PIN_ENABLE, LOW); // enable controller (check HW216 wiring: LOW/ HIGH may differ)

  if (MODE_STEPPER) {
    pinMode(PIN_STEP, OUTPUT);
    digitalWrite(PIN_STEP, LOW);
  } else {
    pinMode(PIN_PWM, OUTPUT);
    analogWrite(PIN_PWM, 0);
  }

  Serial.begin(115200);
  Serial.println("nema_hw216: starting");
}

void loop() {
  if (MODE_STEPPER) {
    stepMove(STEPS_PER_MOVE, true);
    delay(500);
    stepMove(STEPS_PER_MOVE, false);
    delay(1000);
  } else {
    // Simple DC ramp test
    digitalWrite(PIN_DIR, HIGH);
    rampPWM(0, 255, 10);
    delay(500);
    digitalWrite(PIN_DIR, LOW);
    rampPWM(255, 0, 10);
    delay(1000);
  }
}

// Stepping helper: toggles STEP pin 'steps' times
void stepMove(int steps, bool direction) {
  digitalWrite(PIN_DIR, direction ? HIGH : LOW);
  for (int i = 0; i < steps; ++i) {
    digitalWrite(PIN_STEP, HIGH);
    delayMicroseconds(STEP_DELAY_US);
    digitalWrite(PIN_STEP, LOW);
    delayMicroseconds(STEP_DELAY_US);
  }
}

// Ramp PWM from start to end (inclusive) with step delay in ms
void rampPWM(int startVal, int endVal, unsigned int stepDelayMs) {
  if (startVal < endVal) {
    for (int v = startVal; v <= endVal; v += 5) {
      analogWrite(PIN_PWM, v);
      delay(stepDelayMs);
    }
  } else {
    for (int v = startVal; v >= endVal; v -= 5) {
      analogWrite(PIN_PWM, v);
      delay(stepDelayMs);
    }
  }
}
