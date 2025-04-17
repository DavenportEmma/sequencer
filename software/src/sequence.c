#include "FreeRTOS.h"
#include "semphr.h"
#include "midi.h"
#include "sequence.h"
#include "autoconf.h"
#include "uart.h"
#include "w25q128jv.h"
#include "common.h"

extern SemaphoreHandle_t sq_mutex;
extern SemaphoreHandle_t edit_buffer_mutex;

extern MIDISequence_t sequences[CONFIG_TOTAL_SEQUENCES];

extern step_t edit_buffer[CONFIG_STEPS_PER_SEQUENCE];

extern uint8_t ACTIVE_SQ;
extern uint8_t ACTIVE_ST;
extern uint8_t SQ_EDIT_READY;

/*
    get the midi channel of the sequence from flash memory. The sequence
    metadata is stored in the first sector of the block. The midi channel
    information is stored in the first byte of the first page of the block 

    @param sq_index     Index of sequence (0-63)

    @return The midi channel of the sequence
*/
static MIDIChannel_t get_channel(uint8_t sq_index) {
    uint32_t sq_base_addr = CONFIG_SEQ_ADDR_OFFSET * sq_index;
    uint8_t tx[1] = {0};
    uint8_t rx[1] = {0};
    SPIRead(sq_base_addr, tx, rx, 1);

    return (MIDIChannel_t)rx[0];
}

/*
    play the notes contained in the step

    @param c    The midi channel the notes should be played over
    @param st   A pointer to a step struct containing note_on, and note_off
                notes, and the end of step byte (0x00 or 0xFF depending on if
                it's the end of the step or the whole sequence)
*/
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

/*
    read step from the sequence currently loaded into the edit buffer
    
    @param sq   A pointer to the struct of the current sequence in sequences
    @param st   A pointer to the step struct to be assigned to the current step
                in the sequence
*/
static void read_step_from_edit_buffer(MIDISequence_t* sq, step_t* st) {
    if(xSemaphoreTake(edit_buffer_mutex, portMAX_DELAY) == pdTRUE) {
        *st = edit_buffer[sq->counter];
        
        xSemaphoreGive(edit_buffer_mutex);
    }
}

/*
    read step from flash memory

    @param sq       A pointer to the struct of the current sequence in sequences
    @param sq_index The index of the currently processed sequence in sequences
    @param data     The buffer to be filled with step data from flash
*/
static void read_step_from_memory(MIDISequence_t* sq, uint8_t sq_index, uint8_t* data) {
    // get the address for the step pointer in flash memory
    // read MEMORY.md for more info
    uint32_t sq_base_addr = CONFIG_SEQ_ADDR_OFFSET * sq_index;
    uint32_t steps_base_addr = sq_base_addr + 0x1000;
    uint32_t current_step_addr = steps_base_addr + (sq->counter * BYTES_PER_STEP);

    SPIRead(current_step_addr, data, data, BYTES_PER_STEP);
}

/*
    convert a buffer of bytes to a step struct

    @param data     Bytes buffer to be converted to a step
    @param st       Pointer to a step struct to be filled with the data from
                    the data buffer
*/
void bytes_to_step(uint8_t* data, step_t* st) {
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

/*
    read a step from the edit buffer or from memory depending on if the current
    sequence is being edited or not
    
    @param sq       A pointer to the currently processed sequence in sequences
    @param sq_index The index of the currently processed sequence in sequences
    @param st       A pointer to the step struct to be filled with data

    @return 1 if the data being read is malformed
*/
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

/*
    load step data into a step struct then send the note data over midi

    @param sq_index The index of the currently process sequence in sequences
*/
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

/*
    play the current steps in the currently active sequences
*/
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

/*
    toggle a sequence between enabled and disabled

    @param sq_index The index of the currently process sequence in sequences
*/
void toggle_sequence(uint8_t sq_index) {
    if(xSemaphoreTake(sq_mutex, portMAX_DELAY) == pdTRUE) {
        sequences[sq_index].enabled ^= 1;

        if(!sequences[sq_index].enabled) {
            MIDICC_t p = {
                .status = CONTROLLER,
                .channel = sequences[sq_index].channel,
                .control = ALL_NOTES_OFF,
                .value = 0,
            };

            send_midi_control(USART1, &p);
        } else {
            sequences[sq_index].channel = get_channel(sq_index);
        }

        xSemaphoreGive(sq_mutex);
    }
}
