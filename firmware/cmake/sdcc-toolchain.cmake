# SPDX-FileCopyrightText: 2026 Vadym Zakovinko
# SPDX-License-Identifier: BSD-3-Clause

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR mcs51)

set(CMAKE_C_COMPILER sdcc)
set(CMAKE_C_FLAGS_INIT "-mmcs51")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-mmcs51")

# SDCC cannot link a host test program during compiler detection.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
