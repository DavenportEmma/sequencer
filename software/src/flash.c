#include "w25q128jv.h"
#include "FreeRTOS.h"
#include "semphr.h"

extern SemaphoreHandle_t flash_mutex;

void flash_eraseSector(uint32_t addr) {
    if(xSemaphoreTake(flash_mutex, portMAX_DELAY) == pdTRUE) {
        eraseSector(addr);
        xSemaphoreGive(flash_mutex);
    }
}

void flash_programPage(uint32_t addr, uint8_t* tx, uint8_t* rx, uint16_t len) {
    if(xSemaphoreTake(flash_mutex, portMAX_DELAY) == pdTRUE) {
        programPage(addr, tx, rx, len);
        xSemaphoreGive(flash_mutex);
    }
}

void flash_SPIRead(uint32_t addr, uint8_t* tx, uint8_t* rx, uint32_t len) {
    if(xSemaphoreTake(flash_mutex, portMAX_DELAY) == pdTRUE) {
        SPIRead(addr, tx, rx, len);
        xSemaphoreGive(flash_mutex);
    }
}
