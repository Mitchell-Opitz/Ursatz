#ifndef URSATZ_ANALYZER_PITCH_CLASS_UTIL_H
#define URSATZ_ANALYZER_PITCH_CLASS_UTIL_H

#include <stdbool.h>

/* Reduces a semitone number (a MIDI note number, or the number segmentation reads from a spelling) to a pitch class
   0-11. The library reduces only spellings (pitch_spelling_pitch_class), not a bare number. */
int pitch_class_of(long midi_note_number);

/* pitch_class_name(pitch_class, prefer_flats) is the library's: the flat spelling when prefer_flats is true. */
#include "pitch/pitch_arithmetic.h"

/* Whether the key on tonic_pitch_class (0-11) is signed in flats: the library's spelling of the tonic with the fewer
   accidentals in its signature (standard_tonality_key_tonic_of_pitch_class), a minor key judged by its relative major.
   Eb major and F minor are, F# major and D# minor (a tie) are not. */
bool tonic_prefers_flats(int tonic_pitch_class, bool is_major);

/* "<tonic> major" / "<tonic> minor" for a tonic pitch class, spelled by tonic_prefers_flats; a heap
   string the caller frees, or NULL on allocation failure. */
char *key_label_create(int tonic_pitch_class, bool is_major);

/* The major-key tonic pitch class for a key signature's sharps/flats count (-7..7, negative flats);
   the minor key of the same signature is 9 semitones above it. -1 for a count outside -7..7. */
int key_signature_major_tonic(int sharps_flats);

#endif
