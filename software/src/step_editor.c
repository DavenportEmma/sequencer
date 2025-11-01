#include "step_editor.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "sequence.h"
#include "uart.h"
#include "util.h"
#include "midi.h"

extern step_t steps[CONFIG_TOTAL_SEQUENCES * CONFIG_STEPS_PER_SEQUENCE];

extern MIDISequence_t sequences[CONFIG_TOTAL_SEQUENCES];

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

            note_on[i].note = note | 0x80;
            note_on[i].velocity = vel;

            note_on[next].note = note_on[next].note & 0x7F; 
            return;
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

            note_off[i] = note | 0x80;

            note_off[next] = note_off[next] & 0x7F; 
            return;
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
    uint8_t sq,
    uint8_t step,
    MIDIStatus_t status,
    MIDINote_t note,
    uint8_t velocity,
    uint8_t auto_fill_next_note_off
) {
    step_t s, next_s;
    uint16_t seq_base_index = ((uint16_t)sq * CONFIG_STEPS_PER_SEQUENCE);
    uint16_t index = seq_base_index + (uint16_t)step;
    uint16_t end_of_seq_index = seq_base_index + CONFIG_STEPS_PER_SEQUENCE + 64;
    uint16_t next_s_index = index+1 >= end_of_seq_index ? 0 : index+1;

    s = steps[index];
    next_s = steps[next_s_index];

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

    steps[index] = s;

    if(auto_fill_next_note_off) {
        steps[next_s_index] = next_s;
    }

}

void mute_step(uint8_t sequence, uint8_t step) {
    uint32_t* muted_steps = sequences[sequence].muted_steps;
    toggle_bit(muted_steps, step, CONFIG_STEPS_PER_SEQUENCE);
}

void toggle_step(uint8_t sequence, uint8_t step) {
    uint32_t* en_steps = sequences[sequence].enabled_steps;
    toggle_bit(en_steps, step, CONFIG_STEPS_PER_SEQUENCE);
}

void edit_step_velocity(uint8_t sq, uint8_t step, int8_t velocity_dir) {
    step_t s;
    uint16_t index = ((uint16_t)sq * CONFIG_STEPS_PER_SEQUENCE) + (uint16_t)step;

    s = steps[index];

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

    steps[index] = s;
}

void clear_step(uint8_t sq, uint8_t step) {
    step_t s;
    uint16_t index = ((uint16_t)sq * CONFIG_STEPS_PER_SEQUENCE) + (uint16_t)step;

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

    steps[index] = s;
}