#ifndef _DISPLAY_H
#define _DISPLAY_H

void num_to_str(uint8_t n, char* s, uint8_t len);
void display_line(char* s, uint8_t line);
void clear_display();
void diplay_number(uint8_t n, uint8_t line);
void clear_line(uint8_t line);
void update_display();
void display_piano_roll();
void show_note(uint8_t note);

#endif // _DISPLAY_H