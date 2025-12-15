#include "FreeRTOSConfig.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
#include "tasks.h"
#include "midi.h"
#include "setup.h"
#include <string.h>
#include "sequence.h"
#include "stm32f722xx.h"

SemaphoreHandle_t flash_mutex, midi_uart_mutex;
MIDISequence_t sequences[CONFIG_TOTAL_SEQUENCES];
step_t steps[CONFIG_TOTAL_SEQUENCES * CONFIG_STEPS_PER_SEQUENCE];
TaskHandle_t saveTask;

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    __disable_irq();
}

int main(void) {
    flash_mutex = xSemaphoreCreateMutex();
    midi_uart_mutex = xSemaphoreCreateMutex();
    
    memset(sequences, 0, sizeof(sequences));
    memset(steps, 0, sizeof(steps));
    setup(sequences);

    all_channels_off(USART1);
    all_channels_off(USART2);
    all_channels_off(UART4);
    all_channels_off(USART6);

    xTaskCreate(sq_play_task, "sq_play_task", 2048, NULL, 3, NULL);
    xTaskCreate(key_scan_task, "key_scan_task", 2048, NULL, 2, NULL);
    xTaskCreate(save_task, "save task", 512, NULL, 1, &saveTask);
    vTaskStartScheduler();
    while(1){

    }
}