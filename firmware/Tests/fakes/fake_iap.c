/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "fake_iap.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FAKE_IAP_CONTR_IAPEN 0x80U
#define FAKE_IAP_CONTR_CMD_FAIL 0x10U
#define FAKE_IAP_COMMAND_LIMIT 4U
#define IAP_CMD_READ_VALUE 1U
#define IAP_CMD_PROGRAM_VALUE 2U
#define IAP_CMD_ERASE_VALUE 3U

static uint8_t fake_memory[FAKE_IAP_SIZE];
static size_t fake_erase_counts[FAKE_IAP_PAGE_COUNT];
static size_t fake_program_counts[FAKE_IAP_SIZE];
static size_t fake_command_counts[FAKE_IAP_COMMAND_LIMIT];
static size_t fake_operations;
static size_t fake_accesses;

static size_t fake_power_loss_after;
static bool fake_is_power_lost;
static size_t fake_fail_operation;
static size_t fake_interrupted_erase;
static uint8_t fake_erase_pattern[FAKE_IAP_PAGE_SIZE];
static bool fake_is_supply_flag_set;
static size_t fake_supply_low_from;
static size_t fake_supply_clears;
static uint8_t fake_rstcfg;
static uint8_t fake_rstcfg_and_count;
static uint8_t fake_rstcfg_or_count;

static uint8_t fake_contr;
static uint8_t fake_command;
static uint8_t fake_trig;
static uint16_t fake_address;
static uint8_t fake_data;
static uint8_t fake_tps;
static bool fake_has_tps_since_idle;
static bool fake_were_triggers_enabled;

void fake_iap_erase_all(void) { memset(fake_memory, 0xFF, sizeof(fake_memory)); }

void fake_iap_clear_counters(void) {
  memset(fake_erase_counts, 0, sizeof(fake_erase_counts));
  memset(fake_program_counts, 0, sizeof(fake_program_counts));
  memset(fake_command_counts, 0, sizeof(fake_command_counts));
  fake_operations = 0U;
  fake_accesses = 0U;
  fake_power_loss_after = 0U;
  fake_is_power_lost = false;
  fake_fail_operation = 0U;
  fake_interrupted_erase = 0U;
  fake_is_supply_flag_set = false;
  fake_supply_low_from = 0U;
  fake_supply_clears = 0U;
  fake_were_triggers_enabled = true;
}

void fake_iap_reset(void) {
  fake_iap_erase_all();
  fake_iap_clear_counters();
  fake_rstcfg = 0U;
  fake_rstcfg_and_count = 0U;
  fake_rstcfg_or_count = 0U;
  fake_tps = 0U;
  fake_data = 0U;
  fake_iap_idle();
  fake_accesses = 0U;
}

uint8_t* fake_iap_memory(void) { return fake_memory; }

size_t fake_iap_operation_count(void) { return fake_operations; }

size_t fake_iap_command_count(uint8_t command) {
  return (command < FAKE_IAP_COMMAND_LIMIT) ? fake_command_counts[command] : 0U;
}

size_t fake_iap_erase_count(uint8_t page) { return (page < FAKE_IAP_PAGE_COUNT) ? fake_erase_counts[page] : 0U; }

size_t fake_iap_program_count(uint16_t address) {
  return (address < FAKE_IAP_SIZE) ? fake_program_counts[address] : 0U;
}

size_t fake_iap_access_count(void) { return fake_accesses; }

void fake_iap_set_power_loss_after(size_t count) {
  fake_power_loss_after = count;
  fake_is_power_lost = false;
}

bool fake_iap_has_lost_power(void) { return fake_is_power_lost; }

void fake_iap_set_fail_operation(size_t number) { fake_fail_operation = number; }

void fake_iap_set_interrupted_erase(size_t number, const uint8_t* pattern) {
  fake_interrupted_erase = number;
  memcpy(fake_erase_pattern, pattern, sizeof(fake_erase_pattern));
}

void fake_iap_set_supply_low(bool is_low) { fake_is_supply_flag_set = is_low; }

void fake_iap_set_supply_low_from(size_t number) { fake_supply_low_from = number; }

size_t fake_iap_supply_clear_count(void) { return fake_supply_clears; }

void fake_iap_set_rstcfg(uint8_t value) { fake_rstcfg = value; }

uint8_t fake_iap_rstcfg(void) { return fake_rstcfg; }

uint16_t fake_iap_rstcfg_writes(void) {
  return (uint16_t)(((uint16_t)fake_rstcfg_or_count << 8) | fake_rstcfg_and_count);
}

bool fake_iap_is_idle(void) {
  return (fake_contr == 0U) && (fake_command == 0U) && (fake_trig == 0U) && (fake_address == 0x8000U);
}

