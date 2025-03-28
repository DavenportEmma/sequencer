#ifndef _MENU_H
#define _MENU_H

typedef enum {
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
} menu_lut_t;

#endif // _MENU_H