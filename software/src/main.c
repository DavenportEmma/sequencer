#include "stm32f722xx.h"
#include "FreeRTOSConfig.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
#include "tasks.h"
#include "midi.h"
#include "setup.h"
#include "autoconf.h"
#include <string.h>

SemaphoreHandle_t sq_mutex;
MIDISequence_t sq_states[CONFIG_TOTAL_SEQUENCES];

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    __disable_irq();
}

int main(void) {
    setup();

    sq_mutex = xSemaphoreCreateMutex();

    memset(sq_states, 0, sizeof(sq_states));

    all_channels_off(USART1);

    xTaskCreate(sq_play_task, "sq_play_task", 2048, NULL, 2, NULL);
    xTaskCreate(key_scan_task, "key_scan_task", 2048, NULL, 1, NULL);
    vTaskStartScheduler();
    while(1){

    }
}