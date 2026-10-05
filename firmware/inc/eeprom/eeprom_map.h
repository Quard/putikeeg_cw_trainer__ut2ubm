#ifndef EEPROM_EEPROM_MAP_H
#define EEPROM_EEPROM_MAP_H

/* Fixed EEPROM layout. Every user of the EEPROM takes its address from here. */

#define EEPROM_MAP_SETTINGS_PAGE 0U
#define EEPROM_MAP_SETTINGS_ADDRESS 0x0000U
#define EEPROM_MAP_FIRST_FREE_PAGE 1U /* pages 1-7 stay free, for example for statistics */
#define EEPROM_MAP_FREE_ADDRESS 0x0200U
#define EEPROM_MAP_FREE_SIZE 0x0E00U /* 0x0200-0x0FFF */

#endif /* EEPROM_EEPROM_MAP_H */
