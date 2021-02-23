#include "triad_rules.h"

#include <stddef.h>

#include "interval_size_rules.h"
#include "rule_pair.h"
#include "tonal_pitch.h"

static const char *const NAMES[STANDARD_TONALITY_TRIAD_RULE_COUNT] = {"triad-stacked-in-thirds", "triad-lines-or-spaces",
                                                                       "triad-root-third-fifth", "triad-qualities",
                                                                       "triad-naming"};

static StandardTonalityRulePair g_pairs[STANDARD_TONALITY_TRIAD_RULE_COUNT];
static bool g_built[STANDARD_TONALITY_TRIAD_RULE_COUNT];

static bool valid_rule(StandardTonalityTriadRule which) {
    return which >= 0 && which < STANDARD_TONALITY_TRIAD_RULE_COUNT;
}

static void ensure_built(StandardTonalityTriadRule which) {
    if (!valid_rule(which) || g_built[which]) {
        return;
    }
    g_built[which] = true;
    standard_tonality_build_rule_pair(NAMES[which], &g_pairs[which]);
}

const Rule *standard_tonality_triad_rule(StandardTonalityTriadRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].rule : NULL;
}

const Constraint *standard_tonality_triad_constraint(StandardTonalityTriadRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].constraint : NULL;
}

static bool valid_quality(StandardTonalityTriadQuality quality) {
    return quality >= 0 && quality < STANDARD_TONALITY_TRIAD_QUALITY_COUNT;
}

static const char *const QUALITY_NAMES[STANDARD_TONALITY_TRIAD_QUALITY_COUNT] = {"major", "minor", "diminished",
                                                                                  "augmented"};

const char *standard_tonality_triad_quality_name(StandardTonalityTriadQuality quality) {
    return valid_quality(quality) ? QUALITY_NAMES[quality] : NULL;
}

static const char *const MEMBER_NAMES[STANDARD_TONALITY_TRIAD_MEMBER_COUNT] = {"root", "third", "fifth"};

const char *standard_tonality_triad_member_name(StandardTonalityTriadMember member) {
    if (member < 0 || member >= STANDARD_TONALITY_TRIAD_MEMBER_COUNT) {
        return NULL;
    }
    return MEMBER_NAMES[member];
}

int standard_tonality_triad_member_size(StandardTonalityTriadMember member) {
    switch (member) {
        case STANDARD_TONALITY_TRIAD_ROOT:
            return 1;
        case STANDARD_TONALITY_TRIAD_THIRD:
            return 3;
        case STANDARD_TONALITY_TRIAD_FIFTH:
            return 5;
        default:
            return 0;
    }
}

/* Third and fifth qualities of each triad quality. */
static const StandardTonalityIntervalQuality THIRDS[STANDARD_TONALITY_TRIAD_QUALITY_COUNT] = {
    STANDARD_TONALITY_QUALITY_MAJOR, STANDARD_TONALITY_QUALITY_MINOR, STANDARD_TONALITY_QUALITY_MINOR,
    STANDARD_TONALITY_QUALITY_MAJOR};
static const StandardTonalityIntervalQuality FIFTHS[STANDARD_TONALITY_TRIAD_QUALITY_COUNT] = {
    STANDARD_TONALITY_QUALITY_PERFECT, STANDARD_TONALITY_QUALITY_PERFECT, STANDARD_TONALITY_QUALITY_DIMINISHED,
    STANDARD_TONALITY_QUALITY_AUGMENTED};

bool standard_tonality_triad_intervals(StandardTonalityTriadQuality quality, StandardTonalityIntervalQuality *third,
                                       StandardTonalityIntervalQuality *fifth) {
    if (!valid_quality(quality) || third == NULL || fifth == NULL) {
        return false;
    }
    *third = THIRDS[quality];
    *fifth = FIFTHS[quality];
    return true;
}

bool standard_tonality_triad_quality_from_intervals(StandardTonalityIntervalQuality third,
                                                    StandardTonalityIntervalQuality fifth,
                                                    StandardTonalityTriadQuality *out) {
    if (out == NULL) {
        return false;
    }
    for (int q = 0; q < STANDARD_TONALITY_TRIAD_QUALITY_COUNT; q++) {
        if (THIRDS[q] == third && FIFTHS[q] == fifth) {
            *out = (StandardTonalityTriadQuality)q;
            return true;
        }
    }
    return false;
}

bool standard_tonality_triad_named_for_third(StandardTonalityTriadQuality quality) {
    return quality == STANDARD_TONALITY_TRIAD_MAJOR || quality == STANDARD_TONALITY_TRIAD_MINOR;
}

bool standard_tonality_triad_named_for_fifth(StandardTonalityTriadQuality quality) {
    return quality == STANDARD_TONALITY_TRIAD_DIMINISHED || quality == STANDARD_TONALITY_TRIAD_AUGMENTED;
}

bool standard_tonality_triad_naming_interval(StandardTonalityTriadQuality quality, int *size,
                                             StandardTonalityIntervalQuality *interval_quality) {
    if (!valid_quality(quality) || size == NULL || interval_quality == NULL) {
        return false;
    }
    if (standard_tonality_triad_named_for_third(quality)) {
        *size = 3;
        *interval_quality = THIRDS[quality];
    } else {
        *size = 5;
        *interval_quality = FIFTHS[quality];
    }
    return true;
}

bool standard_tonality_is_triad_stack(const PitchSpelling *root, const PitchSpelling *third,
                                      const PitchSpelling *fifth) {
    int third_size;
    int fifth_size;
    if (!standard_tonality_interval_generic_size(root, third, &third_size) ||
        !standard_tonality_interval_generic_size(root, fifth, &fifth_size)) {
        return false;
    }
    return third_size == 3 && fifth_size == 5;
}

/* Place of the note on the staff counted in letters: 7 per octave. A line
   and a space alternate, so places of the same parity are all lines or
   all spaces whatever the clef. */
static int staff_place(const PitchSpelling *spelling) {
    int index = 0;
    PitchLetter walk = PITCH_LETTER_C;
    while (walk != pitch_spelling_letter(spelling)) {
        walk = standard_tonality_next_letter(walk);
        index++;
    }
    return 7 * pitch_spelling_octave(spelling) + index;
}

bool standard_tonality_triad_on_lines_or_spaces(const PitchSpelling *a, const PitchSpelling *b,
                                                const PitchSpelling *c) {
    if (a == NULL || b == NULL || c == NULL) {
        return false;
    }
    int parity_a = ((staff_place(a) % 2) + 2) % 2;
    int parity_b = ((staff_place(b) % 2) + 2) % 2;
    int parity_c = ((staff_place(c) % 2) + 2) % 2;
    return parity_a == parity_b && parity_b == parity_c;
}

bool standard_tonality_triad_stack_quality(const PitchSpelling *root, const PitchSpelling *third,
                                           const PitchSpelling *fifth, StandardTonalityTriadQuality *quality) {
    if (quality == NULL || !standard_tonality_is_triad_stack(root, third, fifth)) {
        return false;
    }
    int third_size;
    int fifth_size;
    StandardTonalityIntervalQuality third_quality;
    StandardTonalityIntervalQuality fifth_quality;
    if (!standard_tonality_interval_quality(root, third, &third_size, &third_quality) ||
        !standard_tonality_interval_quality(root, fifth, &fifth_size, &fifth_quality)) {
        return false;
    }
    return standard_tonality_triad_quality_from_intervals(third_quality, fifth_quality, quality);
}
