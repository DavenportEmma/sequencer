#include "FreeRTOS.h"
#include "semphr.h"
#include "midi.h"
#include "sequence.h"
#include "autoconf.h"
#include "uart.h"
#include "w25q128jv.h"
#include "common.h"
#include <string.h>

typedef struct {
    MIDINote_t note;
    uint8_t velocity;
} note_t;

typedef struct {
    note_t note_on[CONFIG_MAX_POLYPHONY];
    MIDINote_t note_off[CONFIG_MAX_POLYPHONY];
    uint8_t end_of_step;
} step_t;

extern SemaphoreHandle_t sq_mutex;
extern SemaphoreHandle_t edit_buffer_mutex;

extern MIDISequence_t sequences[CONFIG_TOTAL_SEQUENCES];

static step_t edit_buffer[CONFIG_STEPS_PER_SEQUENCE];

extern uint8_t ACTIVE_SQ;
extern uint8_t ACTIVE_ST;
extern uint8_t SQ_EDIT_READY;

static MIDIChannel_t get_channel(uint8_t sq_index) {
    uint32_t sq_base_addr = CONFIG_SEQ_ADDR_OFFSET * sq_index;
    uint8_t tx[1] = {0};
    uint8_t rx[1] = {0};
    SPIRead(sq_base_addr, tx, rx, 1);

    return (MIDIChannel_t)rx[0];
}

static void play_step_notes(MIDIChannel_t c, step_t* st) {
    for(int i = 0; i < CONFIG_MAX_POLYPHONY; i++) {
        MIDINote_t n = st->note_off[i];

        if(n <= C8 && n >= A0) {
            MIDIPacket_t p = {
                .channel = c,
                .status = NOTE_OFF,
                .note = n,
                .velocity = 0,
            };

            send_midi_note(USART1, &p);
        }
    }

    for(int i = 0; i < CONFIG_MAX_POLYPHONY; i++) {
        MIDINote_t n = st->note_on[i].note;

        if(n <= C8 && n >= A0) {
            MIDIPacket_t p = {
                .channel = c,
                .status = NOTE_ON,
                .note = n,
                .velocity = st->note_on[i].velocity,
            };
    
            send_midi_note(USART1, &p);
        }
    }
}

static void read_step_from_edit_buffer(MIDISequence_t* sq, step_t* st) {
    if(xSemaphoreTake(edit_buffer_mutex, portMAX_DELAY) == pdTRUE) {
        *st = edit_buffer[sq->counter];
        
        xSemaphoreGive(edit_buffer_mutex);
    }
}

static void read_step_from_memory(MIDISequence_t* sq, uint8_t sq_index, uint8_t* data) {
    // get the address for the step pointer in flash memory
    // read MEMORY.md for more info
    uint32_t sq_base_addr = CONFIG_SEQ_ADDR_OFFSET * sq_index;
    uint32_t steps_base_addr = sq_base_addr + 0x1000;
    uint32_t current_step_addr = steps_base_addr + (sq->counter * BYTES_PER_STEP);

    SPIRead(current_step_addr, data, data, BYTES_PER_STEP);
}

static void bytes_to_step(uint8_t* data, step_t* st) {
    int i = 0;

    while(data[i] != NOTE_OFF) { i++; }
    i++;    // iterate past NOTE_OFF byte

    int note_index = 0;
    while(data[i] != NOTE_ON) {
        st->note_off[note_index] = data[i];

        note_index++;
        i++;
    }

    i++;    // iterate past NOTE_ON byte
    note_index = 0;
    while(data[i] != SEQ_END && data[i] != STEP_END) {
        st->note_on[note_index].note = data[i];
        st->note_on[note_index].velocity = data[i+1];

        note_index+=2;
        i+=2;
    }

    st->end_of_step = data[i];
}

