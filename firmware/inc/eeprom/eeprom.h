#ifndef EEPROM_EEPROM_H
#define EEPROM_EEPROM_H

/*
 * Data EEPROM through the IAP unit: 4 KB at IAP address 0x0000, 8 pages of 512 bytes. A program only clears bits;
 * an erase sets a page to 0xFF. The CPU stops during each operation (an erase takes 4-6 ms) and interrupts wait.
 * Call from the main loop only; not reentrant. Parameters after the first are in xdata on the target.
 */

#include <stdint.h>

#include "mcu/compiler.h"

typedef uint8_t eeprom_status_t;

#define EEPROM_OK 0U
#define EEPROM_ERR_RANGE 1U      /* address or length outside the EEPROM; nothing done */
#define EEPROM_ERR_FAIL 2U       /* CMD_FAIL was set or the read-back did not match */
#define EEPROM_ERR_LOW_SUPPLY 3U /* the low-voltage flag was set; the command was not started */

#define EEPROM_SIZE 4096U
#define EEPROM_PAGE_SIZE 512U
#define EEPROM_PAGE_COUNT 8U

/** Reads @p length bytes from @p address into @p data. Stops at the first CMD_FAIL. Length 0 gives EEPROM_OK. */
eeprom_status_t eeprom_read(uint16_t address, MEM_XDATA uint8_t* MEM_XDATA data, MEM_XDATA uint16_t length);

/**
 * Programs @p length bytes and reads each one back. Stops at the first low-voltage flag (before the command),
 * CMD_FAIL or read-back mismatch; earlier bytes stay programmed. Never erases.
 */
eeprom_status_t eeprom_write(uint16_t address, const MEM_XDATA uint8_t* MEM_XDATA data, MEM_XDATA uint16_t length);

/** Erases page @p page (0..7) to 0xFF. Does not start when the low-voltage flag is set. */
eeprom_status_t eeprom_erase_page(uint8_t page);

/**
 * Sets the low-voltage detector to 3.0 V, turns the low-voltage reset off, keeps the other RSTCFG bits and clears
 * the low-voltage flag. Call once at start-up.
 */
void eeprom_init(void);

/** Clears the low-voltage flag. Call it right before a group of erase and write calls. */
void eeprom_clear_low_supply(void);

#endif /* EEPROM_EEPROM_H */
