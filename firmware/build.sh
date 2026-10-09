#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Vadym Zakovinko
# SPDX-License-Identifier: BSD-3-Clause

# Build the firmware image and print its memory use.
set -euo pipefail

firmware_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
build_dir="${firmware_dir}/build"

cmake -S "${firmware_dir}" -B "${build_dir}" \
  -DCMAKE_TOOLCHAIN_FILE="${firmware_dir}/cmake/sdcc-toolchain.cmake"
cmake --build "${build_dir}" --parallel

cat "${build_dir}/putikeeg_cw_trainer.mem"
echo "Firmware version: $(head -n 1 "${firmware_dir}/VERSION")"
echo "Firmware image: ${build_dir}/putikeeg_cw_trainer.ihx"
