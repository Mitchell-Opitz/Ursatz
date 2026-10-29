#include "pitch/pitch_arithmetic.h"

#include <stdlib.h>
#include <string.h>

#define PITCH_LETTER_COUNT 7
#define PITCH_NAME_MAX_OCTAVE_DIGITS 9

static const PitchLetter LETTERS_BY_INDEX[PITCH_LETTER_COUNT] = {
    PITCH_LETTER_C, PITCH_LETTER_D, PITCH_LETTER_E, PITCH_LETTER_F,
    PITCH_LETTER_G, PITCH_LETTER_A, PITCH_LETTER_B};

int pitch_letter_semitone(PitchLetter letter) {
    switch (letter) {
    case PITCH_LETTER_C:
        return 0;
    case PITCH_LETTER_D:
        return 2;
    case PITCH_LETTER_E:
        return 4;
    case PITCH_LETTER_F:
        return 5;
    case PITCH_LETTER_G:
        return 7;
    case PITCH_LETTER_A:
        return 9;
    case PITCH_LETTER_B:
    default:
        return 11;
    }
}

int pitch_letter_index(PitchLetter letter) {
    switch (letter) {
    case PITCH_LETTER_C:
        return 0;
    case PITCH_LETTER_D:
        return 1;
    case PITCH_LETTER_E:
        return 2;
    case PITCH_LETTER_F:
        return 3;
    case PITCH_LETTER_G:
        return 4;
    case PITCH_LETTER_A:
        return 5;
    case PITCH_LETTER_B:
    default:
        return 6;
    }
}

static int floor_mod(int value, int modulus) {
    int result = value % modulus;
    return result < 0 ? result + modulus : result;
}

static int floor_div(int value, int divisor) {
    int quotient = value / divisor;
    if (value % divisor != 0 && value < 0) {
        quotient--;
    }
    return quotient;
}

PitchLetter pitch_letter_offset(PitchLetter letter, int steps) {
    return LETTERS_BY_INDEX[floor_mod(
        pitch_letter_index(letter) + floor_mod(steps, PITCH_LETTER_COUNT), PITCH_LETTER_COUNT)];
}

int pitch_spelling_pitch_class(const PitchSpelling *spelling) {
    return floor_mod(pitch_letter_semitone(pitch_spelling_letter(spelling)) +
                         pitch_spelling_accidental(spelling),
                     PITCH_SEMITONES_PER_OCTAVE);
}

int pitch_spelling_midi_number(const PitchSpelling *spelling) {
    int number = PITCH_SEMITONES_PER_OCTAVE * (pitch_spelling_octave(spelling) + 1) +
                 pitch_letter_semitone(pitch_spelling_letter(spelling)) +
                 pitch_spelling_accidental(spelling);
    return (number < 0 || number > 127) ? -1 : number;
}

static int letter_fifths(PitchLetter letter) {
    switch (letter) {
    case PITCH_LETTER_F:
        return -1;
    case PITCH_LETTER_C:
        return 0;
    case PITCH_LETTER_G:
        return 1;
    case PITCH_LETTER_D:
        return 2;
    case PITCH_LETTER_A:
        return 3;
    case PITCH_LETTER_E:
        return 4;
    case PITCH_LETTER_B:
    default:
        return 5;
    }
}

int pitch_spelling_fifths(PitchLetter letter, int accidental) {
    return letter_fifths(letter) + 7 * accidental;
}

void pitch_spelling_from_fifths(int fifths, PitchLetter *letter, int *accidental) {
    static const PitchLetter FIFTHS_LETTERS[PITCH_LETTER_COUNT] = {
        PITCH_LETTER_F, PITCH_LETTER_C, PITCH_LETTER_G, PITCH_LETTER_D,
        PITCH_LETTER_A, PITCH_LETTER_E, PITCH_LETTER_B};
    *letter = FIFTHS_LETTERS[floor_mod(fifths + 1, PITCH_LETTER_COUNT)];
    *accidental = floor_div(fifths + 1, PITCH_LETTER_COUNT);
}

