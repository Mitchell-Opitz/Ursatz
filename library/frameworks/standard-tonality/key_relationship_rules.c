#include "key_relationship_rules.h"

#include <stddef.h>

#include "major_scale_rules.h"
#include "minor_scale_rules.h"
#include "rule_pair.h"
#include "tonal_pitch.h"
#include "pitch/pitch_arithmetic.h"

static const char *const NAMES[STANDARD_TONALITY_KEY_RELATIONSHIP_RULE_COUNT] = {
    "key-relationship-parallel", "key-relationship-relative", "key-relationship-relative-spelling"};

static RulePairTable g_table = RULE_PAIR_TABLE_INIT(standard_tonality_framework, "standard-tonality",
    NAMES, STANDARD_TONALITY_KEY_RELATIONSHIP_RULE_COUNT);

static bool valid_rule(StandardTonalityKeyRelationshipRule which) {
    return which >= 0 && which < STANDARD_TONALITY_KEY_RELATIONSHIP_RULE_COUNT;
}

const Rule *standard_tonality_key_relationship_rule(StandardTonalityKeyRelationshipRule which) {
    const RulePair *pair = valid_rule(which) ? rule_pair_table_get(&g_table, (size_t)which) : NULL;
    return pair != NULL ? pair->rule : NULL;
}

const Constraint *standard_tonality_key_relationship_constraint(StandardTonalityKeyRelationshipRule which) {
    const RulePair *pair = valid_rule(which) ? rule_pair_table_get(&g_table, (size_t)which) : NULL;
    return pair != NULL ? pair->constraint : NULL;
}

bool standard_tonality_keys_parallel(PitchLetter major_letter, int major_accidental, PitchLetter minor_letter,
                                     int minor_accidental) {
    return major_letter == minor_letter && major_accidental == minor_accidental;
}

/* Name of the index-th note (0-based) of the major or natural minor
   scale on the given tonic. Octave is irrelevant to the name. */
static bool scale_note_name(bool major, PitchLetter letter, int accidental, size_t index, PitchLetter *out_letter,
                            int *out_accidental) {
    if (out_letter == NULL || out_accidental == NULL) {
        return false;
    }

    PitchSpelling *tonic;
    if (pitch_spelling_create(letter, accidental, 4, &tonic) != PITCH_SPELLING_OK) {
        return false;
    }

    PitchSpelling *scale[8];
    bool built = major ? standard_tonality_build_major_scale(tonic, scale)
                       : standard_tonality_build_minor_scale(STANDARD_TONALITY_MINOR_NATURAL, tonic, scale);
    pitch_spelling_destroy(tonic);
    if (!built) {
        return false;
    }

    *out_letter = pitch_spelling_letter(scale[index]);
    *out_accidental = pitch_spelling_accidental(scale[index]);
    for (size_t i = 0; i < 8; i++) {
        pitch_spelling_destroy(scale[i]);
    }
    return true;
}

bool standard_tonality_relative_minor(PitchLetter major_letter, int major_accidental, PitchLetter *minor_letter,
                                      int *minor_accidental) {
    return scale_note_name(true, major_letter, major_accidental, 5, minor_letter, minor_accidental);
}

bool standard_tonality_relative_major(PitchLetter minor_letter, int minor_accidental, PitchLetter *major_letter,
                                      int *major_accidental) {
    return scale_note_name(false, minor_letter, minor_accidental, 2, major_letter, major_accidental);
}

bool standard_tonality_keys_relative(PitchLetter major_letter, int major_accidental, PitchLetter minor_letter,
                                     int minor_accidental) {
    PitchLetter expected_letter;
    int expected_accidental;
    if (!standard_tonality_relative_minor(major_letter, major_accidental, &expected_letter, &expected_accidental)) {
        return false;
    }
    if (expected_letter != minor_letter || expected_accidental != minor_accidental) {
        return false;
    }
    int below = pitch_class_of_letter(major_letter, major_accidental) -
                pitch_class_of_letter(minor_letter, minor_accidental);
    if (below < 0) {
        below += STANDARD_TONALITY_SEMITONES_PER_OCTAVE;
    }
    return below == 3;
}
