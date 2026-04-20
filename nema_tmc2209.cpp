/*
 * nema_tmc2209.cpp
 * NEMA 17 bipolar stepper deployment controller — TMC2209 STEP/DIR
 * controlled directly by the Libre Computer AML-S905X-CC (Le Potato) GPIO header.
 *
 * Replaces the L298N 4-wire full-step implementation with a simple STEP/DIR
 * interface. The TMC2209 handles all microstepping internally; this program
 * only needs to pulse the STEP pin at the target frequency.
 *
 * One-shot UART configuration (via /dev/ttyAML0) writes GCONF, IHOLD_IRUN,
 * CHOPCONF, and PWMCONF at startup. If the UART port is unavailable the
 * driver runs with power-on defaults and motion continues unaffected.
 *
 * GPIO access uses libgpiod (character device API). No root required —
 * gpio group membership is sufficient.
 *
 * Build:
 *   g++ nema_tmc2209.cpp -lgpiod -o nema_tmc2209
 *
 * Usage:
 *   ./nema_tmc2209 D [rpm]    - full downward deployment (default 60 RPM)
 *   ./nema_tmc2209 U [rpm]    - full upward deployment   (default 60 RPM)
 *
 * Stdout on success:
 *   DONE:<steps_executed>:<elapsed_ms>
 *   (parsed by main.py to update motorRPM and stepCount in the JSON payload)
 *
 * ─────────────────────────────────────────────────────────────────────────────
 * WIRING — TMC2209 ↔ Le Potato 40-pin header
 * ─────────────────────────────────────────────────────────────────────────────
 *  TMC2209  │ 40-pin physical │ GPIO bank  │ gpiochip0 offset │ Function
 *  ─────────┼─────────────────┼────────────┼──────────────────┼────────────────
 *  STEP     │ 11              │ GPIOX_6    │ 52               │ Step pulse
 *  DIR      │ 13              │ GPIOX_7    │ 53               │ Direction
 *  EN       │ 29              │ GPIOX_4    │ 50               │ Enable (active LOW)
 *  (spare)  │ 31              │ GPIOX_5    │ 51               │ Available (DIAG)
 *
 * TMC2209 UART (single-wire PDN_UART pin) → UART_AO_A (/dev/ttyAML0):
 *  TX (AO_A) │ 8              │ GPIOAO_0   │ gpiochip1        │ Le Potato → TMC2209
 *  RX (AO_A) │ 10             │ GPIOAO_1   │ gpiochip1        │ TMC2209 → Le Potato
 *
 * PREREQUISITE: disable the serial console on ttyAML0 before use:
 *   sudo systemctl disable serial-getty@ttyAML0.service
 *   Remove "console=ttyAML0,115200n8" from /boot/armbianEnv.txt, then reboot.
 *
 * ─────────────────────────────────────────────────────────────────────────────
 * CONFLICT CHECK
 * ─────────────────────────────────────────────────────────────────────────────
 *  /dev/ttyAML6 = UART_AO_B → GPIOAO_4/5 (phy 24/26) — used by LoRa (no conflict).
 *  GPIOX lines (gpiochip0 offsets 50–53) are entirely separate from the GPIOAO
 *  bank (gpiochip1) used by ttyAML0 and ttyAML6. No overlap. ✓
 *
 *  Verify line offsets on target board:
 *    gpioinfo gpiochip0 | grep -E "line (50|51|52|53)"
 * ─────────────────────────────────────────────────────────────────────────────
 */

#include <iostream>
#include <string>
#include <unistd.h>     // usleep, read, write, close
#include <fcntl.h>      // open, O_RDWR, O_NOCTTY
#include <termios.h>    // termios, tcsetattr
#include <chrono>
#include <stdexcept>
#include <cstdlib>      // std::stoi
#include <cstring>      // memset, memcpy
#include <cstdint>
#include <gpiod.h>

// ---------------------------------------------------------------------------
// Pin assignments  (gpiochip0 line offsets on AML-S905X periphs-banks)
// GPIOX bank starts at offset 46 within gpiochip0.
// ---------------------------------------------------------------------------
static const int GPIOX_BASE = 46;
static const int PIN_STEP   = GPIOX_BASE + 6;  // GPIOX_6, phy 11 — STEP pulse
static const int PIN_DIR    = GPIOX_BASE + 7;  // GPIOX_7, phy 13 — DIR
static const int PIN_EN     = GPIOX_BASE + 4;  // GPIOX_4, phy 29 — EN (active LOW)

