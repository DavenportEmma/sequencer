#include "FreeRTOSConfig.h"
#include "FreeRTOS.h"
#include "task.h"
#include "tasks.h"
#include "timers.h"
#include "midi.h"
#include "keyboard.h"
#include "menu.h"
#include "sequence.h"
#include "m_buf.h"
#include "k_buf.h"
#include "rotary_encoder.h"

#define NOTE_BUFFER_SIZE (CONFIG_MAX_SEQUENCES * CONFIG_MAX_POLYPHONY)

kbuf_handle_t uart_intr_kbuf;

extern TaskHandle_t uartTxTask[4];

static void uart_tx_task(void *pvParameters) {
    UARTTaskParams_t* params = (UARTTaskParams_t*)pvParameters;
    
    while(1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        play_notes(params->note_off, params->port);
        play_notes(params->note_on, params->port);
    }
}

void sq_play_task(void *pvParameters) {
    float TEMPO_PERIOD_MS = 15000/(CONFIG_TEMPO);
    TickType_t lastWakeTime;
    
    uint8_t num_ports = 4;
    UARTTaskParams_t uart_tx_params[num_ports];
    
    MIDIPacket_t note_on_buffer_A[NOTE_BUFFER_SIZE];
    MIDIPacket_t note_off_buffer_A[NOTE_BUFFER_SIZE];
    uart_tx_params->port = 0;
    uart_tx_params[0].note_on = mbuf_init(note_on_buffer_A, NOTE_BUFFER_SIZE);
    uart_tx_params[0].note_off = mbuf_init(note_off_buffer_A, NOTE_BUFFER_SIZE);
    
    MIDIPacket_t note_on_buffer_B[NOTE_BUFFER_SIZE];
    MIDIPacket_t note_off_buffer_B[NOTE_BUFFER_SIZE];
    uart_tx_params->port = 1;
    uart_tx_params[1].note_on = mbuf_init(note_on_buffer_B, NOTE_BUFFER_SIZE);
    uart_tx_params[1].note_off = mbuf_init(note_off_buffer_B, NOTE_BUFFER_SIZE);
    
    MIDIPacket_t note_on_buffer_C[NOTE_BUFFER_SIZE];
    MIDIPacket_t note_off_buffer_C[NOTE_BUFFER_SIZE];
    uart_tx_params->port = 2;
    uart_tx_params[2].note_on = mbuf_init(note_on_buffer_C, NOTE_BUFFER_SIZE);
    uart_tx_params[2].note_off = mbuf_init(note_off_buffer_C, NOTE_BUFFER_SIZE);
    
    
    MIDIPacket_t note_on_buffer_D[NOTE_BUFFER_SIZE];
    MIDIPacket_t note_off_buffer_D[NOTE_BUFFER_SIZE];
    uart_tx_params->port = 3;
    uart_tx_params[3].note_on = mbuf_init(note_on_buffer_D, NOTE_BUFFER_SIZE);
    uart_tx_params[3].note_off = mbuf_init(note_off_buffer_D, NOTE_BUFFER_SIZE);

    TaskHandle_t uartTxTask[4];
    xTaskCreate(uart_tx_task, "UARTA_TX", 512, &uart_tx_params[0], 2, &uartTxTask[0]);
    xTaskCreate(uart_tx_task, "UARTB_TX", 512, &uart_tx_params[1], 2, &uartTxTask[1]);
    xTaskCreate(uart_tx_task, "UARTC_TX", 512, &uart_tx_params[2], 2, &uartTxTask[2]);
    xTaskCreate(uart_tx_task, "UARTD_TX", 512, &uart_tx_params[3], 2, &uartTxTask[3]);

    while(1) {
        lastWakeTime = xTaskGetTickCount();

        load_sequences(uart_tx_params, num_ports);

        xTaskNotifyGive(uartTxTask[0]);
        xTaskNotifyGive(uartTxTask[1]);
        xTaskNotifyGive(uartTxTask[2]);
        xTaskNotifyGive(uartTxTask[3]);
        vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(TEMPO_PERIOD_MS));
    }
}

void key_scan_task(void *pvParameters) {
    TickType_t lastWakeTime;

    keyboard_t k;
    kb_handle_t kb = &k;

    kb_reset(kb);
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

        scan(kb);

        if(kb->ready) {
            menu(kb->key, kb->hold);
            kb->ready = 0;
        } else if(kbuf_ready(uart_intr_kbuf)) {
            menu(E_ST_NOTE, E_NO_HOLD);
        } else if(encoder_dir != 0) {
            /*
                wtf is this? encoder will either be 1 or -1. we need to pass
                unique event codes to the menu state machine depending on a
                clockwise or anti clockwise turn. E_ENCODER_UP and
                E_ENCODER_DOWN are 1 either side of 0xFFFD
            */
            menu(0xFFFD + encoder_dir, 0xFFFF);
        } else {
            kb_reset(kb);
        }

        vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(CONFIG_KEY_SCAN_MS));
    }

    vTaskDelete(NULL);
}
