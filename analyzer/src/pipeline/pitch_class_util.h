#ifndef URSATZ_ANALYZER_PITCH_CLASS_UTIL_H
#define URSATZ_ANALYZER_PITCH_CLASS_UTIL_H

#include <stdbool.h>

/* Index 0-11 = C, C#, D, ... B. */
extern const char *const PITCH_CLASS_NAMES[12];

/* Index 0-11 = C, Db, D, ... B -- the flat spelling of each pitch
   class, for keys whose signature is more naturally written in flats
   (F major and its relatives, and every key with a negative
   sharps_flats count from a MIDI key signature meta-event). */
extern const char *const PITCH_CLASS_NAMES_FLAT[12];

/* Reduces a MIDI note number (the numeric value MIDIImporter exposes
   per NoteEvent, under its "MIDI note number" tuning system) to a
   pitch class 0-11. */
int pitch_class_of(long midi_note_number);

/* pitch_class_name(pitch_class, prefer_flats) is the library's: the flat spelling when prefer_flats is true. */
#include "pitch/pitch_arithmetic.h"

/* Whether tonic_pitch_class (0-11) reads in flats under minimal-accidental spelling: true when its
   flat-side circle-of-fifths count is strictly lower than its sharp-side count (Eb, not D#). The one
   tie, F#/Gb, reads in sharps. */
bool tonic_prefers_flats(int tonic_pitch_class);

/* "<tonic> major" / "<tonic> minor" for a tonic pitch class, spelled by tonic_prefers_flats; a heap
   string the caller frees, or NULL on allocation failure. */
char *key_label_create(int tonic_pitch_class, bool is_major);

/* The major-key tonic pitch class for a key signature's sharps/flats count (-7..7, negative flats);
   the minor key of the same signature is 9 semitones above it. -1 for a count outside -7..7. */
int key_signature_major_tonic(int sharps_flats);

#endif
