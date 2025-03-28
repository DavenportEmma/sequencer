#ifndef STEP_EDIT_BUFFER_H
#define STEP_EDIT_BUFFER_H

#include "midi.h"

void edit_buffer_read(uint16_t addr, uint8_t* data, uint16_t num_bytes);
void edit_buffer_reset();
int edit_buffer_load(uint8_t sq_index);
void edit_step_note(uint8_t step, MIDINote_t note);

#endif // STEP_EDIT_BUFFER_H