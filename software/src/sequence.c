#include "FreeRTOS.h"
#include "semphr.h"
#include "midi.h"
#include "sequence.h"
#include "autoconf.h"
#include "uart.h"
#include "w25q128jv.h"
#include "common.h"
#include <string.h>

extern SemaphoreHandle_t sq_mutex;
extern SemaphoreHandle_t edit_buffer_mutex;
extern MIDISequence_t sq_states[CONFIG_TOTAL_SEQUENCES];

static uint8_t step_edit_buffer[BYTES_PER_SEQ];
extern uint8_t ACTIVE_SQ;
extern uint8_t SQ_EDIT_READY;

static MIDIChannel_t get_channel(uint8_t sq_index) {
    uint32_t sq_base_addr = CONFIG_SEQ_ADDR_OFFSET * sq_index;
    uint8_t tx[1] = {0};
    uint8_t rx[1] = {0};
    SPIRead(sq_base_addr, tx, rx, 1);

    return (MIDIChannel_t)rx[0];
}

static void read_step_edit_buffer(uint8_t addr, uint8_t* data, uint16_t num_bytes) {
    for(int i = 0; i < num_bytes; i++) {
        data[i] = step_edit_buffer[i + addr];
    }
}

static void parse_step_buffer(MIDIChannel_t c, uint8_t* step_buffer, int8_t len) {
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

static void load_step_buffer(MIDISequence_t* sq, uint8_t sq_index, uint8_t* step_buffer) {
    if(ACTIVE_SQ == sq_index && SQ_EDIT_READY == 1) {
        if(xSemaphoreTake(edit_buffer_mutex, portMAX_DELAY) == pdTRUE) {
            read_step_edit_buffer(sq->counter, step_buffer, (uint16_t)BYTES_PER_STEP);
            
            xSemaphoreGive(edit_buffer_mutex);
        }
    } else {
        // get the address for the step pointer in flash memory
        // read MEMORY.md for more info
        uint32_t sq_base_addr = CONFIG_SEQ_ADDR_OFFSET * sq_index;
        uint32_t steps_base_addr = sq_base_addr + 0x1000;
        uint32_t current_step_addr = steps_base_addr + sq->counter;
    
        // traverse memory and load the step info until and end of step or end of
        // sequence byte is hit
        uint8_t tx[BYTES_PER_STEP] = {0};
        SPIRead(current_step_addr, tx, step_buffer, (uint16_t)BYTES_PER_STEP);
    }
}

static void play_step(uint8_t sq_index) {
    MIDISequence_t* sq = &sq_states[sq_index];

    // see MEMORY.md
    uint8_t* step_buffer = (uint8_t*)pvPortCalloc(BYTES_PER_STEP, sizeof(uint8_t));

    load_step_buffer(sq, sq_index, step_buffer);

    // if the end of sequence byte is hit then put the counter back to the start
    if(step_buffer[BYTES_PER_STEP-1] == 0xFF) {
        sq->counter = 0;
    } else {
        sq->counter += BYTES_PER_STEP;
    }

    for(int i = 0; i < BYTES_PER_STEP; i++) {
        send_hex(USART3, step_buffer[i]);
        send_uart(USART3, "\n\r", 2);
    }
    send_uart(USART3, "\n\r", 2);

    parse_step_buffer(sq->channel, step_buffer, BYTES_PER_STEP);

    vPortFree(step_buffer);
}

static void load_step_edit_buffer(uint8_t sq_index) {
    uint32_t sq_base_addr = CONFIG_SEQ_ADDR_OFFSET * sq_index;
    uint32_t steps_base_addr = sq_base_addr + 0x1000;

    if(xSemaphoreTake(edit_buffer_mutex, portMAX_DELAY) == pdTRUE) {
        SPIRead(steps_base_addr, step_edit_buffer, step_edit_buffer, (uint16_t)BYTES_PER_SEQ);
        xSemaphoreGive(edit_buffer_mutex);
    }
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

void reset_step_edit_buffer() {
    if(xSemaphoreTake(edit_buffer_mutex, portMAX_DELAY) == pdTRUE) {
        memset(&step_edit_buffer, 0xFF, sizeof(step_edit_buffer));
        
        xSemaphoreGive(edit_buffer_mutex);
    }
}

int load_sq_for_edit(uint8_t seq) {
    load_step_edit_buffer(seq);

    SQ_EDIT_READY = 1;

    return 0;
}