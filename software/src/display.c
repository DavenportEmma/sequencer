#include "display.h"
#include "ssd1306.h"
#include "ascii.h"
#include <string.h>

extern uint8_t* display_buffer;

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

void display_line(char* s, uint8_t line) {
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
    memset(display_buffer, 0, 1024);
    display_ssd1306(display_buffer);
}