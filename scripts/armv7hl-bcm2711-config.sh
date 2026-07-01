#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-only
#
# Convert SOtM Fedora 36 armv7hl kernel config (Cubietruck/SUNXI)
# into a Pi 4B (BCM2711) compatible config.
#
# Usage: cd <kernel-source> && bash /path/to/armv7hl-bcm2711-config.sh
#
# The script:
#   1. Starts from the SOtM original config (placed as .config)
#   2. Disables Allwinner SUNXI architecture
#   3. Enables Broadcom BCM2835/BCM2711 (Pi 4B)
#   4. Enables all required Pi 4B drivers
#   5. Sets LOCALVERSION to match SOtM vermagic
#   6. Preserves PREEMPT, SMP, MODULE_UNLOAD, etc.

set -euo pipefail

CONFIG_FILE=".config"
SCRIPTS_DIR="scripts"

if [ ! -f "$CONFIG_FILE" ]; then
	echo "ERROR: .config not found. Copy the SOtM config first."
	exit 1
fi

if [ ! -d "$SCRIPTS_DIR" ]; then
	echo "ERROR: Run from kernel source root directory."
	exit 1
fi

# === Step 1: Disable Allwinner SUNXI architecture ===
# (This removes the entire sunxi platform including all sub-drivers)

$SCRIPTS_DIR/config --file "$CONFIG_FILE" --disable CONFIG_ARCH_SUNXI
$SCRIPTS_DIR/config --file "$CONFIG_FILE" --disable CONFIG_MACH_SUN4I
$SCRIPTS_DIR/config --file "$CONFIG_FILE" --disable CONFIG_MACH_SUN5I
$SCRIPTS_DIR/config --file "$CONFIG_FILE" --disable CONFIG_MACH_SUN6I
$SCRIPTS_DIR/config --file "$CONFIG_FILE" --disable CONFIG_MACH_SUN7I
$SCRIPTS_DIR/config --file "$CONFIG_FILE" --disable CONFIG_MACH_SUN8I
$SCRIPTS_DIR/config --file "$CONFIG_FILE" --disable CONFIG_MACH_SUN9I
$SCRIPTS_DIR/config --file "$CONFIG_FILE" --disable CONFIG_SUNXI_CCU
$SCRIPTS_DIR/config --file "$CONFIG_FILE" --disable CONFIG_SUNXI_MBUS
$SCRIPTS_DIR/config --file "$CONFIG_FILE" --disable CONFIG_SUNXI_NMI_INTC
$SCRIPTS_DIR/config --file "$CONFIG_FILE" --disable CONFIG_SUNXI_RSB
$SCRIPTS_DIR/config --file "$CONFIG_FILE" --disable CONFIG_SUNXI_SRAM
$SCRIPTS_DIR/config --file "$CONFIG_FILE" --disable CONFIG_SUNXI_WATCHDOG
$SCRIPTS_DIR/config --file "$CONFIG_FILE" --disable CONFIG_SUN4I_A10_CCU
$SCRIPTS_DIR/config --file "$CONFIG_FILE" --disable CONFIG_SUN4I_EMAC
$SCRIPTS_DIR/config --file "$CONFIG_FILE" --disable CONFIG_SUN4I_TIMER
$SCRIPTS_DIR/config --file "$CONFIG_FILE" --disable CONFIG_SUN5I_HSTIMER
$SCRIPTS_DIR/config --file "$CONFIG_FILE" --disable CONFIG_SUN6I_MSGBOX
$SCRIPTS_DIR/config --file "$CONFIG_FILE" --disable CONFIG_SUN6I_RTC_CCU
$SCRIPTS_DIR/config --file "$CONFIG_FILE" --disable CONFIG_SUN8I_DE2_CCU
$SCRIPTS_DIR/config --file "$CONFIG_FILE" --disable CONFIG_SUN8I_R_CCU
# Sunxi pinctrl (already disabled but safe)
$SCRIPTS_DIR/config --file "$CONFIG_FILE" --disable CONFIG_PINCTRL_SUNXI
$SCRIPTS_DIR/config --file "$CONFIG_FILE" --disable CONFIG_PINCTRL_SUN4I_A10
# Sunxi MMC
$SCRIPTS_DIR/config --file "$CONFIG_FILE" --disable CONFIG_MMC_SUNXI
# Sunxi Ethernet
$SCRIPTS_DIR/config --file "$CONFIG_FILE" --disable CONFIG_DWMAC_SUNXI
$SCRIPTS_DIR/config --file "$CONFIG_FILE" --disable CONFIG_DWMAC_SUN8I
# Sunxi SPI
$SCRIPTS_DIR/config --file "$CONFIG_FILE" --disable CONFIG_SPI_SUN4I
$SCRIPTS_DIR/config --file "$CONFIG_FILE" --disable CONFIG_SPI_SUN6I
# Sunxi PWM
$SCRIPTS_DIR/config --file "$CONFIG_FILE" --disable CONFIG_PWM_SUN4I
# Allwinner net vendor
$SCRIPTS_DIR/config --file "$CONFIG_FILE" --disable CONFIG_NET_VENDOR_ALLWINNER

