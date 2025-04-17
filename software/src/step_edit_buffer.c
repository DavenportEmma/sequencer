#include "FreeRTOS.h"
#include "semphr.h"
#include "autoconf.h"
#include "midi.h"
#include "step_edit_buffer.h"
#include "common.h"
#include "w25q128jv.h"
#include "sequence.h"
#include <string.h>

step_t edit_buffer[CONFIG_STEPS_PER_SEQUENCE];

extern SemaphoreHandle_t edit_buffer_mutex;
extern uint8_t ACTIVE_SQ;
extern uint8_t ACTIVE_ST;
extern uint8_t SQ_EDIT_READY;

static void stringify_step(step_t st, uint16_t* pos, uint8_t* data) {
    data[(*pos)++] = NOTE_OFF;
    
    for(int i = 0; i < CONFIG_MAX_POLYPHONY; i++) {
        data[(*pos)++] = st.note_off[i];
    }
    
    data[(*pos)++] = NOTE_ON;

    for(int i = 0; i < CONFIG_MAX_POLYPHONY; i++) {
        data[(*pos)++] = st.note_on[i].note;
        data[(*pos)++] = st.note_on[i].velocity;
    }

    data[(*pos)++] = st.end_of_step;
}

static void stringify_buffer(uint8_t* data) {
    uint16_t pos = 0;
    for(uint16_t i = 0; i < CONFIG_STEPS_PER_SEQUENCE; i++) {
        stringify_step(edit_buffer[i], &pos, data);
    }
}

static void write_buffer_to_memory() {
    uint8_t ebuf_data[BYTES_PER_SEQ];

    stringify_buffer(ebuf_data);

    uint32_t sq_base_addr = CONFIG_SEQ_ADDR_OFFSET * ACTIVE_SQ;
    uint32_t steps_addr = sq_base_addr + 0x1000;

    /*
        If an entire 256 byte page is to be programmed, the last address byte
        (the 8 least significant address bits) should be set to 0. If the last
        address byte is not zero, and the number of clocks exceeds the remaining
        page length, the addressing will wrap to the beginning of the page
    */
    int16_t remaining_bytes = BYTES_PER_SEQ;

    while(remaining_bytes > 0) {
        uint16_t len;

        if(remaining_bytes >= 256) {
            len = 256;
        } else {
            len = remaining_bytes;
        }

        uint8_t tx[256] = {0xFF};
        uint16_t buf_index = BYTES_PER_SEQ - remaining_bytes;
        memcpy(tx, &ebuf_data[buf_index], len);
        programPage(steps_addr, tx, tx, len);

        steps_addr+=256;
        remaining_bytes-=256;
    }
}

void edit_buffer_reset() {
    if(xSemaphoreTake(edit_buffer_mutex, portMAX_DELAY) == pdTRUE) {
        write_buffer_to_memory();

        memset(&edit_buffer, 0xFF, sizeof(edit_buffer));
        
        xSemaphoreGive(edit_buffer_mutex);
    }
}

int edit_buffer_load(uint8_t sq_index) {
    uint32_t sq_base_addr = CONFIG_SEQ_ADDR_OFFSET * sq_index;
    uint32_t steps_base_addr = sq_base_addr + 0x1000;

    if(xSemaphoreTake(edit_buffer_mutex, portMAX_DELAY) == pdTRUE) {
        uint8_t seq_data[BYTES_PER_SEQ];
        SPIRead(steps_base_addr, seq_data, seq_data, (uint16_t)BYTES_PER_SEQ);

        for(int i = 0; i < CONFIG_STEPS_PER_SEQUENCE; i++) {
            uint8_t st_data[BYTES_PER_STEP];
            
            memcpy(st_data, &seq_data[BYTES_PER_STEP * i], BYTES_PER_STEP);

            if(
                st_data[0] != NOTE_OFF &&
                !(st_data[BYTES_PER_STEP - 1] == SEQ_END ||
                st_data[BYTES_PER_STEP - 1] == STEP_END)
            ) {
                return 1;
            }

            step_t st;
            bytes_to_step(st_data, &st);
            
            edit_buffer[i] = st;

            if(st.end_of_step == 0xFF) {
                break;
            }
        }

        xSemaphoreGive(edit_buffer_mutex);
    }

    // the sector must be erased so that we can write the updated sequence
    eraseSector(steps_base_addr);

    return 0;
}
