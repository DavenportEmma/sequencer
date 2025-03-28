#ifndef _SEQUENCE_H
#define _SEQUENCE_H

#include "FreeRTOS.h"
#include "semphr.h"
#include "autoconf.h"

/*
    STEP_MAX_BYTES is the max number of bytes one step data can be (see
    MEMORY.md)

    ALL_STEPS_MAX_BYTES is simply the maximum bytes per step times the maximum
    sequence length (max steps per sequence)
*/
#define STEP_MAX_BYTES (3+(3*CONFIG_MAX_POLYPHONY))
#define ALL_STEPS_MAX_BYTES (STEP_MAX_BYTES * CONFIG_STEPS_PER_SEQUENCE)

void toggle_sequence(uint8_t seq);
void play_sequences();
int load_sq_for_edit(uint8_t seq);

#endif // _SEQUENCE_H