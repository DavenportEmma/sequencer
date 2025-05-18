#include "step_editor.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "sequence.h"
#include "uart.h"
#include "util.h"

extern SemaphoreHandle_t edit_buffer_mutex;
extern step_t edit_buffer[CONFIG_STEPS_PER_SEQUENCE];

extern SemaphoreHandle_t sq_mutex;
extern MIDISequence_t sequences[CONFIG_TOTAL_SEQUENCES];

extern SemaphoreHandle_t st_mask_mutex;

/*
    edit the note of a step in the step edit buffer

    @param step     the index (0-63) of the step to be edited
    @param note     the new note value for the step
*/
void edit_step_note(uint8_t step, MIDINote_t note) {
    if(xSemaphoreTake(edit_buffer_mutex, portMAX_DELAY) == pdTRUE) {
        step_t* st = &edit_buffer[step];
        // TODO this won't do for polyphonic sequences
        st->note_on[0].note = note;

        if(st->end_of_step == 0xFF) {
            step = 0;
        } else {
            step++;
        }

        st = &edit_buffer[step];
        // TODO this won't do for polyphonic sequences
        st->note_off[0] = note;

        xSemaphoreGive(edit_buffer_mutex);
    }
}

MIDIStatus_t edit_step_note_midi(uint8_t step, uint8_t* buf) {
    MIDIStatus_t status = buf[0] & 0xF0;
    MIDINote_t note = buf[1];
    uint8_t velocity = buf[2];

    note_t n = {
        .note = note,
        .velocity = velocity
    };

    switch(status) {
        case NOTE_OFF:
            // TODO this won't do for polyphonic sequences
            edit_buffer[step].note_off[0] = note;
            return NOTE_OFF;

        case NOTE_ON:
            // TODO this won't do for polyphonic sequences
            edit_buffer[step].note_on[0] = n;
            return NOTE_ON;

        default:
            return 0;
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