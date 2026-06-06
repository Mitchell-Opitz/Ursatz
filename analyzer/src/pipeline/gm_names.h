#ifndef URSATZ_ANALYZER_GM_NAMES_H
#define URSATZ_ANALYZER_GM_NAMES_H

/* The General MIDI instrument name of a program (0-based, 0 to 127), e.g. 0 is "Acoustic Grand Piano". NULL for a
   program outside that range, including -1 (none). */
const char *gm_instrument_name(int program);

#endif
