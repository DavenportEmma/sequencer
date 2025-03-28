#include "uart.h"
#include "menu.h"
#include "sequence.h"
#include "midi.h"

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
static uint8_t key_to_sq_st(uint8_t key) {
    if(key > 0x0F && key < 0x50) {
        return key - 0x10;
    }

    return -1;
}

static MIDINote_t key_to_note(uint8_t key) {
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
volatile uint8_t SQ_EDIT_READY = 0;

static void main_menu(uint8_t key) {
    send_uart(USART3, "main_menu\n\r", 11);

    reset_step_edit_buffer();

    SQ_EDIT_READY=0;
}

static void sq_select(uint8_t key) {
    ACTIVE_SQ = key_to_sq_st(key);

    send_uart(USART3, "sq_select ", 10);
    send_hex(USART3, ACTIVE_SQ);
    send_uart(USART3, "\n\r", 2);

    menu(E_AUTO);
}

static void sq_menu(uint8_t key) {
    send_uart(USART3, "sq_menu ", 8);
    send_hex(USART3, ACTIVE_SQ);
    send_uart(USART3, "\n\r", 2);
}

static void sq_en(uint8_t key) {
    send_uart(USART3, "sq_en ", 6);
    send_hex(USART3, ACTIVE_SQ);
    send_uart(USART3, "\n\r", 2);

    toggle_sequence(ACTIVE_SQ);

    menu(E_AUTO);
}

static void st_landing(uint8_t key) {
    send_uart(USART3, "edit sq ", 8);
    send_hex(USART3, ACTIVE_SQ);
    send_uart(USART3, "\n\r", 2);

    if(load_sq_for_edit(ACTIVE_SQ)) {
        send_uart(USART3, "error loading sequence\n\r", 24);
    }
}

static void st_select(uint8_t key) {
    ACTIVE_ST = key_to_sq_st(key);

    send_uart(USART3, "st_select ", 10);
    send_hex(USART3, ACTIVE_ST);
    send_uart(USART3, "\n\r", 2);

    menu(E_AUTO);
}

static void st_menu(uint8_t key) {
    send_uart(USART3, "st_menu ", 8);
    send_hex(USART3, ACTIVE_ST);
    send_uart(USART3, "\n\r", 2);
}

static void prev(uint8_t key) { }

static void st_note(uint8_t key) {
    MIDINote_t note = key_to_note(key);

    if(note > 0) {
        send_uart(USART3, "st_note ", 8);
        send_hex(USART3, ACTIVE_ST);
        send_uart(USART3, " ", 1);
        send_hex(USART3, note);
        send_uart(USART3, "\n\r", 2);
    }

    menu(E_AUTO);
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
    { S_ST_LANDING, st_landing },
    { S_ST_SELECT, st_select },
    { S_ST_MENU, st_menu },
    { S_PREV, prev },
    { S_ST_NOTE, st_note },
};

void menu(uint16_t key) {
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

            (state_machine[current].func)(key);
            break;
        }
    }
}