// load step from flash or from the edit buffer
static int load_step(MIDISequence_t* sq, uint8_t sq_index, step_t* st) {
    if(ACTIVE_SQ == sq_index && SQ_EDIT_READY == 1) {
        read_step_from_edit_buffer(sq, st);
    } else {
        uint8_t data[BYTES_PER_STEP];
        read_step_from_memory(sq, sq_index, data);

        if(
            data[0] != NOTE_OFF &&
            !(data[BYTES_PER_STEP - 1] == SEQ_END ||
            data[BYTES_PER_STEP - 1] == STEP_END)
        ) {
            return 1;
        }
        
        bytes_to_step(data, st);
    }

    return 0;
}

static void play_step(uint8_t sq_index) {
    MIDISequence_t* sq = &sequences[sq_index];
    step_t st;

    if(load_step(sq, sq_index, &st)) {
        send_uart(USART3, "Error data alignment\n\r", 22);
    }
    
    // if the end of sequence byte is hit then put the counter back to the start
    if(st.end_of_step == 0xFF){
        sq->counter = 0;
    } else {
        sq->counter++;
    }

    play_step_notes(sq->channel, &st);
}

void play_sequences() {
    if(xSemaphoreTake(sq_mutex, portMAX_DELAY) == pdTRUE) {
        for(int i = 0; i < CONFIG_TOTAL_SEQUENCES; i++) {
            if(sequences[i].enabled) {
                play_step(i);
            }
        }
        xSemaphoreGive(sq_mutex);
    }
}

void toggle_sequence(uint8_t seq) {
    if(xSemaphoreTake(sq_mutex, portMAX_DELAY) == pdTRUE) {
        sequences[seq].enabled ^= 1;

        if(!sequences[seq].enabled) {
            MIDICC_t p = {
                .status = CONTROLLER,
                .channel = sequences[seq].channel,
                .control = ALL_NOTES_OFF,
                .value = 0,
            };

            send_midi_control(USART1, &p);
        } else {
            sequences[seq].channel = get_channel(seq);
        }

        xSemaphoreGive(sq_mutex);
    }
}


// -------------- buffer methods --------------

void edit_buffer_reset() {
    if(xSemaphoreTake(edit_buffer_mutex, portMAX_DELAY) == pdTRUE) {
        memset(&edit_buffer, 0xFF, sizeof(edit_buffer));
        
        xSemaphoreGive(edit_buffer_mutex);
    }
}

int edit_buffer_load(uint8_t sq_index) {
    uint32_t sq_base_addr = CONFIG_SEQ_ADDR_OFFSET * sq_index;
    uint32_t steps_base_addr = sq_base_addr + 0x1000;

    if(xSemaphoreTake(edit_buffer_mutex, portMAX_DELAY) == pdTRUE) {
        uint8_t seq_data[BYTES_PER_SEQ];
        SPIRead(steps_base_addr, seq_data, seq_data, (uint16_t)BYTES_PER_SEQ);

        for(int i = 0; i < CONFIG_STEPS_PER_SEQUENCE; i++) {
            uint8_t st_data[BYTES_PER_STEP];
            
            memcpy(st_data, &seq_data[BYTES_PER_STEP * i], BYTES_PER_STEP);

            if(
                st_data[0] != NOTE_OFF &&
                !(st_data[BYTES_PER_STEP - 1] == SEQ_END ||
                st_data[BYTES_PER_STEP - 1] == STEP_END)
            ) {
                return 1;
            }

            step_t st;
            bytes_to_step(st_data, &st);
            
            edit_buffer[i] = st;

            if(st.end_of_step == 0xFF) {
                break;
            }
        }

        xSemaphoreGive(edit_buffer_mutex);
    }

    return 0;
}

// --------------- step methods ---------------

void edit_step_note(uint8_t step, MIDINote_t note) {
    if(xSemaphoreTake(edit_buffer_mutex, portMAX_DELAY) == pdTRUE) {
        step_t* st = &edit_buffer[step];
        st->note_on[0].note = note;

        if(st->end_of_step == 0xFF) {
            step = 0;
        } else {
            step++;
        }

        st = &edit_buffer[step];
        st->note_off[0] = note;

        xSemaphoreGive(edit_buffer_mutex);
    }
}