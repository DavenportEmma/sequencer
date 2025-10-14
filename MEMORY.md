# sequencer flash memory


Here is a diagram of how the memory in the W25Q128JV flash chip is organised.

![](images/memory.drawio.svg)

Pages cannot be written to without a prior erase. This is a NOR flash chip which means that bits can only be set to 0, an erase is the only operation that can set a bit to 1. Erase operations can only be done per sector so updating an individual page cannot be done without erasing the entire sector (16 pages).

## sequences in memory
Each sequence has its own block (64kB). The first sector in the block stores sequence metadata (channel, tempo, etc).

Here is the structure of the first page in the first sector

```
0           1 - 255
CHANNEL     UNASSIGNED
```

Reading from memory will happen a lot more than writing so it's ok to optimise for read speed. There is also way more memory than needed so we don't need to optimise for limited storage.

The data for all steps are stored in a sector. Below is an example of a section from the sector detailing 4 steps.

```
0x80    NOTE_OFF
0x30    C3
0x90    NOTE_ON
0x3C    C4
0x7F    127     // velocity
0x00            // step border
0x80    NOTE_OFF
0x3C    C4
0x90    NOTE_ON
0x34    E3
0x7F    127
0x00
0x80    NOTE_OFF
0x34    E3
0x90    NOTE_ON
0x37    G3
0x7F    127
0x00
0x80    NOTE_OFF
0x37    G3
0x90    NOTE_ON
0x30    C3
0x7F    127
0xFF            // end of sequence
```

Polyphony can be achieved with more note and velocity bytes. There is no need to duplicate the note on and note off bytes, we know that all notes after note off and before note on are note off signals. Below is an example step for a 4 voice polyphonic sequence.

```
0x80    NOTE_OFF
0x35    F3
0x38    G#3
0x39    A3
0x41    F4
0x90    NOTE_ON
0x30    C3
0x7F    127
0x34    E3
0x7F    127
0x37    G3
0x7F    127
0x3C    C4
0x7F    127
0x00
```

The max number of bytes per step for a 4 voice polyphonic sequence is 15.

num_bytes = 3 + (3 * num_voices)
When a sequence is selected for editing the entire sequence should be read into memory. All modifications are made to the sequence in memory. When a different sequence is selected the previous sequence data is written to storage.

If the sequence being edited is also being played currently, the program needs to switch from reading from storage to reading from memory.

Before a write operation the sector must be erased.

page program - typical: 0.4ms, max: 3ms

sector erase - typical: 45ms, max: 400ms

Sector erases have a very poor worst case scenario time. Maybe we should consider using something with byte level erase. OR since each sequence is given a block of memory (16 sectors, sector 0 being used for sequence metadata) we can write the updated sequence data to an empty sector then erase the previous sector.