#include "step_editor.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "autoconf.h"
#include "sequence.h"

extern SemaphoreHandle_t edit_buffer_mutex;
extern step_t edit_buffer[CONFIG_STEPS_PER_SEQUENCE];

/*
    edit the note of a step in the step edit buffer

    @param step     the index (0-63) of the step to be edited
    @param note     the new note value for the step
*/
void edit_step_note(uint8_t step, MIDINote_t note) {
    if(xSemaphoreTake(edit_buffer_mutex, portMAX_DELAY) == pdTRUE) {
        step_t* st = &edit_buffer[step];
        st->note_on[0].note = note;

        if(st->end_of_step == 0xFF) {
            step = 0;
        } else {
            step++;
        }

        st = &edit_buffer[step];
        st->note_off[0] = note;

        xSemaphoreGive(edit_buffer_mutex);
    }
}