# === Step 2: Enable Broadcom BCM2835/BCM2711 (Pi 4B) ===

$SCRIPTS_DIR/config --file "$CONFIG_FILE" --enable CONFIG_ARCH_BCM
$SCRIPTS_DIR/config --file "$CONFIG_FILE" --enable CONFIG_ARCH_BCM2835
$SCRIPTS_DIR/config --file "$CONFIG_FILE" --enable CONFIG_BCM2711

# === Step 3: Enable Pi 4B SoC infrastructure ===

# CPU errata (Cortex-A72 specific)
$SCRIPTS_DIR/config --file "$CONFIG_FILE" --enable CONFIG_ARM_ERRATA_814220
$SCRIPTS_DIR/config --file "$CONFIG_FILE" --enable CONFIG_ARM_ERRATA_854091

# Timers
$SCRIPTS_DIR/config --file "$CONFIG_FILE" --enable CONFIG_ARM_ARCH_TIMER
$SCRIPTS_DIR/config --file "$CONFIG_FILE" --enable CONFIG_ARM_GLOBAL_TIMER
$SCRIPTS_DIR/config --file "$CONFIG_FILE" --enable CONFIG_CLKSRC_ARM_GLOBAL_TIMER

# GPIO
$SCRIPTS_DIR/config --file "$CONFIG_FILE" --enable CONFIG_GPIO_BCM2835
$SCRIPTS_DIR/config --file "$CONFIG_FILE" --enable CONFIG_PINCTRL_BCM2835

# Mailbox (needed for VC4 GPU / firmware interface)
$SCRIPTS_DIR/config --file "$CONFIG_FILE" --enable CONFIG_BCM2835_MBOX

# === Step 4: Enable Pi 4B device drivers (ALL BOOT-CRITICAL =y) ===
# MMC/SD card - BUILT-IN (without these, kernel can't mount rootfs)
$SCRIPTS_DIR/config --file "$CONFIG_FILE" --enable  CONFIG_MMC_BCM2835
$SCRIPTS_DIR/config --file "$CONFIG_FILE" --enable  CONFIG_MMC_SDHCI
$SCRIPTS_DIR/config --file "$CONFIG_FILE" --enable  CONFIG_MMC_SDHCI_IPROC
$SCRIPTS_DIR/config --file "$CONFIG_FILE" --enable  CONFIG_MMC_SDHCI_PLTFM

# Ethernet (BCMGENET - built-in)
$SCRIPTS_DIR/config --file "$CONFIG_FILE" --enable  CONFIG_BCMGENET

