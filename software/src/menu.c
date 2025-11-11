#include "uart.h"
#include "menu.h"
#include "sequence.h"
// #include "step_edit_buffer.h"
#include "step_editor.h"
#include "midi.h"
#include "k_buf.h"
#include "util.h"
#include "display.h"
#include <string.h>

#define CONFIG_DEBUG_PRINT

extern kbuf_handle_t uart_intr_kbuf;
extern MIDISequence_t sequences[CONFIG_TOTAL_SEQUENCES];

uint32_t SQ_MSEL_MASK[2];  // 64 bit field to identify multi selected sq
uint32_t ST_MSEL_MASK[2];

static MenuEvent_t decode_step_operation(MenuState_t current, uint16_t key) {
    if(key > 0x0F && key < 0x50) {
        return E_ST_SELECT;
    } else if ((key >= 0x61 && key <= 0x66) || (key >= 0x70 && key <= 0x77)) {
        return E_ST_NOTE;
    } else {
        return key;
    }
}

static MenuEvent_t decode_key(MenuState_t current, uint16_t key) {
    /*
        these are the 64 keys that represent the sequences or steps depending on
        the current state
    */
    switch(current) {
        case S_MAIN_MENU:
        case S_SQ_MENU:
        case S_QUEUE_TRIG_SEL:
            if(key > 0x0F && key < 0x50) {
                return E_SQ_SELECT;
            }
            break;

        case S_ST_LANDING:
        case S_ST_MENU:
            return decode_step_operation(current, key);

        default:
            return key;
    }

    return key;
}

// converts a key to a sequence or step number
// this is needed as the key representing the first step/sequence is not key 0
static uint8_t key_to_sq_st(uint16_t key) {
    if(key > 0x0F && key < 0x50) {
        return key - 0x10;
    }

    return -1;
}

static MIDINote_t key_to_note(uint16_t key) {
    switch(key) {
        case 0x61:
            return CSHARP3;
        case 0x62:
            return DSHARP3;
        case 0x64:
            return FSHARP3;
        case 0x65:
            return GSHARP3;
        case 0x66:
            return ASHARP3;
        case 0x70:
            return C3;
        case 0x71:
            return D3;
        case 0x72:
            return E3;
        case 0x73:
            return F3;
        case 0x74:
            return G3;
        case 0x75:
            return A3;
        case 0x76:
            return B3;
        case 0x77:
            return C4;
    }

    return 0;
}

volatile uint8_t ACTIVE_SQ; 
volatile uint8_t ACTIVE_ST; 

extern float TEMPO_PERIOD_MS;

static void advance_active_st() {
    ACTIVE_ST++;

    if(ACTIVE_ST >= CONFIG_STEPS_PER_SEQUENCE) {
        ACTIVE_ST = 0;
    }
}

static void retreat_active_st() {
    if(ACTIVE_ST == 0) {
        ACTIVE_ST = CONFIG_STEPS_PER_SEQUENCE - 1;
    } else {
        ACTIVE_ST--;
    }

}

static void main_menu(uint16_t key, uint16_t hold) {
    // reset active sq
    ACTIVE_SQ = 0xFF;
    clear_display();
    display_line("select sequence", 0);

    #ifdef CONFIG_DEBUG_PRINT
        send_uart(USART3, "select sequence\n\r", 17);
    #endif
}

static void sq_select(uint16_t key, uint16_t hold) {
    uint8_t sq_val = key_to_sq_st(key);

    if(hold == E_NO_HOLD) {
        clear_field(SQ_MSEL_MASK, CONFIG_TOTAL_SEQUENCES);

        set_bit(SQ_MSEL_MASK, sq_val, CONFIG_TOTAL_SEQUENCES);
    } else if(hold == E_SHIFT) {
        set_bit_range(SQ_MSEL_MASK, ACTIVE_SQ, sq_val, CONFIG_TOTAL_SEQUENCES);
    } else if(hold == E_CTRL) {
        set_bit(SQ_MSEL_MASK, sq_val, CONFIG_TOTAL_SEQUENCES);
    }

    ACTIVE_SQ = sq_val;

    menu(E_AUTO, E_NO_HOLD);
}

static void sq_menu(uint16_t key, uint16_t hold) {
    display_line("edit sequence", 0);
    
    #ifdef CONFIG_DEBUG_PRINT
        send_uart(USART3, "edit sequence ", 14);
        send_hex(USART3, ACTIVE_SQ);
        send_uart(USART3, "\n\r", 2);
    #endif
}

