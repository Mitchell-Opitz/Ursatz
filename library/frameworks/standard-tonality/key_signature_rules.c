#include "key_signature_rules.h"

#include <stddef.h>

#include "rule_pair.h"
#include "tonal_pitch.h"

static const char *const NAMES[STANDARD_TONALITY_KEY_SIGNATURE_RULE_COUNT] = {
    "key-signature-sharp-tonic", "key-signature-flat-tonic", "key-signature-memorized", "key-signature-c-family"};

static StandardTonalityRulePair g_pairs[STANDARD_TONALITY_KEY_SIGNATURE_RULE_COUNT];
static bool g_built[STANDARD_TONALITY_KEY_SIGNATURE_RULE_COUNT];

static bool valid_rule(StandardTonalityKeySignatureRule which) {
    return which >= 0 && which < STANDARD_TONALITY_KEY_SIGNATURE_RULE_COUNT;
}

static void ensure_built(StandardTonalityKeySignatureRule which) {
    if (!valid_rule(which) || g_built[which]) {
        return;
    }
    g_built[which] = true;
    standard_tonality_build_rule_pair(NAMES[which], &g_pairs[which]);
}

const Rule *standard_tonality_key_signature_rule(StandardTonalityKeySignatureRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].rule : NULL;
}

const Constraint *standard_tonality_key_signature_constraint(StandardTonalityKeySignatureRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].constraint : NULL;
}

#define MAX_ACCIDENTALS 7

static const PitchLetter SHARP_ORDER[MAX_ACCIDENTALS] = {PITCH_LETTER_F, PITCH_LETTER_C, PITCH_LETTER_G, PITCH_LETTER_D,
                                                         PITCH_LETTER_A, PITCH_LETTER_E, PITCH_LETTER_B};

/* Flats come in the reverse order of sharps. */
static PitchLetter flat_order_letter(size_t index) {
    return SHARP_ORDER[MAX_ACCIDENTALS - 1 - index];
}

bool standard_tonality_is_conventional_signature(const KeySignature *key_signature) {
    if (key_signature == NULL) {
        return false;
    }
    size_t count = key_signature_entry_count(key_signature);
    if (count == 0) {
        return true;
    }
    if (count > MAX_ACCIDENTALS) {
        return false;
    }

    int accidental = key_signature_accidental_at(key_signature, 0);
    if (accidental != 1 && accidental != -1) {
        return false;
    }
    for (size_t i = 0; i < count; i++) {
        if (key_signature_accidental_at(key_signature, i) != accidental) {
            return false;
        }
        PitchLetter expected = accidental == 1 ? SHARP_ORDER[i] : flat_order_letter(i);
        if (key_signature_letter_at(key_signature, i) != expected) {
            return false;
        }
    }
    return true;
}

bool standard_tonality_major_key_of_signature(const KeySignature *key_signature, PitchLetter *letter,
                                              int *accidental) {
    if (letter == NULL || accidental == NULL || !standard_tonality_is_conventional_signature(key_signature)) {
        return false;
    }

    size_t count = key_signature_entry_count(key_signature);
    if (count == 0) {
        /* MEMORIZED: C major. */
        *letter = PITCH_LETTER_C;
        *accidental = 0;
        return true;
    }

    if (key_signature_accidental_at(key_signature, 0) == 1) {
        /* SHARP_TONIC: the tonic is the next letter, a half step above
           the last sharp. */
        PitchLetter last = key_signature_letter_at(key_signature, count - 1);
        PitchLetter tonic = standard_tonality_next_letter(last);
        int target = standard_tonality_letter_semitone(last) + 1 + 1;
        int offset = (target - standard_tonality_letter_semitone(tonic)) % STANDARD_TONALITY_SEMITONES_PER_OCTAVE;
        if (offset < 0) {
            offset += STANDARD_TONALITY_SEMITONES_PER_OCTAVE;
        }
        if (offset > STANDARD_TONALITY_SEMITONES_PER_OCTAVE / 2) {
            offset -= STANDARD_TONALITY_SEMITONES_PER_OCTAVE;
        }
        *letter = tonic;
        *accidental = offset;
        return true;
    }

    if (count == 1) {
        /* MEMORIZED: F major, one flat (B-flat). */
        *letter = PITCH_LETTER_F;
        *accidental = 0;
        return true;
    }

    /* FLAT_TONIC: the second-to-last flat is the tonic, flat included. */
    *letter = key_signature_letter_at(key_signature, count - 2);
    *accidental = -1;
    return true;
}