uint8_t fake_iap_tps(void) { return fake_tps; }

bool fake_iap_were_triggers_enabled(void) { return fake_were_triggers_enabled; }

void fake_iap_enable(uint8_t tps) {
  ++fake_accesses;
  fake_contr |= FAKE_IAP_CONTR_IAPEN;
  fake_tps = tps;
  fake_has_tps_since_idle = true;
}

void fake_iap_set_command(uint8_t command) {
  ++fake_accesses;
  fake_command = command;
}

void fake_iap_set_address(uint16_t address) {
  ++fake_accesses;
  fake_address = address;
}

void fake_iap_write_data(uint8_t value) {
  ++fake_accesses;
  fake_data = value;
}

uint8_t fake_iap_read_data(void) {
  ++fake_accesses;
  return fake_data;
}

static void fake_lose_power_if_due(void) {
  if ((fake_power_loss_after != 0U) && (fake_operations >= fake_power_loss_after)) {
    fake_is_power_lost = true;
  }
}

static void fake_execute(uint8_t command, uint16_t address) {
  const uint16_t page_start = (uint16_t)(address & ~(FAKE_IAP_PAGE_SIZE - 1U));

  if (command == IAP_CMD_READ_VALUE) {
    fake_data = fake_memory[address];
    return;
  }
  if (fake_is_power_lost) {
    return;
  }
  if (command == IAP_CMD_PROGRAM_VALUE) {
    fake_memory[address] &= fake_data;
    ++fake_program_counts[address];
  } else if ((command == IAP_CMD_ERASE_VALUE) && (fake_operations == fake_interrupted_erase)) {
    memcpy(&fake_memory[page_start], fake_erase_pattern, FAKE_IAP_PAGE_SIZE);
    ++fake_erase_counts[address / FAKE_IAP_PAGE_SIZE];
    fake_is_power_lost = true;
  } else if (command == IAP_CMD_ERASE_VALUE) {
    memset(&fake_memory[page_start], 0xFF, FAKE_IAP_PAGE_SIZE);
    ++fake_erase_counts[address / FAKE_IAP_PAGE_SIZE];
  }
}

void fake_iap_trigger(void) {
  ++fake_accesses;
  fake_trig = 0xA5U;
  if (((fake_contr & FAKE_IAP_CONTR_IAPEN) == 0U) || !fake_has_tps_since_idle) {
    fake_were_triggers_enabled = false;
  }
  if ((fake_command == 0U) || (fake_command >= FAKE_IAP_COMMAND_LIMIT)) {
    fprintf(stderr, "fake_iap: trigger with command %u\n", fake_command);
    abort();
  }
  ++fake_operations;
  ++fake_command_counts[fake_command];
  if ((fake_address >= FAKE_IAP_SIZE) || (fake_operations == fake_fail_operation)) {
    fake_contr |= FAKE_IAP_CONTR_CMD_FAIL;
  } else {
    fake_execute(fake_command, fake_address);
  }
  fake_lose_power_if_due();
}

bool fake_iap_has_failed(void) {
  ++fake_accesses;
  return (fake_contr & FAKE_IAP_CONTR_CMD_FAIL) != 0U;
}

void fake_iap_clear_fail(void) {
  ++fake_accesses;
  fake_contr &= (uint8_t)~FAKE_IAP_CONTR_CMD_FAIL;
}

void fake_iap_idle(void) {
  ++fake_accesses;
  fake_contr = 0U;
  fake_command = 0U;
  fake_trig = 0U;
  fake_address = 0x8000U;
  fake_has_tps_since_idle = false;
}

void fake_iap_rstcfg_and(uint8_t mask) {
  ++fake_accesses;
  ++fake_rstcfg_and_count;
  /* The OR must come after the AND, as on the target. */
  if (fake_rstcfg_or_count != 0U) {
    fake_rstcfg_or_count = 0xFFU;
  }
  fake_rstcfg &= mask;
}

void fake_iap_rstcfg_or(uint8_t mask) {
  ++fake_accesses;
  if (fake_rstcfg_or_count != 0xFFU) {
    ++fake_rstcfg_or_count;
  }
  fake_rstcfg |= mask;
}

bool fake_iap_is_supply_low(void) {
  ++fake_accesses;
  if ((fake_supply_low_from != 0U) && ((fake_operations + 1U) >= fake_supply_low_from)) {
    fake_is_supply_flag_set = true;
  }
  return fake_is_supply_flag_set;
}

void fake_iap_clear_supply_low(void) {
  ++fake_accesses;
  ++fake_supply_clears;
  fake_is_supply_flag_set = false;
}
