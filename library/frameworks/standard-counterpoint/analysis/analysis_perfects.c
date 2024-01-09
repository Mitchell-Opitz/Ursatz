#include "analysis_perfects.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "analysis_claim.h"
#include "core/value.h"
#include "counterpoint_interval.h"
#include "counterpoint_line.h"
#include "harmonic_rules.h"
#include "identity/entity_id.h"

static const char *const RULE_NAMES[STANDARD_COUNTERPOINT_VIOLATION_COUNT] = {"parallel-fifths", "parallel-octaves",
                                                                              "direct-perfects"};

const char *standard_counterpoint_violation_rule_name(StandardCounterpointViolationRule rule) {
    return rule >= 0 && rule < STANDARD_COUNTERPOINT_VIOLATION_COUNT ? RULE_NAMES[rule] : NULL;
}

static void destroy_violation(StandardCounterpointViolation *violation) {
    interpretation_destroy(violation->interpretation);
    structure_destroy(violation->shell);
    memset(violation, 0, sizeof(*violation));
}

void standard_counterpoint_violations_destroy(StandardCounterpointViolations *violations) {
    if (violations == NULL) {
        return;
    }
    for (size_t i = 0; i < violations->count; i++) {
        destroy_violation(&violations->items[i]);
    }
    free(violations->items);
    violations->items = NULL;
    violations->count = 0;
}

/* Appends one occurrence. */
static bool add_violation(const StandardCounterpointPreparedGrid *prepared, StandardCounterpointViolations *found,
                          StandardCounterpointViolationRule rule, size_t s, size_t upper, size_t lower, int arrival,
                          const StandardCounterpointVoiceCell *const cells[4]) {
    const StandardCounterpointVoiceGrid *grid = prepared->grid;
    StandardCounterpointViolation *violation = &found->items[found->count];
    violation->rule = rule;
    violation->segment_from = s;
    violation->segment_to = s + 1;
    violation->upper_voice = upper;
    violation->lower_voice = lower;
    violation->arrival_interval = arrival;
    double least = 1.0;
    for (int i = 0; i < 4; i++) {
        least = cells[i]->confidence < least ? cells[i]->confidence : least;
    }
    violation->uncertainty = 1.0 - least;

    char key[256];
    snprintf(key, sizeof(key), "%s-%s-%s-v%zu-v%zu", RULE_NAMES[rule], grid->segment_ids[s], grid->segment_ids[s + 1],
             upper, lower);
    char shell_id[300];
    snprintf(shell_id, sizeof(shell_id), "standard-counterpoint-violation-notes-%s", key);
    const char *note_ids[4] = {cells[0]->note_id, cells[1]->note_id, cells[2]->note_id, cells[3]->note_id};
    violation->shell = standard_counterpoint_analysis_note_group(shell_id, note_ids, 4);

    StandardCounterpointViolationClaim claim;
    memset(&claim, 0, sizeof(claim));
    claim.rule = (int)rule;
    snprintf(claim.rule_name, sizeof(claim.rule_name), "%s", RULE_NAMES[rule]);
    claim.upper_voice = (int)upper;
    claim.lower_voice = (int)lower;
    claim.segment_from = (int)s;
    claim.segment_to = (int)(s + 1);
    claim.arrival_interval = arrival;
    char interpretation_id[340];
    snprintf(interpretation_id, sizeof(interpretation_id), "standard-counterpoint-violation-%s", key);
    if (violation->shell != NULL) {
        violation->interpretation = standard_counterpoint_analysis_interpretation(
            interpretation_id, entity_id_value(entity_id(structure_entity(violation->shell))),
            STANDARD_COUNTERPOINT_VIOLATION_CLAIM_TYPE, &claim, sizeof(claim), violation->uncertainty, "perfects-adapter");
    }
    if (violation->shell == NULL || violation->interpretation == NULL) {
        destroy_violation(violation);
        return false;
    }
    found->count++;
    return true;
}

