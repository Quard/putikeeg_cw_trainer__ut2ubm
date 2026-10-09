/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <string.h>

#include "eeprom/eeprom.h"
#include "eeprom/eeprom_map.h"
#include "fake_iap.h"
#include "support/test_assert.h"

#define CMD_READ 1U
#define CMD_PROGRAM 2U
#define CMD_ERASE 3U

static void test_map_constants(void) {
  TEST_ASSERT_EQUAL(4096U, EEPROM_SIZE);
  TEST_ASSERT_EQUAL(EEPROM_SIZE, EEPROM_PAGE_SIZE * EEPROM_PAGE_COUNT);
  TEST_ASSERT_EQUAL(0U, EEPROM_MAP_SETTINGS_ADDRESS);
  TEST_ASSERT_EQUAL(EEPROM_MAP_FIRST_FREE_PAGE * EEPROM_PAGE_SIZE, EEPROM_MAP_FREE_ADDRESS);
  TEST_ASSERT_EQUAL(EEPROM_SIZE, EEPROM_MAP_FREE_ADDRESS + EEPROM_MAP_FREE_SIZE);
}

static void test_range_errors_touch_nothing(void) {
  static const uint16_t cases[][2] = {
      {0xFFFFU, 2U}, {0xFFFFU, 0U}, {4097U, 0U},   {4096U, 1U},   {4095U, 2U},
      {0U, 4097U},   {1U, 4096U},   {0U, 0xFFFFU}, {0x8000U, 1U}, {2048U, 2049U},
  };
  uint8_t buffer[8] = {0};
  size_t i;
  fake_iap_reset();
  for (i = 0U; i < sizeof(cases) / sizeof(cases[0]); ++i) {
    TEST_ASSERT_EQUAL(EEPROM_ERR_RANGE, eeprom_read(cases[i][0], buffer, cases[i][1]));
    TEST_ASSERT_EQUAL(EEPROM_ERR_RANGE, eeprom_write(cases[i][0], buffer, cases[i][1]));
  }
  TEST_ASSERT_EQUAL(EEPROM_ERR_RANGE, eeprom_erase_page(EEPROM_PAGE_COUNT));
  TEST_ASSERT_EQUAL(EEPROM_ERR_RANGE, eeprom_erase_page(0xFFU));
  TEST_ASSERT_EQUAL(0U, fake_iap_access_count());
}

static void test_boundaries_in_range(void) {
  uint8_t buffer[2] = {0x12U, 0x34U};
  fake_iap_reset();
  TEST_ASSERT_EQUAL(EEPROM_OK, eeprom_write(4094U, buffer, 2U));
  TEST_ASSERT_EQUAL(EEPROM_OK, eeprom_read(4095U, buffer, 1U));
  TEST_ASSERT_EQUAL(0x34U, buffer[0]);
  TEST_ASSERT_EQUAL(EEPROM_OK, eeprom_read(4096U, buffer, 0U));
  TEST_ASSERT_EQUAL(EEPROM_OK, eeprom_write(0U, buffer, 0U));
  TEST_ASSERT_EQUAL(EEPROM_OK, eeprom_erase_page(7U));
  TEST_ASSERT_EQUAL(1U, fake_iap_erase_count(7U));
}

static void test_read_write_erase(void) {
  uint8_t data[16];
  uint8_t back[16];
  size_t i;
  fake_iap_reset();
  for (i = 0U; i < sizeof(data); ++i) {
    data[i] = (uint8_t)(i * 17U);
  }
  TEST_ASSERT_EQUAL(EEPROM_OK, eeprom_read(0x0300U, back, sizeof(back)));
  for (i = 0U; i < sizeof(back); ++i) {
    TEST_ASSERT_EQUAL(0xFFU, back[i]);
  }
  TEST_ASSERT_EQUAL(EEPROM_OK, eeprom_write(0x0300U, data, sizeof(data)));
  TEST_ASSERT_EQUAL(0, memcmp(&fake_iap_memory()[0x0300], data, sizeof(data)));
  TEST_ASSERT_EQUAL(sizeof(data), fake_iap_command_count(CMD_PROGRAM));
  TEST_ASSERT_EQUAL(sizeof(data), fake_iap_command_count(CMD_READ) - sizeof(back));
  memset(back, 0, sizeof(back));
  TEST_ASSERT_EQUAL(EEPROM_OK, eeprom_read(0x0300U, back, sizeof(back)));
  TEST_ASSERT_EQUAL(0, memcmp(back, data, sizeof(data)));
  TEST_ASSERT_EQUAL(EEPROM_OK, eeprom_erase_page(1U));
  TEST_ASSERT_EQUAL(1U, fake_iap_erase_count(1U));
  for (i = 0x200U; i < 0x400U; ++i) {
    TEST_ASSERT_EQUAL(0xFFU, fake_iap_memory()[i]);
  }
  TEST_ASSERT(fake_iap_were_triggers_enabled());
  TEST_ASSERT_EQUAL(24U, fake_iap_tps());
}

