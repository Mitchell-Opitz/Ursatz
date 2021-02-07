#include "triad_in_key_rules.h"

#include <stddef.h>

#include "major_scale_rules.h"
#include "minor_scale_rules.h"
#include "rule_pair.h"

static const char *const NAMES[STANDARD_TONALITY_TRIAD_IN_KEY_RULE_COUNT] = {
    "triad-in-key-major-key", "triad-in-key-minor-key", "triad-in-key-raised-leading-tone",
    "triad-in-key-key-independent"};

static StandardTonalityRulePair g_pairs[STANDARD_TONALITY_TRIAD_IN_KEY_RULE_COUNT];
static bool g_built[STANDARD_TONALITY_TRIAD_IN_KEY_RULE_COUNT];

static bool valid_rule(StandardTonalityTriadInKeyRule which) {
    return which >= 0 && which < STANDARD_TONALITY_TRIAD_IN_KEY_RULE_COUNT;
}

static void ensure_built(StandardTonalityTriadInKeyRule which) {
    if (!valid_rule(which) || g_built[which]) {
        return;
    }
    g_built[which] = true;
    standard_tonality_build_rule_pair(NAMES[which], &g_pairs[which]);
}

const Rule *standard_tonality_triad_in_key_rule(StandardTonalityTriadInKeyRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].rule : NULL;
}

const Constraint *standard_tonality_triad_in_key_constraint(StandardTonalityTriadInKeyRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].constraint : NULL;
}

#define MA STANDARD_TONALITY_TRIAD_MAJOR
#define MI STANDARD_TONALITY_TRIAD_MINOR
#define DIM STANDARD_TONALITY_TRIAD_DIMINISHED
#define AUG STANDARD_TONALITY_TRIAD_AUGMENTED

static const StandardTonalityTriadQuality QUALITIES[STANDARD_TONALITY_TRIAD_KEY_COUNT][7] = {
    {MA, MI, MI, MA, MA, MI, DIM}, /* major */
    {MI, DIM, MA, MI, MI, MA, MA}, /* natural minor */
    {MI, DIM, AUG, MI, MA, MA, DIM}, /* harmonic minor: raised leading tone */
};

static bool valid_key(StandardTonalityTriadKey key) {
    return key >= 0 && key < STANDARD_TONALITY_TRIAD_KEY_COUNT;
}

bool standard_tonality_triad_quality_in_key(StandardTonalityTriadKey key, int degree,
                                            StandardTonalityTriadQuality *quality) {
    if (!valid_key(key) || degree < 1 || degree > 7 || quality == NULL) {
        return false;
    }
    *quality = QUALITIES[key][degree - 1];
    return true;
}

bool standard_tonality_spell_diatonic_triad(StandardTonalityTriadKey key, const PitchSpelling *tonic, int degree,
                                            PitchSpelling *out[3]) {
    if (!valid_key(key) || tonic == NULL || degree < 1 || degree > 7 || out == NULL) {
        return false;
    }

    PitchSpelling *scale[8];
    bool built;
    switch (key) {
        case STANDARD_TONALITY_TRIAD_KEY_MAJOR:
            built = standard_tonality_build_major_scale(tonic, scale);
            break;
        case STANDARD_TONALITY_TRIAD_KEY_MINOR_NATURAL:
            built = standard_tonality_build_minor_scale(STANDARD_TONALITY_MINOR_NATURAL, tonic, scale);
            break;
        default:
            built = standard_tonality_build_minor_scale(STANDARD_TONALITY_MINOR_HARMONIC, tonic, scale);
            break;
    }
    if (!built) {
        return false;
    }

    /* Root, third, and fifth are the scale notes at the degree, two
       above, and four above; the scale repeats an octave up. */
    PitchSpelling *built_notes[3] = {NULL, NULL, NULL};
    bool ok = true;
    for (size_t i = 0; i < 3 && ok; i++) {
        size_t index = (size_t)(degree - 1) + 2 * i;
        const PitchSpelling *source = scale[index % 7];
        int octave = pitch_spelling_octave(source) + (int)(index / 7);
        ok = pitch_spelling_create(pitch_spelling_letter(source), pitch_spelling_accidental(source), octave,
                                   &built_notes[i]) == PITCH_SPELLING_OK;
    }

    for (size_t i = 0; i < 8; i++) {
        pitch_spelling_destroy(scale[i]);
    }

    if (!ok) {
        for (size_t i = 0; i < 3; i++) {
            if (built_notes[i] != NULL) {
                pitch_spelling_destroy(built_notes[i]);
            }
        }
        return false;
    }
    for (size_t i = 0; i < 3; i++) {
        out[i] = built_notes[i];
    }
    return true;
}
