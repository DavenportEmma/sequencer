#include "stm32f722xx.h"
#include "FreeRTOSConfig.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
#include "tasks.h"
#include "midi.h"
#include "setup.h"
#include "autoconf.h"

SemaphoreHandle_t sq_mutex;

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    __disable_irq();
}

int main(void) {
    setup();

    sq_mutex = xSemaphoreCreateMutex();

    all_channels_off(USART1);

    xTaskCreate(sq_play_task, "sq_play_task", 2048, NULL, 2, NULL);
    xTaskCreate(key_scan_task, "key_scan_task", 2048, NULL, 1, NULL);
    vTaskStartScheduler();
    while(1){

    }
}