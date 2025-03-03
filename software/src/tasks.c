#include "FreeRTOSConfig.h"
#include "FreeRTOS.h"
#include "task.h"
#include "tasks.h"
#include "timers.h"
#include "autoconf.h"
#include "midi.h"
#include "keyboard.h"
#include "menu.h"
#include "sequence.h"

void sq_play_task(void *pvParameters) {
    float TEMPO_PERIOD_MS = 60000/(CONFIG_TEMPO);

    TickType_t lastWakeTime;

    while(1) {
        lastWakeTime = xTaskGetTickCount();



        vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(TEMPO_PERIOD_MS));
    }
}

void key_scan_task(void *pvParameters) {
    TickType_t lastWakeTime;

    uint8_t buffer[CONFIG_ROLLOVER];
    kbuf_handle_t kbuf = kbuf_init(buffer, CONFIG_ROLLOVER);
    kbuf_reset(kbuf);

    int err;
    uint8_t d;

    while(1) {
        lastWakeTime = xTaskGetTickCount();
        scan(kbuf);

        if(!kbuf_empty(kbuf) && kbuf_ready(kbuf)) {
            uint8_t size = kbuf_size(kbuf);

            for(int i = 0; i < size; i++) {
                kbuf_pop(kbuf, &d);
                
                menu(kbuf->buffer[0]);
                #ifdef CONFIG_DEBUG
                    send_hex(USART3, d);
                    send_uart(USART3, " ", 1);
                #endif

            }

            kbuf->ready = 0;
        } else {
            kbuf_reset(kbuf);
        }

        vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(CONFIG_KEY_SCAN_MS));
    }

    kbuf_free(kbuf);
    vTaskDelete(NULL);
}
