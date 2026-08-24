#!/usr/bin/env bash
# Build the STM32 firmware locally with CMake/Ninja, ship the binary to the
# Raspberry Pi 4 over SSH/SCP, then flash it to the STM32F103C8T6 over an
# ST-Link probe attached to the Pi's USB, using st-flash (stlink-tools).
#
# Requires stlink-tools installed on the Pi: sudo apt install stlink-tools
# Requires arm-none-eabi-gcc, cmake and ninja on PATH locally.
#
# Config: copy scripts/.env.example to scripts/.env and fill in real values
# (scripts/.env is gitignored), or set the same variables before running.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
STM32_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

if [ -f "$SCRIPT_DIR/.env" ]; then
    # Strip CR before sourcing: a .env written by a Windows editor has CRLF
    # line endings, which would otherwise leave \r stuck on every value.
    set -a
    # shellcheck disable=SC1090
    . <(sed 's/\r$//' "$SCRIPT_DIR/.env")
    set +a
fi

if [ -z "${RPI_HOST:-}" ]; then
    echo "error: set RPI_HOST (hostname or IP of the Raspberry Pi)" 1>&2
    exit 1
fi
RPI_USER="${RPI_USER:-pi}"
RPI_PORT="${RPI_PORT:-22}"
RPI_REMOTE_DIR="${RPI_REMOTE_DIR:-/tmp/stm32-firmware}"
FLASH_ADDR="${FLASH_ADDR:-0x08000000}"
BUILD_DIR="${BUILD_DIR:-build}"

BIN_FILE="$STM32_DIR/$BUILD_DIR/firmware.bin"

echo "==> Building (CMake/Ninja)"
if [ ! -f "$STM32_DIR/$BUILD_DIR/build.ninja" ]; then
    cmake -G Ninja -S "$STM32_DIR" -B "$STM32_DIR/$BUILD_DIR"
fi
ninja -C "$STM32_DIR/$BUILD_DIR"

if [ ! -f "$BIN_FILE" ]; then
    echo "error: $BIN_FILE not found after build" 1>&2
    exit 1
fi

echo "==> Copying firmware.bin to $RPI_USER@$RPI_HOST:$RPI_REMOTE_DIR"
ssh -p "$RPI_PORT" "$RPI_USER@$RPI_HOST" "mkdir -p '$RPI_REMOTE_DIR'"
scp -P "$RPI_PORT" "$BIN_FILE" "$RPI_USER@$RPI_HOST:$RPI_REMOTE_DIR/firmware.bin"

echo "==> Flashing over ST-Link on $RPI_HOST (st-flash)"
ssh -p "$RPI_PORT" "$RPI_USER@$RPI_HOST" \
    "st-flash --reset write '$RPI_REMOTE_DIR/firmware.bin' $FLASH_ADDR"

echo "==> Done"
# Logs go out over USART1 @9600 to the Bluetooth module, not to a local
# serial port -- open an SPP terminal on the phone to see them.
