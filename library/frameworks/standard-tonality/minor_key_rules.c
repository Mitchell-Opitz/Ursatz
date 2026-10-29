#include "minor_key_rules.h"

#include <stddef.h>

#include "circle_of_fifths_rules.h"
#include "key_relationship_rules.h"
#include "key_signature_rules.h"
#include "rule_pair.h"

static const char *const NAMES[STANDARD_TONALITY_MINOR_KEY_RULE_COUNT] = {
    "minor-key-natural-basis", "minor-key-signature-tonic", "minor-key-real-imaginary", "minor-key-candidates"};

static RulePairTable g_table = RULE_PAIR_TABLE_INIT(standard_tonality_framework, "standard-tonality",
    NAMES, STANDARD_TONALITY_MINOR_KEY_RULE_COUNT);

static bool valid_rule(StandardTonalityMinorKeyRule which) {
    return which >= 0 && which < STANDARD_TONALITY_MINOR_KEY_RULE_COUNT;
}

const Rule *standard_tonality_minor_key_rule(StandardTonalityMinorKeyRule which) {
    const RulePair *pair = valid_rule(which) ? rule_pair_table_get(&g_table, (size_t)which) : NULL;
    return pair != NULL ? pair->rule : NULL;
}

const Constraint *standard_tonality_minor_key_constraint(StandardTonalityMinorKeyRule which) {
    const RulePair *pair = valid_rule(which) ? rule_pair_table_get(&g_table, (size_t)which) : NULL;
    return pair != NULL ? pair->constraint : NULL;
}

StandardTonalityMinorForm standard_tonality_minor_key_scale_form(void) {
    return STANDARD_TONALITY_MINOR_NATURAL;
}

bool standard_tonality_minor_key_of_signature(const KeySignature *key_signature, PitchLetter *letter,
                                              int *accidental) {
    if (letter == NULL || accidental == NULL) {
        return false;
    }
    PitchLetter major_letter;
    int major_accidental;
    if (!standard_tonality_major_key_of_signature(key_signature, &major_letter, &major_accidental)) {
        return false;
    }
    return standard_tonality_relative_minor(major_letter, major_accidental, letter, accidental);
}

/* A minor key keeps its relative major's signature: the tonic sits three
   steps down the line of fifths from the major tonic's, which is a
   count of 3 less than the same-named major key's. */
int standard_tonality_minor_key_accidentals(PitchLetter letter, int accidental) {
    return standard_tonality_major_key_accidentals(letter, accidental) - 3;
}

bool standard_tonality_minor_key_is_real(PitchLetter letter, int accidental) {
    int count = standard_tonality_minor_key_accidentals(letter, accidental);
    return count >= -7 && count <= 7;
}

bool standard_tonality_minor_key_signature(PitchLetter letter, int accidental, KeySignature **out) {
    if (!standard_tonality_minor_key_is_real(letter, accidental)) {
        return false;
    }
    PitchLetter major_letter;
    int major_accidental;
    if (!standard_tonality_relative_major(letter, accidental, &major_letter, &major_accidental)) {
        return false;
    }
    return standard_tonality_major_key_signature(major_letter, major_accidental, out);
}

bool standard_tonality_signature_key_candidates(const KeySignature *key_signature, PitchLetter *major_letter,
                                                int *major_accidental, PitchLetter *minor_letter,
                                                int *minor_accidental) {
    if (major_letter == NULL || major_accidental == NULL || minor_letter == NULL || minor_accidental == NULL) {
        return false;
    }
    PitchLetter major;
    int major_acc;
    if (!standard_tonality_major_key_of_signature(key_signature, &major, &major_acc)) {
        return false;
    }
    PitchLetter minor;
    int minor_acc;
    if (!standard_tonality_relative_minor(major, major_acc, &minor, &minor_acc)) {
        return false;
    }
    *major_letter = major;
    *major_accidental = major_acc;
    *minor_letter = minor;
    *minor_accidental = minor_acc;
    return true;
}
