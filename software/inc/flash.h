#ifndef _FLASH_H
#define _FLASH_H

/*
    these functions are wrappers for those found in w25q128jv.c in the platform.
    the wrapping allows for the use of mutexes
*/
void flash_eraseSector(uint32_t addr);
void flash_eraseChip();
void flash_programPage(uint32_t addr, uint8_t* tx, uint8_t* rx, uint16_t len);
void flash_SPIRead(uint32_t addr, uint8_t* tx, uint8_t* rx, uint32_t len);

#endif