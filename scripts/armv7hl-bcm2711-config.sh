#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-only
#
# Overlay SOtM vermagic-compatible settings on top of
# Raspberry Pi's bcm2711_defconfig (32-bit armv7hl).
#
# Usage: cd <rpi-kernel-source> && bash /path/to/armv7hl-bcm2711-config.sh
#
# Strategy: start from bcm2711_defconfig, then modify ONLY what
# needs to match the SOtM Fedora 36 kernel's vermagic so that
# alsa_bridge.ko loads correctly.

set -euo pipefail

CONFIG_FILE=".config"
SCRIPTS_DIR="scripts"

if [ ! -f "$CONFIG_FILE" ]; then
	echo "ERROR: .config not found. Run 'make ARCH=arm bcm2711_defconfig' first."
	exit 1
fi

if [ ! -d "$SCRIPTS_DIR" ]; then
	echo "ERROR: Run from RPi kernel source root directory."
	exit 1
fi

echo "=== Starting from RPi bcm2711_defconfig ==="
echo "Original PREEMPT model:"
grep '^CONFIG_PREEMPT' "$CONFIG_FILE" || echo "(none)"

# === Step 1: Vermagic matching settings ===
# The SOtM alsa_bridge.ko vermagic is:
#   6.2.15-215.fc36.armv7hl SMP preempt mod_unload ARMv7 p2v8

# LOCALVERSION: make uname -r = 6.2.15-215.fc36.armv7hl
$SCRIPTS_DIR/config --file "$CONFIG_FILE" --set-str CONFIG_LOCALVERSION "-215.fc36.armv7hl"

# PREEMPT model: SOtM uses PREEMPT=y (not VOLUNTARY or NONE)
# RPi default is PREEMPT_VOLUNTARY=y, so we must switch
$SCRIPTS_DIR/config --file "$CONFIG_FILE" --disable CONFIG_PREEMPT_VOLUNTARY
$SCRIPTS_DIR/config --file "$CONFIG_FILE" --enable  CONFIG_PREEMPT
$SCRIPTS_DIR/config --file "$CONFIG_FILE" --enable  CONFIG_PREEMPT_COUNT
$SCRIPTS_DIR/config --file "$CONFIG_FILE" --enable  CONFIG_PREEMPTION
$SCRIPTS_DIR/config --file "$CONFIG_FILE" --enable  CONFIG_PREEMPT_RCU

# SMP (already set in bcm2711_defconfig)
$SCRIPTS_DIR/config --file "$CONFIG_FILE" --enable  CONFIG_SMP

# MODVERSIONS: SOtM has this OFF
$SCRIPTS_DIR/config --file "$CONFIG_FILE" --disable CONFIG_MODVERSIONS

# MODULE_UNLOAD
$SCRIPTS_DIR/config --file "$CONFIG_FILE" --enable  CONFIG_MODULE_UNLOAD

# Module signing (match SOtM RPM behavior)
$SCRIPTS_DIR/config --file "$CONFIG_FILE" --enable  CONFIG_MODULE_SIG
$SCRIPTS_DIR/config --file "$CONFIG_FILE" --enable  CONFIG_MODULE_SIG_ALL
$SCRIPTS_DIR/config --file "$CONFIG_FILE" --enable  CONFIG_MODULE_SIG_SHA256
$SCRIPTS_DIR/config --file "$CONFIG_FILE" --set-str CONFIG_MODULE_SIG_HASH "sha256"

# === Step 2: Boot-critical drivers built-in (=y) ===
# (Most are already =y in bcm2711_defconfig, but ensure)

$SCRIPTS_DIR/config --file "$CONFIG_FILE" --enable  CONFIG_MMC_BCM2835_MMC
$SCRIPTS_DIR/config --file "$CONFIG_FILE" --enable  CONFIG_MMC_BCM2835_DMA
$SCRIPTS_DIR/config --file "$CONFIG_FILE" --enable  CONFIG_MMC_BCM2835_SDHOST
$SCRIPTS_DIR/config --file "$CONFIG_FILE" --enable  CONFIG_MMC_SDHCI
$SCRIPTS_DIR/config --file "$CONFIG_FILE" --enable  CONFIG_MMC_SDHCI_PLTFM
$SCRIPTS_DIR/config --file "$CONFIG_FILE" --enable  CONFIG_MMC_SDHCI_IPROC

$SCRIPTS_DIR/config --file "$CONFIG_FILE" --enable  CONFIG_EXT4_FS
$SCRIPTS_DIR/config --file "$CONFIG_FILE" --enable  CONFIG_BCMGENET
$SCRIPTS_DIR/config --file "$CONFIG_FILE" --enable  CONFIG_USB_XHCI_HCD
$SCRIPTS_DIR/config --file "$CONFIG_FILE" --enable  CONFIG_USB_XHCI_PLATFORM

# === Step 3: Finalize ===

# Ensure the config is valid (resolve dependencies)
make ARCH=arm olddefconfig 2>/dev/null

# ARM_MODULE_PLTS: Required for loading Fedora-compiled modules
# Must be set AFTER olddefconfig, otherwise it gets reverted
$SCRIPTS_DIR/config --file "$CONFIG_FILE" --enable  CONFIG_ARM_MODULE_PLTS

echo ""
echo "=== Config overlay complete ==="
echo "Verified key settings:"
for key in CONFIG_LOCALVERSION CONFIG_PREEMPT CONFIG_SMP CONFIG_MODVERSIONS CONFIG_MODULE_UNLOAD CONFIG_ARM_LPAE CONFIG_MMC_BCM2835_MMC CONFIG_MMC_BCM2835_SDHOST CONFIG_EXT4_FS CONFIG_FB_SIMPLE CONFIG_FRAMEBUFFER_CONSOLE CONFIG_RASPBERRYPI_FIRMWARE; do
	grep "^$key[= ]" "$CONFIG_FILE" | head -1 || echo "$key=NOT SET"
done
echo ""
echo "SUCCESS: RPi bcm2711 config with SOtM vermagic overlay."
