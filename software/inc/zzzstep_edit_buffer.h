#ifndef _STEP_EDIT_BUFFER
#define _STEP_EDIT_BUFFER

#include "midi.h"

void edit_buffer_reset();
int edit_buffer_load(uint8_t sq_index);
void edit_buffer_clear();

#endif // _STEP_EDIT_BUFFER