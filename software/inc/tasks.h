#ifndef _TASKS_H
#define _TASKS_H

#include "m_buf.h"

typedef struct {
    USART_TypeDef* port;
    mbuf_handle_t note_on;
    mbuf_handle_t note_off;
} UARTTaskParams_t;

void sq_play_task();

void key_scan_task();

void save_task();

#endif // _TASKS_H