// ---------------------------------------------------------------------------
// TMC2209 UART  (/dev/ttyAML0 = UART_AO_A, phy 8 TX / phy 10 RX)
// ---------------------------------------------------------------------------
static const char*    TMC_UART_PORT   = "/dev/ttyAML0";
static const uint8_t  TMC_DRIVER_ADDR = 0b00;  // MS1=GND, MS2=GND → node addr 0

static const uint8_t REG_GCONF      = 0x00;
static const uint8_t REG_IHOLD_IRUN = 0x10;
static const uint8_t REG_CHOPCONF   = 0x6C;
static const uint8_t REG_PWMCONF    = 0x70;

// ---------------------------------------------------------------------------
// Motor parameters
// ---------------------------------------------------------------------------
static const int STEPS_PER_REV = 200;    // NEMA 17: 1.8° per step
static const int DEPLOY_STEPS  = 4000;  // Full travel distance (tune to mechanism)
static const int DEFAULT_RPM   = 60;

// Compute inter-step delay in µs from target RPM.
// Mirrors the timing used by the Arduino Stepper library:
//   delay_us = 60,000,000 / (rpm × steps_per_rev)
static int rpm_to_step_delay_us(int rpm) {
    if (rpm <= 0) rpm = DEFAULT_RPM;
    return 60000000 / (rpm * STEPS_PER_REV);
}

// ---------------------------------------------------------------------------
// TMC2209 UART helpers
// CRC-8 per TMC2209 datasheet: poly=0x07, init=0x00, bits processed LSB-first.
// ---------------------------------------------------------------------------
static uint8_t tmc_crc8(const uint8_t *data, int len) {
    uint8_t crc = 0;
    for (int i = 0; i < len; i++) {
        uint8_t byte = data[i];
        for (int b = 0; b < 8; b++) {
            if (((crc >> 7) ^ (byte & 0x01)) != 0)
                crc = ((crc << 1) ^ 0x07) & 0xFF;
            else
                crc = (crc << 1) & 0xFF;
            byte >>= 1;
        }
    }
    return crc;
}

static int open_uart() {
    int fd = open(TMC_UART_PORT, O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0) return -1;

    struct termios tty;
    memset(&tty, 0, sizeof(tty));
    if (tcgetattr(fd, &tty) != 0) { close(fd); return -1; }

    cfsetospeed(&tty, B115200);
    cfsetispeed(&tty, B115200);
    tty.c_cflag  = (tty.c_cflag & ~CSIZE) | CS8;
    tty.c_cflag &= ~(PARENB | CSTOPB | CRTSCTS);
    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_lflag  = 0;
    tty.c_oflag  = 0;
    tty.c_iflag &= ~(IXON | IXOFF | IXANY | ICRNL);
    tty.c_cc[VMIN]  = 0;
    tty.c_cc[VTIME] = 1;   // 100 ms read timeout

    tcsetattr(fd, TCSANOW, &tty);
    tcflush(fd, TCIOFLUSH);
    return fd;
}

// 8-byte write datagram: [SYNC, ADDR, REG|0x80, data[31:0 MSB-first], CRC]
// TMC2209 single-wire UART echoes TX bytes back on RX — discard after write.
static void tmc_write_reg(int fd, uint8_t reg, uint32_t value) {
    uint8_t payload[7] = {
        0x05,
        TMC_DRIVER_ADDR,
        static_cast<uint8_t>(reg | 0x80),
        static_cast<uint8_t>((value >> 24) & 0xFF),
        static_cast<uint8_t>((value >> 16) & 0xFF),
        static_cast<uint8_t>((value >>  8) & 0xFF),
        static_cast<uint8_t>(value         & 0xFF),
    };
    uint8_t dgram[8];
    memcpy(dgram, payload, 7);
    dgram[7] = tmc_crc8(payload, 7);

    write(fd, dgram, 8);
    fsync(fd);
    uint8_t echo[8]; read(fd, echo, 8);  // discard echo
}

