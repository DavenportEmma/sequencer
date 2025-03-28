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

void main_menu() {
    send_uart(USART3, "main_menu\n", 10);
}

void sq_menu() {
    send_uart(USART3, "sq_menu\n", 8);
}

void st_landing() {
    send_uart(USART3, "st_landing\n", 11);
}

void st_menu() {
    send_uart(USART3, "st_menu\n", 8);
}

void st_edit() {
    send_uart(USART3, "st_edit\n", 8);
}

void menu(uint8_t key) {
    static MenuState_t current = S_MAIN_MENU;
    MenuEvent_t event = decode_key(current, key);
    
    for(uint8_t i = 0; i < STATE_TABLE_SIZE; i++) {
        if( state_table[i].current == current &&
            state_table[i].event == event
        ) {
            current = state_table[i].next;
            (state_machine[current].func)();
        }
    }
}