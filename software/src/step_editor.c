#include "step_editor.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "sequence.h"
#include "uart.h"
#include "util.h"
#include "midi.h"

extern SemaphoreHandle_t edit_buffer_mutex;
extern step_t edit_buffer[CONFIG_STEPS_PER_SEQUENCE];

extern SemaphoreHandle_t sq_mutex;
extern MIDISequence_t sequences[CONFIG_TOTAL_SEQUENCES];

extern SemaphoreHandle_t st_mask_mutex;

/*
    i'm using the msb of the note to determine the lru note of the fifo
    set the msb to 1 for the current note, and 0 for the next note so the
    sequencer knows which note to overwrite next
*/
static void fifo_push_note_on(step_t* s, int size, uint8_t note, uint8_t vel) {
    note_t* note_on = s->note_on;

    for(int i = 0; i < size; i++) {
        uint8_t n = note_on[i].note;

        if((n & 0x80) == 0) {
            uint8_t next = i+1;
            if(i == size) {
                next = 0;
            }

            if(xSemaphoreTake(edit_buffer_mutex, portMAX_DELAY) == pdTRUE) {
                note_on[i].note = note | 0x80;
                note_on[i].velocity = vel;

                note_on[next].note = note_on[next].note & 0x7F; 

                xSemaphoreGive(edit_buffer_mutex);
            }
        }
    }
}

static void fifo_push_note_off(step_t* s, int size, uint8_t note) {
    uint8_t* note_off = s->note_off;

    for(int i = 0; i < size; i++) {
        if((note_off[i] & 0x80) == 0) {
            uint8_t next = i+1;
            if(i == size) {
                next = 0;
            }

            if(xSemaphoreTake(edit_buffer_mutex, portMAX_DELAY) == pdTRUE) {
                note_off[i] = note | 0x80;

                note_off[next] = note_off[next] & 0x7F; 

                xSemaphoreGive(edit_buffer_mutex);
            }
        }
    }
}

/*
    edit the note of a step in the step edit buffer

    @param active_sq the index of the actively edited sequence
    @param step     the index (0-63) of the step to be edited
    @param status   NOTE_ON or NOTE_OFF
    @param note     the new note value for the step
    @param velocity the new velocity for the note
    @param auto_fill_next_note_off
*/
void edit_step_note(
    uint8_t step,
    MIDIStatus_t status,
    MIDINote_t note,
    uint8_t velocity,
    uint8_t auto_fill_next_note_off
) {
    step_t s, next_s;
    uint8_t next_s_index = step+1 >= CONFIG_STEPS_PER_SEQUENCE ? 0 : step+1;

    if(xSemaphoreTake(edit_buffer_mutex, portMAX_DELAY) == pdTRUE) {
        s = edit_buffer[step];
        next_s = edit_buffer[next_s_index];
        xSemaphoreGive(edit_buffer_mutex);
    }

    switch(status) {
        case NOTE_ON:
            fifo_push_note_on(&s, CONFIG_MAX_POLYPHONY, (uint8_t)note, velocity);

            if(auto_fill_next_note_off) {
                fifo_push_note_off(&next_s, CONFIG_MAX_POLYPHONY, (uint8_t)note);
            }

            break;

        case NOTE_OFF:
            fifo_push_note_off(&s, CONFIG_MAX_POLYPHONY, (uint8_t)note);

            break;

        default:
            break;
    }

    if(xSemaphoreTake(edit_buffer_mutex, portMAX_DELAY) == pdTRUE) {
        edit_buffer[step] = s;

        if(auto_fill_next_note_off) {
            edit_buffer[next_s_index] = next_s;
        }

        xSemaphoreGive(edit_buffer_mutex);
    }
}

void mute_step(uint8_t sequence, uint8_t step) {
    if(xSemaphoreTake(st_mask_mutex, portMAX_DELAY) == pdTRUE) {
        uint32_t* muted_steps = sequences[sequence].muted_steps;
        toggle_bit(muted_steps, step, CONFIG_STEPS_PER_SEQUENCE);
        xSemaphoreGive(st_mask_mutex);
    }
}

void toggle_step(uint8_t sequence, uint8_t step) {
    if(xSemaphoreTake(st_mask_mutex, portMAX_DELAY) == pdTRUE) {
        uint32_t* en_steps = sequences[sequence].enabled_steps;
        toggle_bit(en_steps, step, CONFIG_STEPS_PER_SEQUENCE);
        xSemaphoreGive(st_mask_mutex);
    }
}

void edit_step_velocity(uint8_t step, int8_t velocity_dir) {
    step_t s;
    
    if(xSemaphoreTake(edit_buffer_mutex, portMAX_DELAY) == pdTRUE) {
        s = edit_buffer[step];
        xSemaphoreGive(edit_buffer_mutex);
    }

    uint8_t v = s.note_on[0].velocity;

    if(velocity_dir <= 0) {
        if(v++ > 127) {
            v = 127;
        }
    } else {
        if(v > 0) {
            v--;
        }
    }

    for(uint8_t i = 0; i < CONFIG_MAX_POLYPHONY; i++) {
        s.note_on[i].velocity = v;
    }

    if(xSemaphoreTake(edit_buffer_mutex, portMAX_DELAY) == pdTRUE) {
        edit_buffer[step] = s;
        xSemaphoreGive(edit_buffer_mutex);
    }
}

void clear_step(uint8_t step) {
    step_t s;

    for(uint8_t i = 0; i < CONFIG_MAX_POLYPHONY; i++) {
        /*
            why these values?
            0x01 is outside the standard range of midi note commands so nothing
            should play. load_step_notes() only plays a note if it's within the
            range of valid midi notes

            0x3F is half the standard range of midi velocity commands
        */
        s.note_on[i].note = 0x01;
        s.note_on[i].velocity = 0x3F;
        s.note_off[i] = 0x01;
    }

    if(xSemaphoreTake(edit_buffer_mutex, portMAX_DELAY) == pdTRUE) {
        edit_buffer[step] = s;
        
        xSemaphoreGive(edit_buffer_mutex);
    }
}