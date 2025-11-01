#include "FreeRTOSConfig.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
#include "tasks.h"
#include "midi.h"
#include "setup.h"
#include <string.h>
#include "sequence.h"

SemaphoreHandle_t flash_mutex;
MIDISequence_t sequences[CONFIG_TOTAL_SEQUENCES];
step_t steps[CONFIG_TOTAL_SEQUENCES * CONFIG_STEPS_PER_SEQUENCE];

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    __disable_irq();
}

int main(void) {
    flash_mutex = xSemaphoreCreateMutex();
    
    memset(sequences, 0, sizeof(sequences));
    memset(steps, 0, sizeof(steps));
    setup(sequences);

    all_channels_off(USART1);

    xTaskCreate(sq_play_task, "sq_play_task", 2048, NULL, 2, NULL);
    xTaskCreate(key_scan_task, "key_scan_task", 2048, NULL, 1, NULL);
    vTaskStartScheduler();
    while(1){

    }
}