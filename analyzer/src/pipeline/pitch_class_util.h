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

/* Names pitch_class (0-11) from PITCH_CLASS_NAMES_FLAT when prefer_flats
   is true, PITCH_CLASS_NAMES otherwise. */
const char *pitch_class_name(int pitch_class, bool prefer_flats);

#endif
