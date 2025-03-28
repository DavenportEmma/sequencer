#include "uart.h"
#include "menu.h"

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

static uint8_t ACTIVE_SQ; 
static uint8_t ACTIVE_ST; 

void main_menu(uint8_t key) {
    send_uart(USART3, "main_menu\n", 10);
}

void sq_select(uint8_t key) {
    ACTIVE_SQ = key;
    send_uart(USART3, "sq_select ", 10);
    send_hex(USART3, key);
    send_uart(USART3, "\n", 1);
}

void sq_menu(uint8_t key) {
    send_uart(USART3, "sq_menu\n", 8);
}

void sq_en(uint8_t key) {
    send_uart(USART3, "sq_en ", 6);
    send_hex(USART3, ACTIVE_SQ);
    send_uart(USART3, "\n", 1);
}

void st_landing(uint8_t key) {
    send_uart(USART3, "st_landing\n", 11);
}

void st_select(uint8_t key) {
    ACTIVE_ST = key;
    send_uart(USART3, "st_select ", 10);
    send_hex(USART3, key);
    send_uart(USART3, "\n", 1);
}

void st_menu(uint8_t key) {
    send_uart(USART3, "st_menu\n", 8);
}

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