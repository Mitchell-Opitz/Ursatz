#include <ursatz_composer/counterpoint.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <counterpoint_limits.h>
#include <counterpoint_rules.h>
#include <counterpoint_slots.h>
#include <first_species_ends_rules.h>
#include <first_species_harmony_rules.h>
#include <first_species_independence_rules.h>
#include <first_species_line_rules.h>
#include <first_species_rules.h>
#include <fourth_species_ends_rules.h>
#include <fourth_species_line_rules.h>
#include <fourth_species_rules.h>
#include <harmonic_rules.h>
#include <melodic_rules.h>
#include <second_species_ends_rules.h>
#include <second_species_line_rules.h>
#include <second_species_rules.h>
#include <third_species_ends_rules.h>
#include <third_species_line_rules.h>
#include <third_species_rules.h>

#include <ursatz_composer/harmony_phrase.h>
#include <ursatz_composer/plan_state.h>

#include "counterpoint/counterpoint_internal.h"

/* The cantus firmus group is the first of every aggregate check: the given line is not the operator's to judge. */
#define CANTUS_GROUP_BIT 1u
#define FINAL_STEP_BIT 2u

static unsigned bits_set(unsigned bits) {
    unsigned count = 0;
    for (; bits; bits >>= 1) {
        count += bits & 1u;
    }
    return count;
}

static const char *first_group_name(unsigned bits, const char *(*name)(unsigned)) {
    for (unsigned group = 0; group < 16; group++) {
        if (bits & (1u << group)) {
            const char *found = name(group);
            return found ? found : "a rule group";
        }
    }
    return NULL;
}

static const char *first_species_name(unsigned group) {
    return standard_counterpoint_first_species_group_name((StandardCounterpointFirstSpeciesGroup)group);
}

/* ------------------------------------------- species 2 to 4, by their exercises */

/* What a species with more than one note to a bar asks of the library: its whole check, its ends and its line. */
typedef struct {
    bool (*check)(const StandardCounterpointLine *, const StandardCounterpointSlotLine *, StandardCounterpointCantusPart,
                  const PitchSpelling *, StandardCounterpointResult *);
    bool (*ends)(const StandardCounterpointLine *, const StandardCounterpointSlotLine *, StandardCounterpointCantusPart,
                 const PitchSpelling *, unsigned *);
    bool (*line)(const StandardCounterpointLine *, const StandardCounterpointSlotLine *, StandardCounterpointCantusPart,
                 const PitchSpelling *, unsigned *, unsigned *);
    const char *(*group_name)(unsigned);
    unsigned groups;
    unsigned ends_group;  /* the groups the prefix is judged by rule, not as a whole */
    unsigned line_group;
    unsigned ends_open;   /* the end rules a line that is not finished may break */
} Species;

#define SPECIES_ADAPTERS(lower, Camel)                                                                                \
    static bool lower##_check(const StandardCounterpointLine *cantus, const StandardCounterpointSlotLine *line,       \
                              StandardCounterpointCantusPart part, const PitchSpelling *tonic,                        \
                              StandardCounterpointResult *out) {                                                      \
        StandardCounterpoint##Camel##SpeciesExercise exercise = {cantus, line, part};                                 \
        StandardCounterpoint##Camel##SpeciesInput input = {&exercise, tonic};                                         \
        return standard_counterpoint_##lower##_species_check(&input, out);                                            \
    }                                                                                                                 \
    static bool lower##_ends(const StandardCounterpointLine *cantus, const StandardCounterpointSlotLine *line,        \
                             StandardCounterpointCantusPart part, const PitchSpelling *tonic, unsigned *mask) {       \
        StandardCounterpoint##Camel##SpeciesExercise exercise = {cantus, line, part};                                 \
        return standard_counterpoint_##lower##_species_ends_check(&exercise, tonic, mask);                            \
    }                                                                                                                 \
    static bool lower##_line(const StandardCounterpointLine *cantus, const StandardCounterpointSlotLine *line,        \
                             StandardCounterpointCantusPart part, const PitchSpelling *tonic, unsigned *hard,         \
                             unsigned *soft) {                                                                        \
        StandardCounterpoint##Camel##SpeciesExercise exercise = {cantus, line, part};                                 \
        return standard_counterpoint_##lower##_species_line_check(&exercise, tonic, hard, soft);                      \
    }                                                                                                                 \
    static const char *lower##_group_name(unsigned group) {                                                           \
        return standard_counterpoint_##lower##_species_group_name(                                                    \
            (StandardCounterpoint##Camel##SpeciesGroup)group);                                                        \
    }

