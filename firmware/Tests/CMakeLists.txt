cmake_minimum_required(VERSION 3.20)

# Host unit tests for the firmware modules. Built with the host compiler, separate from the SDCC firmware project.
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
# The module groups below link their own sources. All other test_*.c files are LCD tests and run against
# every chunk size.
file(GLOB TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/test_*.c")
set(TICK_TEST_PATTERN "^test_(qa_)?tick")
set(DELAY_TEST_PATTERN "^test_(qa_)?delay")

add_library(tick_real STATIC "${FIRMWARE_DIR}/src/tick/tick.c")
target_include_directories(tick_real PUBLIC ${HOST_INCLUDE_DIRS} "${FIRMWARE_DIR}/src/tick")

add_library(delay_on_fake_tick STATIC "${FIRMWARE_DIR}/src/delay/delay.c" fakes/fake_tick.c)
target_include_directories(delay_on_fake_tick PUBLIC ${HOST_INCLUDE_DIRS})

# Module test groups: a test_<group>*.c or test_qa_<group>*.c file links the sources of its group, once.
set(GFX_TEST_PATTERN "^test_(qa_)?gfx")
set(GFX_SOURCES
    "${FIRMWARE_DIR}/src/gfx/font_u8g2.c"
    "${FIRMWARE_DIR}/src/gfx/fonts.c"
    "${FIRMWARE_DIR}/src/gfx/gfx.c"
    "${FIRMWARE_DIR}/src/lcd/lcd_fb.c")
add_library(gfx_group STATIC ${GFX_SOURCES})
target_include_directories(gfx_group PUBLIC ${HOST_INCLUDE_DIRS} "${FIRMWARE_DIR}/src/gfx")

# The firmware build gets these values from CMake; host tests use the default target configuration.
set(TARGET_DEFINITIONS MCU_CLOCK_HZ=24000000UL FLASH_CODE_SIZE=61440UL)

set(EEPROM_TEST_PATTERN "^test_(qa_)?eeprom")
set(EEPROM_SOURCES "${FIRMWARE_DIR}/src/eeprom/eeprom.c" fakes/fake_iap.c)
add_library(eeprom_group STATIC ${EEPROM_SOURCES})
target_include_directories(eeprom_group PUBLIC ${HOST_INCLUDE_DIRS})
target_compile_definitions(eeprom_group PUBLIC ${TARGET_DEFINITIONS})

set(PROGRESS_TEST_PATTERN "^test_(qa_)?progress")
set(PROGRESS_SOURCES "${FIRMWARE_DIR}/src/progress/progress.c" "${FIRMWARE_DIR}/src/util/crc8.c")
add_library(progress_group STATIC ${PROGRESS_SOURCES} ${EEPROM_SOURCES})
target_include_directories(progress_group PUBLIC ${HOST_INCLUDE_DIRS})
target_compile_definitions(progress_group PUBLIC ${TARGET_DEFINITIONS})

set(SETTINGS_TEST_PATTERN "^test_(qa_)?settings")
add_library(settings_group STATIC
    "${FIRMWARE_DIR}/src/settings/settings.c"
    "${FIRMWARE_DIR}/src/util/crc8.c"
    ${EEPROM_SOURCES})
target_include_directories(settings_group PUBLIC ${HOST_INCLUDE_DIRS} "${FIRMWARE_DIR}/src/settings")
target_compile_definitions(settings_group PUBLIC ${TARGET_DEFINITIONS})

# fake_delay.c records the LCD pin event count, so it needs fake_lcd_pins.c.
set(BUTTONS_TEST_PATTERN "^test_(qa_)?buttons")
add_library(buttons_group STATIC
    "${FIRMWARE_DIR}/src/buttons/buttons.c"
    fakes/fake_tick.c
    fakes/fake_delay.c
    fakes/fake_lcd_pins.c
    fakes/fake_button_pins.c)
target_include_directories(buttons_group PUBLIC ${HOST_INCLUDE_DIRS})

set(BACKLIGHT_TEST_PATTERN "^test_(qa_)?backlight")
add_library(backlight_group STATIC "${FIRMWARE_DIR}/src/backlight/backlight.c" fakes/fake_backlight_pwm.c)
target_include_directories(backlight_group PUBLIC ${HOST_INCLUDE_DIRS})
target_compile_definitions(backlight_group PUBLIC ${TARGET_DEFINITIONS})

# The sidetone group links the settings module, so the tests can check the tone range against settings_min/max.
set(SIDETONE_TEST_PATTERN "^test_(qa_)?sidetone")
add_library(sidetone_group STATIC
    "${FIRMWARE_DIR}/src/sidetone/sidetone.c"
    fakes/fake_sidetone_pwm.c
    fakes/fake_tick.c
    "${FIRMWARE_DIR}/src/settings/settings.c"
    "${FIRMWARE_DIR}/src/util/crc8.c"
    ${EEPROM_SOURCES})
target_include_directories(sidetone_group PUBLIC ${HOST_INCLUDE_DIRS} "${FIRMWARE_DIR}/src/settings")
target_compile_definitions(sidetone_group PUBLIC ${TARGET_DEFINITIONS})

# The keyer tests include cw/keyer_service.h and run the real sidetone for the envelope integration tests.
set(KEYER_TEST_PATTERN "^test_(qa_)?keyer")
add_library(keyer_group STATIC
    "${FIRMWARE_DIR}/src/cw/keyer.c"
    "${FIRMWARE_DIR}/src/cw/timing.c"
    fakes/fake_paddle_pins.c
    "${FIRMWARE_DIR}/src/sidetone/sidetone.c"
    fakes/fake_sidetone_pwm.c
    fakes/fake_tick.c
    "${FIRMWARE_DIR}/src/settings/settings.c"
    "${FIRMWARE_DIR}/src/util/crc8.c"
    ${EEPROM_SOURCES})
target_include_directories(keyer_group PUBLIC ${HOST_INCLUDE_DIRS} "${FIRMWARE_DIR}/src/settings")
target_compile_definitions(keyer_group PUBLIC ${TARGET_DEFINITIONS})

set(TIMING_TEST_PATTERN "^test_(qa_)?timing")
add_library(timing_group STATIC "${FIRMWARE_DIR}/src/cw/timing.c")
target_include_directories(timing_group PUBLIC ${HOST_INCLUDE_DIRS})
target_compile_definitions(timing_group PUBLIC ${TARGET_DEFINITIONS})

set(RNG_TEST_PATTERN "^test_(qa_)?rng")
add_library(rng_group STATIC "${FIRMWARE_DIR}/src/util/rng.c")
target_include_directories(rng_group PUBLIC ${HOST_INCLUDE_DIRS})

# The Koch group also links the Morse table, so the tests can prove that every Koch character has a code.
set(KOCH_TEST_PATTERN "^test_(qa_)?koch")
set(KOCH_SOURCES
    "${FIRMWARE_DIR}/src/koch/koch.c"
    "${FIRMWARE_DIR}/src/koch/keyboard.c"
    "${FIRMWARE_DIR}/src/util/rng.c")
add_library(koch_group STATIC ${KOCH_SOURCES} "${FIRMWARE_DIR}/src/koch/koch_item.c" "${FIRMWARE_DIR}/src/cw/morse.c")
target_include_directories(koch_group PUBLIC ${HOST_INCLUDE_DIRS})

set(TRAINING_TEST_PATTERN "^test_(qa_)?training")
add_library(training_group STATIC
    "${FIRMWARE_DIR}/src/modes/training.c"
    ${KOCH_SOURCES}
    "${FIRMWARE_DIR}/src/koch/koch_item.c"
    ${PROGRESS_SOURCES}
    ${EEPROM_SOURCES}
    fakes/fake_tick.c
    support/progress_fixture.c)
target_include_directories(training_group PUBLIC ${HOST_INCLUDE_DIRS})
target_compile_definitions(training_group PUBLIC ${TARGET_DEFINITIONS})

# The player tests include cw/player_service.h and run the real sidetone for the envelope tests.
set(PLAYER_TEST_PATTERN "^test_(qa_)?player")
add_library(player_group STATIC
    "${FIRMWARE_DIR}/src/cw/player.c"
    "${FIRMWARE_DIR}/src/cw/timing.c"
    "${FIRMWARE_DIR}/src/cw/morse.c"
    "${FIRMWARE_DIR}/src/sidetone/sidetone.c"
    fakes/fake_sidetone_pwm.c
    fakes/fake_tick.c
    "${FIRMWARE_DIR}/src/settings/settings.c"
    "${FIRMWARE_DIR}/src/util/crc8.c"
    ${EEPROM_SOURCES})
target_include_directories(player_group PUBLIC ${HOST_INCLUDE_DIRS})
target_compile_definitions(player_group PUBLIC ${TARGET_DEFINITIONS})

set(MORSE_TEST_PATTERN "^test_(qa_)?morse")
add_library(morse_group STATIC "${FIRMWARE_DIR}/src/cw/morse.c")
target_include_directories(morse_group PUBLIC ${HOST_INCLUDE_DIRS})

set(FREE_INPUT_TEST_PATTERN "^test_(qa_)?free_input")
add_library(free_input_group STATIC
    "${FIRMWARE_DIR}/src/modes/free_input.c"
    "${FIRMWARE_DIR}/src/cw/morse.c"
    ${GFX_SOURCES}
    fakes/fake_keyer.c
    fakes/fake_sidetone.c
    "${FIRMWARE_DIR}/src/settings/settings.c"
    "${FIRMWARE_DIR}/src/util/crc8.c"
    ${EEPROM_SOURCES}
    "${FIRMWARE_DIR}/src/buttons/buttons.c"
    fakes/fake_tick.c
    fakes/fake_delay.c
    fakes/fake_lcd_pins.c
    fakes/fake_button_pins.c)
target_include_directories(free_input_group PUBLIC
    ${HOST_INCLUDE_DIRS} "${FIRMWARE_DIR}/src/gfx" "${FIRMWARE_DIR}/src/settings")
target_compile_definitions(free_input_group PUBLIC ${TARGET_DEFINITIONS})

# The menu group also links the buttons, so the start-up tests run the real recovery window. The mode tests of
# Receiving and Sending use the same group: their scenarios start from the menu (ui-ux.md section 9.4). Items come
# from fake_koch_item.c (training_spec.md D-T18), the player and the keyer are fakes.
set(MENU_TEST_PATTERN "^test_(qa_)?menu")
set(MODES_TEST_PATTERN "^test_(qa_)?(receiving|sending)")
file(GLOB MENU_SOURCES CONFIGURE_DEPENDS "${FIRMWARE_DIR}/src/menu/menu*.c")
add_library(menu_group STATIC
    ${MENU_SOURCES}
    "${FIRMWARE_DIR}/src/modes/free_input.c"
    "${FIRMWARE_DIR}/src/modes/receiving.c"
    "${FIRMWARE_DIR}/src/modes/sending.c"
    "${FIRMWARE_DIR}/src/modes/training.c"
    "${FIRMWARE_DIR}/src/modes/training_screen.c"
    ${KOCH_SOURCES}
    fakes/fake_koch_item.c
    ${PROGRESS_SOURCES}
    support/progress_fixture.c
    "${FIRMWARE_DIR}/src/cw/morse.c"
    fakes/fake_keyer.c
    fakes/fake_player.c
    ${GFX_SOURCES}
    "${FIRMWARE_DIR}/src/settings/settings.c"
    ${EEPROM_SOURCES}
    fakes/fake_lcd_contrast.c
    fakes/fake_backlight.c
    fakes/fake_stats.c
    fakes/fake_sidetone.c
    "${FIRMWARE_DIR}/src/buttons/buttons.c"
    fakes/fake_tick.c
    fakes/fake_delay.c
    fakes/fake_lcd_pins.c
    fakes/fake_button_pins.c)
target_include_directories(menu_group PUBLIC
    ${HOST_INCLUDE_DIRS} "${FIRMWARE_DIR}/src/menu" "${FIRMWARE_DIR}/src/gfx" "${FIRMWARE_DIR}/src/settings")
target_compile_definitions(menu_group PUBLIC ${TARGET_DEFINITIONS})

set(MODULE_TEST_PATTERNS
    GFX_TEST_PATTERN EEPROM_TEST_PATTERN SETTINGS_TEST_PATTERN BUTTONS_TEST_PATTERN BACKLIGHT_TEST_PATTERN
    MENU_TEST_PATTERN SIDETONE_TEST_PATTERN KEYER_TEST_PATTERN MORSE_TEST_PATTERN FREE_INPUT_TEST_PATTERN
    TIMING_TEST_PATTERN RNG_TEST_PATTERN KOCH_TEST_PATTERN PROGRESS_TEST_PATTERN
    PLAYER_TEST_PATTERN TRAINING_TEST_PATTERN MODES_TEST_PATTERN)
set(KEYER_TEST_PATTERN_LIBRARIES keyer_group)
set(TIMING_TEST_PATTERN_LIBRARIES timing_group)
set(RNG_TEST_PATTERN_LIBRARIES rng_group)
set(KOCH_TEST_PATTERN_LIBRARIES koch_group)
set(PROGRESS_TEST_PATTERN_LIBRARIES progress_group)
set(PLAYER_TEST_PATTERN_LIBRARIES player_group)
set(TRAINING_TEST_PATTERN_LIBRARIES training_group)
set(MORSE_TEST_PATTERN_LIBRARIES morse_group)
set(FREE_INPUT_TEST_PATTERN_LIBRARIES free_input_group)
set(SIDETONE_TEST_PATTERN_LIBRARIES sidetone_group)
set(MENU_TEST_PATTERN_LIBRARIES menu_group)
set(MODES_TEST_PATTERN_LIBRARIES menu_group)
set(BACKLIGHT_TEST_PATTERN_LIBRARIES backlight_group)
set(BUTTONS_TEST_PATTERN_LIBRARIES buttons_group)
set(SETTINGS_TEST_PATTERN_LIBRARIES settings_group)
set(GFX_TEST_PATTERN_LIBRARIES gfx_group)
set(EEPROM_TEST_PATTERN_LIBRARIES eeprom_group)

foreach(test_source IN LISTS TEST_SOURCES)
  get_filename_component(test_name "${test_source}" NAME_WE)
  set(module_libraries "")
  foreach(pattern_name IN LISTS MODULE_TEST_PATTERNS)
    if(test_name MATCHES "${${pattern_name}}")
      set(module_libraries ${${pattern_name}_LIBRARIES})
      break()
    endif()
  endforeach()
  if(module_libraries)
    add_executable(${test_name} "${test_source}")
    target_link_libraries(${test_name} PRIVATE ${module_libraries})
    add_test(NAME ${test_name} COMMAND ${test_name})
  elseif(test_name MATCHES "${TICK_TEST_PATTERN}")
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
