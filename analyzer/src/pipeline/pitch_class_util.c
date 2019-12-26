#include "pipeline/pitch_class_util.h"

const char *const PITCH_CLASS_NAMES[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

const char *const PITCH_CLASS_NAMES_FLAT[12] = {"C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"};

int pitch_class_of(long midi_note_number) {
    long remainder = midi_note_number % 12;
    if (remainder < 0) {
        remainder += 12;
    }
    return (int)remainder;
}

const char *pitch_class_name(int pitch_class, bool prefer_flats) {
    return prefer_flats ? PITCH_CLASS_NAMES_FLAT[pitch_class] : PITCH_CLASS_NAMES[pitch_class];
}
