#ifndef _STEP_EDITOR_H
#define _STEP_EDITOR_H

#include "midi.h"

void edit_step_note(uint8_t step, MIDINote_t note);
void mute_step(uint8_t sequence, uint8_t step);

#endif // _STEP_EDITOR_H