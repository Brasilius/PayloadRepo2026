#!/usr/bin/env python3
"""
test_permissions.py — Pre-flight permission and resource check for main.py
                       on Le Potato (AML-S905X-CC).

Verifies every external dependency that main.py touches:
  - Serial devices  : /dev/ttyAML6 (LoRa), /dev/ttyAML0 (TMC2209 UART),
                      /dev/ttyUSB0 (Modbus sensor)
  - GPIO            : /dev/gpiochip0, gpiod Python library, user gpio group
  - Python modules  : gpiod, pyserial
  - Executables     : receivermodule, transmittermodule, modbus_reader
  - Groups          : dialout (serial), gpio

Exit code 0 = all checks pass.
Exit code 1 = one or more checks failed (details printed).
"""

import os
import sys
import stat
import grp
import subprocess

# ── Resources taken directly from main.py ─────────────────────────────────────
LORA_PORT   = "/dev/ttyAML6"
TMC_PORT    = "/dev/ttyAML0"   # TMC2209 UART (requires serial console disabled)
SENSOR_PORT = "/dev/ttyUSB0"
GPIOCHIP    = "/dev/gpiochip1"

EXECUTABLES = [
    "./receivermodule",
    "./transmittermodule",
    "./modbus_reader",
]

REQUIRED_GROUPS = ["dialout", "gpio"]

# ── ANSI colours (suppressed when not a tty) ──────────────────────────────────
_tty = sys.stdout.isatty()
GRN  = "\033[32m" if _tty else ""
RED  = "\033[31m" if _tty else ""
YLW  = "\033[33m" if _tty else ""
RST  = "\033[0m"  if _tty else ""

passed = []
failed = []
warned = []


def ok(label, detail=""):
    tag = f"{GRN}PASS{RST}"
    print(f"  [{tag}]  {label}" + (f"  ({detail})" if detail else ""))
    passed.append(label)


def fail(label, detail=""):
    tag = f"{RED}FAIL{RST}"
    print(f"  [{tag}]  {label}" + (f"  ({detail})" if detail else ""))
    failed.append(label)


def warn(label, detail=""):
    tag = f"{YLW}WARN{RST}"
    print(f"  [{tag}]  {label}" + (f"  ({detail})" if detail else ""))
    warned.append(label)


# ── Helpers ────────────────────────────────────────────────────────────────────

def check_device(path, label=None):
    label = label or path
    if not os.path.exists(path):
        fail(f"{label} exists", f"not found — enable overlay or plug device")
        return
    ok(f"{label} exists")

    # Check character device
    mode = os.stat(path).st_mode
    if not stat.S_ISCHR(mode):
        warn(f"{label} is a character device", "unexpected file type")

    # Check read+write permission for current user
    readable  = os.access(path, os.R_OK)
    writable  = os.access(path, os.W_OK)
    if readable and writable:
        ok(f"{label} readable+writable")
    elif readable:
        fail(f"{label} writable", "read-only — check group membership")
    else:
        fail(f"{label} readable+writable", "no access — check group membership")

    # Show owning group
    import grp as _grp
    gid = os.stat(path).st_gid
    try:
        grp_name = _grp.getgrgid(gid).gr_name
        print(f"         group owner: {grp_name}")
    except KeyError:
        pass


def check_executable(path):
    if not os.path.exists(path):
        fail(f"executable {path}", "not found — run build.sh first")
        return
    if not os.access(path, os.X_OK):
        fail(f"executable {path}", "exists but not executable — chmod +x?")
        return
    ok(f"executable {path}")


def check_group(group_name):
    try:
        grp_entry = grp.getgrnam(group_name)
    except KeyError:
        warn(f"group '{group_name}' exists on system", "group not found")
        return

    username = os.environ.get("SUDO_USER") or os.environ.get("USER") or ""
    # Also check by GID (covers the primary group)
    current_groups = os.getgroups()
    in_group = (
        username in grp_entry.gr_mem
        or grp_entry.gr_gid in current_groups
    )
    if in_group:
        ok(f"user in group '{group_name}'")
    else:
        fail(
            f"user in group '{group_name}'",
            f"run: sudo usermod -aG {group_name} $USER  then log out/in"
        )


