#include "FreeRTOSConfig.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
#include "tasks.h"
#include "midi.h"
#include "setup.h"
#include <string.h>

SemaphoreHandle_t sq_mutex;
SemaphoreHandle_t edit_buffer_mutex;
SemaphoreHandle_t flash_mutex;
SemaphoreHandle_t st_mask_mutex;
MIDISequence_t sequences[CONFIG_TOTAL_SEQUENCES];

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    __disable_irq();
}

int main(void) {
    edit_buffer_mutex = xSemaphoreCreateMutex();
    sq_mutex = xSemaphoreCreateMutex();
    flash_mutex = xSemaphoreCreateMutex();
    st_mask_mutex = xSemaphoreCreateMutex();
    
    memset(sequences, 0, sizeof(sequences));
    
    setup(sequences);

    all_channels_off(USART1);

    xTaskCreate(sq_play_task, "sq_play_task", 2048, NULL, 2, NULL);
    xTaskCreate(key_scan_task, "key_scan_task", 2048, NULL, 1, NULL);
    vTaskStartScheduler();
    while(1){

    }
}