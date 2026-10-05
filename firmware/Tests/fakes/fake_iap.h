#ifndef FAKE_IAP_H
#define FAKE_IAP_H

/*
 * Host model of the IAP unit and the 4 KB data EEPROM: erased bytes are 0xFF, a program is a bitwise AND, an erase
 * sets 512 bytes to 0xFF, an address outside the EEPROM sets CMD_FAIL. Operations are counted from 1 in trigger
 * order (reads, programs and erases). Faults: power loss, CMD_FAIL on one operation, an interrupted erase and a
 * low-voltage flag.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define FAKE_IAP_SIZE 4096U
#define FAKE_IAP_PAGE_SIZE 512U
#define FAKE_IAP_PAGE_COUNT 8U

/** Erases the whole model, clears all counters, faults and registers, and leaves the IAP idle. */
void fake_iap_reset(void);

/** Sets every byte to 0xFF without counting an erase. Keeps the counters. */
void fake_iap_erase_all(void);

/** Clears the counters and faults; keeps the memory. */
void fake_iap_clear_counters(void);

/** Direct access to the 4 KB model. */
uint8_t* fake_iap_memory(void);

size_t fake_iap_operation_count(void);
size_t fake_iap_command_count(uint8_t command);
size_t fake_iap_erase_count(uint8_t page);
size_t fake_iap_program_count(uint16_t address);
/** Returns the number of fake register accesses of any kind, including the low-voltage flag. */
size_t fake_iap_access_count(void);

/** After operation @p count (1 or more), later programs and erases have no effect. 0 disables. */
void fake_iap_set_power_loss_after(size_t count);
/** Returns true when the modelled power loss has happened. */
bool fake_iap_has_lost_power(void);

/** Operation @p number fails with CMD_FAIL and changes nothing. 0 disables. */
void fake_iap_set_fail_operation(size_t number);

/** Operation @p number, an erase, leaves @p pattern (512 bytes) in its page, and then the power is lost. */
void fake_iap_set_interrupted_erase(size_t number, const uint8_t* pattern);

/** Sets or clears the low-voltage flag now. */
void fake_iap_set_supply_low(bool is_low);
/**
 * From operation @p number on, the supply stays low: the flag reads set before operation @p number starts and
 * comes back after each clear. 0 disables.
 */
void fake_iap_set_supply_low_from(size_t number);
size_t fake_iap_supply_clear_count(void);
/** Sets the fake RSTCFG, for example to a reset value that the ISP options give. fake_iap_reset() sets 0. */
void fake_iap_set_rstcfg(uint8_t value);
uint8_t fake_iap_rstcfg(void);
/** Returns the RSTCFG writes since the reset: AND writes in the low byte, OR writes in the high byte. */
uint16_t fake_iap_rstcfg_writes(void);

/** Returns true when the registers hold the idle state: CONTR 0, CMD 0, TRIG 0, ADDRH 0x80, ADDRL 0. */
bool fake_iap_is_idle(void);
/** Returns the IAP_TPS value of the last enable. */
uint8_t fake_iap_tps(void);
/** Returns true when every trigger came with IAPEN set and a TPS write before it. */
bool fake_iap_were_triggers_enabled(void);

/* Register operations behind Tests/fakes/mcu/iap_port.h. */
void fake_iap_enable(uint8_t tps);
void fake_iap_set_command(uint8_t command);
void fake_iap_set_address(uint16_t address);
void fake_iap_write_data(uint8_t value);
uint8_t fake_iap_read_data(void);
void fake_iap_trigger(void);
bool fake_iap_has_failed(void);
void fake_iap_clear_fail(void);
void fake_iap_idle(void);
void fake_iap_rstcfg_and(uint8_t mask);
void fake_iap_rstcfg_or(uint8_t mask);
bool fake_iap_is_supply_low(void);
void fake_iap_clear_supply_low(void);

#endif /* FAKE_IAP_H */