def check_gpiod_module():
    try:
        import gpiod  # noqa: F401
        ok("Python gpiod module importable")
    except ImportError:
        fail("Python gpiod module importable",
             "pip install 'gpiod>=1.5,<2.0'  or  apt install python3-gpiod")


def check_pyserial_module():
    try:
        import serial  # noqa: F401
        ok("Python pyserial module importable")
    except ImportError:
        fail("Python pyserial module importable",
             "pip install pyserial  or  apt install python3-serial")


def check_gpiod_chip():
    check_device(GPIOCHIP, "gpiochip1")


def check_ldto_overlay():
    """Warn (not fail) if ttyAML6 is missing and ldto overlay isn't enabled."""
    if os.path.exists(LORA_PORT):
        return  # already shown as passing in check_device
    if not os.path.exists("/usr/bin/ldto") and not os.path.exists("/usr/local/bin/ldto"):
        warn("ldto overlay status", "ldto not in PATH — cannot verify uart-b")
        return
    try:
        result = subprocess.run(
            ["ldto", "list-enabled"],
            capture_output=True, text=True, timeout=5
        )
        if "uart-b" in result.stdout:
            warn("uart-b overlay enabled but ttyAML6 missing",
                 "reboot required for device node to appear")
        else:
            fail("uart-b overlay enabled",
                 "run: sudo ldto enable uart-b  then reboot")
    except Exception:
        warn("ldto overlay check", "could not run ldto")


def check_python_modules():
    for mod in ["subprocess", "threading", "queue", "time", "json"]:
        try:
            __import__(mod)
            ok(f"stdlib module '{mod}'")
        except ImportError:
            fail(f"stdlib module '{mod}'", "should never happen — check Python install")


# ── Main ───────────────────────────────────────────────────────────────────────

def main():
    print()
    print("=" * 56)
    print("  main.py Pre-flight Permission Check")
    print("  Le Potato (AML-S905X-CC)")
    print("=" * 56)

    print("\n--- Serial devices ---")
    check_device(LORA_PORT,   "LoRa port    (ttyAML6)")
    check_ldto_overlay()
    check_device(TMC_PORT,    "TMC2209 UART (ttyAML0) — requires serial console disabled")
    check_device(SENSOR_PORT, "Sensor port  (ttyUSB0)")

    print("\n--- GPIO ---")
    check_gpiod_chip()
    check_gpiod_module()
    check_pyserial_module()

    print("\n--- User group membership ---")
    for g in REQUIRED_GROUPS:
        check_group(g)

    print("\n--- Required executables ---")
    for exe in EXECUTABLES:
        check_executable(exe)

    print("\n--- Python stdlib ---")
    check_python_modules()

    # ── Results ───────────────────────────────────────────────────────────────
    print()
    print("=" * 56)
    total = len(passed) + len(failed) + len(warned)
    print(f"  Results: {len(passed)} passed, {len(failed)} failed, {len(warned)} warned  "
          f"({total} checks)")

    if failed:
        print(f"\n{RED}  FAILED checks:{RST}")
        for f in failed:
            print(f"    - {f}")
        print()
        print("  Fix the items above, then re-run this script.")
        print("  For UART setup run:  bash setup_uart.sh")
        print()
        sys.exit(1)
    elif warned:
        print(f"\n{YLW}  Warnings present — main.py may still work but review above.{RST}")
        print()
        sys.exit(0)
    else:
        print(f"\n{GRN}  All checks passed — system is ready to run main.py.{RST}")
        print()
        sys.exit(0)


if __name__ == "__main__":
    main()
