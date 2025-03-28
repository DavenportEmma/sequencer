#include "FreeRTOS.h"
#include "semphr.h"
#include "midi.h"
#include "sequence.h"
#include "autoconf.h"
#include "uart.h"
#include "w25q128jv.h"

extern SemaphoreHandle_t sq_mutex;
extern MIDISequence_t sq_states[CONFIG_TOTAL_SEQUENCES];

static uint8_t step_edit_buffer[ALL_STEPS_MAX_BYTES];
static uint8_t SQ_UNDER_EDIT;

static MIDIChannel_t get_channel(uint8_t sq_index) {
    uint32_t sq_base_addr = CONFIG_SEQ_ADDR_OFFSET * sq_index;
    uint8_t tx[1] = {0};
    uint8_t rx[1] = {0};
    SPIRead(sq_base_addr, tx, rx, 1);

    return (MIDIChannel_t)rx[0];
}

void toggle_sequence(uint8_t seq) {
    if(xSemaphoreTake(sq_mutex, portMAX_DELAY) == pdTRUE) {
        sq_states[seq].enabled ^= 1;

        if(!sq_states[seq].enabled) {
            MIDICC_t p = {
                .status = CONTROLLER,
                .channel = sq_states[seq].channel,
                .control = ALL_NOTES_OFF,
                .value = 0,
            };

            send_midi_control(USART1, &p);
        } else {
            sq_states[seq].channel = get_channel(seq);
        }

        xSemaphoreGive(sq_mutex);
    }
}

static void parse_step_buffer(MIDIChannel_t c, uint8_t* step_buffer, int8_t len) {
    int byte_index = 0;

    // hopefully we never run into the case where NOTE_OFF isn't the first byte
    while(step_buffer[byte_index] != NOTE_OFF) { byte_index++; }

    byte_index++;

    // while after NOTE_OFF and before NOTE_ON send all the note off packet
    while(step_buffer[byte_index] != NOTE_ON) {
        MIDIPacket_t p = {
            .channel = c,
            .status = NOTE_OFF,
            .note = step_buffer[byte_index],
            .velocity = 0,
        };

        send_midi_note(USART1, &p);
        byte_index++;
    }

    byte_index++;

    while(byte_index < len) {
        MIDIPacket_t p = {
            .channel = c,
            .status = NOTE_ON,
            .note = step_buffer[byte_index],
            .velocity = step_buffer[byte_index+1],
        };

        send_midi_note(USART1, &p);
        // every note on packet takes up two bytes in memory (note and velocity)
        byte_index+=2;
    }
}

static void play_step(uint8_t sq_index) {
    MIDISequence_t* sq = &sq_states[sq_index];

    // get the address for the step pointer in flash memory
    // read MEMORY.md for more info
    uint32_t sq_base_addr = CONFIG_SEQ_ADDR_OFFSET * sq_index;
    uint32_t steps_base_addr = sq_base_addr + 0x1000;
    uint32_t current_step_addr = steps_base_addr + sq->counter;

    // see MEMORY.md
    int8_t buffer_max = 3+(3*CONFIG_MAX_POLYPHONY);
    uint8_t* step_buffer = (uint8_t*)pvPortCalloc(buffer_max, sizeof(uint8_t));

    // traverse memory and load the step info until and end of step or end of
    // sequence byte is hit
    int8_t num_bytes = readSteps(current_step_addr, step_buffer, 1, buffer_max);

    // if the end of sequence byte is hit then put the counter back to the start
    if(step_buffer[num_bytes] == 0xFF) {
        sq->counter = 0;
    } else {
        sq->counter += num_bytes + 1;
    }

    parse_step_buffer(sq->channel, step_buffer, num_bytes);

    vPortFree(step_buffer);
}

void play_sequences() {
    if(xSemaphoreTake(sq_mutex, portMAX_DELAY) == pdTRUE) {
        for(int i = 0; i < CONFIG_TOTAL_SEQUENCES; i++) {
            if(sq_states[i].enabled) {
                play_step(i);
            }
        }
        xSemaphoreGive(sq_mutex);
    }
}

static int load_step_edit_buffer(uint8_t sq_index) {
    uint32_t sq_base_addr = CONFIG_SEQ_ADDR_OFFSET * sq_index;
    uint32_t steps_base_addr = sq_base_addr + 0x1000;

    int8_t num_bytes = readSteps(steps_base_addr, step_edit_buffer, -1, ALL_STEPS_MAX_BYTES);
    
    for(int i = 0; i < num_bytes; i++) {
        uint8_t d = step_edit_buffer[i];
        send_hex(USART3, d);
        send_uart(USART3, "\n", 1);
    }

    return 0;
}

int load_sq_for_edit(uint8_t seq) {
    if(load_step_edit_buffer(seq)) {
        send_uart(USART3, "error loading step edit buffer\n", 31);
    }



    return 0;
}