cmake_minimum_required(VERSION 3.20)

# Host unit tests for the LCD, tick and delay modules. Built with the host compiler, separate from the SDCC firmware project.
project(putikeeg_cw_trainer_host_tests LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

enable_testing()

set(FIRMWARE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/..")

add_compile_options(-Wall -Wextra -Wpedantic -Werror -g -fsanitize=address,undefined -fno-sanitize-recover=all
                    -fno-omit-frame-pointer)
add_link_options(-fsanitize=address,undefined)

# The fakes directory comes first, so its board/lcd_pins.h replaces the target header.
set(HOST_INCLUDE_DIRS
    "${CMAKE_CURRENT_SOURCE_DIR}/fakes"
    "${FIRMWARE_DIR}/inc"
    "${CMAKE_CURRENT_SOURCE_DIR}/support"
    "${CMAKE_CURRENT_SOURCE_DIR}")

set(LCD_SOURCES
    "${FIRMWARE_DIR}/src/lcd/lcd.c"
    "${FIRMWARE_DIR}/src/lcd/lcd_fb.c"
    "${FIRMWARE_DIR}/src/lcd/lcd_spi.c")

add_library(lcd_test_support STATIC
    fakes/fake_lcd_pins.c
    fakes/fake_delay.c
    support/lcd_protocol_decoder.c
    support/lcd_controller_model.c)
target_include_directories(lcd_test_support PUBLIC ${HOST_INCLUDE_DIRS})

# Every test runs against each flush chunk size the configuration allows.
set(LCD_CHUNK_SIZES 32 64 128)

foreach(chunk IN LISTS LCD_CHUNK_SIZES)
  add_library(lcd_chunk${chunk} STATIC ${LCD_SOURCES})
  target_include_directories(lcd_chunk${chunk} PUBLIC ${HOST_INCLUDE_DIRS})
  target_compile_definitions(lcd_chunk${chunk} PUBLIC LCD_FLUSH_CHUNK_COLUMNS=${chunk})
endforeach()

# test_tick* and test_delay* (QA: test_qa_tick*, test_qa_delay*) test the tick and delay modules once each.
# All other test_*.c files are LCD tests and run against every chunk size.
file(GLOB TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/test_*.c")
set(TICK_TEST_PATTERN "^test_(qa_)?tick")
set(DELAY_TEST_PATTERN "^test_(qa_)?delay")

add_library(tick_real STATIC "${FIRMWARE_DIR}/src/tick/tick.c")
target_include_directories(tick_real PUBLIC ${HOST_INCLUDE_DIRS} "${FIRMWARE_DIR}/src/tick")

add_library(delay_on_fake_tick STATIC "${FIRMWARE_DIR}/src/delay/delay.c" fakes/fake_tick.c)
target_include_directories(delay_on_fake_tick PUBLIC ${HOST_INCLUDE_DIRS})

foreach(test_source IN LISTS TEST_SOURCES)
  get_filename_component(test_name "${test_source}" NAME_WE)
  if(test_name MATCHES "${TICK_TEST_PATTERN}")
    add_executable(${test_name} "${test_source}")
    target_link_libraries(${test_name} PRIVATE tick_real)
    add_test(NAME ${test_name} COMMAND ${test_name})
  elseif(test_name MATCHES "${DELAY_TEST_PATTERN}")
    add_executable(${test_name} "${test_source}")
    target_link_libraries(${test_name} PRIVATE delay_on_fake_tick)
    add_test(NAME ${test_name} COMMAND ${test_name})
  else()
    foreach(chunk IN LISTS LCD_CHUNK_SIZES)
      set(target_name "${test_name}_chunk${chunk}")
      add_executable(${target_name} "${test_source}")
      target_link_libraries(${target_name} PRIVATE lcd_chunk${chunk} lcd_test_support)
      add_test(NAME ${target_name} COMMAND ${target_name})
    endforeach()
  endif()
endforeach()
