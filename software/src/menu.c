#include "uart.h"
#include "menu.h"
#include "sequence.h"

static MenuEvent_t decode_key(MenuState_t current, uint16_t key) {
    /*
        these are the 64 keys that represent the sequences or steps depending on
        the current state
    */
    if(key > 0x0F && key < 0x50) {
        /*
            menu states 0 and 1 are main menu and sequence menu. the central 64
            keys represent sequences here
        */
        if(current < 2) {
            return E_SQ_SELECT;
        } else {
            return E_ST_SELECT;
        }
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