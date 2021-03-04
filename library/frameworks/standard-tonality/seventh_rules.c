#include "seventh_rules.h"

#include <stddef.h>

#include "interval_size_rules.h"
#include "rule_pair.h"

static const char *const NAMES[STANDARD_TONALITY_SEVENTH_RULE_COUNT] = {
    "seventh-stacked-in-thirds", "seventh-lines-or-spaces", "seventh-root-third-fifth-seventh", "seventh-qualities",
    "seventh-naming",            "seventh-alternate-names"};

static StandardTonalityRulePair g_pairs[STANDARD_TONALITY_SEVENTH_RULE_COUNT];
static bool g_built[STANDARD_TONALITY_SEVENTH_RULE_COUNT];

static bool valid_rule(StandardTonalitySeventhRule which) {
    return which >= 0 && which < STANDARD_TONALITY_SEVENTH_RULE_COUNT;
}

static void ensure_built(StandardTonalitySeventhRule which) {
    if (!valid_rule(which) || g_built[which]) {
        return;
    }
    g_built[which] = true;
    standard_tonality_build_rule_pair(NAMES[which], &g_pairs[which]);
}

const Rule *standard_tonality_seventh_rule(StandardTonalitySeventhRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].rule : NULL;
}

const Constraint *standard_tonality_seventh_constraint(StandardTonalitySeventhRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].constraint : NULL;
}

static bool valid_quality(StandardTonalitySeventhQuality quality) {
    return quality >= 0 && quality < STANDARD_TONALITY_SEVENTH_QUALITY_COUNT;
}

static const char *const QUALITY_NAMES[STANDARD_TONALITY_SEVENTH_QUALITY_COUNT] = {
    "major-major seventh", "major-minor seventh", "minor-minor seventh", "half-diminished seventh",
    "fully diminished seventh"};

const char *standard_tonality_seventh_quality_name(StandardTonalitySeventhQuality quality) {
    return valid_quality(quality) ? QUALITY_NAMES[quality] : NULL;
}

static const char *const ALTERNATE_NAMES[STANDARD_TONALITY_SEVENTH_QUALITY_COUNT] = {
    "major seventh", "dominant seventh", "minor seventh", NULL, "diminished seventh"};

const char *standard_tonality_seventh_quality_alternate_name(StandardTonalitySeventhQuality quality) {
    return valid_quality(quality) ? ALTERNATE_NAMES[quality] : NULL;
}

static const char *const MEMBER_NAMES[STANDARD_TONALITY_SEVENTH_MEMBER_COUNT] = {"root", "third", "fifth", "seventh"};

const char *standard_tonality_seventh_member_name(StandardTonalitySeventhMember member) {
    if (member < 0 || member >= STANDARD_TONALITY_SEVENTH_MEMBER_COUNT) {
        return NULL;
    }
    return MEMBER_NAMES[member];
}

int standard_tonality_seventh_member_size(StandardTonalitySeventhMember member) {
    switch (member) {
        case STANDARD_TONALITY_SEVENTH_ROOT:
            return 1;
        case STANDARD_TONALITY_SEVENTH_THIRD:
            return 3;
        case STANDARD_TONALITY_SEVENTH_FIFTH:
            return 5;
        case STANDARD_TONALITY_SEVENTH_SEVENTH:
            return 7;
        default:
            return 0;
    }
}

/* Triad quality and seventh quality of each seventh chord quality. */
static const StandardTonalityTriadQuality TRIADS[STANDARD_TONALITY_SEVENTH_QUALITY_COUNT] = {
    STANDARD_TONALITY_TRIAD_MAJOR, STANDARD_TONALITY_TRIAD_MAJOR, STANDARD_TONALITY_TRIAD_MINOR,
    STANDARD_TONALITY_TRIAD_DIMINISHED, STANDARD_TONALITY_TRIAD_DIMINISHED};
static const StandardTonalityIntervalQuality SEVENTHS[STANDARD_TONALITY_SEVENTH_QUALITY_COUNT] = {
    STANDARD_TONALITY_QUALITY_MAJOR, STANDARD_TONALITY_QUALITY_MINOR, STANDARD_TONALITY_QUALITY_MINOR,
    STANDARD_TONALITY_QUALITY_MINOR, STANDARD_TONALITY_QUALITY_DIMINISHED};

bool standard_tonality_seventh_parts(StandardTonalitySeventhQuality quality, StandardTonalityTriadQuality *triad,
                                     StandardTonalityIntervalQuality *seventh) {
    if (!valid_quality(quality) || triad == NULL || seventh == NULL) {
        return false;
    }
    *triad = TRIADS[quality];
    *seventh = SEVENTHS[quality];
    return true;
}

bool standard_tonality_seventh_quality_from_parts(StandardTonalityTriadQuality triad,
                                                  StandardTonalityIntervalQuality seventh,
                                                  StandardTonalitySeventhQuality *out) {
    if (out == NULL) {
        return false;
    }
    for (int q = 0; q < STANDARD_TONALITY_SEVENTH_QUALITY_COUNT; q++) {
        if (TRIADS[q] == triad && SEVENTHS[q] == seventh) {
            *out = (StandardTonalitySeventhQuality)q;
            return true;
        }
    }
    return false;
}

bool standard_tonality_is_seventh_stack(const PitchSpelling *root, const PitchSpelling *third,
                                        const PitchSpelling *fifth, const PitchSpelling *seventh) {
    if (!standard_tonality_is_triad_stack(root, third, fifth)) {
        return false;
    }
    int seventh_size;
    if (!standard_tonality_interval_generic_size(root, seventh, &seventh_size)) {
        return false;
    }
    return seventh_size == 7;
}

bool standard_tonality_seventh_on_lines_or_spaces(const PitchSpelling *a, const PitchSpelling *b,
                                                  const PitchSpelling *c, const PitchSpelling *d) {
    /* Parity is transitive: a, b, c alike and b, c, d alike make all four
       alike. */
    return standard_tonality_triad_on_lines_or_spaces(a, b, c) && standard_tonality_triad_on_lines_or_spaces(b, c, d);
}

bool standard_tonality_seventh_stack_quality(const PitchSpelling *root, const PitchSpelling *third,
                                             const PitchSpelling *fifth, const PitchSpelling *seventh,
                                             StandardTonalitySeventhQuality *quality) {
    if (quality == NULL || !standard_tonality_is_seventh_stack(root, third, fifth, seventh)) {
        return false;
    }
    StandardTonalityTriadQuality triad;
    if (!standard_tonality_triad_stack_quality(root, third, fifth, &triad)) {
        return false;
    }
    int seventh_size;
    StandardTonalityIntervalQuality seventh_quality;
    if (!standard_tonality_interval_quality(root, seventh, &seventh_size, &seventh_quality)) {
        return false;
    }
    return standard_tonality_seventh_quality_from_parts(triad, seventh_quality, quality);
}
