#include "step_editor.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "autoconf.h"
#include "sequence.h"
#include "uart.h"

extern SemaphoreHandle_t edit_buffer_mutex;
extern step_t edit_buffer[CONFIG_STEPS_PER_SEQUENCE];

extern SemaphoreHandle_t sq_mutex;
extern MIDISequence_t sequences[CONFIG_TOTAL_SEQUENCES];

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

void edit_step_note_midi(uint8_t step, uint8_t* buf) {
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
            break;

        case NOTE_ON:
            // TODO this won't do for polyphonic sequences
            edit_buffer[step].note_on[0] = n;
            break;

        default:
            break;
    }
}

static void toggle_bit(uint32_t* field, uint8_t bit) {
    uint8_t mask_size = 32;

    if (bit < CONFIG_STEPS_PER_SEQUENCE) {
        uint8_t mask_index = bit / mask_size;
        uint32_t mute_mask = 1 << (bit % mask_size);

        field[mask_index] ^= mute_mask;
    }
}

void mute_step(uint8_t sequence, uint8_t step) {
    if (xSemaphoreTake(sq_mutex, portMAX_DELAY) == pdTRUE) {
        uint32_t* muted_steps = sequences[sequence].muted_steps;

        toggle_bit(muted_steps, step);

        xSemaphoreGive(sq_mutex);
    }
}

void toggle_step(uint8_t sequence, uint8_t step) {
    if (xSemaphoreTake(sq_mutex, portMAX_DELAY) == pdTRUE) {
        uint32_t* en_steps = sequences[sequence].enabled_steps;

        toggle_bit(en_steps, step);

        xSemaphoreGive(sq_mutex);
    }
}

void edit_step_velocity(uint8_t step, int8_t velocity_dir) {
    step_t* s = &edit_buffer[step];
    // TODO this won't do for polyphonic sequences
    uint8_t v = s->note_on[0].velocity;

    if(velocity_dir <= 0) {
        if(v++ > 127) {
            v = 127;
        }
    } else {
        if(v > 0) {
            v--;
        }
    }

    s->note_on[0].velocity = v;

    send_hex(USART3, s->note_on[0].velocity);
    send_uart(USART3, "\n\r", 2);
}
