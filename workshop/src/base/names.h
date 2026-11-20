#ifndef URSATZ_WORKSHOP_BASE_NAMES_H
#define URSATZ_WORKSHOP_BASE_NAMES_H

#include <cJSON.h>

#include <common/status.h>

/*
 * The tool "pitch-names" (R-656): MIDI numbers as note names, the one spelling every printer of the command line uses. midi is an array of
 * whole numbers 0 to 127 (at most 4096); *names is an array of the sharp spelling of the pitch class and the octave, MIDI 60 is "C4", 61 "C#4",
 * 127 "G9" (the caller deletes it). A value out of range is URSATZ_ERR_REQUEST at "/params/midi/<i>".
 */
UrsatzStatus workshop_pitch_names(const cJSON *midi, cJSON **names, UrsatzError *error);

#endif
