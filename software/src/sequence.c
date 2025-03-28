#include "FreeRTOS.h"
#include "semphr.h"
#include "midi.h"
#include "sequence.h"
#include "autoconf.h"
#include "uart.h"
#include "w25q128jv.h"
#include "common.h"
#include "step_edit.h"
#include <string.h>

extern SemaphoreHandle_t sq_mutex;

extern MIDISequence_t sq_states[CONFIG_TOTAL_SEQUENCES];

extern uint8_t ACTIVE_SQ;
extern uint8_t SQ_EDIT_READY;

static MIDIChannel_t get_channel(uint8_t sq_index) {
    uint32_t sq_base_addr = CONFIG_SEQ_ADDR_OFFSET * sq_index;
    uint8_t tx[1] = {0};
    uint8_t rx[1] = {0};
    SPIRead(sq_base_addr, tx, rx, 1);

    return (MIDIChannel_t)rx[0];
}

static void play_step_notes(MIDIChannel_t c, uint8_t* step_buffer, int8_t len) {
    int byte_index = 0;

    // hopefully we never run into the case where NOTE_OFF isn't the first byte
    while(step_buffer[byte_index] != NOTE_OFF) { byte_index++; }

    byte_index++;

    // while after NOTE_OFF and before NOTE_ON send all the note off packet
    while(step_buffer[byte_index] != NOTE_ON) {
        MIDIPacket_t p = {
            .channel = c,
            .status = NOTE_OFF,
            .note = step_buffer[byte_index],
            .velocity = 0,
        };

        send_midi_note(USART1, &p);
        byte_index++;
    }

    byte_index++;

    while(byte_index < len) {
        MIDIPacket_t p = {
            .channel = c,
            .status = NOTE_ON,
            .note = step_buffer[byte_index],
            .velocity = step_buffer[byte_index+1],
        };

        send_midi_note(USART1, &p);
        // every note on packet takes up two bytes in memory (note and velocity)
        byte_index+=2;
    }
}

static void read_step_from_memory(MIDISequence_t* sq, uint8_t sq_index, uint8_t* data, uint16_t len) {
    // get the address for the step pointer in flash memory
    // read MEMORY.md for more info
    uint32_t sq_base_addr = CONFIG_SEQ_ADDR_OFFSET * sq_index;
    uint32_t steps_base_addr = sq_base_addr + 0x1000;
    uint32_t current_step_addr = steps_base_addr + sq->counter;

    SPIRead(current_step_addr, data, data, len);
}

// load step from flash or from the edit buffer
static int load_step(MIDISequence_t* sq, uint8_t sq_index, uint8_t* data) {
    if(ACTIVE_SQ == sq_index && SQ_EDIT_READY == 1) {
        edit_buffer_read(sq->counter, data, BYTES_PER_STEP);
    } else {
        read_step_from_memory(sq, sq_index, data, BYTES_PER_STEP);
    }

    for(int i = 0; i < BYTES_PER_STEP; i++) {
        send_hex(USART3, data[i]);
        send_uart(USART3, "\n\r", 2);
    }
    send_uart(USART3, "\n\r", 2);

    return data[BYTES_PER_STEP-1];
}

static void play_step(uint8_t sq_index) {
    MIDISequence_t* sq = &sq_states[sq_index];
    uint8_t data[BYTES_PER_STEP];

    // if the end of sequence byte is hit then put the counter back to the start
    if(load_step(sq, sq_index, data) == 0xFF) {
        sq->counter = 0;
    } else {
        sq->counter += BYTES_PER_STEP;
    }

    play_step_notes(sq->channel, data, BYTES_PER_STEP);
}

void toggle_sequence(uint8_t seq) {
    if(xSemaphoreTake(sq_mutex, portMAX_DELAY) == pdTRUE) {
        sq_states[seq].enabled ^= 1;

        if(!sq_states[seq].enabled) {
            MIDICC_t p = {
                .status = CONTROLLER,
                .channel = sq_states[seq].channel,
                .control = ALL_NOTES_OFF,
                .value = 0,
            };

            send_midi_control(USART1, &p);
        } else {
            sq_states[seq].channel = get_channel(seq);
        }

        xSemaphoreGive(sq_mutex);
    }
}

void play_sequences() {
    if(xSemaphoreTake(sq_mutex, portMAX_DELAY) == pdTRUE) {
        for(int i = 0; i < CONFIG_TOTAL_SEQUENCES; i++) {
            if(sq_states[i].enabled) {
                play_step(i);
            }
        }
        xSemaphoreGive(sq_mutex);
    }
}
