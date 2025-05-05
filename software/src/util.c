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