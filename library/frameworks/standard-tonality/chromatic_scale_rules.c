#include "chromatic_scale_rules.h"

#include <stddef.h>

#include "rule_pair.h"
#include "solfege_rules.h"
#include "tonal_pitch.h"

static const char *const NAMES[STANDARD_TONALITY_CHROMATIC_RULE_COUNT] = {
    "chromatic-half-steps", "chromatic-natural-half-steps", "chromatic-spelling", "chromatic-solfege"};

static RulePairTable g_table = RULE_PAIR_TABLE_INIT(standard_tonality_framework, "standard-tonality",
    NAMES, STANDARD_TONALITY_CHROMATIC_RULE_COUNT);

URSATZ_RULE_ACCESSORS(standard_tonality_chromatic, StandardTonalityChromaticRule, STANDARD_TONALITY_CHROMATIC_RULE_COUNT, g_table)

static bool valid_direction(StandardTonalityChromaticDirection direction) {
    return direction == STANDARD_TONALITY_CHROMATIC_ASCENDING || direction == STANDARD_TONALITY_CHROMATIC_DESCENDING;
}

static bool all_notes_present(const PitchSpelling *const *notes, size_t count) {
    if (notes == NULL || count != STANDARD_TONALITY_CHROMATIC_NOTE_COUNT) {
        return false;
    }
    for (size_t i = 0; i < count; i++) {
        if (notes[i] == NULL) {
            return false;
        }
    }
    return true;
}

bool standard_tonality_is_chromatic_scale(StandardTonalityChromaticDirection direction,
                                          const PitchSpelling *const *notes, size_t count) {
    if (!valid_direction(direction) || !all_notes_present(notes, count)) {
        return false;
    }
    int expected = direction == STANDARD_TONALITY_CHROMATIC_ASCENDING ? 1 : -1;
    for (size_t i = 0; i < STANDARD_TONALITY_CHROMATIC_STEP_COUNT; i++) {
        int distance = standard_tonality_semitone_number(notes[i + 1]) - standard_tonality_semitone_number(notes[i]);
        if (distance != expected) {
            return false;
        }
    }
    return true;
}

bool standard_tonality_natural_half_step_above(PitchLetter letter) {
    return letter == PITCH_LETTER_E || letter == PITCH_LETTER_B;
}

typedef struct {
    PitchLetter letter;
    int accidental;
} Name;

/* Indexed by pitch class, C = 0. */
static const Name SHARP_NAMES[12] = {
    {PITCH_LETTER_C, 0}, {PITCH_LETTER_C, 1}, {PITCH_LETTER_D, 0}, {PITCH_LETTER_D, 1},
    {PITCH_LETTER_E, 0}, {PITCH_LETTER_F, 0}, {PITCH_LETTER_F, 1}, {PITCH_LETTER_G, 0},
    {PITCH_LETTER_G, 1}, {PITCH_LETTER_A, 0}, {PITCH_LETTER_A, 1}, {PITCH_LETTER_B, 0},
};

static const Name FLAT_NAMES[12] = {
    {PITCH_LETTER_C, 0}, {PITCH_LETTER_D, -1}, {PITCH_LETTER_D, 0}, {PITCH_LETTER_E, -1},
    {PITCH_LETTER_E, 0}, {PITCH_LETTER_F, 0},  {PITCH_LETTER_G, -1}, {PITCH_LETTER_G, 0},
    {PITCH_LETTER_A, -1}, {PITCH_LETTER_A, 0}, {PITCH_LETTER_B, -1}, {PITCH_LETTER_B, 0},
};

static int pitch_class_of(int semitone_number) {
    int value = semitone_number % STANDARD_TONALITY_SEMITONES_PER_OCTAVE;
    if (value < 0) {
        value += STANDARD_TONALITY_SEMITONES_PER_OCTAVE;
    }
    return value;
}