static void sq_en(uint16_t key, uint16_t hold) {
    #ifdef CONFIG_DEBUG_PRINT
        send_uart(USART3, "enable sequence\n\r", 17);
    #endif

    if(one_bit_set(SQ_MSEL_MASK)) {
        toggle_sequence(ACTIVE_SQ);
    } else {
        toggle_sequences(SQ_MSEL_MASK, CONFIG_TOTAL_SEQUENCES);
    }

    menu(E_AUTO, E_NO_HOLD);
}

static void sq_midi(uint16_t key, uint16_t hold) {
    display_line("midi", 0);

    static MIDIChannel_t channel;
    switch(key) {
        case E_SQ_MIDI:
            disable_sequence(ACTIVE_SQ);

            channel = get_channel(ACTIVE_SQ);

            break;
        case E_ENCODER_UP:
            if(channel < PORT_D_CHANNEL_16) {
                channel++;
            }

            set_midi_channel(ACTIVE_SQ, channel);

            break;

        case E_ENCODER_DOWN:
            if(channel > PORT_A_CHANNEL_1) {
                channel--;
            }

            set_midi_channel(ACTIVE_SQ, channel);

            break;
        default:
            break;
    }

    #ifdef CONFIG_DEBUG_PRINT
        send_uart(USART3, "midi ", 5);
        send_uart(USART3, "channel ", 8);
        send_hex(USART3, (channel & 0x0F));
        send_uart(USART3, " port ", 6);
        send_hex(USART3, ((channel & 0xF0) >> 4));
        send_uart(USART3, "\n\r", 2);
    #endif

    char port_str[] = "00";
    uint8_t port_num = (uint8_t)(((channel & 0xF0) >> 4) + 1);
    num_to_str(port_num, port_str, 2);
    display_line(port_str, 1);

    char channel_str[] = "00";
    uint8_t channel_num = (uint8_t)((channel & 0x0F)+1); 
    num_to_str(channel_num, channel_str, 2);
    display_line(channel_str, 2);
}

static void st_landing(uint16_t key, uint16_t hold) {
    display_line("select step", 0);

    #ifdef CONFIG_DEBUG_PRINT
        send_uart(USART3, "select step\n\r", 13);
    #endif
}

static void st_select(uint16_t key, uint16_t hold) {
    uint8_t st_val = key_to_sq_st(key);
    
    #ifdef CONFIG_DEBUG_PRINT
        send_uart(USART3, "step ", 5);
        send_hex(USART3, st_val);
        send_uart(USART3, "\n\r", 2);
    #endif

    if(hold == E_NO_HOLD) {
        clear_field(ST_MSEL_MASK, CONFIG_STEPS_PER_SEQUENCE);

        set_bit(ST_MSEL_MASK, st_val, CONFIG_STEPS_PER_SEQUENCE);
    } else if(hold == E_SHIFT) {
        set_bit_range(ST_MSEL_MASK, ACTIVE_ST, st_val, CONFIG_STEPS_PER_SEQUENCE);
    } else if(hold == E_CTRL) {
        set_bit(ST_MSEL_MASK, st_val, CONFIG_STEPS_PER_SEQUENCE);
    }

    ACTIVE_ST = st_val;

    menu(E_AUTO, E_NO_HOLD);
}

static void st_menu(uint16_t key, uint16_t hold) {
    display_line("edit step", 0);

    #ifdef CONFIG_DEBUG_PRINT
        send_uart(USART3, "edit step ", 10);
        send_hex(USART3, ACTIVE_ST);
        send_uart(USART3, "\n\r", 2);
    #endif

    kbuf_reset(uart_intr_kbuf);
    uart_enable_rx_intr(USART1);
}

/*
    this is a dummy state used to represent a return to the previous state of
    the state machine
*/
static void prev(uint16_t key, uint16_t hold) { }