StandardCounterpointPerfectsStatus standard_counterpoint_check_perfects(const StandardCounterpointPreparedGrid *prepared,
                                                                        StandardCounterpointViolations *out) {
    if (prepared == NULL || prepared->grid == NULL || out == NULL) {
        return STANDARD_COUNTERPOINT_PERFECTS_ERR_NULL_ARGUMENT;
    }
    const StandardCounterpointVoiceGrid *grid = prepared->grid;
    size_t pairs = grid->voice_count * (grid->voice_count - 1) / 2;
    size_t capacity = grid->segment_count > 1 ? (grid->segment_count - 1) * pairs * 2 : 0;
    StandardCounterpointViolations found = {NULL, 0};
    if (capacity > 0) {
        found.items = (StandardCounterpointViolation *)calloc(capacity, sizeof(StandardCounterpointViolation));
        if (found.items == NULL) {
            return STANDARD_COUNTERPOINT_PERFECTS_ERR_ALLOC;
        }
    }
    for (size_t s = 0; s + 1 < grid->segment_count; s++) {
        for (size_t upper = 0; upper < grid->voice_count; upper++) {
            for (size_t lower = upper + 1; lower < grid->voice_count; lower++) {
                const StandardCounterpointVoiceCell *cells[4] = {
                    standard_counterpoint_grid_cell(prepared, s, upper),
                    standard_counterpoint_grid_cell(prepared, s + 1, upper),
                    standard_counterpoint_grid_cell(prepared, s, lower),
                    standard_counterpoint_grid_cell(prepared, s + 1, lower)};
                if (cells[0] == NULL || cells[1] == NULL || cells[2] == NULL || cells[3] == NULL) {
                    continue;
                }
                const PitchSpelling *upper_notes[2] = {standard_counterpoint_grid_spelling(prepared, s, upper),
                                                       standard_counterpoint_grid_spelling(prepared, s + 1, upper)};
                const PitchSpelling *lower_notes[2] = {standard_counterpoint_grid_spelling(prepared, s, lower),
                                                       standard_counterpoint_grid_spelling(prepared, s + 1, lower)};
                StandardCounterpointLine upper_line = {upper_notes, 2};
                StandardCounterpointLine lower_line = {lower_notes, 2};
                StandardCounterpointPair pair = {&upper_line, &lower_line};
                StandardCounterpointInterval from_interval;
                StandardCounterpointInterval to_interval;
                if (!standard_counterpoint_pair_valid(&pair) ||
                    !standard_counterpoint_interval_between(lower_notes[0], upper_notes[0], &from_interval) ||
                    !standard_counterpoint_interval_between(lower_notes[1], upper_notes[1], &to_interval)) {
                    continue; /* the voices cross: not checked here */
                }
                bool parallel = false;
                if (!standard_counterpoint_harmonic_rule_holds(STANDARD_COUNTERPOINT_HARMONIC_RULE_PARALLEL_FIFTHS, &pair)) {
                    parallel = true;
                    if (!add_violation(prepared, &found, STANDARD_COUNTERPOINT_VIOLATION_PARALLEL_FIFTHS, s, upper,
                                       lower, 5, cells)) {
                        standard_counterpoint_violations_destroy(&found);
                        return STANDARD_COUNTERPOINT_PERFECTS_ERR_ALLOC;
                    }
                }
                if (!standard_counterpoint_harmonic_rule_holds(STANDARD_COUNTERPOINT_HARMONIC_RULE_PARALLEL_OCTAVES, &pair)) {
                    parallel = true;
                    if (!add_violation(prepared, &found, STANDARD_COUNTERPOINT_VIOLATION_PARALLEL_OCTAVES, s, upper,
                                       lower, to_interval.simple_size, cells)) {
                        standard_counterpoint_violations_destroy(&found);
                        return STANDARD_COUNTERPOINT_PERFECTS_ERR_ALLOC;
                    }
                }
                bool outer = upper == 0 && lower == grid->voice_count - 1;
                if (outer && !parallel &&
                    !standard_counterpoint_harmonic_rule_holds(STANDARD_COUNTERPOINT_HARMONIC_RULE_DIRECT_PERFECTS,
                                                               &pair)) {
                    StandardCounterpointInterval top_move;
                    if (standard_counterpoint_interval_between(upper_notes[0], upper_notes[1], &top_move) &&
                        standard_counterpoint_interval_is_leap(&top_move)) {
                        if (!add_violation(prepared, &found, STANDARD_COUNTERPOINT_VIOLATION_DIRECT_PERFECTS, s, upper,
                                           lower, to_interval.simple_size, cells)) {
                            standard_counterpoint_violations_destroy(&found);
                            return STANDARD_COUNTERPOINT_PERFECTS_ERR_ALLOC;
                        }
                    }
                }
            }
        }
    }
    if (found.count == 0) {
        free(found.items);
        found.items = NULL;
    }
    *out = found;
    return STANDARD_COUNTERPOINT_PERFECTS_OK;
}

bool standard_counterpoint_violation_claim_read(const Interpretation *interpretation,
                                                StandardCounterpointViolationClaim *out) {
    if (interpretation == NULL || out == NULL) {
        return false;
    }
    const Value *claim = interpretation_claim(interpretation);
    size_t size = 0;
    if (claim == NULL || strcmp(value_type(claim), STANDARD_COUNTERPOINT_VIOLATION_CLAIM_TYPE) != 0) {
        return false;
    }
    const void *content = value_content(claim, &size);
    if (content == NULL || size != sizeof(*out)) {
        return false;
    }
    memcpy(out, content, sizeof(*out));
    return true;
}
