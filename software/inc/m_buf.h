#ifndef _MIDI_NOTE_BUFFER
#define _MIDI_NOTE_BUFFER

#include "midi.h"

typedef struct {
    MIDIPacket_t* buffer;
    int16_t head;
    int16_t max;
    int8_t full;
} midi_buf_t;

typedef midi_buf_t* mbuf_handle_t;

mbuf_handle_t mbuf_init(MIDIPacket_t* buffer, int16_t size);

void mbuf_free(mbuf_handle_t k);

void mbuf_reset(mbuf_handle_t k);

int8_t mbuf_full(mbuf_handle_t k);

int8_t mbuf_empty(mbuf_handle_t k);

int16_t mbuf_size(mbuf_handle_t k);

void mbuf_push(mbuf_handle_t k, MIDIPacket_t d);

void mbuf_pop(mbuf_handle_t k, MIDIPacket_t* d);

#endif // _MIDI_NOTE_BUFFER