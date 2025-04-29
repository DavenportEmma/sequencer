#ifndef _STEP_EDITOR_H
#define _STEP_EDITOR_H

#include "midi.h"

void edit_step_note(uint8_t step, MIDINote_t note);
void edit_step_note_midi(uint8_t step, uint8_t* buf);
void mute_step(uint8_t sequence, uint8_t step);
void toggle_step(uint8_t sequence, uint8_t step);
void edit_step_velocity(uint8_t step, int8_t velocity_dir);
void clear_step(uint8_t step);

#endif // _STEP_EDITOR_H