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
#include "semphr.h"
#include "uart.h"
#include "queue.h"

#define NOTE_BUFFER_SIZE (CONFIG_MAX_SEQUENCES * CONFIG_MAX_POLYPHONY)

#define EXT_MIDI_BUFF_SIZE 3
kbuf_handle_t uart_intr_kbuf;

extern SemaphoreHandle_t midi_uart_mutex;
extern QueueHandle_t midiControllerQueue;
extern TimerHandle_t midiStreamMutexTimer;
extern uint8_t midi_uart_mutex_held;

// this is updated in menu.c tempo state
volatile float TEMPO_PERIOD_MS = 15000/(CONFIG_TEMPO);

static void uart_tx_task(void *pvParameters) {
    UARTTaskParams_t* params = (UARTTaskParams_t*)pvParameters;
    
    while(1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        play_notes(params->note_off, params->port);
        play_notes(params->note_on, params->port);
    }
}

void sq_play_task(void *pvParameters) {
    TickType_t lastWakeTime;
    
    uint8_t num_ports = 4;
    UARTTaskParams_t uart_tx_params[num_ports];
    
    MIDIPacket_t note_on_buffer_A[NOTE_BUFFER_SIZE];
    MIDIPacket_t note_off_buffer_A[NOTE_BUFFER_SIZE];
    uart_tx_params[0].port = USART1;
    uart_tx_params[0].note_on = mbuf_init(note_on_buffer_A, NOTE_BUFFER_SIZE);
    uart_tx_params[0].note_off = mbuf_init(note_off_buffer_A, NOTE_BUFFER_SIZE);
    
    MIDIPacket_t note_on_buffer_B[NOTE_BUFFER_SIZE];
    MIDIPacket_t note_off_buffer_B[NOTE_BUFFER_SIZE];
    uart_tx_params[1].port = USART2;
    uart_tx_params[1].note_on = mbuf_init(note_on_buffer_B, NOTE_BUFFER_SIZE);
    uart_tx_params[1].note_off = mbuf_init(note_off_buffer_B, NOTE_BUFFER_SIZE);
    
    MIDIPacket_t note_on_buffer_C[NOTE_BUFFER_SIZE];
    MIDIPacket_t note_off_buffer_C[NOTE_BUFFER_SIZE];
    uart_tx_params[2].port = UART4;
    uart_tx_params[2].note_on = mbuf_init(note_on_buffer_C, NOTE_BUFFER_SIZE);
    uart_tx_params[2].note_off = mbuf_init(note_off_buffer_C, NOTE_BUFFER_SIZE);
    
    
    MIDIPacket_t note_on_buffer_D[NOTE_BUFFER_SIZE];
    MIDIPacket_t note_off_buffer_D[NOTE_BUFFER_SIZE];
    uart_tx_params[3].port = USART6;
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

        xSemaphoreTake(midi_uart_mutex, portMAX_DELAY);

        xTaskNotifyGive(uartTxTask[0]);
        xTaskNotifyGive(uartTxTask[1]);
        xTaskNotifyGive(uartTxTask[2]);
        xTaskNotifyGive(uartTxTask[3]);

        xSemaphoreGive(midi_uart_mutex);

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
    uint8_t uart_intr_buf[EXT_MIDI_BUFF_SIZE];
    uart_intr_kbuf = kbuf_init(uart_intr_buf, EXT_MIDI_BUFF_SIZE);
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

void save_task(void *pvParameters) {
    while(1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        #ifdef CONFIG_DEBUG_PRINT
            send_uart(USART3, "saving data\n\r", 13);
        #endif

        save_data();

        #ifdef CONFIG_DEBUG_PRINT
            send_uart(USART3, "finished saving\n\r", 17);
        #endif    
    }
}

static void resetTimer() {
    /*
        why am I using this instead of xTimerIsTimerActive?
        xTimerReset doesn't synchronously update the timer's state. It just
        posts a command onto the timer command queue which the timer service
        daemon processes whenever it runs next.
        xTimerIsTimerActive() only reflects the timer's actual internal state
        which isn't updated until the daemon task dequeues and processes the
        reset command.
        So the sequence for a fast-arriving MIDI packet can look like this:
        Byte 1 arrives → xTimerIsTimerActive() returns pdFALSE → you take the
        mutex → you call xTimerReset(), which just enqueues a "reset" command
        for the daemon task.
        Byte 2 arrives a few hundred microseconds later, before the timer daemon
        task has actually run and processed that command → xTimerIsTimerActive()
        still returns pdFALSE (the timer hasn't started yet from FreeRTOS's
        point of view) → you take the mutex again → deadlock/blocks forever on
        portMAX_DELAY, or double-take if it's recursive.

        This window depends entirely on the daemon task's priority relative to
        midi_stream_task and how busy it is — which is why it feels
        intermittent/timing-dependent rather than deterministic.
    */
    if(!midi_uart_mutex_held) {
        xSemaphoreTake(midi_uart_mutex, portMAX_DELAY);
        midi_uart_mutex_held = 1;
    }   
    
    xTimerReset(midiStreamMutexTimer, 0);
}


void midi_stream_task(void *pvParameters) {
    char byte;

    while(1) {
        if(xQueueReceive(midiControllerQueue, &byte, portMAX_DELAY) == pdTRUE) {
            USART_TypeDef *port = getActivePort();
            resetTimer();
            send_uart(port, &byte, 1);
        }
    }
}
