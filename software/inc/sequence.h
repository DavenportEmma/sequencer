#ifndef _SEQUENCE_H
#define _SEQUENCE_H

#include "FreeRTOS.h"
#include "semphr.h"
#include "autoconf.h"

void toggle_sequence(uint8_t seq);
void play_sequences();

#endif // _SEQUENCE_H