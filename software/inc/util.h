#ifndef _UTIL_H
#define _UTIL_H

void toggle_bit(uint32_t* field, uint8_t bit, uint8_t max);
uint8_t check_bit(uint32_t* field, uint8_t bit, uint8_t max);
void clear_field(uint32_t* field, uint8_t max);
void set_bit(uint32_t* field, uint8_t bit, uint8_t max);
void set_bit_range(uint32_t* field, uint8_t start, uint8_t end, uint8_t max);

#endif // _UTIL_H