SPECIES_ADAPTERS(second, Second)
SPECIES_ADAPTERS(third, Third)
SPECIES_ADAPTERS(fourth, Fourth)

static const Species SPECIES[3] = {
    {second_check, second_ends, second_line, second_group_name, STANDARD_COUNTERPOINT_SECOND_SPECIES_GROUP_COUNT,
     STANDARD_COUNTERPOINT_SECOND_SPECIES_GROUP_ENDS, STANDARD_COUNTERPOINT_SECOND_SPECIES_GROUP_COUNTERPOINT_LINE,
     (1u << STANDARD_COUNTERPOINT_SECOND_SPECIES_ENDS_FINAL_BAR) |
         (1u << STANDARD_COUNTERPOINT_SECOND_SPECIES_ENDS_CLAUSULA_VERA)},
    {third_check, third_ends, third_line, third_group_name, STANDARD_COUNTERPOINT_THIRD_SPECIES_GROUP_COUNT,
     STANDARD_COUNTERPOINT_THIRD_SPECIES_GROUP_ENDS, STANDARD_COUNTERPOINT_THIRD_SPECIES_GROUP_COUNTERPOINT_LINE,
     (1u << STANDARD_COUNTERPOINT_THIRD_SPECIES_ENDS_FINAL_BAR) |
         (1u << STANDARD_COUNTERPOINT_THIRD_SPECIES_ENDS_CLAUSULA_VERA)},
    {fourth_check, fourth_ends, fourth_line, fourth_group_name, STANDARD_COUNTERPOINT_FOURTH_SPECIES_GROUP_COUNT,
     STANDARD_COUNTERPOINT_FOURTH_SPECIES_GROUP_ENDS, STANDARD_COUNTERPOINT_FOURTH_SPECIES_GROUP_COUNTERPOINT_LINE,
     (1u << STANDARD_COUNTERPOINT_FOURTH_SPECIES_ENDS_CANTUS_ENDS_RE_DO) |
         (1u << STANDARD_COUNTERPOINT_FOURTH_SPECIES_ENDS_PENULTIMATE_SUSPENSION) |
         (1u << STANDARD_COUNTERPOINT_FOURTH_SPECIES_ENDS_FINAL_BAR)}};

/* The pair of the given line and a counter-melody line, the cantus firmus in the part the context says. */
static StandardCounterpointPair make_pair(const CounterpointContext *context, const StandardCounterpointLine *cantus,
                                          const StandardCounterpointLine *counter) {
    StandardCounterpointPair pair = {context->above ? counter : cantus, context->above ? cantus : counter};
    return pair;
}

static StandardCounterpointCantusPart cantus_part(const CounterpointContext *context) {
    return context->above ? STANDARD_COUNTERPOINT_CANTUS_IN_LOWER : STANDARD_COUNTERPOINT_CANTUS_IN_UPPER;
}

/* ----------------------------------------------------------- species 1 */