bool standard_tonality_chromatic_spelling_conventional(StandardTonalityChromaticDirection direction,
                                                       const PitchSpelling *const *notes, size_t count) {
    if (!valid_direction(direction) || !all_notes_present(notes, count)) {
        return false;
    }
    const Name *names = direction == STANDARD_TONALITY_CHROMATIC_ASCENDING ? SHARP_NAMES : FLAT_NAMES;
    for (size_t i = 1; i < STANDARD_TONALITY_CHROMATIC_STEP_COUNT; i++) {
        int pitch_class = pitch_class_of(standard_tonality_semitone_number(notes[i]));
        if (pitch_spelling_letter(notes[i]) != names[pitch_class].letter ||
            pitch_spelling_accidental(notes[i]) != names[pitch_class].accidental) {
            return false;
        }
    }
    return true;
}

bool standard_tonality_build_chromatic_scale(StandardTonalityChromaticDirection direction, const PitchSpelling *tonic,
                                             PitchSpelling *out[STANDARD_TONALITY_CHROMATIC_NOTE_COUNT]) {
    if (!valid_direction(direction) || tonic == NULL || out == NULL) {
        return false;
    }

    int sign = direction == STANDARD_TONALITY_CHROMATIC_ASCENDING ? 1 : -1;
    const Name *names = direction == STANDARD_TONALITY_CHROMATIC_ASCENDING ? SHARP_NAMES : FLAT_NAMES;
    int tonic_number = standard_tonality_semitone_number(tonic);

    PitchSpelling *built[STANDARD_TONALITY_CHROMATIC_NOTE_COUNT];
    size_t created = 0;

    for (size_t i = 0; i < STANDARD_TONALITY_CHROMATIC_NOTE_COUNT; i++) {
        PitchLetter letter = pitch_spelling_letter(tonic);
        int accidental = pitch_spelling_accidental(tonic);
        int number = tonic_number + sign * (int)i;
        /* The endpoints (first and last) are spelled as the tonic is;
           the notes between follow the convention. */
        if (i > 0 && i < STANDARD_TONALITY_CHROMATIC_STEP_COUNT) {
            const Name *name = &names[pitch_class_of(number)];
            letter = name->letter;
            accidental = name->accidental;
        }
        /* The octave belongs to the letter: number = 12 * octave +
           letter base + accidental, which divides exactly. */
        int octave = (number - standard_tonality_letter_semitone(letter) - accidental) /
                     STANDARD_TONALITY_SEMITONES_PER_OCTAVE;
        if (pitch_spelling_create(letter, accidental, octave, &built[i]) != PITCH_SPELLING_OK) {
            for (size_t j = 0; j < created; j++) {
                pitch_spelling_destroy(built[j]);
            }
            return false;
        }
        created++;
    }

    for (size_t i = 0; i < STANDARD_TONALITY_CHROMATIC_NOTE_COUNT; i++) {
        out[i] = built[i];
    }
    return true;
}

/* Degree and alteration of each semitone above (ascending) or below
   (descending) the tonic. */
typedef struct {
    int degree;
    int alteration;
} Syllable;

static const Syllable ASCENDING[12] = {{1, 0}, {1, 1}, {2, 0}, {2, 1}, {3, 0}, {4, 0},
                                       {4, 1}, {5, 0}, {5, 1}, {6, 0}, {6, 1}, {7, 0}};

static const Syllable DESCENDING[12] = {{1, 0}, {7, 0},  {7, -1}, {6, 0}, {6, -1}, {5, 0},
                                        {5, -1}, {4, 0}, {3, 0},  {3, -1}, {2, 0}, {2, -1}};

const char *standard_tonality_chromatic_scale_solfege(StandardTonalityChromaticDirection direction, size_t index) {
    if (!valid_direction(direction) || index >= STANDARD_TONALITY_CHROMATIC_NOTE_COUNT) {
        return NULL;
    }
    if (index == STANDARD_TONALITY_CHROMATIC_STEP_COUNT) {
        return standard_tonality_chromatic_solfege(1, 0);
    }
    const Syllable *table = direction == STANDARD_TONALITY_CHROMATIC_ASCENDING ? ASCENDING : DESCENDING;
    return standard_tonality_chromatic_solfege(table[index].degree, table[index].alteration);
}
