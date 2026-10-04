cmake_minimum_required(VERSION 3.20)

project(putikeeg_cw_trainer LANGUAGES C)

set(MCU_CLOCK_HZ 24000000 CACHE STRING "System clock in Hz. flash.sh trims the internal oscillator to this value.")
set(FLASH_CODE_SIZE 61440 CACHE STRING "Program flash in bytes. The rest of the 64 KiB flash is EEPROM.")
set(MCU_IRAM_SIZE 256 CACHE STRING "Internal RAM in bytes.")
set(MCU_XRAM_SIZE 3072 CACHE STRING "Extended RAM (xdata) in bytes.")

file(GLOB_RECURSE FIRMWARE_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")

add_executable(putikeeg_cw_trainer ${FIRMWARE_SOURCES})
target_include_directories(putikeeg_cw_trainer PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/inc")
target_compile_definitions(putikeeg_cw_trainer PRIVATE MCU_CLOCK_HZ=${MCU_CLOCK_HZ}UL)
target_compile_options(putikeeg_cw_trainer PRIVATE --std=sdcc11 --opt-code-size)
# The linker fails when the image or the RAM use exceeds these limits.
target_link_options(putikeeg_cw_trainer PRIVATE
  --code-size ${FLASH_CODE_SIZE}
  --iram-size ${MCU_IRAM_SIZE}
  --xram-size ${MCU_XRAM_SIZE}
)
