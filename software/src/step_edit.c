#include "FreeRTOS.h"
#include "semphr.h"
#include "midi.h"
#include "autoconf.h"
#include "w25q128jv.h"
#include "common.h"
#include <string.h>

static uint8_t step_edit_buffer[BYTES_PER_SEQ];
extern SemaphoreHandle_t edit_buffer_mutex;
extern uint8_t SQ_EDIT_READY;
extern uint8_t ACTIVE_ST;

typedef struct {
    MIDINote_t note;
    uint8_t velocity;
} note_t;

typedef struct {
    note_t note_on[CONFIG_MAX_POLYPHONY];
    uint8_t note_off[CONFIG_MAX_POLYPHONY];
} step_t;


static uint16_t get_next_step_addr(uint16_t addr, uint8_t* step_buffer) {
    while(
        !(step_buffer[addr] == STEP_END || step_buffer[addr] == SEQ_END)
    ) { addr++; }

    addr++;

    if(step_buffer[addr] == SEQ_END) { 
        return 0;
    } else {
        return addr;
    }
}

// -------------- buffer methods --------------

void edit_buffer_read(uint16_t addr, uint8_t* data, uint16_t num_bytes) {
    if(xSemaphoreTake(edit_buffer_mutex, portMAX_DELAY) == pdTRUE) {
        for(int i = 0; i < num_bytes; i++) {
            data[i] = step_edit_buffer[i + addr];
        }
        
        xSemaphoreGive(edit_buffer_mutex);
    }
}

void edit_buffer_reset() {
    if(xSemaphoreTake(edit_buffer_mutex, portMAX_DELAY) == pdTRUE) {
        memset(&step_edit_buffer, 0xFF, sizeof(step_edit_buffer));
        
        xSemaphoreGive(edit_buffer_mutex);
    }
}

int edit_buffer_load(uint8_t sq_index) {
    uint32_t sq_base_addr = CONFIG_SEQ_ADDR_OFFSET * sq_index;
    uint32_t steps_base_addr = sq_base_addr + 0x1000;

    if(xSemaphoreTake(edit_buffer_mutex, portMAX_DELAY) == pdTRUE) {
        SPIRead(steps_base_addr, step_edit_buffer, step_edit_buffer, (uint16_t)BYTES_PER_SEQ);
        xSemaphoreGive(edit_buffer_mutex);
    }
    SQ_EDIT_READY = 1;

    return 0;
}

// --------------- step methods ---------------

static void update_step(uint16_t addr, MIDINote_t note) {
    if(xSemaphoreTake(edit_buffer_mutex, portMAX_DELAY) == pdTRUE) {
        while(step_edit_buffer[addr] != NOTE_ON) { addr++; }

        addr++;

        step_edit_buffer[addr] = note;

        addr = get_next_step_addr(addr, step_edit_buffer);

        while(step_edit_buffer[addr] != NOTE_ON) { addr++; }

        addr++;

        step_edit_buffer[addr] = note;

        xSemaphoreGive(edit_buffer_mutex);
    }
}

void edit_step_note(uint8_t step, MIDINote_t note) {
    uint16_t step_addr = step * BYTES_PER_STEP;

    update_step(step_addr, note);

}