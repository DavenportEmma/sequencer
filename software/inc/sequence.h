#ifndef _SEQUENCE_H
#define _SEQUENCE_H

#include "FreeRTOS.h"
#include "semphr.h"
#include "autoconf.h"

/*
    BYTES_PER_STEP is the number of bytes one step data is (see MEMORY.md)

    BYTES_PER_SEQ is the number of bytes per step times the maximum sequence
    length (max steps per sequence)
*/
#define BYTES_PER_STEP (3+(3*CONFIG_MAX_POLYPHONY))
#define BYTES_PER_SEQ (BYTES_PER_STEP * CONFIG_STEPS_PER_SEQUENCE)

void toggle_sequence(uint8_t seq);
void play_sequences();
void reset_step_edit_buffer();
int load_sq_for_edit(uint8_t seq);

#endif // _SEQUENCE_H