static bool eval_first(const CounterpointContext *context, const StandardCounterpointLine *cantus,
                       const StandardCounterpointLine *counter, bool final, CounterpointVerdict *verdict) {
    StandardCounterpointPair pair = make_pair(context, cantus, counter);
    StandardCounterpointInput input = {&pair, cantus_part(context), context->tonic, NULL};
    StandardCounterpointLimits limits = standard_counterpoint_default_limits();
    unsigned hard = 0;
    unsigned soft = 0;
    if (final) {
        StandardCounterpointResult base;
        StandardCounterpointResult first;
        if (!standard_counterpoint_check(&input, &base) || !standard_counterpoint_first_species_check(&input, &first)) {
            return false;
        }
        hard = (base.hard_violations | first.hard_violations) & ~CANTUS_GROUP_BIT;
        soft = (base.soft_violations | first.soft_violations) & ~CANTUS_GROUP_BIT;
        verdict->hard = first_group_name(hard, first_species_name);
        verdict->groups = STANDARD_COUNTERPOINT_FIRST_SPECIES_GROUP_COUNT;
        verdict->hard_groups = hard;
        verdict->soft_groups = soft;
        verdict->group_name = first_species_name;
        verdict->total = STANDARD_COUNTERPOINT_FIRST_SPECIES_GROUP_COUNT - 1;
        verdict->held = verdict->total - bits_set(soft);
        return true;
    }
    /* A line that is not finished: the rules that look at the end are asked later. */
    if (!standard_counterpoint_harmonic_check(&pair, &hard, &soft)) {
        return false;
    }
    hard &= ~(1u << STANDARD_COUNTERPOINT_HARMONIC_RULE_LAST_INTERVAL);
    if (hard) {
        verdict->hard = "harmonic";
        return true;
    }
    if (!standard_counterpoint_melodic_check(counter, &limits, &hard, &soft)) {
        return false;
    }
    hard &= ~(1u << STANDARD_COUNTERPOINT_MELODIC_RULE_CLAUSULA_VERA);
    if (hard) {
        verdict->hard = "melodic";
        return true;
    }
    if (!standard_counterpoint_first_species_harmony_check(&pair, &hard, &soft)) {
        return false;
    }
    if (hard) {
        verdict->hard = "harmony";
        return true;
    }
    if (!standard_counterpoint_first_species_independence_check(&pair, cantus_part(context), &hard, &soft)) {
        return false;
    }
    if (hard) {
        verdict->hard = "independence";
        return true;
    }
    if (!standard_counterpoint_first_species_line_check(counter, context->tonic, &hard, &soft)) {
        return false;
    }
    hard &= ~((1u << STANDARD_COUNTERPOINT_CANTUS_RULE_LENGTH) | (1u << STANDARD_COUNTERPOINT_CANTUS_RULE_FINAL_STEP));
    if (hard) {
        verdict->hard = "counterpoint-line";
        return true;
    }
    if (!standard_counterpoint_first_species_ends_rule_holds(STANDARD_COUNTERPOINT_FIRST_SPECIES_ENDS_BEGIN, &pair,
                                                             cantus_part(context), context->tonic)) {
        verdict->hard = "begin";
    }
    return true;
}

/* ------------------------------------------------------ species 2 to 4 */

static bool eval_slots(const CounterpointContext *context, const StandardCounterpointLine *cantus,
                       const StandardCounterpointSlotLine *counter, bool final, CounterpointVerdict *verdict) {
    const Species *species = &SPECIES[context->species - 2];
    StandardCounterpointResult result;
    StandardCounterpointCantusPart part = cantus_part(context);
    unsigned hard;
    unsigned soft;
    unsigned ends;
    if (!species->check(cantus, counter, part, context->tonic, &result)) {
        /* A line too short to be an exercise cannot be judged yet; a finished one that is not one is refused. */
        if (final) {
            verdict->hard = "not a counter-melody of the species";
        }
        return true;
    }
    if (final) {
        /* The pair check compares note with note and does not fit more than one note to a bar: the species' own check, which
           runs the common spacing rules on every note, takes its place. */
        verdict->hard = first_group_name(result.hard_violations & ~CANTUS_GROUP_BIT, species->group_name);
        verdict->groups = species->groups;
        verdict->hard_groups = result.hard_violations & ~CANTUS_GROUP_BIT;
        verdict->soft_groups = result.soft_violations & ~CANTUS_GROUP_BIT;
        verdict->group_name = species->group_name;
        verdict->total = species->groups - 1;
        verdict->held = verdict->total - bits_set(result.soft_violations & ~CANTUS_GROUP_BIT);
        return true;
    }
    /* A line that is not finished is judged group by group, but for the ends and the line, which are judged by rule. */
    hard = result.hard_violations & ~(CANTUS_GROUP_BIT | (1u << species->ends_group) | (1u << species->line_group));
    if (hard) {
        verdict->hard = first_group_name(hard, species->group_name);
        return true;
    }
    if (!species->ends(cantus, counter, part, context->tonic, &ends)) {
        return true;
    }
    if (ends & ~species->ends_open) {
        verdict->hard = "ends";
        return true;
    }
    if (!species->line(cantus, counter, part, context->tonic, &hard, &soft)) {
        return true;
    }
    if (hard & ~FINAL_STEP_BIT) {
        verdict->hard = "counterpoint-line";
    }
    return true;
}

