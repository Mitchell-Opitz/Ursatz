#include "interval_inversion_rules.h"

#include <stddef.h>

#include "interval_size_rules.h"
#include "rule_pair.h"

static const char *const NAMES[STANDARD_TONALITY_INTERVAL_INVERSION_RULE_COUNT] = {
    "interval-inversion-sizes", "interval-inversion-qualities", "interval-inversion-identification"};

static StandardTonalityRulePair g_pairs[STANDARD_TONALITY_INTERVAL_INVERSION_RULE_COUNT];
static bool g_built[STANDARD_TONALITY_INTERVAL_INVERSION_RULE_COUNT];

static bool valid_rule(StandardTonalityIntervalInversionRule which) {
    return which >= 0 && which < STANDARD_TONALITY_INTERVAL_INVERSION_RULE_COUNT;
}

static void ensure_built(StandardTonalityIntervalInversionRule which) {
    if (!valid_rule(which) || g_built[which]) {
        return;
    }
    g_built[which] = true;
    standard_tonality_build_rule_pair(NAMES[which], &g_pairs[which]);
}

const Rule *standard_tonality_interval_inversion_rule(StandardTonalityIntervalInversionRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].rule : NULL;
}

const Constraint *standard_tonality_interval_inversion_constraint(StandardTonalityIntervalInversionRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].constraint : NULL;
}

int standard_tonality_interval_inverted_size(int size) {
    int simple = standard_tonality_interval_simple_size(size);
    if (simple == 0) {
        return 0;
    }
    return 9 - simple;
}

StandardTonalityIntervalQuality standard_tonality_interval_inverted_quality(StandardTonalityIntervalQuality quality) {
    switch (quality) {
        case STANDARD_TONALITY_QUALITY_MAJOR:
            return STANDARD_TONALITY_QUALITY_MINOR;
        case STANDARD_TONALITY_QUALITY_MINOR:
            return STANDARD_TONALITY_QUALITY_MAJOR;
        case STANDARD_TONALITY_QUALITY_AUGMENTED:
            return STANDARD_TONALITY_QUALITY_DIMINISHED;
        case STANDARD_TONALITY_QUALITY_DOUBLY_AUGMENTED:
            return STANDARD_TONALITY_QUALITY_DOUBLY_DIMINISHED;
        case STANDARD_TONALITY_QUALITY_TRIPLY_AUGMENTED:
            return STANDARD_TONALITY_QUALITY_TRIPLY_DIMINISHED;
        case STANDARD_TONALITY_QUALITY_DIMINISHED:
            return STANDARD_TONALITY_QUALITY_AUGMENTED;
        case STANDARD_TONALITY_QUALITY_DOUBLY_DIMINISHED:
            return STANDARD_TONALITY_QUALITY_DOUBLY_AUGMENTED;
        case STANDARD_TONALITY_QUALITY_TRIPLY_DIMINISHED:
            return STANDARD_TONALITY_QUALITY_TRIPLY_AUGMENTED;
        case STANDARD_TONALITY_QUALITY_PERFECT:
        default:
            return quality;
    }
}

bool standard_tonality_interval_invert(int size, StandardTonalityIntervalQuality quality, int *inverted_size,
                                       StandardTonalityIntervalQuality *inverted_quality) {
    if (inverted_size == NULL || inverted_quality == NULL ||
        !standard_tonality_interval_quality_fits_size(size, quality)) {
        return false;
    }
    *inverted_size = standard_tonality_interval_inverted_size(size);
    *inverted_quality = standard_tonality_interval_inverted_quality(quality);
    return true;
}

bool standard_tonality_interval_quality_via_inversion(const PitchSpelling *lower, const PitchSpelling *upper,
                                                      int *size, StandardTonalityIntervalQuality *quality) {
    if (lower == NULL || upper == NULL || size == NULL || quality == NULL) {
        return false;
    }

    int generic;
    if (!standard_tonality_interval_generic_size(lower, upper, &generic) ||
        !standard_tonality_interval_is_simple(generic)) {
        return false;
    }

    /* The old lower note, an octave up, becomes the new top. */
    PitchSpelling *raised;
    if (pitch_spelling_create(pitch_spelling_letter(lower), pitch_spelling_accidental(lower),
                              pitch_spelling_octave(lower) + 1, &raised) != PITCH_SPELLING_OK) {
        return false;
    }

    int inverse_size;
    StandardTonalityIntervalQuality inverse_quality;
    bool identified = standard_tonality_interval_quality_by_major_scale(upper, raised, &inverse_size, &inverse_quality);
    pitch_spelling_destroy(raised);
    if (!identified) {
        return false;
    }

    return standard_tonality_interval_invert(inverse_size, inverse_quality, size, quality);
}
