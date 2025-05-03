#include "FreeRTOS.h"
#include "semphr.h"
#include "midi.h"
#include "sequence.h"
#include "uart.h"
#include "flash.h"
#include "common.h"
#include "m_buf.h"

extern SemaphoreHandle_t sq_mutex;
extern SemaphoreHandle_t edit_buffer_mutex;

extern MIDISequence_t sequences[CONFIG_TOTAL_SEQUENCES];

extern step_t edit_buffer[CONFIG_STEPS_PER_SEQUENCE];

extern uint8_t ACTIVE_SQ;
extern uint8_t ACTIVE_ST;
extern uint8_t SQ_EDIT_READY;

/*
    read the midi channel of the sequence from flash memory. The sequence
    metadata is stored in the first sector of the block. The midi channel
    information is stored in the first byte of the first page of the block 

    @param sq_index     Index of sequence (0-63)

    @return The midi channel of the sequence
*/
MIDIChannel_t read_channel(uint8_t sq_index) {
    uint32_t sq_base_addr = CONFIG_SEQ_ADDR_OFFSET * sq_index;
    uint8_t tx[1] = {0};
    uint8_t rx[1] = {0};
    flash_SPIRead(sq_base_addr, tx, rx, 1);

    return (MIDIChannel_t)rx[0];
}

uint32_t get_step_data_offset(MIDISequence_t* sq, uint8_t sq_index) {
    uint32_t sq_base_addr = CONFIG_SEQ_ADDR_OFFSET * sq_index;

    uint32_t offset = 0;
    // 15 possible sectors for the step data
    for(int i = 0; i < 15; i++) {
        offset += 0x1000;

        uint32_t addr = sq_base_addr + offset;
        uint8_t d = 0xFF;

        flash_SPIRead(addr, &d, &d, 1);
        
        if(d != 0xFF) {
            return offset;
        }
    }
    // TODO handle this error properly
    return 0xFFFF;
}

