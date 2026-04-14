/*
 * nema_l298n.cpp
 * NEMA 17 bipolar stepper deployment controller - L298N H-bridge wired directly
 * to the Libre Computer AML-S905X-CC (Le Potato) GPIO header.
 *
 * Step sequencing follows the Arduino Stepper library convention for a 4-wire
 * bipolar motor driven via L298N dual H-bridge (IN1, IN2, IN3, IN4 ordering).
 * Speed is set in RPM - matching setSpeed() from the Arduino Stepper library.
 *
 * GPIO access uses libgpiod (character device API) instead of the deprecated
 * sysfs interface. No root required - gpio group membership is sufficient.
 *
 * Build:
 *   g++ nema_l298n.cpp -lgpiod -o nema_l298n
 *
 * Usage:
 *   ./nema_l298n D [rpm]    - full downward deployment (default 60 RPM)
 *   ./nema_l298n U [rpm]    - full upward deployment   (default 60 RPM)
 *
 * Stdout on success:
 *   DONE:<steps_executed>:<elapsed_ms>
 *   (parsed by main.py to update motorRPM and stepCount in the JSON payload)
 *
 * ─────────────────────────────────────────────────────────────────────────────
 * WIRING - L298N ↔ Le Potato 40-pin header
 * ─────────────────────────────────────────────────────────────────────────────
 *  L298N    │ 40-pin physical │ GPIO bank  │ gpiochip0 offset │ Function
 *  ─────────┼─────────────────┼────────────┼──────────────────┼──────────
 *  IN1      │ 11              │ GPIOX_6    │ 52               │ Coil A+
 *  IN2      │ 13              │ GPIOX_7    │ 53               │ Coil A−
 *  IN3      │ 29              │ GPIOX_4    │ 50               │ Coil B+
 *  IN4      │ 31              │ GPIOX_5    │ 51               │ Coil B−
 *  ENA      │ jumper to 5V    │ -          │ -                │ Always enabled
 *  ENB      │ jumper to 5V    │ -          │ -                │ Always enabled
 *  VS (12V) │ external PSU    │ -          │ -                │ Motor power
 *  GND      │ any GND pin     │ -          │ -                │ Common ground
 *
 *  Verify offsets on target board:
 *    gpioinfo gpiochip0 | grep -E "line (50|51|52|53)"
 *
 * ─────────────────────────────────────────────────────────────────────────────
 * LoRa UART CONFLICT CHECK
 * ─────────────────────────────────────────────────────────────────────────────
 *  /dev/ttyAML6 = UART_AO_B → GPIOAO_4 (TX, phy 24) & GPIOAO_5 (RX, phy 26).
 *  GPIOAO is a separate bank exposed on gpiochip1 - entirely independent of
 *  the GPIOX lines (gpiochip0 offsets 46-65) used here. No overlap. ✓
 *
 *  Other reserved pin ranges on the 40-pin header (also avoided):
 *    Physical  3 / 5   → GPIODV_24/25 (I2C)
 *    Physical  8 / 10  → GPIOAO_0/1   (UART_AO_A, ttyAML0)
 *    Physical 19/21/23 → GPIOX_8/9/11 (SPI MOSI/MISO/CLK)
 *    Physical 24       → GPIOX_10     (SPI CE0)
 * ─────────────────────────────────────────────────────────────────────────────
 */

#include <iostream>
#include <string>
#include <unistd.h>     // usleep
#include <chrono>
#include <stdexcept>
#include <cstdlib>      // std::stoi
#include <gpiod.h>

// ---------------------------------------------------------------------------
// GPIO numbering  (line offsets within gpiochip0)
// ---------------------------------------------------------------------------

// GPIOX bank starts at offset 46 within gpiochip0 on AML-S905X periphs-banks.
// (Layout: GPIODV[0-29] + GPIOY[30-45] + GPIOX[46-65] + …)
static const int GPIOX_BASE = 46;

// gpiochip0 line offsets for the four L298N control pins
static const int PIN_IN1 = GPIOX_BASE + 6;  // GPIOX_6, phy 11 - Coil A+
static const int PIN_IN2 = GPIOX_BASE + 7;  // GPIOX_7, phy 13 - Coil A−
static const int PIN_IN3 = GPIOX_BASE + 4;  // GPIOX_4, phy 29 - Coil B+
static const int PIN_IN4 = GPIOX_BASE + 5;  // GPIOX_5, phy 31 - Coil B−

static const int PIN_OFFSETS[4] = { PIN_IN1, PIN_IN2, PIN_IN3, PIN_IN4 };

// ---------------------------------------------------------------------------
// Motor parameters
// ---------------------------------------------------------------------------

// NEMA 17: 200 full steps per revolution (1.8° per step).
static const int STEPS_PER_REV = 200;

static const int DEPLOY_STEPS  = 4000;   // Full travel distance (tune to mechanism)
static const int DEFAULT_RPM   = 60;     // Matches Arduino Stepper setSpeed(60)
static const int SEQ_LEN       = 4;