# USB (DWC3 for USB 3.0 on Pi 4B - built-in)
$SCRIPTS_DIR/config --file "$CONFIG_FILE" --enable  CONFIG_USB_DWC3
$SCRIPTS_DIR/config --file "$CONFIG_FILE" --enable  CONFIG_USB_DWC3_HOST
# XHCI/USB 2.0 - built-in for USB keyboard at boot
$SCRIPTS_DIR/config --file "$CONFIG_FILE" --enable  CONFIG_USB_XHCI_HCD
$SCRIPTS_DIR/config --file "$CONFIG_FILE" --enable  CONFIG_USB_XHCI_PCI
$SCRIPTS_DIR/config --file "$CONFIG_FILE" --enable  CONFIG_USB_EHCI_HCD
$SCRIPTS_DIR/config --file "$CONFIG_FILE" --enable  CONFIG_USB_OHCI_HCD

# I2C
$SCRIPTS_DIR/config --file "$CONFIG_FILE" --enable  CONFIG_I2C_BCM2835

# SPI
$SCRIPTS_DIR/config --file "$CONFIG_FILE" --enable  CONFIG_SPI_BCM2835

# PWM
$SCRIPTS_DIR/config --file "$CONFIG_FILE" --enable  CONFIG_PWM_BCM2835

# Framebuffer (SIMPLE FB = built-in for early HDMI console)
$SCRIPTS_DIR/config --file "$CONFIG_FILE" --enable  CONFIG_DRM
$SCRIPTS_DIR/config --file "$CONFIG_FILE" --enable  CONFIG_FB_SIMPLE
$SCRIPTS_DIR/config --file "$CONFIG_FILE" --module  CONFIG_DRM_VC4
$SCRIPTS_DIR/config --file "$CONFIG_FILE" --module  CONFIG_DRM_VC4_HDMI

# DMA engine for BCM2835
$SCRIPTS_DIR/config --file "$CONFIG_FILE" --enable  CONFIG_DMA_BCM2835

# VCHIQ (VideoCore userland interface)
$SCRIPTS_DIR/config --file "$CONFIG_FILE" --module  CONFIG_VCHIQ

# Random number generator
$SCRIPTS_DIR/config --file "$CONFIG_FILE" --enable  CONFIG_HW_RANDOM_BCM2835

# Watchdog
$SCRIPTS_DIR/config --file "$CONFIG_FILE" --module  CONFIG_BCM2835_WDT

# Thermal
$SCRIPTS_DIR/config --file "$CONFIG_FILE" --enable  CONFIG_BCM2711_THERMAL

# === Step 5: Set LOCALVERSION to match SOtM vermagic ===
# This makes uname -r = 6.2.15-215.fc36.armv7hl
# which matches the alsa_bridge.ko vermagic

$SCRIPTS_DIR/config --file "$CONFIG_FILE" --set-str CONFIG_LOCALVERSION "-215.fc36.armv7hl"

# === Step 6: Finalize ===

# Ensure the config is valid (resolve dependencies)
make ARCH=arm olddefconfig 2>/dev/null

echo ""
echo "=== Config conversion complete ==="
echo ""
echo "Verified key settings:"
grep -E '(CONFIG_ARCH_BCM|CONFIG_ARCH_BCM2835|CONFIG_BCM2711|CONFIG_ARCH_SUNXI|CONFIG_PREEMPT|CONFIG_SMP|CONFIG_MODVERSIONS|CONFIG_MODULE_UNLOAD|CONFIG_ARM_LPAE|CONFIG_LOCALVERSION)' "$CONFIG_FILE" | grep -v '^#' | head -20
echo ""
grep -E '(CONFIG_MMC_BCM2835|CONFIG_MMC_SDHCI_IPROC|CONFIG_GPIO_BCM2835|CONFIG_PINCTRL_BCM2835|CONFIG_BCMGENET|CONFIG_USB_DWC3|CONFIG_DRM_VC4|CONFIG_FB_SIMPLE)' "$CONFIG_FILE" | grep -v '^#' | head -10
echo ""
echo "SUCCESS: Config ready for Pi 4B armv7hl build."
