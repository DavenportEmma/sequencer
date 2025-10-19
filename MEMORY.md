# sequencer flash memory


Here is a diagram of how the memory in the W25Q128JV flash chip is organised.

![](images/memory.drawio.svg)

Pages cannot be written to without a prior erase. This is a NOR flash chip which means that bits can only be set to 0, an erase is the only operation that can set a bit to 1. Erase operations can only be done per sector so updating an individual page cannot be done without erasing the entire sector (16 pages).

## sequences in memory

On startup the application reads the sequence metadata and step data from flash into memory.

### metadata

Page 0x000000 contains the sequence metadata. 4 bytes are allocated to each sequence (64 sequences for a 256 byte page), the first byte contains the midi channel for the sequence, the rest are unused as of now.

### step data

Each sequence contains 64 steps, each step containing 8 note-off commands, 8 note-on commands, 8 velocity values for the note-on commands. This is assuming 8 note polyphony. Page 0x000100 onwards contains step data.