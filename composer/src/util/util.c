#include "util/util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pitch/pitch_arithmetic.h>
#include <standard-tonality/tonal_key.h>

int util_absolute(int value) {
    return value < 0 ? -value : value;
}

unsigned util_bits_set(unsigned bits) {
    unsigned count = 0;
    for (; bits; bits >>= 1) {
        count += bits & 1u;
    }
    return count;
}

int util_sign(int value) {
    return value > 0 ? 1 : (value < 0 ? -1 : 0);
}

int util_pitch_class(int pitch) {
    return ((pitch % PITCH_SEMITONES_PER_OCTAVE) + PITCH_SEMITONES_PER_OCTAVE) % PITCH_SEMITONES_PER_OCTAVE;
}

void util_pitch_name(int midi_pitch, bool prefer_flats, char *out, size_t size) {
    snprintf(out, size, "%s%d", pitch_class_name(util_pitch_class(midi_pitch), prefer_flats),
             midi_pitch / PITCH_SEMITONES_PER_OCTAVE - 1);
}

void util_key_name(int tonic_pitch_class, bool minor, char *out, size_t size) {
    PitchLetter letter;
    int accidental;
    char tonic[8];
    if (!standard_tonality_key_tonic_of_pitch_class(util_pitch_class(tonic_pitch_class), minor, &letter, &accidental) ||
        !pitch_spelling_name(letter, accidental, tonic, sizeof(tonic))) {
        snprintf(tonic, sizeof(tonic), "?");
    }
    snprintf(out, size, "%s %s", tonic, minor ? "minor" : "major");
}

char *util_copy_string(const char *text) {
    size_t size = text ? strlen(text) + 1 : 1;
    char *out = malloc(size);
    if (out) {
        memcpy(out, text ? text : "", size);
    }
    return out;
}

bool util_read_file(const char *path, char **text, size_t *size) {
    *text = NULL;
    FILE *file = fopen(path, "rb");
    if (!file) {
        return false;
    }
    long length = -1;
    if (fseek(file, 0, SEEK_END) == 0) {
        length = ftell(file);
        if (length < 0 || fseek(file, 0, SEEK_SET) != 0) {
            length = -1;
        }
    }
    char *buffer = length >= 0 ? malloc((size_t)length + 1) : NULL;
    if (buffer && fread(buffer, 1, (size_t)length, file) == (size_t)length) {
        buffer[length] = '\0';
        *text = buffer;
        if (size) {
            *size = (size_t)length;
        }
    } else {
        free(buffer);
    }
    fclose(file);
    return *text != NULL;
}
