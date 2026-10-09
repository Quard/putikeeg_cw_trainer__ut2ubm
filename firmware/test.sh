#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Vadym Zakovinko
# SPDX-License-Identifier: BSD-3-Clause

# Build and run the host unit tests and the check_asm.sh self-test. Exits non-zero when any step fails.
set -euo pipefail

firmware_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
build_dir="${firmware_dir}/Tests/build"

cmake -S "${firmware_dir}/Tests" -B "${build_dir}"
cmake --build "${build_dir}" --parallel

# Both steps run even when the first fails, so one report shows every failure.
status=0
ctest --test-dir "${build_dir}" --output-on-failure || status=1
"${firmware_dir}/tools/check_asm.sh" --self-test || status=1
exit "${status}"