static void test_program_cannot_set_bits(void) {
  uint8_t value = 0x0FU;
  fake_iap_reset();
  TEST_ASSERT_EQUAL(EEPROM_OK, eeprom_write(10U, &value, 1U));
  value = 0xF0U;
  TEST_ASSERT_EQUAL(EEPROM_ERR_FAIL, eeprom_write(10U, &value, 1U));
  TEST_ASSERT_EQUAL(0x00U, fake_iap_memory()[10]);
  value = 0x00U;
  TEST_ASSERT_EQUAL(EEPROM_OK, eeprom_write(10U, &value, 1U));
}

static void test_write_stops_at_first_mismatch(void) {
  uint8_t data[4] = {0x01U, 0xFFU, 0x02U, 0x03U};
  fake_iap_reset();
  fake_iap_memory()[21] = 0x00U;
  TEST_ASSERT_EQUAL(EEPROM_ERR_FAIL, eeprom_write(20U, data, 4U));
  TEST_ASSERT_EQUAL(0x01U, fake_iap_memory()[20]);
  TEST_ASSERT_EQUAL(0xFFU, fake_iap_memory()[22]);
  TEST_ASSERT_EQUAL(2U, fake_iap_command_count(CMD_PROGRAM));
  TEST_ASSERT(fake_iap_is_idle());
}

static void test_cmd_fail(void) {
  uint8_t data[4] = {1U, 2U, 3U, 4U};
  uint8_t back[4];
  size_t operation;

  for (operation = 1U; operation <= 4U; ++operation) {
    fake_iap_reset();
    fake_iap_set_fail_operation(operation);
    TEST_ASSERT_EQUAL(EEPROM_ERR_FAIL, eeprom_read(0U, back, 4U));
    TEST_ASSERT_EQUAL(operation, fake_iap_operation_count());
    TEST_ASSERT(fake_iap_is_idle());
  }
  /* Operation 1 is the program of byte 0, 2 its read-back, 3 the program of byte 1. */
  for (operation = 1U; operation <= 3U; ++operation) {
    fake_iap_reset();
    fake_iap_set_fail_operation(operation);
    TEST_ASSERT_EQUAL(EEPROM_ERR_FAIL, eeprom_write(0U, data, 4U));
    TEST_ASSERT_EQUAL(operation, fake_iap_operation_count());
    TEST_ASSERT(fake_iap_is_idle());
  }
  fake_iap_reset();
  fake_iap_memory()[600] = 0x00U;
  fake_iap_set_fail_operation(1U);
  TEST_ASSERT_EQUAL(EEPROM_ERR_FAIL, eeprom_erase_page(1U));
  TEST_ASSERT_EQUAL(0x00U, fake_iap_memory()[600]);
  TEST_ASSERT(fake_iap_is_idle());
  /* The failure flag does not stay set for the next call. */
  TEST_ASSERT_EQUAL(EEPROM_OK, eeprom_erase_page(1U));
  TEST_ASSERT_EQUAL(0xFFU, fake_iap_memory()[600]);
}

static void test_idle_after_every_return(void) {
  uint8_t value = 0x5AU;
  fake_iap_reset();
  TEST_ASSERT_EQUAL(EEPROM_OK, eeprom_write(5U, &value, 1U));
  TEST_ASSERT(fake_iap_is_idle());
  TEST_ASSERT_EQUAL(EEPROM_OK, eeprom_read(5U, &value, 1U));
  TEST_ASSERT(fake_iap_is_idle());
  TEST_ASSERT_EQUAL(EEPROM_OK, eeprom_erase_page(0U));
  TEST_ASSERT(fake_iap_is_idle());
  value = 0x00U;
  TEST_ASSERT_EQUAL(EEPROM_OK, eeprom_write(5U, &value, 1U));
  value = 0xFFU;
  TEST_ASSERT_EQUAL(EEPROM_ERR_FAIL, eeprom_write(5U, &value, 1U));
  TEST_ASSERT(fake_iap_is_idle());
}

