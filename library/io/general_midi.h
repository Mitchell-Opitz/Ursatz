#ifndef URSATZ_IO_GENERAL_MIDI_H
#define URSATZ_IO_GENERAL_MIDI_H

/* The General MIDI instrument name of a program (0-based, 0 to 127), e.g. 0 is "Acoustic Grand Piano". NULL for a
   program outside that range, including -1 (none). */
const char *general_midi_instrument_name(int program);

#endif
