#include "display.h"
#include "ssd1306.h"
#include "ascii.h"
#include <string.h>
#include "common.h"

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
    uint16_t line_index = line * (WIDTH * 2);

    memset(&display_buffer[line_index], 0, WIDTH);
    memset(&display_buffer[line_index+WIDTH], 0, WIDTH);
}

static void update_display() {
    display_ssd1306(display_buffer);
}

void clear_and_update() {
    clear_display();
    update_display();
}