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
#include "m_buf.h"

#define NOTE_BUFFER_SIZE (CONFIG_MAX_SEQUENCES * CONFIG_MAX_POLYPHONY)

void sq_play_task(void *pvParameters) {
    float TEMPO_PERIOD_MS = 60000/(CONFIG_TEMPO);

    TickType_t lastWakeTime;
    
    MIDIPacket_t note_on_buffer[NOTE_BUFFER_SIZE];
    mbuf_handle_t note_on_mbuf = mbuf_init(note_on_buffer, NOTE_BUFFER_SIZE);

    MIDIPacket_t note_off_buffer[NOTE_BUFFER_SIZE];
    mbuf_handle_t note_off_mbuf = mbuf_init(note_off_buffer, NOTE_BUFFER_SIZE);

    while(1) {
        lastWakeTime = xTaskGetTickCount();

        load_sequences(note_on_mbuf, note_off_mbuf);

        play_notes(note_off_mbuf);
        play_notes(note_on_mbuf);
        vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(TEMPO_PERIOD_MS));
    }
}

void key_scan_task(void *pvParameters) {
    TickType_t lastWakeTime;

    uint8_t buffer[CONFIG_ROLLOVER];
    kbuf_handle_t kbuf = kbuf_init(buffer, CONFIG_ROLLOVER);
    kbuf_reset(kbuf);

    uint8_t d;

    while(1) {
        lastWakeTime = xTaskGetTickCount();
        scan(kbuf);

        if(!kbuf_empty(kbuf) && kbuf_ready(kbuf)) {
            uint8_t size = kbuf_size(kbuf);

            for(int i = 0; i < size; i++) {
                kbuf_pop(kbuf, &d);
                
                menu(kbuf->buffer[0]);
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
