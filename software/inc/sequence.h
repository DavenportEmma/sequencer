#ifndef _SEQUENCE_H
#define _SEQUENCE_H

#include "FreeRTOS.h"
#include "semphr.h"
#include "autoconf.h"
#include "midi.h"

void toggle_sequence(uint8_t seq);
void play_sequences();
void edit_buffer_reset();
int edit_buffer_load(uint8_t sq_index);
void edit_step_note(uint8_t step, MIDINote_t note);

#endif // _SEQUENCE_H