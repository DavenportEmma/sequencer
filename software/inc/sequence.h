#ifndef _SEQUENCE_H
#define _SEQUENCE_H

#include "midi.h"
#include "m_buf.h"
#include "autoconf.h"
#include "tasks.h"

typedef struct {
    MIDINote_t note;
    uint8_t velocity;
} note_t;

typedef struct {
    note_t note_on[CONFIG_MAX_POLYPHONY];
    MIDINote_t note_off[CONFIG_MAX_POLYPHONY];
} step_t;

uint8_t init_sequences();
uint32_t get_step_data_offset(uint8_t sq_index);
void toggle_sequence(uint8_t seq);
void toggle_sequences(uint32_t* select_mask, uint8_t max);
void enable_sequence(uint8_t sq_index);
void disable_sequence(uint8_t sq_index);
void load_sequences(UARTTaskParams_t* port_buffers, uint8_t num_ports);
void break_sequence(uint8_t sq_index);
void clear_sequence(uint8_t sq_index);
void save_data();
void play_notes(mbuf_handle_t m, USART_TypeDef* port);
void set_midi_channel(uint8_t sq_index, MIDIChannel_t channel);
MIDIChannel_t get_channel(uint8_t sq_index);
uint8_t is_sq_enabled(uint8_t sq_index);
step_t get_step_from_index(uint16_t st_index);

#endif // _SEQUENCE_H