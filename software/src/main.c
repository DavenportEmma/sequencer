#include "stm32f722xx.h"
#include "FreeRTOSConfig.h"
#include "FreeRTOS.h"
#include "task.h"
#include "tasks.h"
#include "midi.h"
#include "setup.h"
#include "autoconf.h"

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    __disable_irq();
}

int main(void) {
    setup();

    all_channels_off(USART1);

    // xTaskCreate(sq_play_task, "sq_play_task", 2048, NULL, configMAX_PRIORITIES-1, NULL);
    xTaskCreate(key_scan_task, "key_scan_task", 2048, NULL, 2, NULL);
    vTaskStartScheduler();
    while(1){

    }
}