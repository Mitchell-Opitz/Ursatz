#include "pipeline/pitch_class_util.h"

#include <stdio.h>
#include <stdlib.h>

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

/* Major-key tonic pitch class for each sharps/flats count 0-7, per the circle of fifths. */
static const int SHARPS_MAJOR_TONIC[8] = {0, 7, 2, 9, 4, 11, 6, 1}; /* C G D A E B F# C# */
static const int FLATS_MAJOR_TONIC[8] = {0, 5, 10, 3, 8, 1, 6, 11}; /* C F Bb Eb Ab Db Gb Cb */

#define NO_ACCIDENTAL_COUNT 99

/* How many sharps (or flats) the major key on tonic_pitch_class needs, or NO_ACCIDENTAL_COUNT when it
   would need a double sharp/flat. */
static int accidentals_needed(const int table[8], int tonic_pitch_class) {
    for (int count = 0; count < 8; count++) {
        if (table[count] == tonic_pitch_class) {
            return count;
        }
    }
    return NO_ACCIDENTAL_COUNT;
}

bool tonic_prefers_flats(int tonic_pitch_class) {
    return accidentals_needed(FLATS_MAJOR_TONIC, tonic_pitch_class) <
           accidentals_needed(SHARPS_MAJOR_TONIC, tonic_pitch_class);
}

char *key_label_create(int tonic_pitch_class, bool is_major) {
    const char *name = pitch_class_name(tonic_pitch_class, tonic_prefers_flats(tonic_pitch_class));
    const char *mode = is_major ? "major" : "minor";
    size_t size = (size_t)snprintf(NULL, 0, "%s %s", name, mode) + 1;
    char *label = malloc(size);
    if (label) {
        snprintf(label, size, "%s %s", name, mode);
    }
    return label;
}

int key_signature_major_tonic(int sharps_flats) {
    if (sharps_flats < -7 || sharps_flats > 7) {
        return -1;
    }
    return sharps_flats >= 0 ? SHARPS_MAJOR_TONIC[sharps_flats] : FLATS_MAJOR_TONIC[-sharps_flats];
}
