#!/bin/bash
# setup_uart.sh — Enable /dev/ttyAML6 (UART_B) on Le Potato (AML-S905X-CC)
#                 and ensure the running user has the groups main.py needs.
#
# Usage:
#   bash setup_uart.sh          # enable overlay + fix groups, then reboot prompt
#   bash setup_uart.sh --status # just report current state, no changes
#
# The Le Potato device-tree overlay tool is "ldto".
# UART_B maps to /dev/ttyAML6.  The overlay name is "uart-b".
# Full enable command: sudo ldto enable uart-b
# Changes take effect after reboot (or sudo ldto merge uart-b for live load).

set -euo pipefail

STATUS_ONLY=false
[[ "${1:-}" == "--status" ]] && STATUS_ONLY=true

CURRENT_USER="${SUDO_USER:-$USER}"
OVERLAY="uart-b"
TTYAML6="/dev/ttyAML6"
REQUIRED_GROUPS=("dialout" "gpio")

# ── Helpers ────────────────────────────────────────────────────────────────────
ok()   { echo "  [OK]  $*"; }
warn() { echo "  [!!]  $*"; }
info() { echo "  [--]  $*"; }

# ── 0. Sanity: must be root (or run via sudo) for changes ─────────────────────
if [[ "$STATUS_ONLY" == false && "$EUID" -ne 0 ]]; then
    echo "Re-running with sudo..."
    exec sudo bash "$0" "$@"
fi

echo ""
echo "=== Le Potato UART / Permission Setup ==="
echo "    User   : $CURRENT_USER"
echo "    Overlay: $OVERLAY  ->  $TTYAML6"
echo ""

# ── 1. Check ldto availability ────────────────────────────────────────────────
echo "--- ldto (device-tree overlay tool) ---"
if ! command -v ldto &>/dev/null; then
    warn "ldto not found in PATH."
    warn "Install via: sudo apt install ldto   (Libre Computer Raspbian/Ubuntu)"
    LDTO_OK=false
else
    ok "ldto found: $(command -v ldto)"
    LDTO_OK=true
fi

# ── 2. Check / enable the uart-b overlay ─────────────────────────────────────
echo ""
echo "--- UART_B overlay ($OVERLAY) ---"

if [[ "$LDTO_OK" == true ]]; then
    # ldto list-enabled prints currently active overlays
    if ldto list-enabled 2>/dev/null | grep -q "^${OVERLAY}$"; then
        ok "Overlay '$OVERLAY' is already enabled."
    else
        if [[ "$STATUS_ONLY" == true ]]; then
            warn "Overlay '$OVERLAY' is NOT enabled."
            info "Run:  sudo ldto enable $OVERLAY"
        else
            info "Enabling overlay '$OVERLAY' ..."
            # Persist across reboots
            ldto enable "$OVERLAY"
            ok "ldto enable $OVERLAY  — will be active after reboot."

            # Attempt a live merge so /dev/ttyAML6 appears immediately
            info "Attempting live merge (sudo ldto merge $OVERLAY) ..."
            if ldto merge "$OVERLAY" 2>/dev/null; then
                ok "Live merge succeeded — $TTYAML6 should exist now."
            else
                warn "Live merge failed (may need reboot for $TTYAML6 to appear)."
            fi
        fi
    fi
fi

# ── 3. Check /dev/ttyAML6 existence ──────────────────────────────────────────
echo ""
echo "--- $TTYAML6 device node ---"
if [[ -e "$TTYAML6" ]]; then
    ok "$TTYAML6 exists."
    info "Permissions: $(stat -c '%A %U:%G' $TTYAML6)"
else
    warn "$TTYAML6 does not exist yet."
    info "Reboot (or run 'sudo ldto merge $OVERLAY') to create the device node."
fi

# ── 4. Group membership ───────────────────────────────────────────────────────
echo ""
echo "--- Group membership for '$CURRENT_USER' ---"
NEEDS_RELOGIN=false

for grp in "${REQUIRED_GROUPS[@]}"; do
    if id -nG "$CURRENT_USER" | tr ' ' '\n' | grep -qx "$grp"; then
        ok "Already in group '$grp'."
    else
        if [[ "$STATUS_ONLY" == true ]]; then
            warn "NOT in group '$grp'."
            info "Run:  sudo usermod -aG $grp $CURRENT_USER"
        else
            info "Adding '$CURRENT_USER' to group '$grp' ..."
            usermod -aG "$grp" "$CURRENT_USER"
            ok "Added to '$grp'."
            NEEDS_RELOGIN=true
        fi
    fi
done

# ── 5. Summary ────────────────────────────────────────────────────────────────
echo ""
echo "=== Summary ==="

if [[ "$STATUS_ONLY" == true ]]; then
    info "Status check only — no changes made."
    info "Re-run without --status to apply changes."
else
    if [[ "$NEEDS_RELOGIN" == true ]]; then
        warn "Group changes require a logout/login (or reboot) to take effect."
    fi
    if ! [[ -e "$TTYAML6" ]]; then
        warn "Reboot required for $TTYAML6 to appear."
        read -rp "  Reboot now? [y/N] " ans
        [[ "${ans,,}" == "y" ]] && reboot
    else
        ok "Setup complete — no reboot needed."
    fi
fi

echo ""
