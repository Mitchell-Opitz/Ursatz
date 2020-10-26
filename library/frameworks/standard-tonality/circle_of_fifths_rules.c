#include "circle_of_fifths_rules.h"

#include <stddef.h>

#include "rule_pair.h"
#include "tonal_pitch.h"

static const char *const NAMES[STANDARD_TONALITY_CIRCLE_RULE_COUNT] = {
    "circle-of-fifths-order", "circle-of-fifths-neighbors", "circle-of-fifths-enharmonic",
    "circle-of-fifths-real-imaginary"};

static StandardTonalityRulePair g_pairs[STANDARD_TONALITY_CIRCLE_RULE_COUNT];
static bool g_built[STANDARD_TONALITY_CIRCLE_RULE_COUNT];

static bool valid_rule(StandardTonalityCircleRule which) {
    return which >= 0 && which < STANDARD_TONALITY_CIRCLE_RULE_COUNT;
}

static void ensure_built(StandardTonalityCircleRule which) {
    if (!valid_rule(which) || g_built[which]) {
        return;
    }
    g_built[which] = true;
    standard_tonality_build_rule_pair(NAMES[which], &g_pairs[which]);
}

const Rule *standard_tonality_circle_rule(StandardTonalityCircleRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].rule : NULL;
}

const Constraint *standard_tonality_circle_constraint(StandardTonalityCircleRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].constraint : NULL;
}

#define MAX_ACCIDENTALS 7

/* Place of each natural letter's major key on the line of fifths:
   F -1, C 0, G 1, D 2, A 3, E 4, B 5. */
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

int standard_tonality_major_key_accidentals(PitchLetter letter, int accidental) {
    return letter_fifths(letter) + MAX_ACCIDENTALS * accidental;
}

bool standard_tonality_major_key_is_real(PitchLetter letter, int accidental) {
    int count = standard_tonality_major_key_accidentals(letter, accidental);
    return count >= -MAX_ACCIDENTALS && count <= MAX_ACCIDENTALS;
}

static int pitch_class(PitchLetter letter, int accidental) {
    int value = (standard_tonality_letter_semitone(letter) + accidental) % STANDARD_TONALITY_SEMITONES_PER_OCTAVE;
    if (value < 0) {
        value += STANDARD_TONALITY_SEMITONES_PER_OCTAVE;
    }
    return value;
}

bool standard_tonality_circle_neighbor(PitchLetter letter, int accidental, bool clockwise, PitchLetter *next_letter,
                                       int *next_accidental) {
    if (next_letter == NULL || next_accidental == NULL) {
        return false;
    }

    /* A fifth above is four letters up and 7 semitones; a fifth below is
       three letters up and 5 semitones (an octave less 7). */
    int letter_steps = clockwise ? 4 : 3;
    int semitones = clockwise ? 7 : 5;

    PitchLetter next = letter;
    for (int i = 0; i < letter_steps; i++) {
        next = standard_tonality_next_letter(next);
    }

    int offset = (pitch_class(letter, accidental) + semitones - standard_tonality_letter_semitone(next)) %
                 STANDARD_TONALITY_SEMITONES_PER_OCTAVE;
    if (offset < 0) {
        offset += STANDARD_TONALITY_SEMITONES_PER_OCTAVE;
    }
    if (offset > STANDARD_TONALITY_SEMITONES_PER_OCTAVE / 2) {
        offset -= STANDARD_TONALITY_SEMITONES_PER_OCTAVE;
    }
    if (offset < -2 || offset > 2) {
        return false;
    }

    *next_letter = next;
    *next_accidental = offset;
    return true;
}

bool standard_tonality_major_keys_enharmonic(PitchLetter letter_a, int accidental_a, PitchLetter letter_b,
                                             int accidental_b) {
    if (!standard_tonality_major_key_is_real(letter_a, accidental_a) ||
        !standard_tonality_major_key_is_real(letter_b, accidental_b)) {
        return false;
    }
    if (letter_a == letter_b && accidental_a == accidental_b) {
        return false;
    }
    return pitch_class(letter_a, accidental_a) == pitch_class(letter_b, accidental_b);
}

/* Sharps come in the order F C G D A E B; flats in the reverse. */
static const PitchLetter SHARP_ORDER[MAX_ACCIDENTALS] = {PITCH_LETTER_F, PITCH_LETTER_C, PITCH_LETTER_G, PITCH_LETTER_D,
                                                         PITCH_LETTER_A, PITCH_LETTER_E, PITCH_LETTER_B};

bool standard_tonality_major_key_signature(PitchLetter letter, int accidental, KeySignature **out) {
    if (!standard_tonality_major_key_is_real(letter, accidental)) {
        return false;
    }

    int count = standard_tonality_major_key_accidentals(letter, accidental);
    int size = count < 0 ? -count : count;

    KeySignatureEntry entries[MAX_ACCIDENTALS];
    for (int i = 0; i < size; i++) {
        entries[i].letter = count > 0 ? SHARP_ORDER[i] : SHARP_ORDER[MAX_ACCIDENTALS - 1 - i];
        entries[i].accidental = count > 0 ? 1 : -1;
    }
    return key_signature_create(entries, (size_t)size, out) == KEY_SIGNATURE_OK;
}