static bool letter_valid(PitchLetter letter) {
    return letter >= PITCH_LETTER_A && letter <= PITCH_LETTER_G;
}

bool pitch_spelling_name(PitchLetter letter, int accidental, char *buffer, size_t size) {
    static const char LETTER_CHARS[] = {'A', 'B', 'C', 'D', 'E', 'F', 'G'};
    static const char *const SUFFIXES[] = {"bb", "b", "", "#", "x"};
    if (buffer == NULL || !letter_valid(letter) || accidental < -2 || accidental > 2) {
        return false;
    }
    const char *suffix = SUFFIXES[accidental + 2];
    size_t needed = 1 + strlen(suffix) + 1;
    if (size < needed) {
        return false;
    }
    buffer[0] = LETTER_CHARS[letter];
    strcpy(buffer + 1, suffix);
    return true;
}

bool pitch_spelling_parse_name(const char *text, PitchLetter *letter, int *accidental,
                               int *octave_or_null) {
    if (text == NULL || letter == NULL || accidental == NULL) {
        return false;
    }
    PitchLetter parsed_letter;
    switch (text[0]) {
    case 'A':
        parsed_letter = PITCH_LETTER_A;
        break;
    case 'B':
        parsed_letter = PITCH_LETTER_B;
        break;
    case 'C':
        parsed_letter = PITCH_LETTER_C;
        break;
    case 'D':
        parsed_letter = PITCH_LETTER_D;
        break;
    case 'E':
        parsed_letter = PITCH_LETTER_E;
        break;
    case 'F':
        parsed_letter = PITCH_LETTER_F;
        break;
    case 'G':
        parsed_letter = PITCH_LETTER_G;
        break;
    default:
        return false;
    }
    const char *cursor = text + 1;
    int parsed_accidental = 0;
    if (cursor[0] == 'b' && cursor[1] == 'b') {
        parsed_accidental = -2;
        cursor += 2;
    } else if (cursor[0] == 'b') {
        parsed_accidental = -1;
        cursor += 1;
    } else if (cursor[0] == '#' && cursor[1] == '#') {
        parsed_accidental = 2;
        cursor += 2;
    } else if (cursor[0] == '#') {
        parsed_accidental = 1;
        cursor += 1;
    } else if (cursor[0] == 'x') {
        parsed_accidental = 2;
        cursor += 1;
    }

    bool has_octave = false;
    int parsed_octave = 0;
    if (cursor[0] != '\0') {
        const char *digits = cursor;
        bool negative = false;
        if (digits[0] == '-') {
            negative = true;
            digits++;
        }
        size_t digit_count = strspn(digits, "0123456789");
        if (digit_count == 0 || digit_count > PITCH_NAME_MAX_OCTAVE_DIGITS ||
            digits[digit_count] != '\0') {
            return false;
        }
        if (octave_or_null == NULL) {
            return false;
        }
        parsed_octave = atoi(digits);
        if (negative) {
            parsed_octave = -parsed_octave;
        }
        has_octave = true;
    }

    *letter = parsed_letter;
    *accidental = parsed_accidental;
    if (has_octave) {
        *octave_or_null = parsed_octave;
    }
    return true;
}

const char *pitch_class_name(int pitch_class, bool prefer_flats) {
    static const char *const SHARP_NAMES[PITCH_SEMITONES_PER_OCTAVE] = {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    static const char *const FLAT_NAMES[PITCH_SEMITONES_PER_OCTAVE] = {
        "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"};
    if (pitch_class < 0 || pitch_class >= PITCH_SEMITONES_PER_OCTAVE) {
        return NULL;
    }
    return prefer_flats ? FLAT_NAMES[pitch_class] : SHARP_NAMES[pitch_class];
}
