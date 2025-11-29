#include "step_editor.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "sequence.h"
#include "uart.h"
#include "util.h"
#include "midi.h"
#include <string.h>

extern step_t steps[CONFIG_TOTAL_SEQUENCES * CONFIG_STEPS_PER_SEQUENCE];

extern MIDISequence_t sequences[CONFIG_TOTAL_SEQUENCES];

/*
    wots all this then
    the note matrix is used to speed up the clear step function. when a note is
    cleared, we need to delete the corresponding note_off information in the
    following steps. failing to do so will result in a bunch of note_off packets
    being sent out the midi port that never had a corresponding note_on. this
    isn't a huge deal tbh, it just may increase latency

    this is a look up table where the note value is the key and the key-value is
    whether or not this note is being played in the step we are clearing

    to keep memory use down there's no point in having a look up table for
    values 0 - C8, instead go from A0 - C8

    during the step clear function we set 
*/

#define NUM_VALID_NOTES (C8 - A0)

static uint8_t note_matrix[NUM_VALID_NOTES];

/*
    i'm using the msb of the note to determine the lru note of the fifo
    set the msb to 1 for the current note, and 0 for the next note so the
    sequencer knows which note to overwrite next
*/
// TODO if we decide to do away with the circular buffer we should drop the
// msb masking thing
static void fifo_push_note_on(step_t* s, int size, uint8_t note, uint8_t vel) {
    note_t* note_on = s->note_on;

    for(int i = 0; i < size; i++) {
        uint8_t n = note_on[i].note;

        if((n & 0x80) == 0) {
            // uint8_t next = i+1;
            // if(i == size) {
            //     next = 0;
            // }

            note_on[i].note = note | 0x80;
            note_on[i].velocity = vel;

            // note_on[next].note = note_on[next].note & 0x7F; 
            return;
        }
    }
}

static void fifo_push_note_off(step_t* s, int size, uint8_t note) {
    uint8_t* note_off = s->note_off;

    for(int i = 0; i < size; i++) {
        if((note_off[i] & 0x80) == 0) {
            // uint8_t next = i+1;
            // if(i == size) {
            //     next = 0;
            // }

            note_off[i] = note | 0x80;

            // note_off[next] = note_off[next] & 0x7F; 
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
*/
void edit_step_note(
    uint8_t sq,
    uint8_t step,
    MIDIStatus_t status,
    MIDINote_t note,
    uint8_t velocity
) {
    step_t s;
    uint16_t seq_base_index = ((uint16_t)sq * CONFIG_STEPS_PER_SEQUENCE);
    uint16_t index = seq_base_index + (uint16_t)step;

    s = steps[index];

    switch(status) {
        case NOTE_ON:
            fifo_push_note_on(&s, CONFIG_MAX_POLYPHONY, (uint8_t)note, velocity);
            break;

        case NOTE_OFF:
            fifo_push_note_off(&s, CONFIG_MAX_POLYPHONY, (uint8_t)note);
            break;

        default:
            break;
    }

    steps[index] = s;
}

void mute_step(uint8_t sequence, uint8_t step) {
    uint32_t* muted_steps = sequences[sequence].muted_steps;
    toggle_bit(muted_steps, step, CONFIG_STEPS_PER_SEQUENCE);
}

void toggle_step(uint8_t sequence, uint8_t step) {
    uint32_t* en_steps = sequences[sequence].enabled_steps;
    toggle_bit(en_steps, step, CONFIG_STEPS_PER_SEQUENCE);
}

void edit_step_velocity(uint8_t sq, uint8_t step, int8_t amount) {
    step_t s;
    uint16_t index = ((uint16_t)sq * CONFIG_STEPS_PER_SEQUENCE) + (uint16_t)step;

    s = steps[index];

    uint8_t v = s.note_on[0].velocity;
    
    if(v + amount > 127) {
        v = 127;
    } else if(amount < 0 && v <= (amount*-1)) {
        v = 0;
    } else {
        v+=amount;
    }



    for(uint8_t i = 0; i < CONFIG_MAX_POLYPHONY; i++) {
        s.note_on[i].velocity = v;
    }

    steps[index] = s;
}

uint8_t get_step_velocity(uint8_t sq, uint8_t st) {
    uint16_t index = ((uint16_t)sq * CONFIG_STEPS_PER_SEQUENCE) + (uint16_t)st;

    return steps[index].note_on[0].velocity;
}

static uint8_t remove_note(step_t* s) {
    // check if step s note_off contains any of the notes in notes[]
    // remove that note from s.note_off[]
    // defrag s.note_off[]
    // TODO there will be problems with this because note_on and note_off are
    // circular buffers kinda

    uint8_t deletions = 0;

    for(uint8_t i = 0; i < CONFIG_MAX_POLYPHONY; i++) {
        uint8_t note_off = s->note_off[i] & 0x7F;
        uint8_t note_matrix_index = note_off - A0;
        
        // TODO have a look at this. this assumes note_off is always defragged
        if(note_off == 0) {
            return deletions;
        }

        if(note_matrix[note_matrix_index] == 1) {
            deletions++;
            s->note_off[i] = 0;
        }
    }

    return deletions;
}

static uint8_t init_note_matrix(uint16_t step_index) {
    uint8_t num_notes = 0;

    for(uint8_t i = 0; i < CONFIG_MAX_POLYPHONY; i++) {
        if((steps[step_index].note_on[i].note & 0x7F) != 0) {
            uint8_t note_matrix_index = (steps[step_index].note_on[i].note & 0x7F) - A0;
            note_matrix[note_matrix_index] = 1;
            num_notes++;
        }
    }

    return num_notes;
}

static void defrag_buffer(uint8_t* buffer) {
    uint8_t write_pos = 0;
    for(uint8_t i = 0; i < CONFIG_MAX_POLYPHONY; i++) {
        if(buffer[i] != 0) {
            buffer[write_pos] = buffer[i];
            write_pos++;
        }
    }

    while(write_pos < CONFIG_MAX_POLYPHONY) {
        buffer[write_pos] = 0;
        write_pos++;
    }
}

void clear_step(uint8_t sq, uint8_t step) {
    uint16_t sq_start = ((uint16_t)sq * CONFIG_STEPS_PER_SEQUENCE);
    uint16_t index = sq_start + (uint16_t)step;
    memset(&note_matrix, 0, NUM_VALID_NOTES);

    // the number of valid elements in note_on
    uint8_t num_notes = init_note_matrix(index);

    // iterate over the sequence til the last step
    for(uint8_t i = step+1; i < CONFIG_STEPS_PER_SEQUENCE; i++) {
        if(num_notes <= 0) {
            break;
        }
        uint8_t deletions = remove_note(&steps[sq_start + (uint16_t)i]);

        if(deletions > 0) {
            defrag_buffer(steps[sq_start + (uint16_t)i].note_off);
            num_notes -= deletions;
        }

    }
    
    // continue iterating from the first step
    if(num_notes > 0) {
        for(uint8_t i = 0; i < step; i++) {
            if(num_notes <= 0) {
                break;
            }
            uint8_t deletions = remove_note(&steps[sq_start + (uint16_t)i]);

            if(deletions > 0) {
                defrag_buffer(steps[sq_start + (uint16_t)i].note_off);
                num_notes -= deletions;
            }     
        }
    }

    note_t note_on[CONFIG_MAX_POLYPHONY] = {0};

    memcpy(steps[index].note_on, note_on, sizeof(note_on));
}