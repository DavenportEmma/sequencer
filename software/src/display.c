#include "display.h"
#include "ssd1306.h"
#include "ascii.h"
#include <string.h>
#include "common.h"
#include "midi.h"
#include "uart.h"

extern uint8_t display_buffer[DISPLAY_BUFFER_SIZE];

static uint16_t utf_to_index(char c) {
    return c * 5;
}

void num_to_str(uint8_t n, char* s, uint8_t len) {
    while(len > 0) {
        uint8_t i = (n % 10);
        s[len-1] = i + 48;
        n /= 10;
        len--;
    }
}

void _display_line(char* s, uint8_t line) {
    uint16_t line_index = line * WIDTH;
    uint16_t eol = line_index + WIDTH-1;
    uint16_t string_index = 0;

    char c = s[string_index];

    memset(display_buffer + line_index, 0, WIDTH);

    while(line_index < eol && c != '\0') {
        uint16_t utf = utf_to_index(c);
        c = s[++string_index];

        memcpy(&display_buffer[line_index], &font[utf], 5);
        line_index+=6;
    }

    display_ssd1306(display_buffer);
}

void clear_display() {
    memset(display_buffer, 0, DISPLAY_BUFFER_SIZE);
}

static uint16_t utf_to_font16(uint16_t utf) {
    if(utf > 47 && utf < 58) {
        return (1 + (utf - 48)) * 20;
    } else if(utf > 64 && utf < 90) {
        return (11 + (utf - 65)) * 20;
    } else if(utf == 32) {
        return 0;
    } 
    else {
        return 0xFFFF;
    }
}

void display_line(char* s, uint8_t line) {
    uint16_t string_index = 0;
    char c = s[string_index];
    
    uint16_t line_index = line * (WIDTH * 2);
    uint16_t eol = line_index + WIDTH-1;
    
    while(c != '\0' && line_index < eol) {
        uint16_t font16_index = utf_to_font16((uint16_t)(c));
    
        memcpy(&display_buffer[line_index], &font16[font16_index], 10);
        memcpy(&display_buffer[line_index + WIDTH], &font16[font16_index+10], 10);

        c = s[++string_index];
        line_index += 11;
    }

    display_ssd1306(display_buffer);
}

void display_number(uint8_t n, uint8_t line) {
    char s[4];
    memset(s, '\0', 4);
    num_to_str(n, s, 3);
    display_line(s, line);
}

void clear_line(uint8_t line) {
    if(line > 3) {
        #ifdef CONFIG_DEBUG_PRINT
        send_uart(USART3, "WARNING: line index exceeds buffer", 34);
        send_uart(USART3, "\n\r", 2);
        #endif

        line = 3;
    }
    
    uint16_t line_index = line * (WIDTH * 2);

    memset(&display_buffer[line_index], 0, WIDTH);
    memset(&display_buffer[line_index+WIDTH], 0, WIDTH);
}

void update_display() {
    display_ssd1306(display_buffer);
}

void clear_and_update() {
    clear_display();
    update_display();
}

void display_piano_roll() {
    uint8_t white_key_width = 15;

    uint8_t line = 3;
    uint16_t line_index = line * (WIDTH * 2);

    clear_line(3);

    uint8_t black_key_width = 9;
    uint8_t black_key[] = {0xFF, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0xFF};

    for(uint8_t i = 0; i < white_key_width*8; i++) {
        if((i % white_key_width) == 0) {
            switch(i/white_key_width) {
                case 1:
                case 2:
                case 4:
                case 5:
                case 6:
                    memcpy(&display_buffer[line_index+i-4], &black_key[0], black_key_width);
                    break;
                default:
                    display_buffer[line_index+i] = 0xFF;
                    break;
            }

            display_buffer[line_index+i+WIDTH] = 0xFF;
        }
    }
}

void show_note(uint8_t note) {
    MIDINote_t n = note & 0x7F;

    if(n > C8 || n < A0) {
        return;
    }

    uint8_t line = 3;
    uint16_t line_index = line * (WIDTH * 2);

    uint8_t note_class = n%12;

    switch(note_class) {
        // C, F
        case 0:
        case 5:
            ;
            uint8_t key = (note_class == 0) ? 0 : 3;
            memset(&display_buffer[line_index+2+(15*key)], 0xFF, 8);
            memset(&display_buffer[line_index+2+(15*key)+WIDTH], 0xFF, 8);
            memset(&display_buffer[line_index+10+(15*key)+WIDTH], 0xFE, 4);
            break;
        // black keys
        case 1:
            memset(&display_buffer[line_index+13], 0xBF, 5);
            break;
        case 3:
            memset(&display_buffer[line_index+13+15], 0xBF, 5);
            break;
        case 6:
            memset(&display_buffer[line_index+13+45], 0xBF, 5);
            break;
        case 8:
            memset(&display_buffer[line_index+13+60], 0xBF, 5);
            break;
        case 10:
            memset(&display_buffer[line_index+13+75], 0xBF, 5);
            break;
        // D, G, A
        case 2:
            memset(&display_buffer[line_index+21], 0xFF, 4);
            memset(&display_buffer[line_index+17+WIDTH], 0xFE, 4);
            memset(&display_buffer[line_index+21+WIDTH], 0xFF, 4);
            memset(&display_buffer[line_index+25+WIDTH], 0xFE, 4);
            break;
        case 7:
            memset(&display_buffer[line_index+21+45], 0xFF, 4);
            memset(&display_buffer[line_index+17+45+WIDTH], 0xFE, 4);
            memset(&display_buffer[line_index+21+45+WIDTH], 0xFF, 4);
            memset(&display_buffer[line_index+25+45+WIDTH], 0xFE, 4);
            break;
        case 9:
            memset(&display_buffer[line_index+21+60], 0xFF, 4);
            memset(&display_buffer[line_index+17+60+WIDTH], 0xFE, 4);
            memset(&display_buffer[line_index+21+60+WIDTH], 0xFF, 4);
            memset(&display_buffer[line_index+25+60+WIDTH], 0xFE, 4);
            break;
        // E, B
        case 4:
            memset(&display_buffer[line_index+36], 0xFF, 8);
            memset(&display_buffer[line_index+32+WIDTH], 0xFE, 4);
            memset(&display_buffer[line_index+36+WIDTH], 0xFF, 8);
            break;
        case 11:
            memset(&display_buffer[line_index+36+60], 0xFF, 8);
            memset(&display_buffer[line_index+32+60+WIDTH], 0xFE, 4);
            memset(&display_buffer[line_index+36+60+WIDTH], 0xFF, 8);
            break;
        
        default:
            break;
    }
}