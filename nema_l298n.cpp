/*
 * nema_l298n.cpp
 * NEMA bipolar stepper deployment controller — L298N H-bridge wired directly
 * to the Libre Computer AML-S905X-CC (Le Potato) GPIO header.
 *
 * Build:
 *   g++ nema_l298n.cpp -o nema_l298n
 *   (requires root or gpio group membership to write /sys/class/gpio)
 *
 * Usage:
 *   ./nema_l298n D          — full downward deployment
 *   ./nema_l298n U          — full upward deployment
 *
 * Stdout on success:
 *   DONE:<steps_executed>:<elapsed_ms>
 *   (parsed by main.py to update motorRPM and stepCount in the JSON payload)
 *
 * ─────────────────────────────────────────────────────────────────────────────
 * WIRING — L298N ↔ Le Potato 40-pin header
 * ─────────────────────────────────────────────────────────────────────────────
 *  L298N    │ 40-pin physical │ GPIO bank  │ gpiochip0 offset │ Function
 *  ─────────┼─────────────────┼────────────┼──────────────────┼──────────
 *  IN1      │ 11              │ GPIOX_6    │ 52               │ Coil A+
 *  IN2      │ 13              │ GPIOX_7    │ 53               │ Coil A−
 *  IN3      │ 29              │ GPIOX_4    │ 50               │ Coil B+
 *  IN4      │ 31              │ GPIOX_5    │ 51               │ Coil B−
 *  ENA      │ jumper to 5V    │ —          │ —                │ Always enabled
 *  ENB      │ jumper to 5V    │ —          │ —                │ Always enabled
 *  VS (12V) │ external PSU    │ —          │ —                │ Motor power
 *  GND      │ any GND pin     │ —          │ —                │ Common ground
 *
 * ─────────────────────────────────────────────────────────────────────────────
 * LoRa UART CONFLICT CHECK
 * ─────────────────────────────────────────────────────────────────────────────
 *  /dev/ttyAML6 = UART_AO_B → GPIOAO_4 (TX, phy 24) & GPIOAO_5 (RX, phy 26).
 *  GPIOAO is a separate bank exposed on gpiochip1 — entirely independent of
 *  the GPIOX lines (gpiochip0 offsets 46-65) used here. No overlap. ✓
 *
 *  Other reserved pin ranges on the 40-pin header (also avoided):
 *    Physical  3 / 5   → GPIODV_24/25 (I2C)
 *    Physical  8 / 10  → GPIOAO_0/1   (UART_AO_A, ttyAML0)
 *    Physical 19/21/23 → GPIOX_8/9/11 (SPI MOSI/MISO/CLK)
 *    Physical 24       → GPIOX_10     (SPI CE0)
 *
 * ─────────────────────────────────────────────────────────────────────────────
 * TUNING THE GPIOCHIP0 BASE
 * ─────────────────────────────────────────────────────────────────────────────
 *  The sysfs GPIO number = GPIOCHIP0_BASE + gpiochip0_line_offset.
 *  GPIOCHIP0_BASE varies by kernel build. Find it on the board with:
 *
 *    cat /sys/class/gpio/gpiochip* base | sort -n
 *    # The lowest value printed = gpiochip0 (periphs-banks) base.
 *
 *  On the standard Libre Computer Ubuntu 22.04 BSP image: 410 (default below).
 *  Update GPIOCHIP0_BASE if your kernel reports a different value.
 * ─────────────────────────────────────────────────────────────────────────────
 */

#include <iostream>
#include <fstream>
#include <string>
#include <unistd.h>     // usleep
#include <chrono>
#include <stdexcept>

// ---------------------------------------------------------------------------
// GPIO numbering
// ---------------------------------------------------------------------------

// Adjust if `cat /sys/class/gpio/gpiochip*/base | sort -n` gives a different
// value for gpiochip0 on your kernel build.
static const int GPIOCHIP0_BASE = 410;

// GPIOX bank starts at offset 46 within gpiochip0 on AML-S905X periphs-banks.
// (Layout: GPIODV[0-29] + GPIOY[30-45] + GPIOX[46-65] + …)
static const int GPIOX_BASE = 46;

// sysfs GPIO numbers for the four L298N control pins
static const int GPIO_IN1 = GPIOCHIP0_BASE + GPIOX_BASE + 6;  // GPIOX_6, phy 11
static const int GPIO_IN2 = GPIOCHIP0_BASE + GPIOX_BASE + 7;  // GPIOX_7, phy 13
static const int GPIO_IN3 = GPIOCHIP0_BASE + GPIOX_BASE + 4;  // GPIOX_4, phy 29
static const int GPIO_IN4 = GPIOCHIP0_BASE + GPIOX_BASE + 5;  // GPIOX_5, phy 31

// ---------------------------------------------------------------------------
// Motor parameters
// ---------------------------------------------------------------------------

