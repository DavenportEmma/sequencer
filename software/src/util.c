#include "util.h"

void toggle_bit(uint32_t* field, uint8_t bit, uint8_t max) {
    uint8_t mask_size = 32;

    if (bit < max) {
        uint8_t mask_index = bit / mask_size;
        uint32_t mute_mask = 1 << (bit % mask_size);

        field[mask_index] ^= mute_mask;
    }
}

uint8_t check_bit(uint32_t* field, uint8_t bit, uint8_t max) {
    uint8_t mask_size = 32;

    if(bit < max) {
        int8_t mask_index = bit / mask_size;
        uint32_t mute_mask = 1 << (bit % mask_size);

        if(field[mask_index] & mute_mask) {
            return 1;
        } else {
            return 0;
        }
    }

    return 0;
}

void clear_field(uint32_t* field, uint8_t max) {
    uint8_t mask_size = 32;
    uint8_t num_fields = max / mask_size;

    for(int i = 0; i < num_fields; i++) {
        field[i] = 0;
    }
}

void set_bit(uint32_t* field, uint8_t bit, uint8_t max) {
    uint8_t mask_size = 32;

    if (bit < max) {
        uint8_t mask_index = bit / mask_size;
        uint32_t mute_mask = 1 << (bit % mask_size);

        field[mask_index] |= mute_mask;
    }
}

void clear_bit(uint32_t* field, uint8_t bit, uint8_t max) {
    uint8_t mask_size = 32;

    if (bit < max) {
        uint8_t mask_index = bit / mask_size;
        uint32_t mute_mask = 1 << (bit % mask_size);

        field[mask_index] &= ~(mute_mask);
    }
}

void set_bit_range(uint32_t* field, uint8_t start, uint8_t end, uint8_t max) {
    if (start > 63 || end > 63) {
        return;
    }
    
    uint8_t min_bit, max_bit;
    if (start > end) {
        min_bit = end;
        max_bit = start;
    } else {
        min_bit = start;
        max_bit = end;
    }
    
    for (uint8_t bit = min_bit; bit <= max_bit; bit++) {
        uint8_t array_index = bit / 32;
        
        uint8_t bit_position = bit % 32;
        
        field[array_index] |= (1U << bit_position);
    }
}

// return 1 if one bit set across the entire field, else return 0
uint8_t one_bit_set(uint32_t* field) {
    if (field[0] != 0) {
        // First element has bits - for the function to return true:
        // - First element must have exactly one bit set
        // - Second element must have no bits set
        return ((field[0] & (field[0] - 1)) == 0) && (field[1] == 0);
    } 
    // First element has no bits set
    else {
        // For the function to return true:
        // - Second element must have exactly one bit set
        return (field[1] != 0) && ((field[1] & (field[1] - 1)) == 0);
    }

    /*
        When a number has exactly one bit set (is a power of 2), subtracting 1
        changes that bit to 0 and sets all lower bits to 1. When we then perform
        the bitwise AND operation with the original number, there's no overlap
        in set bits, so the result is 0.
        
        For any number with more than one bit set, at least one of the higher
        bits will still be present in both the original number and the number
        minus 1, resulting in a non-zero value after the AND operation.
        
        The only caveat is that this trick also returns true for x = 0
        (since 0 & -1 = 0), which is why we typically add the additional
        check x != 0 when we specifically want to detect numbers with exactly
        one bit set.
    */
}

static uint8_t _find_last_bit(uint32_t field) {
    for(uint8_t i = 31; i > 0; i--) {
        if(field & (1 << i)) {
            return i;
        }
    }
}

uint8_t find_last_bit(uint32_t* field) {
    if(field[1] != 0) {
        return _find_last_bit(field[1]) + 32; 
    } else if(field[0] != 0) {
        return _find_last_bit(field[0]);
    }
    return 0xFF;
}

static uint8_t _find_first_bit(uint32_t field) {
    for(uint8_t i = 0; i < 32; i++) {
        if(field & (1 << i)) {
            return i;
        }
    }
}

uint8_t find_first_bit(uint32_t* field) {
    if(field[0] != 0) {
        return _find_first_bit(field[0]);
    } else if(field[1] != 0) {
        return _find_first_bit(field[1]) + 32;
    }
    return 0xFF;
}