/*
    load the notes contained in the step into the note_on and note_off buffers

    @param note_on_mbuf
    @param note_off_mbuf
    @param c        The midi channel the notes should be played over
    @param muted    A flag to mark muted or unmuted state for the step
    @param st       A pointer to a step struct containing note_on, and note_off
                    notes, and the end of step byte (0x00 or 0xFF depending on
                    if it's the end of the step or the whole sequence)
*/
static void load_step_notes(
    mbuf_handle_t note_on_mbuf,
    mbuf_handle_t note_off_mbuf,
    MIDIChannel_t c,
    uint8_t muted,
    step_t* st
) {
    for(int i = 0; i < CONFIG_MAX_POLYPHONY; i++) {
        MIDINote_t n = st->note_off[i];

        if(n <= C8 && n >= A0) {
            MIDIPacket_t p = {
                .channel = c,
                .status = NOTE_OFF,
                .note = n,
                .velocity = 0,
            };

            mbuf_push(note_off_mbuf, p);
        }
    }

    if(!muted) {
        for(int i = 0; i < CONFIG_MAX_POLYPHONY; i++) {
            MIDINote_t n = st->note_on[i].note;
    
            if(n <= C8 && n >= A0) {
                MIDIPacket_t p = {
                    .channel = c,
                    .status = NOTE_ON,
                    .note = n,
                    .velocity = st->note_on[i].velocity,
                };
        
                mbuf_push(note_on_mbuf, p);
            }
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

     /*
        on startup the step_sector_addr will be zero. we don't immediately know
        which sectors the step data is stored. we need to read the first byte
        of all the sectors allocated to step data for this sequence to figure
        out which one contains the step data, then assign step_sector_addr to
        the address offset of this sector from the base sequence address
    */
    if(sq->step_sector_offset == 0) {
        sq->step_sector_offset = get_step_data_offset(sq, sq_index);
    }

    uint32_t steps_base_addr = sq_base_addr + sq->step_sector_offset;

    uint32_t current_step_addr = steps_base_addr + (sq->counter * BYTES_PER_STEP);

    flash_SPIRead(current_step_addr, data, data, BYTES_PER_STEP);
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
static int read_step(MIDISequence_t* sq, uint8_t sq_index, step_t* st) {
    if(ACTIVE_SQ == sq_index && SQ_EDIT_READY == 1) {
        read_step_from_edit_buffer(sq, st);
    } else {
        uint8_t data[BYTES_PER_STEP];
        read_step_from_memory(sq, sq_index, data);

        if(data[0] != NOTE_OFF) {
            return 1;
        }
        
        bytes_to_step(data, st);
    }

    return 0;
}

static uint8_t check_bit(uint8_t bit, uint32_t* field, uint8_t max) {
    uint8_t mask_size = 32;

    if(bit < max) {
        int8_t mask_index = bit / mask_size;
        uint32_t mute_mask = 1 << (bit % mask_size);

        if(field[mask_index] & mute_mask) {
            return 1;
        } else {
            return 0;
        }
    }

    return 0;
}

static uint8_t is_disabled(uint8_t step, uint32_t* enabled_steps) {
    return check_bit(step, enabled_steps, CONFIG_STEPS_PER_SEQUENCE);
}

static void goto_next_enabled_step(uint8_t* counter, uint32_t* enabled_steps) {
    uint8_t prev_counter = (*counter);

    for(uint8_t i = 0; i < CONFIG_STEPS_PER_SEQUENCE; i++) {
        (*counter)++;
        if((*counter) >= CONFIG_STEPS_PER_SEQUENCE) {
            (*counter) = 0;
        }

        if(!is_disabled((*counter), enabled_steps)) {
            break;
        }

        // exit if we've looped back to the starting 
        if((*counter) == prev_counter) {
            break;
        }


    }
}

/*
    load step data into a step struct then send the note data over midi

    @param sq_index The index of the currently process sequence in sequences
*/
static uint8_t load_step(uint8_t sq_index, step_t* st) {
    MIDISequence_t* sq = &sequences[sq_index];

    uint8_t* counter = &sq->counter;

    if(is_disabled(*counter, sq->enabled_steps)) {
        MIDICC_t p = {
            .status = CONTROLLER,
            .channel = sq->channel,
            .control = ALL_NOTES_OFF,
            .value = 0,
        };

        send_midi_control(USART1, &p);

        goto_next_enabled_step(counter, sq->enabled_steps);
    }

    if(read_step(sq, sq_index, st)) {
        send_uart(USART3, "Error data alignment\n\r", 22);
        return 1;
    }

    return 0;
}

static uint8_t is_muted(uint8_t step, uint32_t* muted_steps) {
    return check_bit(step, muted_steps, CONFIG_STEPS_PER_SEQUENCE);
}

/*
    play the current steps in the currently active sequences

    @param note_on_mbuf     midi packet buffer for note on packets
    @param note_off_mbuf    midi packet buffer for note off packets
*/
void load_sequences(mbuf_handle_t note_on_mbuf, mbuf_handle_t note_off_mbuf) {
    if(xSemaphoreTake(sq_mutex, portMAX_DELAY) == pdTRUE) {
        for(int i = 0; i < CONFIG_TOTAL_SEQUENCES; i++) {
            MIDISequence_t* sq = &sequences[i];
            if(sq->enabled) {
                step_t st;
                if(load_step(i, &st) == 0) {
                    uint8_t muted = is_muted(sq->counter, sq->muted_steps);

                    load_step_notes(
                        note_on_mbuf,
                        note_off_mbuf,
                        sq->channel,
                        muted,
                        &st);
                }
                    // if the end of sequence byte is hit then put the counter back to the start
                if(st.end_of_step == 0xFF){
                    sq->counter = 0;
                } else {
                    sq->counter++;
                }
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
    uint8_t sq_en = sequences[sq_index].enabled;
    if(sq_en) {
        disable_sequence(sq_index);
    } else {
        if(xSemaphoreTake(sq_mutex, portMAX_DELAY) == pdTRUE) {
            sequences[sq_index].enabled = 1;
            xSemaphoreGive(sq_mutex);        
        }
    }
}

void disable_sequence(uint8_t sq_index) {
    if(xSemaphoreTake(sq_mutex, portMAX_DELAY) == pdTRUE) {
        sequences[sq_index].enabled = 0;

        MIDICC_t p = {
            .status = CONTROLLER,
            .channel = sequences[sq_index].channel,
            .control = ALL_NOTES_OFF,
            .value = 0,
        };

        send_midi_control(USART1, &p);

        #ifdef CONFIG_RESET_SEQ_ON_DISABLE
            sequences[sq_index].counter = 0;
        #endif

        xSemaphoreGive(sq_mutex);
    }
}

void play_notes(mbuf_handle_t mbuf) {
    while(!mbuf_empty(mbuf)) {
        MIDIPacket_t p;
        mbuf_pop(mbuf, &p);
        send_midi_note(USART1, &p);
    }
}

void set_midi_channel(uint8_t sq_index, MIDIChannel_t channel) {
    if(xSemaphoreTake(sq_mutex, portMAX_DELAY) == pdTRUE) {
        sequences[sq_index].channel = channel;
        xSemaphoreGive(sq_mutex);
    }

}

MIDIChannel_t get_channel(uint8_t sq_index) {
    return sequences[sq_index].channel;
}