static bool init_tmc2209() {
    int fd = open_uart();
    if (fd < 0) {
        std::cerr << "[TMC2209] Cannot open " << TMC_UART_PORT
                  << " — running with power-on defaults.\n"
                     "[TMC2209] Disable the serial console first:\n"
                     "  sudo systemctl disable serial-getty@ttyAML0.service\n";
        return false;
    }

    // GCONF      0x000000C0 — StealthChop, PDN_UART pin disabled, MRES from CHOPCONF
    // IHOLD_IRUN 0x00060804 — IHOLDDELAY=6, IRUN=8, IHOLD=4
    //                         I_RMS(run)  ≈ 587 mA  at R_SENSE=0.11 Ω
    //                         I_RMS(hold) ≈ 326 mA
    // CHOPCONF   0x18000005 — TOFF=5 (enable), MRES=8 (full-step input), intpol=1
    //                         (driver interpolates each step to 256 µsteps internally)
    // PWMCONF    0xC10D0024 — StealthChop PWM defaults (Trinamic recommended)
    tmc_write_reg(fd, REG_GCONF,      0x000000C0u);
    tmc_write_reg(fd, REG_IHOLD_IRUN, 0x00060804u);
    tmc_write_reg(fd, REG_CHOPCONF,   0x18000005u);
    tmc_write_reg(fd, REG_PWMCONF,    0xC10D0024u);

    close(fd);
    std::cout << "[TMC2209] Driver configured via UART.\n";
    return true;
}

// ---------------------------------------------------------------------------
// libgpiod state
// ---------------------------------------------------------------------------
static gpiod_chip *chip      = nullptr;
static gpiod_line *step_line = nullptr;
static gpiod_line *dir_line  = nullptr;
static gpiod_line *en_line   = nullptr;

static void gpio_setup() {
    chip = gpiod_chip_open_by_number(0);
    if (!chip)
        throw std::runtime_error("Cannot open /dev/gpiochip0 — check gpio group membership");

    step_line = gpiod_chip_get_line(chip, PIN_STEP);
    dir_line  = gpiod_chip_get_line(chip, PIN_DIR);
    en_line   = gpiod_chip_get_line(chip, PIN_EN);

    if (!step_line || !dir_line || !en_line)
        throw std::runtime_error("Cannot get one or more GPIO lines");

    // EN starts HIGH (driver disabled) until motion begins.
    if (gpiod_line_request_output(step_line, "nema_tmc2209", 0) < 0 ||
        gpiod_line_request_output(dir_line,  "nema_tmc2209", 0) < 0 ||
        gpiod_line_request_output(en_line,   "nema_tmc2209", 1) < 0)
        throw std::runtime_error("Cannot request GPIO outputs");
}

static void gpio_release() {
    if (step_line) gpiod_line_set_value(step_line, 0);
    if (en_line)   gpiod_line_set_value(en_line,   1);  // Disable driver (reduces heat at rest)
    if (step_line) gpiod_line_release(step_line);
    if (dir_line)  gpiod_line_release(dir_line);
    if (en_line)   gpiod_line_release(en_line);
    if (chip)      gpiod_chip_close(chip);
}

// ---------------------------------------------------------------------------
// Motion
// ---------------------------------------------------------------------------
// down=true  → DIR=LOW  = downward deployment
// down=false → DIR=HIGH = upward deployment
static long step_motor(int steps, bool down, int step_delay_us) {
    int half = step_delay_us / 2;

    gpiod_line_set_value(dir_line, down ? 0 : 1);
    gpiod_line_set_value(en_line,  0);    // Enable driver (active LOW)
    usleep(1000);                          // 1 ms EN→STEP setup time per TMC2209 datasheet

    auto t0 = std::chrono::steady_clock::now();

    for (int i = 0; i < steps; i++) {
        gpiod_line_set_value(step_line, 1);
        usleep(half);
        gpiod_line_set_value(step_line, 0);
        usleep(half);
    }

    auto t1 = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------
int main(int argc, char *argv[]) {
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

    init_tmc2209();  // non-fatal: motor runs with power-on defaults on UART failure

    try {
        gpio_setup();
    } catch (const std::runtime_error &e) {
        std::cerr << "[NEMA] GPIO setup failed: " << e.what() << "\n"
                  << "[NEMA] Install libgpiod and ensure user is in the gpio group.\n";
        gpio_release();
        return 1;
    }

    long elapsed_ms = step_motor(DEPLOY_STEPS, down, step_delay_us);
    gpio_release();

    // Report to main.py — format: DONE:<steps>:<elapsed_ms>
    std::cout << "DONE:" << DEPLOY_STEPS << ":" << elapsed_ms << std::endl;
    return 0;
}
