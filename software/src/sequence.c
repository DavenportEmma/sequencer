#include "FreeRTOS.h"
#include "semphr.h"
#include "midi.h"
#include "sequence.h"
#include "uart.h"
#include "flash.h"
#include "common.h"
#include "m_buf.h"
#include "util.h"
#include <string.h>

extern SemaphoreHandle_t sq_mutex;
extern SemaphoreHandle_t st_mask_mutex;

extern MIDISequence_t sequences[CONFIG_TOTAL_SEQUENCES];

extern step_t steps[CONFIG_TOTAL_SEQUENCES * CONFIG_STEPS_PER_SEQUENCE];

extern uint8_t ACTIVE_SQ;
extern uint8_t ACTIVE_ST;

static uint32_t enabled_sequences[2];
static uint32_t break_sequences[2];
static uint32_t queued_sequences[2];

/*
    read the midi channel of the sequence from flash memory. The sequence
    metadata is stored in the first sector of the block. The midi channel
    information is stored in the first byte of the first page of the block 

    @param sq_index     Index of sequence (0-63)

    @return The midi channel of the sequence
*/
uint8_t init_sequences() {
    memset(enabled_sequences, 0, sizeof(uint32_t) * 2);

    uint32_t addr = 0x000000;
    // TODO handle this metadata shite
    uint8_t metadata[256];
    flash_SPIRead(addr, metadata, metadata, 256);

    for(int i = 0; i < CONFIG_TOTAL_SEQUENCES; i++) {
        sequences[i].channel = metadata[i*4];
    }

    addr+=256;

    flash_SPIRead(addr, (uint8_t*)steps, (uint8_t*)steps, sizeof(steps));

    return 0;
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
        MIDINote_t n = st->note_off[i] & 0x7F;

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
            MIDINote_t n = st->note_on[i].note & 0x7F;
    
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

static uint8_t is_disabled(uint32_t* enabled_steps, uint8_t step) {
    uint8_t ret;    

    if(xSemaphoreTake(st_mask_mutex, portMAX_DELAY) == pdTRUE) {
        ret = check_bit(enabled_steps, step, CONFIG_STEPS_PER_SEQUENCE);
        xSemaphoreGive(st_mask_mutex);
    }

    return ret; 
}

static void goto_next_enabled_step(uint8_t* counter, uint32_t* enabled_steps) {
    uint8_t prev_counter = (*counter);

    for(uint8_t i = 0; i < CONFIG_STEPS_PER_SEQUENCE; i++) {
        (*counter)++;
        if((*counter) >= CONFIG_STEPS_PER_SEQUENCE) {
            (*counter) = 0;
        }

        if(!is_disabled(enabled_steps, (*counter))) {
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

    @param sq           Pointer to a copy of the active sequence in sq_index
    @param sq_index     Index of the active sequence in sequences[]
    @param st           Step struct to be filled with step data
*/
static step_t get_step(MIDISequence_t* sq, uint8_t sq_index) {
    uint8_t* counter = &sq->counter;

    if(is_disabled(sq->enabled_steps, (*counter))) {
        MIDICC_t p = {
            .status = CONTROLLER,
            .channel = sq->channel,
            .control = ALL_NOTES_OFF,
            .value = 0,
        };

        send_midi_control(USART1, &p);

        goto_next_enabled_step(counter, sq->enabled_steps);
    }

    uint16_t seq_base_index = ((uint16_t)sq_index * CONFIG_STEPS_PER_SEQUENCE);
    uint16_t index = seq_base_index + (uint16_t)sq->counter;

    return steps[index];
}

static uint8_t is_muted(uint32_t* muted_steps, uint8_t step) {
    uint8_t ret;    

    if(xSemaphoreTake(st_mask_mutex, portMAX_DELAY) == pdTRUE) {
        ret = check_bit(muted_steps, step, CONFIG_STEPS_PER_SEQUENCE);
        xSemaphoreGive(st_mask_mutex);
    }

    return ret;
}

static void load_sequence(uint8_t sq_index, mbuf_handle_t note_on_mbuf, mbuf_handle_t note_off_mbuf) {
    MIDISequence_t* sq = &sequences[sq_index];
    /*
        TODO look into potential problems that may arise from other tasks
        modifying sequences[i] in between the first and second mutex takes
    
        potential causes
            midi channel change
            changing enabled steps
            changing sequence enable
            changing muted steps
    */

    if(check_bit(enabled_sequences, sq_index, CONFIG_TOTAL_SEQUENCES)) {
        // queue all sequences that are triggered on this one
        if(sq->counter == 0) {
            queued_sequences[0] |= sq->queue[0];
            queued_sequences[1] |= sq->queue[1];
            memset(sq->queue, 0, sizeof(uint32_t) * 2);
        }

        step_t st = get_step(sq, sq_index);

        uint8_t muted = is_muted(sq->muted_steps, sq->counter);

        load_step_notes(
            note_on_mbuf,
            note_off_mbuf,
            sq->channel,
            muted,
            &st);

        goto_next_enabled_step(&sq->counter, &sq->enabled_steps);
    }
}

/*
    play the current steps in the currently active sequences

    @param note_on_mbuf     midi packet buffer for note on packets
    @param note_off_mbuf    midi packet buffer for note off packets
*/
void load_sequences(mbuf_handle_t note_on_mbuf, mbuf_handle_t note_off_mbuf) {
    for(int i = 0; i < CONFIG_TOTAL_SEQUENCES; i++) {
        load_sequence(i, note_on_mbuf, note_off_mbuf);
    }

    /*
        if a sequence is queued but is already playing then we don't want to
        restart it so we will unset that bit in queued_sequences
    */
    queued_sequences[0] &= ~enabled_sequences[0];
    queued_sequences[1] &= ~enabled_sequences[1];

    /*
        there may be a case where sequence N is triggering on sequence M where
        N < M. N will have already been processed in the first load_sequences()
        but will not be playing yet as it hasn't been enabled. we look at
        the sequences that are queued are are not already enabled, then enable
        them. This will keep N in sync with M
    */
    for(int i = 0; i < CONFIG_TOTAL_SEQUENCES; i++) {
        if(check_bit(queued_sequences, i, CONFIG_TOTAL_SEQUENCES)) {
            enable_sequence(i);
            load_sequence(i, note_on_mbuf, note_off_mbuf);
            clear_bit(queued_sequences, i, CONFIG_TOTAL_SEQUENCES);
        }
    }

    return;
}

/*
    toggle a sequence between enabled and disabled

    @param sq_index The index of the currently process sequence in sequences
*/
void toggle_sequence(uint8_t sq_index) {
    uint8_t array_index = sq_index / 32;
    uint8_t bit_position = sq_index % 32;

    enabled_sequences[array_index] ^= (1 << bit_position);
}

void toggle_sequences(uint32_t* select_mask, uint8_t max) {
    for(int i = 0; i < max; i++) {
        uint8_t array_index = i / 32;
        uint8_t bit_position = i % 32;

        if(select_mask[array_index] & (1 << bit_position)) {
            toggle_sequence(i);
        }
    }
}

void enable_sequence(uint8_t sq_index) {
    uint8_t array_index = sq_index / 32;
    uint8_t bit_position = sq_index % 32;

    enabled_sequences[array_index] |= (1 << bit_position);
}

void disable_sequence(uint8_t sq_index) {
    #ifdef CONFIG_RESET_SEQ_ON_DISABLE
        sequences[sq_index].counter = 0;
    #endif

    uint8_t array_index = sq_index / 32;
    uint8_t bit_position = sq_index % 32;

    enabled_sequences[array_index] &= ~(1 << bit_position);

    MIDICC_t p = {
        .status = CONTROLLER,
        .channel = sequences[sq_index].channel,
        .control = ALL_NOTES_OFF,
        .value = 0,
    };

    send_midi_control(USART1, &p);
}

void clear_sequence(uint8_t sq_index) {
    disable_sequence(sq_index);

    uint16_t seq_base_index = ((uint16_t)sq_index * CONFIG_STEPS_PER_SEQUENCE);
    uint16_t end_of_sequence = seq_base_index + CONFIG_STEPS_PER_SEQUENCE;

    note_t note_on[CONFIG_MAX_POLYPHONY] = {0};
    MIDINote_t note_off[CONFIG_MAX_POLYPHONY] = {0};

    for(uint32_t i = seq_base_index; i < end_of_sequence; i++) {
        memcpy(steps[i].note_on, note_on, sizeof(note_on));
        memcpy(steps[i].note_off, note_off, sizeof(note_off));
    }
}

static void save_array(uint32_t base_addr, uint8_t* ptr, uint32_t size) {
    uint16_t page_size = 0x100;
    uint8_t rx[256];

    for(uint32_t offset = 0; offset < size; offset+=page_size) {
        uint16_t bytes_to_write = page_size;

        if(offset + page_size > size) {
            bytes_to_write = size - offset;
        }

        flash_programPage(base_addr, ptr + offset, rx, bytes_to_write);

        base_addr += bytes_to_write;
    }
}

void save_data() {
    flash_eraseSector(0x000000);
    flash_eraseSector(0x010000);

    uint32_t addr = 0;
    for(int i = 0; i < CONFIG_TOTAL_SEQUENCES; i++) {
        uint8_t tx[4] = {(uint8_t)sequences[i].channel, 0, 0, 0};

        flash_programPage(addr, tx, tx, 4);

        addr+=4;
        if(addr >= 256) {
            break;
        }
    }

    save_array(0x100, (uint8_t*)steps, sizeof(steps));
}

void play_notes(mbuf_handle_t mbuf) {
    while(!mbuf_empty(mbuf)) {
        MIDIPacket_t p;
        mbuf_pop(mbuf, &p);
        send_midi_note(USART1, &p);
    }
}

/*
    why do these two functions not use mutexes? we use the sq_mutex to protect
    the sequences array as the sequences are being played. these functions are
    only called in the sq_midi state which always disables the sequence on entry
*/
void set_midi_channel(uint8_t sq_index, MIDIChannel_t channel) {
    sequences[sq_index].channel = channel;
}

MIDIChannel_t get_channel(uint8_t sq_index) {
    return sequences[sq_index].channel;
}