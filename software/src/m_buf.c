#include "midi.h"
#include "FreeRTOS.h"
#include "m_buf.h"

static void advance_head_pointer(mbuf_handle_t k) {
    k->head++;
    if(k->head >= k->max) {
        k->head = k->max-1;
        k->full = 1;
    }
}

static void retreat_head_pointer(mbuf_handle_t k) {
    if(--k->head < -1) {
        k->head = -1;
    }

    k->full = 0;
}

mbuf_handle_t mbuf_init(MIDIPacket_t* buffer, int16_t size) {
    mbuf_handle_t mbuf = pvPortMalloc(sizeof(midi_buf_t));
    if(!mbuf) return NULL;

    mbuf->buffer = buffer;
    mbuf->max = size;
    mbuf_reset(mbuf);

    return mbuf;
}

void mbuf_free(mbuf_handle_t m) {
    vPortFree(m);
}

void mbuf_reset(mbuf_handle_t m) {
    m->head = -1;
    m->full = 0;
}


int8_t mbuf_full(mbuf_handle_t k) {
    return k->full;
}

int8_t mbuf_empty(mbuf_handle_t k) {
    if(k->head >= 0) {
        return 0;
    } else {
        return 1;
    }
}

int16_t mbuf_size(mbuf_handle_t k) {
    return k->head+1;   // return number of elements in array
}

void mbuf_push(mbuf_handle_t k, MIDIPacket_t data) {
    advance_head_pointer(k);
    k->buffer[k->head] = data;

}

void mbuf_pop(mbuf_handle_t k, MIDIPacket_t* data) {
    if(!mbuf_empty(k)) {
        *data = k->buffer[k->head];
        retreat_head_pointer(k);
    }
}