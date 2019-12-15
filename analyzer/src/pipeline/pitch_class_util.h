#ifndef URSATZ_ANALYZER_PITCH_CLASS_UTIL_H
#define URSATZ_ANALYZER_PITCH_CLASS_UTIL_H

/* Index 0-11 = C, C#, D, ... B. */
extern const char *const PITCH_CLASS_NAMES[12];

/* Reduces a MIDI note number (the numeric value MIDIImporter exposes
   per NoteEvent, under its "MIDI note number" tuning system) to a
   pitch class 0-11. */
int pitch_class_of(long midi_note_number);

#endif
