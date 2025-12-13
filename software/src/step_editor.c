#include "step_editor.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "sequence.h"
#include "uart.h"
#include "util.h"
#include "midi.h"
#include "display.h"
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
// TODO implement circular buffers
static void fifo_push_note_on(step_t* s, int size, uint8_t note, uint8_t vel) {
    note_t* note_on = s->note_on;

    for(int i = 0; i < size; i++) {
        uint8_t n = note_on[i].note;

        if(n == 0) {
            note_on[i].note = note;
            note_on[i].velocity = vel;

            return;
        }
    }

    #ifdef CONFIG_DEBUG_PRINT
    send_uart(USART3, "max polyphony reached\n\r", 23)
    #endif
}

static void fifo_push_note_off(step_t* s, int size, uint8_t note) {
    uint8_t* note_off = s->note_off;

    for(int i = 0; i < size; i++) {
        if(note_off[i] == 0) {
            note_off[i] = note;
            return;
        }
    }
    
    #ifdef CONFIG_DEBUG_PRINT
    send_uart(USART3, "max polyphony reached\n\r", 23)
    #endif
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
        uint8_t note_off = s->note_off[i];
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
        if(steps[step_index].note_on[i].note != 0) {
            uint8_t note_matrix_index = steps[step_index].note_on[i].note - A0;
            note_matrix[note_matrix_index] = 1;
            num_notes++;
        }
    }

    return num_notes;
}

static void defrag_buffer(uint8_t* buffer, uint8_t len) {
    uint8_t write_pos = 0;
    for(uint8_t i = 0; i < len; i++) {
        if(buffer[i] != 0) {
            buffer[write_pos] = buffer[i];
            write_pos++;
        }
    }

    while(write_pos < len) {
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
            defrag_buffer(steps[sq_start + (uint16_t)i].note_off, CONFIG_MAX_POLYPHONY);
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
                defrag_buffer(steps[sq_start + (uint16_t)i].note_off, CONFIG_MAX_POLYPHONY);
                num_notes -= deletions;
            }     
        }
    }

    note_t note_on[CONFIG_MAX_POLYPHONY] = {0};

    memcpy(steps[index].note_on, note_on, sizeof(note_on));
}

/*
    @param note_on_arr  this is a copy of the note_on array of the step being copied
    @param next_st      this is the step being checked for note_off values
    @param note_off_offsets     this is an array that will contain the distance
                                between the step being copied and the note_off
                                values
    @param step_distacne    this is the distance between the step being copied
                            and the current step being checked, this value will
                            be loaded into note_off_offsets at element i if
                            next_st.note_off contains the value located at
                            note_on_arr[i]
*/
static void check_step(note_t* note_on_arr, step_t next_st, uint8_t* note_off_offsets, uint8_t step_distance) {
    uint8_t note_hash_table[C8-A0] = {0};

    for(uint8_t i = 0; i < CONFIG_MAX_POLYPHONY; i++) {
        uint8_t note_off = next_st.note_off[i];
        if(note_off != 0) {
            note_hash_table[note_off-A0] = 1;
        }
    }

    for(uint8_t i = 0; i < CONFIG_MAX_POLYPHONY; i++) {
        uint8_t note_on = note_on_arr[i].note;

        if(note_on != 0 && note_hash_table[note_on-A0]) {
            note_off_offsets[i] = step_distance;
            memset(&note_on_arr[i], 0, sizeof(note_t));
        }
    }
}

static uint8_t array_all_zeroes(note_t* a, uint8_t array_length) {
    if (a[0].note==0 && memcmp(a, a+1, (array_length-1)*sizeof(a[0]) ) == 0) {
        return 1;
    }

    return 0;
}

void copy_step(step_t* temp_st, uint8_t* note_off_offsets, uint8_t sq, uint8_t st) {
    uint16_t st_index = (sq*64)+st;
    memcpy(temp_st, &steps[st_index], sizeof(step_t));

    note_t note_on_arr[CONFIG_MAX_POLYPHONY];
    memcpy(note_on_arr, temp_st->note_on, sizeof(note_t)*CONFIG_MAX_POLYPHONY);

    /*
        we need to know how long each note in step st is to be held for
        taking each note_on value in st we iterate over the entire sequence
        and record how many steps between st and each note_off value for each
        note_on value in st
    */

    for(uint8_t i = st+1; i < CONFIG_STEPS_PER_SEQUENCE; i++) {
        uint8_t next_step_index = (sq*64)+i;
        uint8_t note_distance = i - st;
        check_step(note_on_arr, steps[next_step_index], note_off_offsets, note_distance);

        if(array_all_zeroes(note_on_arr, CONFIG_MAX_POLYPHONY)){
            return;
        }
    }
    
    // continue iterating from the first step
    if(array_all_zeroes(note_on_arr, CONFIG_MAX_POLYPHONY) == 0) {
        for(uint8_t i = 0; i <= st; i++) {
            uint8_t next_step_index = (sq*64)+i;
            uint8_t note_distance = (CONFIG_STEPS_PER_SEQUENCE - st) + i;
            check_step(note_on_arr, steps[next_step_index], note_off_offsets, note_distance);
            
            if(array_all_zeroes(note_on_arr, CONFIG_MAX_POLYPHONY)) {
                return;
            }
        }
    }

}

void paste_step(step_t temp_step, uint8_t* note_off_offsets, uint8_t sq, uint8_t st) {
    clear_step(sq, st);

    memcpy(steps[st].note_on, temp_step.note_on, sizeof(note_t)*CONFIG_MAX_POLYPHONY);

    // TODO protect against buffer overflow or buffer overwriting
    for(uint8_t i = 0; i < CONFIG_MAX_POLYPHONY; i++) {
        uint16_t note_off_index = note_off_offsets[i] + st;
        uint8_t note = temp_step.note_on[i].note;

        if(note_off_index > CONFIG_STEPS_PER_SEQUENCE) {
            note_off_index-=CONFIG_STEPS_PER_SEQUENCE;
        }

        fifo_push_note_off(&steps[note_off_index], CONFIG_MAX_POLYPHONY, note);
    }
}

void display_step_notes(uint8_t sq, uint8_t st) {
    display_piano_roll();

    for(uint8_t i = 0; i < CONFIG_MAX_POLYPHONY; i++) {
        show_note(steps[(sq*64)+st].note_on[i].note);
    }

    update_display();
}

void copy_steps(uint16_t dst_sq, uint16_t src_sq, uint8_t n) {
    memcpy(&steps[dst_sq*64], &steps[src_sq*64], n);
}