static void test_low_supply(void) {
  uint8_t value = 0x00U;
  uint8_t back = 0x11U;
  size_t accesses;

  fake_iap_reset();
  fake_iap_set_supply_low(true);
  TEST_ASSERT_EQUAL(EEPROM_ERR_LOW_SUPPLY, eeprom_erase_page(0U));
  TEST_ASSERT_EQUAL(EEPROM_ERR_LOW_SUPPLY, eeprom_write(0U, &value, 1U));
  TEST_ASSERT_EQUAL(0U, fake_iap_operation_count());
  TEST_ASSERT_EQUAL(EEPROM_OK, eeprom_read(0U, &back, 1U));
  TEST_ASSERT_EQUAL(0xFFU, back);
  TEST_ASSERT_EQUAL(0U, fake_iap_command_count(CMD_PROGRAM) + fake_iap_command_count(CMD_ERASE));

  eeprom_clear_low_supply();
  TEST_ASSERT_EQUAL(1U, fake_iap_supply_clear_count());
  TEST_ASSERT_EQUAL(EEPROM_OK, eeprom_write(0U, &value, 1U));

  fake_iap_set_supply_low(true);
  eeprom_init();
  TEST_ASSERT_EQUAL(0x03U, fake_iap_rstcfg());
  TEST_ASSERT_EQUAL(2U, fake_iap_supply_clear_count());
  TEST_ASSERT_EQUAL(EEPROM_OK, eeprom_erase_page(0U));

  /* A drop during a write stops it before the next program. */
  fake_iap_reset();
  fake_iap_set_supply_low_from(3U);
  {
    uint8_t data[3] = {1U, 2U, 3U};
    TEST_ASSERT_EQUAL(EEPROM_ERR_LOW_SUPPLY, eeprom_write(0U, data, 3U));
  }
  TEST_ASSERT_EQUAL(1U, fake_iap_command_count(CMD_PROGRAM));
  TEST_ASSERT_EQUAL(0x01U, fake_iap_memory()[0]);
  TEST_ASSERT_EQUAL(0xFFU, fake_iap_memory()[1]);
  accesses = fake_iap_access_count();
  TEST_ASSERT(accesses > 0U);
}

/* The ISP options can leave ENLVR set; eeprom_init turns it off, keeps P54RST and sets LVDS to 3.0 V. */
static void test_init_sets_detector_without_reset(void) {
  static const uint8_t cases[][2] = {{0x50U, 0x13U}, {0x00U, 0x03U}, {0x42U, 0x03U}, {0xFFU, 0xBFU}, {0x13U, 0x13U}};
  size_t i;
  for (i = 0U; i < sizeof(cases) / sizeof(cases[0]); ++i) {
    fake_iap_reset();
    fake_iap_set_rstcfg(cases[i][0]);
    fake_iap_set_supply_low(true);
    eeprom_init();
    TEST_ASSERT_EQUAL(cases[i][1], fake_iap_rstcfg());
    TEST_ASSERT_EQUAL(0x0101U, fake_iap_rstcfg_writes());
    TEST_ASSERT_EQUAL(1U, fake_iap_supply_clear_count());
    TEST_ASSERT_EQUAL(0U, fake_iap_operation_count());
    TEST_ASSERT_EQUAL(EEPROM_OK, eeprom_erase_page(0U));
  }
}

int main(void) {
  TEST_RUN(test_map_constants);
  TEST_RUN(test_range_errors_touch_nothing);
  TEST_RUN(test_boundaries_in_range);
  TEST_RUN(test_read_write_erase);
  TEST_RUN(test_program_cannot_set_bits);
  TEST_RUN(test_write_stops_at_first_mismatch);
  TEST_RUN(test_cmd_fail);
  TEST_RUN(test_idle_after_every_return);
  TEST_RUN(test_low_supply);
  TEST_RUN(test_init_sets_detector_without_reset);
  return TEST_FINISH();
}
