#ifndef _SEQUENCE_H
#define _SEQUENCE_H

#include "FreeRTOS.h"
#include "semphr.h"
#include "autoconf.h"
#include "midi.h"

typedef struct {
    MIDINote_t note;
    uint8_t velocity;
} note_t;

typedef struct {
    note_t note_on[CONFIG_MAX_POLYPHONY];
    MIDINote_t note_off[CONFIG_MAX_POLYPHONY];
    uint8_t end_of_step;
} step_t;

void toggle_sequence(uint8_t seq);
void play_sequences();
void bytes_to_step(uint8_t* data, step_t* st);

#endif // _SEQUENCE_H