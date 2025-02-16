#ifndef _MENU_H
#define _MENU_H
#include <stddef.h>

/*
SQ_MENU     = 0x09,
SQ_CLR      = 0x0A,
SQ_MIDI     = 0x0B,
SQ_CLK      = 0x0C,
SQ_PLAY     = 0x00,
SQ_COPY     = 0x05,
SQ_PASTE    = 0x06,
SQ_DEL      = 0x07,
ST_CLR      = 0x58,
ST_COPY     = 0x50,
ST_PASTE    = 0x51,
ST_EN       = 0x52,
ST_MUTE     = 0x53,
ST_EDIT     = 0x54,
ST_PREV     = 0x55,
ST_NEXT     = 0x56,
ST_DEL      = 0x57
*/

/*
the order of the elements in this enum MUST be in the same order as the the 
elements in state_machine[] defined in menu.c. I am dumb
*/
typedef enum {
    S_MAIN_MENU,
    S_SQ_SELECT,
    S_SQ_MENU,
    S_SQ_EN,
    S_ST_LANDING,
    S_ST_SELECT,
    S_ST_MENU,
} MenuState_t;

typedef enum {
    E_MAIN_MENU = 0x09,
    E_SQ_EN = 0x00,
    E_SQ_SELECT,
    E_SQ_EDIT = 0x04,
    E_ST_SELECT,
    E_ST_EDIT = 0x54,
    E_ST_PITCH = 0x61,
    E_ST_EN = 0x52,
    E_ST_MUTE = 0x53,
    E_ST_DEL = 0x57,
    E_AUTO = 0xFFFF,
} MenuEvent_t;

typedef struct {
    MenuState_t current;
    MenuEvent_t event;
    MenuState_t next;
} MenuTransition_t;

static const MenuTransition_t state_table[] = {
    {S_MAIN_MENU, E_MAIN_MENU, S_MAIN_MENU},
    {S_MAIN_MENU, E_SQ_SELECT, S_SQ_SELECT},

    {S_SQ_SELECT, E_AUTO, S_SQ_MENU},

    {S_SQ_MENU, E_MAIN_MENU, S_MAIN_MENU},
    {S_SQ_MENU, E_SQ_EDIT, S_ST_LANDING},
    {S_SQ_MENU, E_SQ_SELECT, S_SQ_SELECT},
    {S_SQ_MENU, E_SQ_EN, S_SQ_EN},

    {S_SQ_EN, E_AUTO, S_SQ_MENU},

    {S_ST_LANDING, E_MAIN_MENU, S_MAIN_MENU},
    {S_ST_LANDING, E_ST_SELECT, S_ST_SELECT},
    
    {S_ST_SELECT, E_AUTO, S_ST_MENU},

    {S_ST_MENU, E_MAIN_MENU, S_MAIN_MENU},
    {S_ST_MENU, E_ST_SELECT, S_ST_SELECT},
};

#define STATE_TABLE_SIZE (sizeof(state_table) / sizeof(state_table[0]))

typedef struct {
    MenuState_t state;
    void (*func)(uint8_t key);
} StateMachine_t;

MenuEvent_t decode_key(MenuState_t current, uint8_t key);
void menu(uint8_t key);

#endif // _MENU_H