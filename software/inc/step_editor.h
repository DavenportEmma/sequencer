#ifndef _STEP_EDITOR_H
#define _STEP_EDITOR_H

#include "midi.h"
#include "sequence.h"

void edit_step_note(
    uint8_t sq,
    uint8_t step,
    MIDIStatus_t status,
    MIDINote_t note,
    uint8_t velocity
);
void mute_step(uint8_t sequence, uint8_t step);
void toggle_step(uint8_t sequence, uint8_t step);
void edit_step_velocity(uint8_t sq, uint8_t step, int8_t amount);
uint8_t get_step_velocity(uint8_t sq, uint8_t step);
void clear_step(uint8_t sq, uint8_t step);
void copy_step(step_t* temp_st, uint8_t* note_off_offsets, uint8_t sq, uint8_t st);
void paste_step(step_t temp_st, uint8_t* note_off_offsets, uint8_t sq, uint8_t st);
void display_step_notes(uint8_t sq, uint8_t st);
void copy_steps(uint16_t dst_sq, uint16_t src_sq, uint8_t n);

#endif // _STEP_EDITOR_H