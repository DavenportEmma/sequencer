#ifndef _SEQUENCE_H
#define _SEQUENCE_H

#include "FreeRTOS.h"
#include "semphr.h"
#include "autoconf.h"

void toggle_sequence(uint8_t seq);
void play_sequences();
void reset_step_edit_buffer();
int load_sq_for_edit(uint8_t seq);

#endif // _SEQUENCE_H