static void st_note(uint16_t key, uint16_t hold) {
    /*
        if the transition to this state was triggered by a keystroke from the
        on-board 13 key keyboard then `key` is going to be able to be decoded
        by key_to_note
        
        if the transition was triggered by an external midi signal processed by
        a usart interrupt then `key` is going to be the value of E_ST_NOTE which
        at the time of writing this is 96 (see menu.h MenuEvent_t definition)
    */
    int midi = kbuf_ready(uart_intr_kbuf);

    MIDIStatus_t status = NOTE_ON;
    MIDINote_t note;
    uint8_t velocity = 0x3F;
    uint8_t auto_fill_next_note_off = 0;

    if(midi) {
        uint8_t* buf = uart_intr_kbuf->buffer;

        status = buf[0] & 0xF0;
        note = buf[1];
        velocity = buf[2];

        kbuf_reset(uart_intr_kbuf);

    } else {
        note = key_to_note(key);
        auto_fill_next_note_off = 1;
    }
    
    // key to note returns 0 for a keystroke outside of 13 key keyboard
    if(note > 0) {
        edit_step_note(ACTIVE_SQ, ACTIVE_ST, status, note, velocity, auto_fill_next_note_off);
        
        #ifdef CONFIG_DEBUG_PRINT
            send_hex(USART3, note);
            send_uart(USART3, "\n\r", 2);
        #endif
    }

    #ifdef CONFIG_AUTO_INC_STEP_ON_MIDI_IN
    if(status == NOTE_ON) {
        advance_active_st();
    }
    #endif

    menu(E_AUTO, E_NO_HOLD);
}

static void st_mute(uint16_t key, uint16_t hold) {
    #ifdef CONFIG_DEBUG_PRINT
        send_uart(USART3, "mute ", 5);
        send_hex(USART3, ACTIVE_ST);
        send_uart(USART3, "\n\r", 2);
    #endif

    if(one_bit_set(ST_MSEL_MASK)) {
        mute_step(ACTIVE_SQ, ACTIVE_ST);
    } else {
        for(int i = 0; i < CONFIG_STEPS_PER_SEQUENCE; i++) {
            uint8_t array_index = i / 32;
            uint8_t bit_position = i % 32;

            if(ST_MSEL_MASK[array_index] & (1 << bit_position)) {
                mute_step(ACTIVE_SQ, i);
            }
        }
    }

    menu(E_AUTO, E_NO_HOLD);
}

static void st_en(uint16_t key, uint16_t hold) {
    #ifdef CONFIG_DEBUG_PRINT
        send_uart(USART3, "enable ", 7);
        send_hex(USART3, ACTIVE_ST);
        send_uart(USART3, "\n\r", 2);
    #endif

    if(one_bit_set(ST_MSEL_MASK)) {
        toggle_step(ACTIVE_SQ, ACTIVE_ST);
    } else {
        for(int i = 0; i < CONFIG_STEPS_PER_SEQUENCE; i++) {
            uint8_t array_index = i / 32;
            uint8_t bit_position = i % 32;

            if(ST_MSEL_MASK[array_index] & (1 << bit_position)) {
                toggle_step(ACTIVE_SQ, i);
            }
        }
    }

    menu(E_AUTO, E_NO_HOLD);
}

static void st_prev(uint16_t key, uint16_t hold) {
    retreat_active_st();

    menu(E_AUTO, E_NO_HOLD);
}

static void st_next(uint16_t key, uint16_t hold) {
    advance_active_st();

    menu(E_AUTO, E_NO_HOLD);
}

static void st_vel_down(uint16_t key, uint16_t hold) {
    #ifdef CONFIG_DEBUG_PRINT
        send_uart(USART3, "decrease velocity\n\r", 19);
    #endif

    edit_step_velocity(ACTIVE_SQ, ACTIVE_ST, -1);

    menu(E_AUTO, E_NO_HOLD);
}

static void st_vel_up(uint16_t key, uint16_t hold) {
    #ifdef CONFIG_DEBUG_PRINT
        send_uart(USART3, "increase velocity\n\r", 19);
    #endif
    
    edit_step_velocity(ACTIVE_SQ, ACTIVE_ST, 1);

    menu(E_AUTO, E_NO_HOLD);
}

static void st_clear(uint16_t key, uint16_t hold) {
    #ifdef CONFIG_DEBUG_PRINT
        send_uart(USART3, "clear step\n\r", 12);
    #endif
    
    clear_step(ACTIVE_SQ, ACTIVE_ST);

    menu(E_AUTO, E_NO_HOLD);
}

static void sq_clear(uint16_t key, uint16_t hold) {
    #ifdef CONFIG_DEBUG_PRINT
        send_uart(USART3, "clear sequence\n\r", 16);
    #endif

    clear_sequence(ACTIVE_SQ);

    menu(E_AUTO, E_NO_HOLD);
}

static void save(uint16_t key, uint16_t hold) {
    #ifdef CONFIG_DEBUG_PRINT
        send_uart(USART3, "saving\n\r", 8);
    #endif

    save_data();

    menu(E_AUTO, E_NO_HOLD);
}

static void sq_queue_trig_sel(uint16_t key, uint16_t hold) {
    #ifdef CONFIG_DEBUG_PRINT
        send_uart(USART3, "select queue trigger\n\r", 22);
    #endif
}

