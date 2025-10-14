/*
    this file contains all the functions that are used to manipulate the step
    edit buffer on a buffer-wide scale. any individual step operations are
    found in step_editor.c
*/

#include "FreeRTOS.h"
#include "semphr.h"
#include "midi.h"
#include "step_edit_buffer.h"
#include "common.h"
#include "flash.h"
#include "sequence.h"
#include <string.h>

step_t edit_buffer[CONFIG_STEPS_PER_SEQUENCE];
extern MIDISequence_t sequences[CONFIG_TOTAL_SEQUENCES];
extern SemaphoreHandle_t sq_mutex;
extern SemaphoreHandle_t edit_buffer_mutex;
extern uint8_t ACTIVE_SQ;
extern uint8_t ACTIVE_ST;
extern uint8_t SQ_EDIT_READY;

/*
    convert a step_t struct to a buffer of bytes. see MEMORY.md for more info
    about the structure of steps in a buffer

    @param st       step struct to be stringified
    @param pos      the location in the buffer the data is to be entered at
    @param data     the byte buffer into which the stringified step is entered
*/
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

/*
    convert the step edit buffer to a buffer of bytes. the step edit buffer is
    an array of step_t structs

    @param data     the buffer to be filled with stringified steps
*/
static void stringify_buffer(uint8_t* data) {
    uint16_t pos = 0;
    for(uint16_t i = 0; i < CONFIG_STEPS_PER_SEQUENCE; i++) {
        stringify_step(edit_buffer[i], &pos, data);
    }
}

/*
    stringify the step eit buffer and write the byte buffer to flash memory. use
    ACTIVE_SQ to get the address to write to
*/
static void write_buffer_to_memory() {
    uint8_t ebuf_data[BYTES_PER_SEQ];

    stringify_buffer(ebuf_data);

    MIDISequence_t* sq = &sequences[ACTIVE_SQ];

    uint32_t sq_base_addr = CONFIG_SEQ_ADDR_OFFSET * ACTIVE_SQ;
    uint32_t steps_addr = sq_base_addr + sq->step_sector_offset;

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
        flash_programPage(steps_addr, tx, tx, len);

        steps_addr+=256;
        remaining_bytes-=256;
    }
}

/*
    write the step edit buffer to memory and reset the step edit buffer to all
    0xFF
*/
void edit_buffer_reset() {
    if(xSemaphoreTake(edit_buffer_mutex, portMAX_DELAY) == pdTRUE) {
        write_buffer_to_memory();

        memset(&edit_buffer, 0xFF, sizeof(edit_buffer));
        
        xSemaphoreGive(edit_buffer_mutex);
    }
}

/*
    read the active sequence from memory and store the contents in the step edit
    buffer

    @param sq_index     the index (0-63) of the active sequence

    @return 1 if the data is malformed
*/
int edit_buffer_load(uint8_t sq_index) {
    uint32_t sq_base_addr = CONFIG_SEQ_ADDR_OFFSET * sq_index;
    
    MIDISequence_t* sq;
    if(xSemaphoreTake(sq_mutex, portMAX_DELAY) == pdTRUE) {
        sq = &sequences[sq_index];
        xSemaphoreGive(sq_mutex);
    }
    
    if(sq->step_sector_offset == 0) {
        sq->step_sector_offset = get_step_data_offset(sq_index);
    }
    
    uint32_t steps_base_addr = sq_base_addr + sq->step_sector_offset;
    
    uint8_t seq_data[BYTES_PER_SEQ];
    flash_SPIRead(steps_base_addr, seq_data, seq_data, (uint16_t)BYTES_PER_SEQ);
    
    step_t steps[CONFIG_STEPS_PER_SEQUENCE];

    for(int i = 0; i < CONFIG_STEPS_PER_SEQUENCE; i++) {
        uint8_t st_data[BYTES_PER_STEP];
        
        memcpy(st_data, &seq_data[BYTES_PER_STEP * i], BYTES_PER_STEP);
        
        if(st_data[0] != NOTE_OFF) {
            return 1;
        }
        
        step_t st;
        if(bytes_to_step(st_data, &st)) {
            return 1;
        }
        
        steps[i] = st;

        if(st.end_of_step == 0xFF) {
            break;
        }
    }

    if(xSemaphoreTake(edit_buffer_mutex, portMAX_DELAY) == pdTRUE) {
        memcpy(edit_buffer, &steps[0], BYTES_PER_SEQ);
        xSemaphoreGive(edit_buffer_mutex);
    }

    if(xSemaphoreTake(sq_mutex, portMAX_DELAY) == pdTRUE) {
        if(sq->step_sector_offset >= 0xF000) {
            sq->step_sector_offset = 0x1000;
        } else {
            sq->step_sector_offset += 0x1000;
        }
        xSemaphoreGive(sq_mutex);
    }
    // the sector must be erased so that we can write the updated sequence
    flash_eraseSector(steps_base_addr);

    return 0;
}

void edit_buffer_clear() {
    note_t note_template[CONFIG_MAX_POLYPHONY];
    for (int i = 0; i < CONFIG_MAX_POLYPHONY; i++) {
        note_template[i].note = 0x01;
        note_template[i].velocity = 0x3F;
    }

    step_t st;
    st.end_of_step = 0x00;
    memset(st.note_off, 0x01, sizeof(st.note_off));
    memcpy(st.note_on, note_template, sizeof(note_template));

    if(xSemaphoreTake(edit_buffer_mutex, portMAX_DELAY) == pdTRUE) {

        for (int i = 0; i < CONFIG_STEPS_PER_SEQUENCE; i++) {
            edit_buffer[i] = st;
        }

        edit_buffer[CONFIG_STEPS_PER_SEQUENCE-1].end_of_step = 0xFF;
        xSemaphoreGive(edit_buffer_mutex);
    }
}