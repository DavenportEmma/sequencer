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
#include "k_buf.h"
#include "rotary_encoder.h"

#define NOTE_BUFFER_SIZE (CONFIG_MAX_SEQUENCES * CONFIG_MAX_POLYPHONY)

kbuf_handle_t uart_intr_kbuf;

void sq_play_task(void *pvParameters) {
    float TEMPO_PERIOD_MS = 15000/(CONFIG_TEMPO);

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

    /*
        uart_intr_buf is a vector style buffer for recieving midi commands from
        a midi controller
    */
    uint8_t uart_intr_buf[3];
    uart_intr_kbuf = kbuf_init(uart_intr_buf, 3);
    kbuf_reset(uart_intr_kbuf);

    while(1) {
        lastWakeTime = xTaskGetTickCount();
        int8_t encoder_dir = get_encoder_direction();

        scan(kbuf);

        if(!kbuf_empty(kbuf) && kbuf_ready(kbuf)) {
            uint8_t size = kbuf_size(kbuf);

            for(int i = 0; i < size; i++) {
                kbuf_pop(kbuf, &d);
                
                menu(kbuf->buffer[0]);
            }

            kbuf->ready = 0;
        } else if(kbuf_ready(uart_intr_kbuf)) {
            menu(E_ST_NOTE);
        } else if(encoder_dir != 0) {
            /*
                wtf is this? encoder will either be 1 or -1. we need to pass
                unique event codes to the menu state machine depending on a
                clockwise or anti clockwise turn. E_ENCODER_UP and
                E_ENCODER_DOWN are 1 either side of 0xFFFD
            */
            menu(0xFFFD + encoder_dir);
        } else {
            kbuf_reset(kbuf);
        }

        vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(CONFIG_KEY_SCAN_MS));
    }

    kbuf_free(kbuf);
    vTaskDelete(NULL);
}
