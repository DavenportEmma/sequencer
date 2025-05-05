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
    S_SQ_MIDI,
    S_ST_LANDING,
    S_ST_SELECT,
    S_ST_MENU,
    S_PREV,
    S_ST_NOTE,
    S_ST_MUTE,
    S_ST_EN,
    S_ST_PREV,
    S_ST_NEXT,
    S_ST_VEL_DOWN,
    S_ST_VEL_UP,
    S_ST_CLR,
    S_SQ_CLR,
} MenuState_t;

typedef enum {
    E_MAIN_MENU = 0x01,
    E_SQ_MIDI = 0x03,
    E_SHIFT = 0x05,
    E_CTRL = 0x06,
    E_SQ_EN = 0x08,
    E_SQ_SELECT,
    E_SQ_EDIT = 0x0C,
    E_SQ_CLR = 0x0F,
    E_ST_SELECT,
    E_ST_EDIT = 0x5C,
    E_ST_PITCH = 0x69,
    E_ST_EN = 0x5A,
    E_ST_MUTE = 0x5B,
    E_ST_PREV = 0x5D,
    E_ST_NEXT = 0x5E,
    E_ST_CLR = 0x5F,
    E_ST_NOTE,
    E_NO_HOLD = 0xFFFB,
    E_ENCODER_UP = 0xFFFE,  // see tasks.c inside key scan task for why I used
    E_ENCODER_DOWN = 0xFFFC,// these values
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
    {S_SQ_MENU, E_SQ_MIDI, S_SQ_MIDI},
    {S_SQ_MENU, E_SQ_CLR, S_SQ_CLR},
    
    {S_SQ_MIDI, E_MAIN_MENU, S_MAIN_MENU},
    {S_SQ_MIDI, E_ENCODER_UP, S_SQ_MIDI},
    {S_SQ_MIDI, E_ENCODER_DOWN, S_SQ_MIDI},

    {S_SQ_EN, E_AUTO, S_PREV},

    {S_SQ_CLR, E_AUTO, S_SQ_MENU},

    {S_ST_LANDING, E_MAIN_MENU, S_MAIN_MENU},
    {S_ST_LANDING, E_ST_SELECT, S_ST_SELECT},
    {S_ST_LANDING, E_SQ_EN, S_SQ_EN},
    
    {S_ST_SELECT, E_AUTO, S_ST_MENU},

    {S_ST_MENU, E_MAIN_MENU, S_MAIN_MENU},
    {S_ST_MENU, E_ST_SELECT, S_ST_SELECT},
    {S_ST_MENU, E_SQ_EN, S_SQ_EN},
    {S_ST_MENU, E_ST_NOTE, S_ST_NOTE},
    {S_ST_MENU, E_ST_MUTE, S_ST_MUTE},
    {S_ST_MENU, E_ST_EN, S_ST_EN},
    {S_ST_MENU, E_ST_PREV, S_ST_PREV},
    {S_ST_MENU, E_ST_NEXT, S_ST_NEXT},
    {S_ST_MENU, E_ENCODER_DOWN, S_ST_VEL_DOWN},
    {S_ST_MENU, E_ENCODER_UP, S_ST_VEL_UP},
    {S_ST_MENU, E_ST_CLR, S_ST_CLR},

    {S_ST_NOTE, E_AUTO, S_ST_MENU},

    {S_ST_MUTE, E_AUTO, S_ST_MENU},

    {S_ST_EN, E_AUTO, S_ST_MENU},

    {S_ST_PREV, E_AUTO, S_ST_MENU},

    {S_ST_NEXT, E_AUTO, S_ST_MENU},

    {S_ST_VEL_DOWN, E_AUTO, S_ST_MENU},
    
    {S_ST_VEL_UP, E_AUTO, S_ST_MENU},

    {S_ST_CLR, E_AUTO, S_ST_MENU},
};

#define STATE_TABLE_SIZE (sizeof(state_table) / sizeof(state_table[0]))

typedef struct {
    MenuState_t state;
    void (*func)(uint16_t key, uint16_t hold);
} StateMachine_t;

void menu(uint16_t key, uint16_t hold);

#endif // _MENU_H