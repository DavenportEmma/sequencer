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
