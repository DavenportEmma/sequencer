#ifndef _SEQUENCE_H
#define _SEQUENCE_H

#include "midi.h"
#include "m_buf.h"

typedef struct {
    MIDINote_t note;
    uint8_t velocity;
} note_t;

typedef struct {
    note_t note_on[CONFIG_MAX_POLYPHONY];
    MIDINote_t note_off[CONFIG_MAX_POLYPHONY];
    uint8_t end_of_step;
} step_t;

MIDIChannel_t read_channel(uint8_t sq_index);
uint32_t get_step_data_offset(MIDISequence_t* sq, uint8_t sq_index);
void toggle_sequence(uint8_t seq);
void toggle_sequences(uint32_t* select_mask, uint8_t max);
void disable_sequence(uint8_t sq_index);
void load_sequences(mbuf_handle_t note_on_mbuf, mbuf_handle_t note_off_mbuf);
void bytes_to_step(uint8_t* data, step_t* st);
void play_notes(mbuf_handle_t m);
void set_midi_channel(uint8_t sq_index, MIDIChannel_t channel);
MIDIChannel_t get_channel(uint8_t sq_index);

#endif // _SEQUENCE_H