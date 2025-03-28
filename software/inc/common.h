#ifndef _COMMON_H
#define _COMMON_H

/*
    BYTES_PER_STEP is the number of bytes one step data is (see MEMORY.md)

    BYTES_PER_SEQ is the number of bytes per step times the maximum sequence
    length (max steps per sequence)
*/
#define BYTES_PER_STEP (3+(3*CONFIG_MAX_POLYPHONY))
#define BYTES_PER_SEQ (BYTES_PER_STEP * CONFIG_STEPS_PER_SEQUENCE)

#endif // _COMMON_H