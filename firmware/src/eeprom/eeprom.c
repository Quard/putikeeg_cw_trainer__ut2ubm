#include "eeprom/eeprom.h"

#include <stdbool.h>

#include "mcu/iap_port.h"

/*
 * IAP_TPS = system clock in MHz, rounded to the nearest integer (datasheet section 16.1): 24 at 24 MHz.
 * IAPTPS has 6 bits.
 */
#define EEPROM_IAP_TPS ((MCU_CLOCK_HZ + 500000UL) / 1000000UL)
#if (EEPROM_IAP_TPS < 1UL) || (EEPROM_IAP_TPS > 63UL)
#error "MCU_CLOCK_HZ gives an IAP_TPS outside 1..63"
#endif

/* flash.sh gives the ISP the program size; the rest of the 64 KB flash is the EEPROM. */
#if !defined(FLASH_CODE_SIZE)
#error "FLASH_CODE_SIZE must be defined"
#endif
#if (65536UL - FLASH_CODE_SIZE) != EEPROM_SIZE
#error "EEPROM_SIZE does not match 65536 - FLASH_CODE_SIZE"
#endif

#define EEPROM_PAGE_SHIFT 9U

/* The data byte of the current IAP operation and its address. */
static MEM_XDATA uint8_t eeprom_byte;
static MEM_XDATA uint16_t eeprom_address;

static bool eeprom_is_in_range(uint16_t address, uint16_t length) {
  return (length <= EEPROM_SIZE) && (address <= (uint16_t)(EEPROM_SIZE - length));
}

/*
 * Runs IAP command @p command at eeprom_address. A program writes eeprom_byte; a read leaves the result in it.
 * Returns false on CMD_FAIL. The IAP is idle on return.
 */
static bool eeprom_run(uint8_t command) {
  bool has_failed;

  IAP_PORT_ENABLE();
  IAP_PORT_SET_COMMAND(command);
  IAP_PORT_SET_ADDRESS(eeprom_address);
  if (command == IAP_CMD_PROGRAM) {
    IAP_PORT_WRITE_DATA(eeprom_byte);
  }
  IAP_PORT_TRIGGER();
  if (command == IAP_CMD_READ) {
    eeprom_byte = IAP_PORT_READ_DATA();
  }
  has_failed = IAP_PORT_HAS_FAILED();
  IAP_PORT_CLEAR_FAIL();
  IAP_PORT_IDLE();
  return !has_failed;
}

eeprom_status_t eeprom_read(uint16_t address, MEM_XDATA uint8_t* MEM_XDATA data, MEM_XDATA uint16_t length) {
  if (!eeprom_is_in_range(address, length)) {
    return EEPROM_ERR_RANGE;
  }
  eeprom_address = address;
  while (length != 0U) {
    if (!eeprom_run(IAP_CMD_READ)) {
      return EEPROM_ERR_FAIL;
    }
    *data = eeprom_byte;
    ++data;
    ++eeprom_address;
    --length;
  }
  return EEPROM_OK;
}

eeprom_status_t eeprom_write(uint16_t address, const MEM_XDATA uint8_t* MEM_XDATA data, MEM_XDATA uint16_t length) {
  if (!eeprom_is_in_range(address, length)) {
    return EEPROM_ERR_RANGE;
  }
  eeprom_address = address;
  while (length != 0U) {
    if (IAP_PORT_IS_SUPPLY_LOW()) {
      return EEPROM_ERR_LOW_SUPPLY;
    }
    eeprom_byte = *data;
    if (!eeprom_run(IAP_CMD_PROGRAM)) {
      return EEPROM_ERR_FAIL;
    }
    if (!eeprom_run(IAP_CMD_READ) || (eeprom_byte != *data)) {
      return EEPROM_ERR_FAIL;
    }
    ++data;
    ++eeprom_address;
    --length;
  }
  return EEPROM_OK;
}

eeprom_status_t eeprom_erase_page(uint8_t page) {
  if (page >= EEPROM_PAGE_COUNT) {
    return EEPROM_ERR_RANGE;
  }
  if (IAP_PORT_IS_SUPPLY_LOW()) {
    return EEPROM_ERR_LOW_SUPPLY;
  }
  eeprom_address = (uint16_t)((uint16_t)page << EEPROM_PAGE_SHIFT);
  if (!eeprom_run(IAP_CMD_ERASE)) {
    return EEPROM_ERR_FAIL;
  }
  return EEPROM_OK;
}

void eeprom_init(void) {
  IAP_PORT_LVD_INIT();
  IAP_PORT_CLEAR_SUPPLY_LOW();
}

void eeprom_clear_low_supply(void) { IAP_PORT_CLEAR_SUPPLY_LOW(); }
