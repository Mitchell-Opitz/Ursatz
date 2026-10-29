#include "scale_octave_rules.h"

#include "galant-schemas/galant_degree.h"
#include "scale_rule_table.h"
#include "standard-counterpoint/counterpoint_interval.h"
#include "standard-counterpoint/counterpoint_pitch.h"

static const char *const NAMES[SCALE_OCTAVE_RULE_COUNT] = {"four-voices", "bass-is-scale", "tonic-chords",
                                                           "no-parallels", "first-inversion"};

static bool table_is_hard(int which) {
    return which <= SCALE_OCTAVE_NO_PARALLELS;
}

static ScaleRulePair g_pairs[SCALE_OCTAVE_RULE_COUNT];
static UrsatzOnce g_built[SCALE_OCTAVE_RULE_COUNT];
static const ScaleRuleTable TABLE = {"octave", NAMES, SCALE_OCTAVE_RULE_COUNT, table_is_hard, g_pairs, g_built};

const Rule *scale_octave_rule(ScaleOctaveRule which) {
    return scale_rule_table_rule(&TABLE, (int)which);
}

const Constraint *scale_octave_constraint(ScaleOctaveRule which) {
    return scale_rule_table_constraint(&TABLE, (int)which);
}

const char *scale_octave_rule_name(ScaleOctaveRule which) {
    return scale_rule_table_name(&TABLE, (int)which);
}

static bool check_bass_is_scale(const ScaleHarmonization *h) {
    static const int ASCENDING[SCALE_OCTAVE_STAGES] = {1, 2, 3, 4, 5, 6, 7, 1};
    static const int DESCENDING[SCALE_OCTAVE_STAGES] = {1, 7, 6, 5, 4, 3, 2, 1};
    if (h->stage_count != SCALE_OCTAVE_STAGES) {
        return false;
    }
    const int *expected = h->direction == SCALE_DIRECTION_ASCENDING ? ASCENDING : DESCENDING;
    for (size_t i = 0; i < SCALE_OCTAVE_STAGES; i++) {
        GalantPitchDegree degree;
        if (!galant_degree_of(h->stages[i].pitches[0], h->tonic, &degree) || degree.degree != expected[i] ||
            (degree.alteration != 0 && degree.alteration != -1)) {
            return false;
        }
    }
    return true;
}

/* A bit mask of the simple intervals the upper voices make above the bass at
   `stage`; 0 when an upper voice is below the bass. */
static unsigned intervals_at(const ScaleHarmonization *h, size_t stage) {
    unsigned mask = 0;
    for (size_t v = 1; v < h->voice_count; v++) {
        int interval;
        if (!scale_interval_above_bass(h, stage, v, &interval)) {
            return 0;
        }
        mask |= 1u << interval;
    }
    return mask;
}

static bool check_tonic_chords(const ScaleHarmonization *h) {
    const unsigned allowed = (1u << 3) | (1u << 5) | (1u << 8);
    const unsigned needed = (1u << 3) | (1u << 5);
    size_t stages[2] = {0, h->stage_count - 1};
    for (size_t k = 0; k < 2; k++) {
        unsigned mask = intervals_at(h, stages[k]);
        if (mask == 0 || (mask & ~allowed) != 0 || (mask & needed) != needed) {
            return false;
        }
    }
    return true;
}

/* 1 for a fifth, 2 for an octave or unison (perfect), 0 for neither. */
static int perfect_class(const PitchSpelling *a, const PitchSpelling *b) {
    StandardCounterpointInterval interval;
    if (!standard_counterpoint_interval_between(a, b, &interval) ||
        interval.quality != STANDARD_COUNTERPOINT_QUALITY_PERFECT) {
        return 0;
    }
    if (interval.simple_size == 5) {
        return 1;
    }
    return interval.simple_size == 8 || interval.simple_size == 1 ? 2 : 0;
}

static bool check_no_parallels(const ScaleHarmonization *h) {
    for (size_t s = 0; s + 1 < h->stage_count; s++) {
        for (size_t a = 0; a < h->voice_count; a++) {
            for (size_t b = a + 1; b < h->voice_count; b++) {
                const PitchSpelling *a0 = h->stages[s].pitches[a];
                const PitchSpelling *a1 = h->stages[s + 1].pitches[a];
                const PitchSpelling *b0 = h->stages[s].pitches[b];
                const PitchSpelling *b1 = h->stages[s + 1].pitches[b];
                int da = standard_counterpoint_direction(a0, a1);
                int db = standard_counterpoint_direction(b0, b1);
                int before = perfect_class(a0, b0);
                if (da != 0 && da == db && before != 0 && before == perfect_class(a1, b1)) {
                    return false;
                }
            }
        }
    }
    return true;
}

static bool check_first_inversion(const ScaleHarmonization *h) {
    const unsigned allowed = (1u << 3) | (1u << 6) | (1u << 8);
    for (size_t s = 1; s + 1 < h->stage_count; s++) {
        unsigned mask = intervals_at(h, s);
        if (mask == 0 || (mask & ~allowed) != 0) {
            return false;
        }
    }
    return true;
}

bool scale_octave_rule_holds(ScaleOctaveRule which, const ScaleHarmonization *harmonization) {
    if (!scale_rule_table_valid(&TABLE, (int)which) || !scale_harmonization_valid(harmonization)) {
        return false;
    }
    switch (which) {
        case SCALE_OCTAVE_FOUR_VOICES:
            return harmonization->voice_count == 4;
        case SCALE_OCTAVE_BASS_IS_SCALE:
            return check_bass_is_scale(harmonization);
        case SCALE_OCTAVE_TONIC_CHORDS:
            return check_tonic_chords(harmonization);
        case SCALE_OCTAVE_NO_PARALLELS:
            return check_no_parallels(harmonization);
        case SCALE_OCTAVE_FIRST_INVERSION:
            return check_first_inversion(harmonization);
        default:
            return false;
    }
}

static bool holds_adapter(int which, const void *subject) {
    return scale_octave_rule_holds((ScaleOctaveRule)which, subject);
}

bool scale_octave_check(const ScaleHarmonization *harmonization, unsigned *hard_violations,
                        unsigned *soft_violations) {
    if (hard_violations == NULL || soft_violations == NULL || !scale_harmonization_valid(harmonization)) {
        return false;
    }
    scale_rule_table_check(&TABLE, holds_adapter, harmonization, hard_violations, soft_violations);
    return true;
}