bool counterpoint_eval(const CounterpointContext *context, size_t notes, const unsigned char *kind, const int *pitch,
                       bool final, CounterpointVerdict *verdict) {
    const PitchSpelling *cantus_notes[CP_MAX_NOTES];
    const PitchSpelling *counter_notes[CP_MAX_SLOTS];
    StandardCounterpointSlot slots[CP_MAX_SLOTS];
    size_t count = notes * context->per_bar;
    StandardCounterpointLine cantus = {cantus_notes, notes};
    bool ok;
    memset(verdict, 0, sizeof(*verdict));
    verdict->total = 1;
    verdict->held = 1;
    if (notes == 0 || notes > context->count) {
        return true;
    }
    for (size_t i = 0; i < notes; i++) {
        cantus_notes[i] = context->spell[context->line[i]];
    }
    for (size_t i = 0; i < count; i++) {
        if (kind[i] != CP_SLOT_REST && (pitch[i] < context->low || pitch[i] > context->high || !context->spell[pitch[i]])) {
            verdict->hard = "a pitch outside the voice or the key";
            return true;
        }
        slots[i].kind = (StandardCounterpointSlotKind)kind[i];
        slots[i].pitch = kind[i] == CP_SLOT_REST ? NULL : context->spell[pitch[i]];
        counter_notes[i] = slots[i].pitch;
    }
    if (context->species == 1) {
        StandardCounterpointLine counter = {counter_notes, count};
        return eval_first(context, &cantus, &counter, final, verdict);
    }
    {
        StandardCounterpointSlotLine counter = {slots, count};
        ok = eval_slots(context, &cantus, &counter, final, verdict);
    }
    return ok;
}

/* ---------------------------------------------------------- the checker */

bool counterpoint_checker(const PiecePlan *plan, FillTarget target, const FillOperator *operator_,
                          const FillCandidate *candidate, void *context, FillCheck *out) {
    if (!plan || !operator_ || !candidate || !out) {
        return false;
    }
    if (strcmp(operator_->generator.name, COUNTERPOINT_OPERATOR_NAME) != 0) {
        return harmony_phrase_checker(plan, target, operator_, candidate, context, out);
    }
    const CounterpointCandidate *line = candidate->payload;
    CounterpointContext *read = calloc(1, sizeof(*read));
    CounterpointVerdict verdict;
    bool ok = read && line && line->magic == CP_PAYLOAD_MAGIC && target.kind == FILL_TARGET_LANE_SPAN &&
              counterpoint_read(plan, (LaneSpanId)target.id, read, NULL) && line->slots == read->count * read->per_bar;
    memset(out, 0, sizeof(*out));
    out->score_denominator = 1;
    ok = ok && counterpoint_eval(read, read->count, line->kind, line->pitch, true, &verdict);
    if (ok) {
        out->hard_violation = verdict.hard != NULL;
        if (verdict.hard) {
            snprintf(out->reason, sizeof(out->reason), "%s", verdict.hard);
        } else {
            out->score_numerator = verdict.held;
            out->score_denominator = verdict.total ? verdict.total : 1;
        }
    }
    counterpoint_context_free(read);
    free(read);
    return ok;
}

/* ------------------------------------------------------------- the report */

bool counterpoint_report(const PiecePlan *plan, LaneSpanId target, CounterpointReport *out) {
    CounterpointContext *read = calloc(1, sizeof(*read));
    CounterpointCandidate *line = calloc(1, sizeof(*line));
    CounterpointVerdict verdict;
    bool ok = read && line && out && counterpoint_read_written(plan, target, read, line, NULL) &&
              counterpoint_eval(read, read->count, line->kind, line->pitch, true, &verdict);
    if (ok) {
        memset(out, 0, sizeof(*out));
        out->species = read->species;
        for (unsigned group = 1; group < verdict.groups && out->count < COUNTERPOINT_REPORT_MAX_GROUPS; group++) {
            out->name[out->count] = verdict.group_name ? verdict.group_name(group) : "group";
            out->hard_ok[out->count] = !(verdict.hard_groups & (1u << group));
            out->soft_ok[out->count] = !(verdict.soft_groups & (1u << group));
            out->count++;
        }
        out->hard_ok_all = verdict.hard == NULL;
    }
    if (ok) {
        counterpoint_context_free(read);
    }
    free(read);
    free(line);
    return ok;
}
