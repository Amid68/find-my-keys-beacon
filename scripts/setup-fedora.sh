#!/usr/bin/env bash
# =============================================================================
# One-shot dev environment setup for Fedora (tested on Asahi Fedora, aarch64).
#
# Everything here is idempotent - safe to re-run. It installs:
#   - the ARM bare-metal cross toolchain (compiler + newlib headers)
#   - build/analysis tools (make, cppcheck, clang-tools for clang-format)
#   - probe-rs (flashing/RTT via the DK's on-board J-Link)
#   - udev rules so probe-rs can open the probe without root
#
# The probe-rs "plugdev group does not exist" warning: the shipped rules both
# assign GROUP="plugdev" *and* TAG+="uaccess". On Fedora, uaccess alone grants
# access to locally-logged-in users, so the warning is harmless - but we create
# the group anyway to silence it and to cover ssh sessions.
# =============================================================================
set -euo pipefail

echo "== dnf packages =="
sudo dnf install -y \
    arm-none-eabi-gcc-cs arm-none-eabi-newlib \
    make cppcheck clang-tools-extra \
    python3-pip

echo "== probe-rs =="
if ! command -v probe-rs >/dev/null 2>&1; then
    curl --proto '=https' --tlsv1.2 -LsSf \
        https://github.com/probe-rs/probe-rs/releases/latest/download/probe-rs-tools-installer.sh | sh
else
    echo "probe-rs already installed: $(probe-rs --version | head -1)"
fi

echo "== udev rules =="
if [ ! -f /etc/udev/rules.d/69-probe-rs.rules ]; then
    curl -sL "https://probe.rs/files/69-probe-rs.rules" |
        sudo tee /etc/udev/rules.d/69-probe-rs.rules >/dev/null
    sudo udevadm control --reload-rules
    sudo udevadm trigger
fi

echo "== plugdev group =="
if ! getent group plugdev >/dev/null; then
    sudo groupadd plugdev
fi
if ! id -nG "$USER" | grep -qw plugdev; then
    sudo usermod -aG plugdev "$USER"
    echo "NOTE: added $USER to plugdev - log out/in (or reboot) to take effect."
fi

echo "== optional: bleak for the live BLE verifier =="
echo "   pip install --user bleak     # only needed for tools/verify_beacon.py --scan"

echo
echo "Done. Plug in the nRF52840-DK (J2 USB, the one next to the power switch),"
echo "flip the power switch ON, then check:  probe-rs list"
