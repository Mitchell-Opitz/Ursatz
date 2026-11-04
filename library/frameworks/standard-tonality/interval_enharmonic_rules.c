#include "interval_enharmonic_rules.h"

#include <stddef.h>

#include "interval_quality_rules.h"
#include "interval_size_rules.h"
#include "rule_pair.h"
#include "tonal_pitch.h"
#include "pitch/pitch_arithmetic.h"

static const char *const NAMES[STANDARD_TONALITY_INTERVAL_ENHARMONIC_RULE_COUNT] = {"interval-enharmonic-semitones",
                                                                                     "interval-enharmonic-notes"};

static RulePairTable g_table = RULE_PAIR_TABLE_INIT(standard_tonality_framework, "standard-tonality",
    NAMES, STANDARD_TONALITY_INTERVAL_ENHARMONIC_RULE_COUNT);

URSATZ_RULE_ACCESSORS(standard_tonality_interval_enharmonic, StandardTonalityIntervalEnharmonicRule, STANDARD_TONALITY_INTERVAL_ENHARMONIC_RULE_COUNT, g_table)

bool standard_tonality_interval_at_semitones(int size, int semitones, StandardTonalityIntervalQuality *quality) {
    if (quality == NULL || size < 1) {
        return false;
    }
    int simple = standard_tonality_interval_simple_size(size);
    int octaves = (size - simple) / 7;
    int reference = standard_tonality_major_interval_semitones(simple) + STANDARD_TONALITY_SEMITONES_PER_OCTAVE * octaves;

    /* Shifting the perfect or major interval by the difference lands on
       the quality, if it is within triply altered. */
    StandardTonalityIntervalQuality base = standard_tonality_interval_size_is_perfect_class(size)
                                               ? STANDARD_TONALITY_QUALITY_PERFECT
                                               : STANDARD_TONALITY_QUALITY_MAJOR;
    return standard_tonality_interval_quality_shift(size, base, semitones - reference, quality);
}

bool standard_tonality_intervals_enharmonic(int size_a, StandardTonalityIntervalQuality quality_a, int size_b,
                                            StandardTonalityIntervalQuality quality_b) {
    int semitones_a;
    int semitones_b;
    if (!standard_tonality_interval_semitones(size_a, quality_a, &semitones_a) ||
        !standard_tonality_interval_semitones(size_b, quality_b, &semitones_b)) {
        return false;
    }
    if (size_a == size_b && quality_a == quality_b) {
        return false;
    }
    return semitones_a == semitones_b;
}

bool standard_tonality_spellings_enharmonic(const PitchSpelling *a, const PitchSpelling *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return standard_tonality_semitone_number(a) == standard_tonality_semitone_number(b) &&
           !pitch_spelling_equals(a, b);
}

/* The plainest spelling of a semitone number: natural, then sharp, then
   flat. Returns false only if none exists, which cannot happen. */
static bool plainest_spelling(int number, PitchLetter *letter, int *accidental, int *octave) {
    static const int ACCIDENTALS[3] = {0, 1, -1};
    for (size_t a = 0; a < 3; a++) {
        for (int place = 0; place < 7; place++) {
            PitchLetter candidate = pitch_letter_offset(PITCH_LETTER_C, place);
            int rest = number - standard_tonality_letter_semitone(candidate) - ACCIDENTALS[a];
            if (rest % STANDARD_TONALITY_SEMITONES_PER_OCTAVE == 0) {
                *letter = candidate;
                *accidental = ACCIDENTALS[a];
                *octave = rest / STANDARD_TONALITY_SEMITONES_PER_OCTAVE;
                return true;
            }
        }
    }
    return false;
}

bool standard_tonality_interval_quality_via_enharmonic(const PitchSpelling *lower, const PitchSpelling *upper,
                                                       int *size, StandardTonalityIntervalQuality *quality) {
    if (lower == NULL || upper == NULL || size == NULL || quality == NULL) {
        return false;
    }
    int generic;
    if (!standard_tonality_interval_generic_size(lower, upper, &generic)) {
        return false;
    }

    PitchLetter new_letter;
    int new_accidental;
    int new_octave;
    if (!plainest_spelling(standard_tonality_semitone_number(lower), &new_letter, &new_accidental, &new_octave)) {
        return false;
    }

    /* Move the upper note the same number of letters as the lower, so
       the size stays; it keeps its sound. */
    int letter_shift = pitch_letter_index(new_letter) - pitch_letter_index(pitch_spelling_letter(lower));
    PitchLetter upper_letter = pitch_letter_offset(PITCH_LETTER_C, pitch_letter_index(pitch_spelling_letter(upper)) + letter_shift);
    int upper_number = standard_tonality_semitone_number(upper);

    /* Find the octave and accidental of that letter with the upper
       note's sound. The octave number belongs to the letter, so it is
       the one that keeps the staff distance the same. */
    int lower_place = 7 * new_octave + pitch_letter_index(new_letter);
    int upper_place = lower_place + (generic - 1);
    int upper_octave = (upper_place - pitch_letter_index(upper_letter)) / 7;
    int upper_accidental = upper_number - (STANDARD_TONALITY_SEMITONES_PER_OCTAVE * upper_octave +
                                           standard_tonality_letter_semitone(upper_letter));

    PitchSpelling *new_lower;
    PitchSpelling *new_upper;
    if (pitch_spelling_create(new_letter, new_accidental, new_octave, &new_lower) != PITCH_SPELLING_OK) {
        return false;
    }
    if (upper_accidental < -2 || upper_accidental > 2 ||
        pitch_spelling_create(upper_letter, upper_accidental, upper_octave, &new_upper) != PITCH_SPELLING_OK) {
        pitch_spelling_destroy(new_lower);
        return false;
    }

    bool identified = standard_tonality_interval_quality_by_major_scale(new_lower, new_upper, size, quality);
    pitch_spelling_destroy(new_lower);
    pitch_spelling_destroy(new_upper);
    return identified;
}
