#include "galant_matching_rules.h"

#include "galant_rule_table.h"
#include "standard-counterpoint/counterpoint_interval.h"
#include "standard-counterpoint/counterpoint_pitch.h"

static const char *const NAMES[GALANT_MATCH_RULE_COUNT] = {"stage-count", "meter",      "melody",
                                                           "bass-defining", "bass-usual", "tenths"};

static bool table_is_hard(int which) {
    return which <= GALANT_MATCH_BASS_DEFINING;
}

static GalantRulePair g_pairs[GALANT_MATCH_RULE_COUNT];
static UrsatzOnce g_built[GALANT_MATCH_RULE_COUNT];
static const GalantRuleTable TABLE = {"matching", NAMES, GALANT_MATCH_RULE_COUNT, table_is_hard, g_pairs, g_built};

const Rule *galant_matching_rule(GalantMatchRule which) {
    return galant_rule_table_rule(&TABLE, (int)which);
}

const Constraint *galant_matching_constraint(GalantMatchRule which) {
    return galant_rule_table_constraint(&TABLE, (int)which);
}

const char *galant_matching_rule_name(GalantMatchRule which) {
    return galant_rule_table_name(&TABLE, (int)which);
}

bool galant_match_valid(const GalantMatch *match) {
    return match != NULL && match->prototype != NULL && galant_exemplar_valid(match->exemplar) &&
           match->prototype->stage_count > 0 && match->prototype->stage_count <= GALANT_MAX_STAGES;
}

static bool same_stage_count(const GalantMatch *match) {
    return match->prototype->stage_count == match->exemplar->stage_count;
}

static bool pitch_allowed(const GalantDegree *wanted, const PitchSpelling *pitch, const PitchSpelling *tonic) {
    GalantPitchDegree actual;
    return galant_degree_of(pitch, tonic, &actual) && galant_degree_allows(wanted, &actual);
}

static bool check_meter(const GalantMatch *match) {
    if (!same_stage_count(match)) {
        return false;
    }
    for (size_t i = 0; i < match->exemplar->stage_count; i++) {
        if (match->exemplar->stages[i].strength != match->prototype->stages[i].strength) {
            return false;
        }
    }
    return true;
}

static bool check_melody(const GalantMatch *match) {
    if (!same_stage_count(match)) {
        return false;
    }
    for (size_t i = 0; i < match->exemplar->stage_count; i++) {
        const GalantStage *wanted = &match->prototype->stages[i];
        const GalantExemplarStage *actual = &match->exemplar->stages[i];
        if (!pitch_allowed(&wanted->melody, actual->melody, match->exemplar->tonic)) {
            return false;
        }
        bool wants_next = wanted->melody_next.degree != 0;
        if (wants_next != (actual->melody_next != NULL) ||
            (wants_next && !pitch_allowed(&wanted->melody_next, actual->melody_next, match->exemplar->tonic))) {
            return false;
        }
    }
    return true;
}

static bool check_bass(const GalantMatch *match) {
    if (!same_stage_count(match)) {
        return false;
    }
    for (size_t i = 0; i < match->exemplar->stage_count; i++) {
        if (!pitch_allowed(&match->prototype->stages[i].bass, match->exemplar->stages[i].bass,
                           match->exemplar->tonic)) {
            return false;
        }
    }
    return true;
}

static bool check_tenths(const GalantMatch *match) {
    if (!match->prototype->melody_in_tenths) {
        return true;
    }
    for (size_t i = 0; i < match->exemplar->stage_count; i++) {
        const PitchSpelling *melody = match->exemplar->stages[i].melody;
        const PitchSpelling *bass = match->exemplar->stages[i].bass;
        StandardCounterpointInterval interval;
        if (standard_counterpoint_semitone_number(melody) <= standard_counterpoint_semitone_number(bass) ||
            !standard_counterpoint_interval_between(bass, melody, &interval) || interval.simple_size != 3) {
            return false;
        }
    }
    return true;
}

bool galant_matching_rule_holds(GalantMatchRule which, const GalantMatch *match) {
    if (!galant_rule_table_valid(&TABLE, (int)which) || !galant_match_valid(match)) {
        return false;
    }
    switch (which) {
        case GALANT_MATCH_STAGE_COUNT:
            return same_stage_count(match);
        case GALANT_MATCH_METER:
            return check_meter(match);
        case GALANT_MATCH_MELODY:
            return check_melody(match);
        case GALANT_MATCH_BASS_DEFINING:
            return !match->prototype->bass_defining || check_bass(match);
        case GALANT_MATCH_BASS_USUAL:
            return match->prototype->bass_defining || check_bass(match);
        case GALANT_MATCH_TENTHS:
            return check_tenths(match);
        default:
            return false;
    }
}

static bool holds_adapter(int which, const void *subject) {
    return galant_matching_rule_holds((GalantMatchRule)which, subject);
}

bool galant_matching_check(const GalantMatch *match, unsigned *hard_violations, unsigned *soft_violations) {
    if (hard_violations == NULL || soft_violations == NULL || !galant_match_valid(match)) {
        return false;
    }
    galant_rule_table_check(&TABLE, holds_adapter, match, hard_violations, soft_violations);
    return true;
}
