#include "uart.h"
#include "menu.h"
#include "sequence.h"

MenuEvent_t decode_key(MenuState_t current, uint8_t key) {
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

static uint8_t ACTIVE_SQ; 
static uint8_t ACTIVE_ST; 

static void main_menu(uint8_t key) {
    send_uart(USART3, "main_menu\n", 10);
}

static void sq_select(uint8_t key) {
    ACTIVE_SQ = key_to_sq_st(key);
    send_uart(USART3, "sq_select ", 10);
    send_hex(USART3, ACTIVE_SQ);
    send_uart(USART3, "\n", 1);
}

static void sq_menu(uint8_t key) {
    send_uart(USART3, "sq_menu\n", 8);
}

static void sq_en(uint8_t key) {
    send_uart(USART3, "sq_en ", 6);
    send_hex(USART3, ACTIVE_SQ);
    send_uart(USART3, "\n", 1);
    toggle_sequence(ACTIVE_SQ);
}

static void st_landing(uint8_t key) {
    send_uart(USART3, "st_landing\n", 11);
}

static void st_select(uint8_t key) {
    ACTIVE_ST = key_to_sq_st(key);
    send_uart(USART3, "st_select ", 10);
    send_hex(USART3, ACTIVE_ST);
    send_uart(USART3, "\n", 1);
}

static void st_menu(uint8_t key) {
    send_uart(USART3, "st_menu\n", 8);
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
};

void menu(uint8_t key) {
    static MenuState_t current = S_MAIN_MENU;
    MenuEvent_t event = decode_key(current, key);
    
    for(uint8_t i = 0; i < STATE_TABLE_SIZE; i++) {
        if((state_table[i].current == current && state_table[i].event == event) ||
            (state_table[i].current == current && state_table[i].event == E_AUTO)
        ) {
            current = state_table[i].next;
            (state_machine[current].func)(key);
        }
    }
}