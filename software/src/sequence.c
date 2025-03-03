#include "FreeRTOS.h"
#include "semphr.h"
#include "midi.h"
#include "sequence.h"
#include "autoconf.h"
#include "uart.h"
#include "common.h"

extern SemaphoreHandle_t sq_mutex;
extern volatile MIDISequence_t sq_states[64];

void toggle_sequence(uint8_t seq) {
    if(xSemaphoreTake(sq_mutex, portMAX_DELAY) == pdTRUE) {
        sq_states[seq].enabled ^= 1;
        xSemaphoreGive(sq_mutex);
    }
}

static void load_buffer(MIDISequence_t* seq, MIDIPacket_t* buf, uint8_t* len) {
    uint32_t seq_base_addr = seq->id * CONFIG_SEQ_ADDR_OFFSET;
    uint32_t step_addr = seq_base_addr + (0x100 * seq->counter);

    if(++seq->counter > 64) {
        seq->counter = 0;
    }
    // MIDIPacket_t p;

    // p.status = NOTE_OFF;
    // p.channel = seq->channel;
    // p.note = A4;
    // p.velocity = 64;
    
    // buf[*len] = p;
    // (*len)++;

    // p.status = NOTE_ON;
    // p.channel = CHANNEL_11;
    // p.note = A4;
    // p.velocity = 64;
    
    // buf[*len] = p;
    // (*len)++;
}

void play_sequences() {
    MIDIPacket_t packet_buffer[CONFIG_PACKET_BUFFER_LENGTH];
    uint8_t buffer_length = 0;

    if(xSemaphoreTake(sq_mutex, portMAX_DELAY) == pdTRUE) {
        for(int i = 0; i < 64; i ++) {
            if( sq_states[i].enabled &&
                buffer_length < CONFIG_PACKET_BUFFER_LENGTH
            ) {
                load_buffer(&sq_states[i], packet_buffer, &buffer_length);
            }
        }
        xSemaphoreGive(sq_mutex);
    }

    for(int i = 0; i < buffer_length; i++) {
        send_midi_note(USART1, &packet_buffer[i]);
    }
}