// Compute the inter-step delay in microseconds from a target RPM.
// Mirrors the timing used by the Arduino Stepper library:
//   delay_us = 60,000,000 / (rpm * steps_per_rev)
static int rpm_to_step_delay_us(int rpm) {
    if (rpm <= 0) rpm = DEFAULT_RPM;
    return 60000000 / (rpm * STEPS_PER_REV);
}

// Full-step sequence for a bipolar NEMA stepper through an L298N dual H-bridge.
// Rows: { IN1, IN2, IN3, IN4 }
// IN1/IN2 drive coil A via H-bridge 1; IN3/IN4 drive coil B via H-bridge 2.
// No row sets IN1=IN2=1 or IN3=IN4=1 simultaneously - no shoot-through risk.
static const int STEP_SEQ[SEQ_LEN][4] = {
    {1, 0, 1, 0},   // phase 0: A+, B+
    {0, 1, 1, 0},   // phase 1: A−, B+
    {0, 1, 0, 1},   // phase 2: A−, B−
    {1, 0, 0, 1},   // phase 3: A+, B−
};

// ---------------------------------------------------------------------------
// libgpiod state
// ---------------------------------------------------------------------------

static gpiod_chip* chip        = nullptr;
static gpiod_line* lines[4]    = {};

static void gpio_setup() {
    chip = gpiod_chip_open_by_number(0);
    if (!chip)
        throw std::runtime_error("Cannot open /dev/gpiochip0 - check gpio group membership");

    for (int i = 0; i < 4; ++i) {
        lines[i] = gpiod_chip_get_line(chip, PIN_OFFSETS[i]);
        if (!lines[i])
            throw std::runtime_error("Cannot get line offset " + std::to_string(PIN_OFFSETS[i]));
        if (gpiod_line_request_output(lines[i], "nema_l298n", 0) < 0)
            throw std::runtime_error("Cannot request output on line " + std::to_string(PIN_OFFSETS[i]));
    }
}

static void gpio_set(int idx, int value) {
    gpiod_line_set_value(lines[idx], value);
}

static void gpio_release() {
    for (int i = 0; i < 4; ++i)
        if (lines[i]) gpiod_line_release(lines[i]);
    if (chip) gpiod_chip_close(chip);
}

// ---------------------------------------------------------------------------
// Motor control
// ---------------------------------------------------------------------------

static void set_phase(int in1, int in2, int in3, int in4) {
    gpio_set(0, in1);
    gpio_set(1, in2);
    gpio_set(2, in3);
    gpio_set(3, in4);
}

// De-energise all coils (prevents heat buildup when stationary).
static void coils_off() {
    set_phase(0, 0, 0, 0);
}

static void release_gpio() {
    coils_off();
    gpio_release();
}

// Execute 'steps' full-steps at the given inter-step delay.
// down = true  → forward sequence (0→1→2→3→…) = downward deployment
// down = false → reverse sequence (3→2→1→0→…) = upward deployment
static long step_motor(int steps, bool down, int step_delay_us) {
    auto t0 = std::chrono::steady_clock::now();

    for (int i = 0; i < steps; ++i) {
        int phase = down ? (i % SEQ_LEN)
                         : ((SEQ_LEN - 1) - (i % SEQ_LEN));
        set_phase(STEP_SEQ[phase][0], STEP_SEQ[phase][1],
                  STEP_SEQ[phase][2], STEP_SEQ[phase][3]);
        usleep(step_delay_us);
    }

    auto t1 = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

int main(int argc, char* argv[]) {
    if (argc < 2 || (argv[1][0] != 'D' && argv[1][0] != 'U')) {
        std::cerr << "Usage: " << argv[0] << " <D|U> [rpm]\n"
                  << "  D      - deploy downward (full travel)\n"
                  << "  U      - deploy upward   (full travel)\n"
                  << "  rpm    - motor speed in RPM (default: " << DEFAULT_RPM << ")\n";
        return 1;
    }

    bool down = (argv[1][0] == 'D');
    int  rpm  = (argc >= 3) ? std::stoi(argv[2]) : DEFAULT_RPM;
    int  step_delay_us = rpm_to_step_delay_us(rpm);

    // Open gpiochip0 and request all four control lines as outputs
    try {
        gpio_setup();
    } catch (const std::runtime_error& e) {
        std::cerr << "[NEMA] GPIO setup failed: " << e.what() << "\n"
                  << "[NEMA] Install libgpiod and ensure user is in the gpio group.\n";
        release_gpio();
        return 1;
    }

    // Execute the deployment move at the requested RPM
    long elapsed_ms = step_motor(DEPLOY_STEPS, down, step_delay_us);

    // De-energise coils and release GPIO before exit
    release_gpio();

    // Report to main.py - format: DONE:<steps>:<elapsed_ms>
    std::cout << "DONE:" << DEPLOY_STEPS << ":" << elapsed_ms << std::endl;
    return 0;
}