static const int DEPLOY_STEPS  = 4000;   // Full travel distance (tune to mechanism)
static const int STEP_DELAY_US = 2000;   // µs between phase changes; 2000 ≈ 150 RPM
static const int SEQ_LEN       = 4;

// Full-step sequence for a bipolar NEMA stepper through an L298N dual H-bridge.
// Rows: { IN1, IN2, IN3, IN4 }
// IN1/IN2 drive coil A via H-bridge 1; IN3/IN4 drive coil B via H-bridge 2.
// No row sets IN1=IN2=1 or IN3=IN4=1 simultaneously — no shoot-through risk.
static const int STEP_SEQ[SEQ_LEN][4] = {
    {1, 0, 1, 0},   // phase 0: A+, B+
    {0, 1, 1, 0},   // phase 1: A−, B+
    {0, 1, 0, 1},   // phase 2: A−, B−
    {1, 0, 0, 1},   // phase 3: A+, B−
};

// ---------------------------------------------------------------------------
// sysfs GPIO helpers
// ---------------------------------------------------------------------------

static void gpio_write_file(const std::string& path, const std::string& value) {
    std::ofstream f(path);
    if (!f.is_open()) {
        throw std::runtime_error("Cannot open " + path);
    }
    f << value;
    if (f.fail()) {
        throw std::runtime_error("Write failed: " + path);
    }
}

// Export a GPIO pin to userspace (safe to call if already exported).
static void gpio_export(int gpio) {
    std::ofstream f("/sys/class/gpio/export");
    if (f.is_open()) {
        f << gpio;  // Suppress error if already exported — kernel returns EBUSY
    }
}

static void gpio_configure_output(int gpio) {
    usleep(50000);  // Wait for sysfs node to appear after export
    gpio_write_file("/sys/class/gpio/gpio" + std::to_string(gpio) + "/direction", "out");
}

static void gpio_set(int gpio, int value) {
    gpio_write_file("/sys/class/gpio/gpio" + std::to_string(gpio) + "/value",
                    std::to_string(value));
}

static void gpio_unexport(int gpio) {
    std::ofstream f("/sys/class/gpio/unexport");
    if (f.is_open()) f << gpio;
}

// ---------------------------------------------------------------------------
// Motor control
// ---------------------------------------------------------------------------

static void set_phase(int in1, int in2, int in3, int in4) {
    gpio_set(GPIO_IN1, in1);
    gpio_set(GPIO_IN2, in2);
    gpio_set(GPIO_IN3, in3);
    gpio_set(GPIO_IN4, in4);
}

// De-energise all coils (prevents heat buildup when stationary).
static void coils_off() {
    set_phase(0, 0, 0, 0);
}

static void release_gpio() {
    coils_off();
    gpio_unexport(GPIO_IN1);
    gpio_unexport(GPIO_IN2);
    gpio_unexport(GPIO_IN3);
    gpio_unexport(GPIO_IN4);
}

// Execute 'steps' full-steps.
// down = true  → forward sequence (0→1→2→3→…) = downward deployment
// down = false → reverse sequence (3→2→1→0→…) = upward deployment
static long step_motor(int steps, bool down) {
    auto t0 = std::chrono::steady_clock::now();

    for (int i = 0; i < steps; ++i) {
        int phase = down ? (i % SEQ_LEN)
                         : ((SEQ_LEN - 1) - (i % SEQ_LEN));
        set_phase(STEP_SEQ[phase][0], STEP_SEQ[phase][1],
                  STEP_SEQ[phase][2], STEP_SEQ[phase][3]);
        usleep(STEP_DELAY_US);
    }

    auto t1 = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

int main(int argc, char* argv[]) {
    if (argc < 2 || (argv[1][0] != 'D' && argv[1][0] != 'U')) {
        std::cerr << "Usage: " << argv[0] << " <D|U>\n"
                  << "  D — deploy downward (full travel)\n"
                  << "  U — deploy upward   (full travel)\n";
        return 1;
    }

    bool down = (argv[1][0] == 'D');

    // Export and configure all four control pins
    const int pins[] = {GPIO_IN1, GPIO_IN2, GPIO_IN3, GPIO_IN4};
    try {
        for (int p : pins) {
            gpio_export(p);
            gpio_configure_output(p);
        }
    } catch (const std::runtime_error& e) {
        std::cerr << "[NEMA] GPIO setup failed: " << e.what() << "\n"
                  << "[NEMA] Run as root or add user to the gpio group.\n";
        release_gpio();
        return 1;
    }

    // Execute the deployment move
    long elapsed_ms = step_motor(DEPLOY_STEPS, down);

    // De-energise coils and release GPIO before exit
    release_gpio();

    // Report to main.py — format: DONE:<steps>:<elapsed_ms>
    std::cout << "DONE:" << DEPLOY_STEPS << ":" << elapsed_ms << std::endl;
    return 0;
}
