#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Vadym Zakovinko
# SPDX-License-Identifier: BSD-3-Clause

# Build the firmware and download it into the MCU with stcgal.
# Usage: flash.sh [serial-port]. The port defaults to STCGAL_PORT, then to /dev/ttyUSB0.
set -euo pipefail

firmware_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
build_dir="${firmware_dir}/build"
port="${1:-${STCGAL_PORT:-/dev/ttyUSB0}}"

"${firmware_dir}/build.sh"

read_cache_value() {
  sed -n "s/^$1:[A-Z]*=//p" "${build_dir}/CMakeCache.txt"
}

clock_khz=$(( $(read_cache_value MCU_CLOCK_HZ) / 1000 ))
code_size="$(read_cache_value FLASH_CODE_SIZE)"

echo "Power-cycle the trainer when stcgal waits for the MCU."
stcgal -P stc8d -p "${port}" -t "${clock_khz}" \
  -o program_eeprom_split="${code_size}" \
  "${build_dir}/putikeeg_cw_trainer.ihx"
