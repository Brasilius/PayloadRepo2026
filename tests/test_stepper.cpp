#include <iostream>
#include <vector>
#include <cassert>

// Mocks
unsigned long _millis = 0;
void delay(unsigned int ms) { _millis += ms; }
unsigned long millis() { return _millis; }
void delayMicroseconds(unsigned int us) { /* no-op for logic test */ }

struct PinState {
    int pin;
    int val;
};
std::vector<PinState> pwm_writes;
void analogWrite(int pin, int val) {
    pwm_writes.push_back({pin, val});
}

// Logic from nema_hw216.ino
const int PIN_PWM = 26;
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

void testRampUp() {
    pwm_writes.clear();
    _millis = 0;
    rampPWM(0, 20, 10);
    // Expected values: 0, 5, 10, 15, 20
    assert(pwm_writes.size() == 5);
    assert(pwm_writes[0].val == 0);
    assert(pwm_writes[4].val == 20);
    assert(_millis == 50); // 5 steps * 10ms
    std::cout << "testRampUp: PASSED" << std::endl;
}

void testRampDown() {
    pwm_writes.clear();
    _millis = 0;
    rampPWM(20, 0, 10);
    // Expected values: 20, 15, 10, 5, 0
    assert(pwm_writes.size() == 5);
    assert(pwm_writes[0].val == 20);
    assert(pwm_writes[4].val == 0);
    assert(_millis == 50);
    std::cout << "testRampDown: PASSED" << std::endl;
}

int main() {
    testRampUp();
    testRampDown();
    std::cout << "All Stepper logic tests PASSED!" << std::endl;
    return 0;
}