static void sq_queue(uint16_t key, uint16_t hold) {
    uint8_t sq_val = key_to_sq_st(key);
    
    #ifdef CONFIG_DEBUG_PRINT
        send_uart(USART3, "triggering on sq ", 17);
        send_hex(USART3, sq_val);
        send_uart(USART3, "\n\r", 2);

    #endif

    memcpy(sequences[sq_val].queue, SQ_MSEL_MASK, sizeof(SQ_MSEL_MASK));

    menu(E_AUTO, E_NO_HOLD);
}

static void sq_break(uint16_t key, uint16_t hold) {
    #ifdef CONFIG_DEBUG_PRINT
        send_uart(USART3, "break sq ", 9);
        send_hex(USART3, ACTIVE_SQ);
        send_uart(USART3, "\n\r", 2);
    #endif


    if(one_bit_set(SQ_MSEL_MASK)) {
        break_sequence(ACTIVE_SQ);
    } else {
        for(int i = 0; i < CONFIG_TOTAL_SEQUENCES; i++) {
            uint8_t array_index = i / 32;
            uint8_t bit_position = i % 32;

            if(SQ_MSEL_MASK[array_index] & (1 << bit_position)) {
                break_sequence(i);
            }
        }
    }
    menu(E_AUTO, E_NO_HOLD);
}

static void tempo(uint16_t key, uint16_t hold) {
    static uint8_t tempo = CONFIG_TEMPO;

    switch(key) {
        case E_TEMPO:
            break;
        case E_ENCODER_UP:
            tempo++;

            if(tempo > 280) {
                tempo = 280;
            }

            break;
        case E_ENCODER_DOWN:
            tempo--;

            if(tempo < 60) {
                tempo = 60;
            }

            break;
        default:
            break;
    }

    TEMPO_PERIOD_MS = 15000/tempo;

    #ifdef CONFIG_DEBUG_PRINT
        send_uart(USART3, "tempo ", 6);
        send_hex(USART3, tempo);
        send_uart(USART3, "\n\r", 2);
    #endif
}

/*
the order of the elements in this array MUST be in the same order as the the 
elements in MenuState_t enum defined in menu.h. I am dumb
*/
StateMachine_t state_machine[] = {
    { S_MAIN_MENU, main_menu },
    { S_SQ_SELECT, sq_select },
    { S_SQ_MENU, sq_menu },
    { S_SQ_EN, sq_en },
    { S_SQ_MIDI, sq_midi },
    { S_ST_LANDING, st_landing },
    { S_ST_SELECT, st_select },
    { S_ST_MENU, st_menu },
    { S_PREV, prev },
    { S_ST_NOTE, st_note },
    { S_ST_MUTE, st_mute },
    { S_ST_EN, st_en },
    { S_ST_PREV, st_prev },
    { S_ST_NEXT, st_next },
    { S_ST_VEL_DOWN, st_vel_down },
    { S_ST_VEL_UP, st_vel_up },
    { S_ST_CLR, st_clear },
    { S_SQ_CLR, sq_clear },
    { S_SAVE, save },
    { S_QUEUE_TRIG_SEL, sq_queue_trig_sel },
    { S_QUEUE, sq_queue },
    { S_BREAK, sq_break },
    { S_TEMPO, tempo },
};

void menu(uint16_t key, uint16_t hold) {
    uart_disable_rx_intr(USART1);

    static MenuState_t current = S_MAIN_MENU;
    static MenuState_t previous = S_MAIN_MENU;

    MenuEvent_t event = decode_key(current, key);
    
    for(uint8_t i = 0; i < STATE_TABLE_SIZE; i++) {
        if((state_table[i].current == current && state_table[i].event == event) ||
            (state_table[i].current == current && state_table[i].event == E_AUTO)
        ) {
            if(state_table[i].next == S_PREV) {
                current = previous;
            } else {
                previous = current;
                current = state_table[i].next;    
            }

            (state_machine[current].func)(key, hold);
            break;
        }
    }
}


void USART1_IRQHandler(void) {
    if(USART1->ISR & USART_ISR_RXNE) {
        while(USART1->ISR & (USART_ISR_RXNE | USART_ISR_ORE)) {
            uint8_t d = USART1->RDR;
            kbuf_push(uart_intr_kbuf, d);

            if(kbuf_size(uart_intr_kbuf) >= 3) {
                USART1->ICR |= USART_ICR_ORECF;
                uart_intr_kbuf->ready = 1;
            }
        }
    }
}