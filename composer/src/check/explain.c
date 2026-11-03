#include <ursatz_composer/explain.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <phrase_level_form.h>

#include <ursatz_composer/constraint.h>
#include <ursatz_composer/lanes.h>
#include <ursatz_composer/regenerate.h>

#include "check/shaping.h"
#include "plan/plan_json_internal.h"

static const char *const LANE_NAMES[] = {"meter", "key", "harmony", "bass", "melody", "inner-voice"};
static const char *const OUTCOME_NAMES[] = {"applied", "no-operator", "all-rejected"};
static const char *const KIND_NAMES[] = {"cadence", "key", "violation"};

__extension__ typedef unsigned __int128 Wide;

/* ------------------------------------------------------------- freeing */

static void target_destroy(ExplainTarget *target) {
    free(target->rejected);
}

static void section_destroy(ExplainSection *section) {
    analyzer_check_destroy(&section->check);
}

void explain_destroy(Explanation *explanation) {
    if (!explanation) {
        return;
    }
    for (size_t i = 0; explanation->targets && i < explanation->target_count; i++) {
        target_destroy(&explanation->targets[i]);
    }
    free(explanation->targets);
    for (size_t i = 0; explanation->sections && i < explanation->section_count; i++) {
        section_destroy(&explanation->sections[i]);
    }
    free(explanation->sections);
    compose_rejections_free(explanation->rejections, explanation->rejection_count);
    memset(explanation, 0, sizeof(*explanation));
}

/* ------------------------------------------------------------- the pieces */

static Wide wide_gcd(Wide a, Wide b) {
    while (b) {
        Wide rest = a % b;
        a = b;
        b = rest;
    }
    return a;
}

/* a - b for rationals, never below 0, in lowest terms; parts that would not fit 64 bits are shifted down together. */
static void ratio_behind(unsigned long an, unsigned long ad, unsigned long bn, unsigned long bd, unsigned long *num,
                         unsigned long *den) {
    Wide left = (Wide)an * bd;
    Wide right = (Wide)bn * ad;
    Wide denominator = (Wide)ad * bd;
    Wide numerator = left > right ? left - right : 0;
    if (denominator == 0) {
        *num = 0;
        *den = 1;
        return;
    }
    Wide divisor = wide_gcd(numerator, denominator);
    if (divisor > 1) {
        numerator /= divisor;
        denominator /= divisor;
    }
    while (numerator > (Wide)~0UL || denominator > (Wide)~0UL) {
        numerator >>= 1;
        denominator >>= 1;
    }
    *num = (unsigned long)numerator;
    *den = denominator ? (unsigned long)denominator : 1;
}

/* Whether a scores higher than b. */
static bool scores_higher(const FillConsidered *a, const FillConsidered *b) {
    return (Wide)a->score_numerator * b->score_denominator > (Wide)b->score_numerator * a->score_denominator;
}

static void candidate_from(const FillConsidered *from, ExplainCandidate *to) {
    memset(to, 0, sizeof(*to));
    snprintf(to->operator_name, sizeof(to->operator_name), "%s", from->operator_name);
    snprintf(to->label, sizeof(to->label), "%s", from->label);
    to->score_numerator = from->score_numerator;
    to->score_denominator = from->score_denominator ? from->score_denominator : 1;
    to->styled = from->styled;
    to->style_numerator = from->styled ? from->style_numerator : 0;
    to->style_denominator = from->styled && from->style_denominator ? from->style_denominator : 1;
    to->rejected = from->rejected;
    snprintf(to->reason, sizeof(to->reason), "%s", from->reason);
    to->behind_numerator = 0;
    to->behind_denominator = 1;
}

/* The meter in force at the start of the piece, for the bars of a target; false when the plan has none. */
static bool first_meter(const PiecePlan *plan, PlanMeter *out) {
    PlanSpan everything = {{0, 1}, {1000000, 1}};
    LaneSpanId first = PLAN_LANE_SPAN_NONE;
    if (plan_lane_find(plan, PLAN_LANE_METER, 0, everything, &first, 1) == 0 || first == PLAN_LANE_SPAN_NONE) {
        return false;
    }
    *out = plan_lane_span_payload(plan, first)->meter;
    return out->beats_per_bar > 0 && out->beat_unit > 0;
}

/* The bars a span covers, 1-based from the start of the piece; 0 and 0 without a usable meter. */
static void bars_of(bool have_meter, const PlanMeter *meter, PlanSpan span, size_t *first, size_t *last) {
    *first = *last = 0;
    PlanTime end;
    if (!have_meter || span.start.num < 0 || span.length.num <= 0 || plan_span_end(span, &end) != PLAN_OK) {
        return;
    }
    unsigned long beats = (unsigned long)meter->beats_per_bar;
    unsigned long unit = (unsigned long)meter->beat_unit;
    unsigned long start_whole = ((unsigned long)span.start.num * unit) / (span.start.den * beats);
    unsigned long end_parts = (unsigned long)end.num * unit;
    unsigned long end_den = end.den * beats;
    unsigned long end_whole = (end_parts + end_den - 1) / end_den;
    *first = start_whole + 1;
    *last = end_whole > *first ? end_whole : *first;
}

static NodeId owner_of(const PiecePlan *plan, FillTarget target) {
    return target.kind == FILL_TARGET_NODE ? (NodeId)target.id : plan_lane_span_owner(plan, (LaneSpanId)target.id);
}

static void letter_of(const PiecePlan *plan, NodeId section, char *out) {
    out[0] = '\0';
    for (size_t i = 0; section != PLAN_NODE_NONE && i < plan_node_constraint_count(plan, section); i++) {
        const PlanConstraint *constraint = plan_node_constraint_at(plan, section, i);
        if (constraint->kind == PLAN_CONSTRAINT_SECTION_LETTER) {
            snprintf(out, PLAN_SECTION_LETTER_MAX, "%s", constraint->params.letter);
        }
    }
}

/* ------------------------------------------------------------- building */

typedef struct {
    ExplainTarget *targets;
    size_t count;
    size_t capacity;
} TargetList;

static bool target_list_add(TargetList *list, const ExplainTarget *target) {
    if (list->count == list->capacity) {
        size_t capacity = list->capacity ? list->capacity * 2 : 64;
        ExplainTarget *grown = realloc(list->targets, capacity * sizeof(ExplainTarget));
        if (!grown) {
            return false;
        }
        list->targets = grown;
        list->capacity = capacity;
    }
    list->targets[list->count++] = *target;
    return true;
}

/* Keeps the best EXPLAIN_RUNNERS_UP in `top` (indexes into considered), by score, then as proposed. */
static void runner_up_insert(const FillConsidered *considered, size_t *top, size_t *count, size_t index) {
    size_t at = *count;
    while (at > 0 && scores_higher(&considered[index], &considered[top[at - 1]])) {
        at--;
    }
    if (at >= EXPLAIN_RUNNERS_UP) {
        return;
    }
    size_t last = *count < EXPLAIN_RUNNERS_UP ? *count : EXPLAIN_RUNNERS_UP - 1;
    for (size_t i = last; i > at; i--) {
        top[i] = top[i - 1];
    }
    top[at] = index;
    if (*count < EXPLAIN_RUNNERS_UP) {
        (*count)++;
    }
}

static bool add_step(const PiecePlan *plan, bool have_meter, const PlanMeter *meter, const char *stage, size_t step_index,
                     const FillStep *step, TargetList *list) {
    ExplainTarget target;
    memset(&target, 0, sizeof(target));
    snprintf(target.stage, sizeof(target.stage), "%s", stage);
    target.step = step_index;
    target.kind = step->target.kind;
    target.id = step->target.id;
    target.outcome = step->outcome;
    target.considered = step->considered_count;
    target.section = PLAN_NODE_NONE;
    if (fill_target_exists(plan, step->target)) {
        NodeId owner = owner_of(plan, step->target);
        const char *name = plan_node_name(plan, owner);
        snprintf(target.name, sizeof(target.name), "%s", name ? name : "");
        target.section = plan_section_of(plan, owner);
        letter_of(plan, target.section, target.letter);
        if (step->target.kind == FILL_TARGET_LANE_SPAN) {
            PlanLaneKind lane = plan_lane_span_lane(plan, (LaneSpanId)step->target.id);
            snprintf(target.lane, sizeof(target.lane), "%s", (size_t)lane < sizeof(LANE_NAMES) / sizeof(LANE_NAMES[0]) ? LANE_NAMES[lane] : "");
            target.voice = plan_lane_span_voice(plan, (LaneSpanId)step->target.id);
        }
        bars_of(have_meter, meter, fill_target_span(plan, step->target), &target.first_bar, &target.last_bar);
        if (step->target.kind == FILL_TARGET_LANE_SPAN) {
            LaneSpanId span = (LaneSpanId)step->target.id;
            for (size_t c = 0; c < plan_lane_span_constraint_count(plan, span); c++) {
                const PlanConstraint *constraint = plan_lane_span_constraint_at(plan, span, c);
                if (constraint->kind == PLAN_CONSTRAINT_PICKUP) {
                    snprintf(target.pickup, sizeof(target.pickup), "%ld/%lu", constraint->params.pickup.num,
                             constraint->params.pickup.den);
                }
            }
            /* The melody operator reads the pickup from the span, else from its owner. */
            for (size_t c = 0; !target.pickup[0] && c < plan_node_constraint_count(plan, owner); c++) {
                const PlanConstraint *constraint = plan_node_constraint_at(plan, owner, c);
                if (constraint->kind == PLAN_CONSTRAINT_PICKUP) {
                    snprintf(target.pickup, sizeof(target.pickup), "%ld/%lu", constraint->params.pickup.num,
                             constraint->params.pickup.den);
                }
            }
        }
    }

    size_t top[EXPLAIN_RUNNERS_UP];
    size_t top_count = 0;
    size_t rejected = 0;
    for (size_t i = 0; i < step->considered_count; i++) {
        if (step->considered[i].rejected) {
            rejected++;
        } else if ((long)i != step->chosen) {
            runner_up_insert(step->considered, top, &top_count, i);
        }
    }
    if (step->chosen >= 0 && (size_t)step->chosen < step->considered_count) {
        target.has_chosen = true;
        candidate_from(&step->considered[step->chosen], &target.chosen);
    }
    for (size_t i = 0; i < top_count; i++) {
        ExplainCandidate *runner = &target.runners_up[i];
        candidate_from(&step->considered[top[i]], runner);
        if (target.has_chosen) {
            ratio_behind(target.chosen.score_numerator, target.chosen.score_denominator, runner->score_numerator,
                         runner->score_denominator, &runner->behind_numerator, &runner->behind_denominator);
        }
    }
    target.runner_up_count = top_count;
    if (rejected > 0) {
        target.rejected = calloc(rejected, sizeof(ExplainCandidate));
        if (!target.rejected) {
            return false;
        }
        for (size_t i = 0; i < step->considered_count; i++) {
            if (step->considered[i].rejected) {
                candidate_from(&step->considered[i], &target.rejected[target.rejected_count++]);
            }
        }
    }
    if (!target_list_add(list, &target)) {
        free(target.rejected);
        return false;
    }
    return true;
}

static bool add_record(const PiecePlan *plan, bool have_meter, const PlanMeter *meter, const char *stage,
                       const FillRecord *record, TargetList *list) {
    for (size_t s = 0; record && s < record->step_count; s++) {
        /* A step no operator applied to weighed nothing: a target set aside until something makes an operator apply. */
        if (record->steps[s].outcome == FILL_STEP_NO_OPERATOR) {
            continue;
        }
        if (!add_step(plan, have_meter, meter, stage, s, &record->steps[s], list)) {
            return false;
        }
    }
    return true;
}

static bool copy_check(const AnalyzerCheck *from, AnalyzerCheck *to) {
    *to = *from;
    to->items = NULL;
    if (from->item_count > 0) {
        to->items = malloc(from->item_count * sizeof(AnalyzerCheckItem));
        if (!to->items) {
            memset(to, 0, sizeof(*to));
            return false;
        }
        memcpy(to->items, from->items, from->item_count * sizeof(AnalyzerCheckItem));
    }
    return true;
}

static void section_fill(const PiecePlan *plan, NodeId id, ExplainSection *section) {
    memset(section, 0, sizeof(*section));
    section->section = id;
    const char *name = plan_node_name(plan, id);
    snprintf(section->name, sizeof(section->name), "%s", name ? name : "");
    letter_of(plan, id, section->letter);
    snprintf(section->heard_form, sizeof(section->heard_form), "none");
    snprintf(section->requested_form, sizeof(section->requested_form), "none");
}

static void form_names(ExplainSection *section, bool heard, int heard_form, int requested_form, bool as_requested) {
    section->heard = heard;
    section->heard_as_requested = as_requested;
    if (heard && heard_form >= 0) {
        snprintf(section->heard_form, sizeof(section->heard_form), "%s", phrase_level_form_name((PhraseLevelForm)heard_form));
    }
    if (requested_form >= 0) {
        snprintf(section->requested_form, sizeof(section->requested_form), "%s",
                 phrase_level_form_name((PhraseLevelForm)requested_form));
    }
}

static bool copy_rejection(const ComposeRejection *from, ComposeRejection *to) {
    *to = *from;
    to->items = NULL;
    if (from->item_count > 0) {
        to->items = malloc(from->item_count * sizeof(ComposeRejectionItem));
        if (!to->items) {
            to->item_count = 0;
            return false;
        }
        memcpy(to->items, from->items, from->item_count * sizeof(ComposeRejectionItem));
    }
    return true;
}

static bool add_rejections(Explanation *out, const ComposeRejection *rejections, size_t count) {
    if (count == 0) {
        return true;
    }
    ComposeRejection *grown = realloc(out->rejections, (out->rejection_count + count) * sizeof(ComposeRejection));
    if (!grown) {
        return false;
    }
    out->rejections = grown;
    for (size_t i = 0; i < count; i++) {
        if (!copy_rejection(&rejections[i], &out->rejections[out->rejection_count])) {
            return false;
        }
        out->rejection_count++;
    }
    return true;
}

ExplainResult explain_build(const PiecePlan *plan, const ComposeOutcome *outcome, Explanation *out) {
    if (!out) {
        return EXPLAIN_ERR_NULL;
    }
    memset(out, 0, sizeof(*out));
    if (!plan || !outcome) {
        return EXPLAIN_ERR_NULL;
    }
    PlanMeter meter = {0};
    bool have_meter = first_meter(plan, &meter);
    TargetList list;
    memset(&list, 0, sizeof(list));
    bool ok = true;
    for (int stage = 0; ok && stage < COMPOSE_STAGE_COUNT; stage++) {
        ok = add_record(plan, have_meter, &meter, compose_stage_name((ComposeStage)stage), outcome->records[stage], &list);
    }
    out->targets = list.targets;
    out->target_count = list.count;
    if (ok && outcome->section_count > 0) {
        out->sections = calloc(outcome->section_count, sizeof(ExplainSection));
        ok = out->sections != NULL;
    }
    for (size_t i = 0; ok && i < outcome->section_count; i++) {
        const ComposeHeard *heard = &outcome->sections[i];
        ExplainSection *section = &out->sections[i];
        section_fill(plan, heard->section, section);
        out->section_count++;
        form_names(section, heard->heard_result == FORM_HEAR_OK, heard->heard_form, heard->requested_form,
                   heard->heard_as_requested);
        if (i < outcome->analyzer_check_count) {
            const AnalyzerCheck *check = &outcome->analyzer_checks[i];
            section->checked = check->planned > 0 || check->item_count > 0;
            ok = copy_check(check, &section->check);
        }
    }
    ok = ok && add_rejections(out, outcome->rejections, outcome->rejection_count);
    if (!ok) {
        explain_destroy(out);
        return EXPLAIN_ERR_ALLOC;
    }
    return EXPLAIN_OK;
}

ExplainResult explain_build_grow(const PiecePlan *plan, NodeId theme, const GrowOutcome *outcome,
                                 const AnalyzerCheck *check, const ComposeRejection *extra, size_t extra_count,
                                 Explanation *out) {
    if (!out) {
        return EXPLAIN_ERR_NULL;
    }
    memset(out, 0, sizeof(*out));
    if (!plan || !outcome || (extra_count > 0 && !extra)) {
        return EXPLAIN_ERR_NULL;
    }
    PlanMeter meter = {0};
    bool have_meter = first_meter(plan, &meter);
    TargetList list;
    memset(&list, 0, sizeof(list));
    bool ok = true;
    for (int stage = 0; ok && stage < GROW_STAGE_COUNT; stage++) {
        ok = add_record(plan, have_meter, &meter, grow_stage_name((GrowStage)stage), outcome->records[stage], &list);
    }
    out->targets = list.targets;
    out->target_count = list.count;
    if (ok && theme != PLAN_NODE_NONE && plan_has_node(plan, theme)) {
        out->sections = calloc(1, sizeof(ExplainSection));
        ok = out->sections != NULL;
        if (ok) {
            ExplainSection *section = &out->sections[0];
            section_fill(plan, theme, section);
            out->section_count = 1;
            form_names(section, outcome->heard_result == FORM_HEAR_OK, outcome->heard_form, outcome->requested_form,
                       outcome->heard_as_requested);
            if (check) {
                section->checked = check->planned > 0 || check->item_count > 0;
                ok = copy_check(check, &section->check);
            }
        }
    }
    ok = ok && add_rejections(out, extra, extra_count) && add_rejections(out, outcome->rejections, outcome->rejection_count);
    if (!ok) {
        explain_destroy(out);
        return EXPLAIN_ERR_ALLOC;
    }
    return EXPLAIN_OK;
}

ExplainResult explain_analyzer_rejection(const PiecePlan *plan, NodeId section, const AnalyzerCheck *check, size_t round,
                                         unsigned long seed, ComposeRejection *out) {
    if (!plan || !check || !out) {
        return EXPLAIN_ERR_NULL;
    }
    memset(out, 0, sizeof(*out));
    out->round = round;
    out->seed = seed;
    snprintf(out->stage, sizeof(out->stage), "check");
    snprintf(out->reason, sizeof(out->reason), "analyzer");
    size_t disagreeing = 0;
    for (size_t i = 0; i < check->item_count; i++) {
        disagreeing += !check->items[i].agreed;
    }
    if (disagreeing == 0) {
        return EXPLAIN_OK;
    }
    out->items = calloc(disagreeing, sizeof(ComposeRejectionItem));
    if (!out->items) {
        return EXPLAIN_ERR_ALLOC;
    }
    const char *name = plan_node_name(plan, section);
    for (size_t i = 0; i < check->item_count; i++) {
        if (!check->items[i].agreed) {
            ComposeRejectionItem *item = &out->items[out->item_count++];
            snprintf(item->section, sizeof(item->section), "%s", name ? name : "");
            item->item = check->items[i];
        }
    }
    return EXPLAIN_OK;
}

/* ------------------------------------------------------------------- text */

typedef struct {
    char *text;
    size_t length;
    size_t capacity;
    bool failed;
} Buffer;

static void put(Buffer *buffer, const char *format, ...) __attribute__((format(printf, 2, 3)));

static void put(Buffer *buffer, const char *format, ...) {
    if (buffer->failed) {
        return;
    }
    va_list args;
    va_start(args, format);
    va_list again;
    va_copy(again, args);
    int needed = vsnprintf(NULL, 0, format, args);
    va_end(args);
    if (needed < 0) {
        va_end(again);
        buffer->failed = true;
        return;
    }
    if (buffer->length + (size_t)needed + 1 > buffer->capacity) {
        size_t capacity = buffer->capacity ? buffer->capacity : 1024;
        while (buffer->length + (size_t)needed + 1 > capacity) {
            capacity *= 2;
        }
        char *grown = realloc(buffer->text, capacity);
        if (!grown) {
            va_end(again);
            buffer->failed = true;
            return;
        }
        buffer->text = grown;
        buffer->capacity = capacity;
    }
    vsnprintf(buffer->text + buffer->length, buffer->capacity - buffer->length, format, again);
    va_end(again);
    buffer->length += (size_t)needed;
}

/* The shaping rules a scored candidate did not hold, as its reason lists them; NULL when all held. */
static const char *not_held_of(const ExplainCandidate *candidate) {
    const char *at = candidate->rejected ? NULL : strstr(candidate->reason, SHAPING_NOT_HELD);
    return at ? at + strlen(SHAPING_NOT_HELD) : NULL;
}

static void put_candidate(Buffer *buffer, const char *role, const ExplainCandidate *candidate) {
    put(buffer, "    %s: %s \"%s\" score %lu/%lu", role, candidate->operator_name, candidate->label,
        candidate->score_numerator, candidate->score_denominator);
    if (candidate->styled) {
        put(buffer, " (style %lu/%lu)", candidate->style_numerator, candidate->style_denominator);
    }
    if (not_held_of(candidate)) {
        put(buffer, " (not held: %s)", not_held_of(candidate));
    }
}

static void put_target(Buffer *buffer, const ExplainTarget *target) {
    char where[128];
    if (target->first_bar == 0) {
        where[0] = '\0';
    } else if (target->first_bar == target->last_bar) {
        snprintf(where, sizeof(where), ", bar %zu", target->first_bar);
    } else {
        snprintf(where, sizeof(where), ", bars %zu-%zu", target->first_bar, target->last_bar);
    }
    if (target->pickup[0]) {
        size_t used = strlen(where);
        snprintf(where + used, sizeof(where) - used, ", pickup %s", target->pickup);
    }
    if (target->kind == FILL_TARGET_NODE) {
        put(buffer, "  [%s] node %s%s (%s, %zu considered)\n", target->stage, target->name, where,
            OUTCOME_NAMES[target->outcome], target->considered);
    } else {
        put(buffer, "  [%s] %s lane of %s%s (%s, %zu considered)\n", target->stage, target->lane, target->name, where,
            OUTCOME_NAMES[target->outcome], target->considered);
    }
    if (target->has_chosen) {
        put_candidate(buffer, "chosen", &target->chosen);
        put(buffer, "\n");
    }
    for (size_t i = 0; i < target->runner_up_count; i++) {
        const ExplainCandidate *runner = &target->runners_up[i];
        put_candidate(buffer, "runner-up", runner);
        put(buffer, ", behind %lu/%lu\n", runner->behind_numerator, runner->behind_denominator);
    }
    for (size_t i = 0; i < target->rejected_count; i++) {
        put_candidate(buffer, "rejected", &target->rejected[i]);
        put(buffer, ": %s\n", target->rejected[i].reason);
    }
}

static void put_item(Buffer *buffer, const AnalyzerCheckItem *item, const char *indent) {
    if (item->kind == ANALYZER_CHECK_VIOLATION) {
        put(buffer, "%s%s at bar %zu: %s\n", indent, KIND_NAMES[item->kind], item->bar + 1, item->heard);
    } else {
        put(buffer, "%s%s at bar %zu: planned %s, heard %s, %s\n", indent, KIND_NAMES[item->kind], item->bar + 1,
            item->planned, item->heard, item->agreed ? "agreed" : "not agreed");
    }
}

char *explain_to_text(const Explanation *explanation) {
    if (!explanation) {
        return NULL;
    }
    Buffer buffer;
    memset(&buffer, 0, sizeof(buffer));
    for (size_t s = 0; s < explanation->section_count; s++) {
        const ExplainSection *section = &explanation->sections[s];
        bool lettered = section->letter[0] && strcmp(section->letter, section->name) != 0;
        put(&buffer, "section %s%s%s%s\n", section->name, lettered ? " (" : "", lettered ? section->letter : "",
            lettered ? ")" : "");
        if (section->heard) {
            put(&buffer, "  heard as %s, asked for %s%s\n", section->heard_form, section->requested_form,
                section->heard_as_requested ? " (as requested)" : "");
        } else {
            put(&buffer, "  not heard\n");
        }
        if (section->checked) {
            put(&buffer, "  analyzer check: %zu of %zu planned agreed\n", section->check.agreed, section->check.planned);
            for (size_t i = 0; i < section->check.item_count; i++) {
                put_item(&buffer, &section->check.items[i], "    ");
            }
        } else {
            put(&buffer, "  analyzer check: not made\n");
        }
        for (size_t t = 0; t < explanation->target_count; t++) {
            if (explanation->targets[t].section == section->section) {
                put_target(&buffer, &explanation->targets[t]);
            }
        }
        put(&buffer, "\n");
    }
    bool header = false;
    for (size_t t = 0; t < explanation->target_count; t++) {
        bool in_section = false;
        for (size_t s = 0; s < explanation->section_count; s++) {
            in_section = in_section || explanation->targets[t].section == explanation->sections[s].section;
        }
        if (!in_section) {
            if (!header) {
                put(&buffer, "outside the sections\n");
                header = true;
            }
            put_target(&buffer, &explanation->targets[t]);
        }
    }
    if (header) {
        put(&buffer, "\n");
    }
    if (explanation->rejection_count > 0) {
        put(&buffer, "rejected runs\n");
    }
    for (size_t r = 0; r < explanation->rejection_count; r++) {
        const ComposeRejection *rejection = &explanation->rejections[r];
        put(&buffer, "  round %zu, seed %lu, stage %s: %s%s%s\n", rejection->round, rejection->seed,
            rejection->stage[0] ? rejection->stage : "none", rejection->reason, rejection->detail[0] ? ", " : "",
            rejection->detail);
        for (size_t i = 0; i < rejection->item_count; i++) {
            char indent[64];
            snprintf(indent, sizeof(indent), "    %s: ", rejection->items[i].section);
            put_item(&buffer, &rejection->items[i].item, indent);
        }
    }
    if (buffer.failed) {
        free(buffer.text);
        return NULL;
    }
    if (!buffer.text) {
        buffer.text = strdup("");
    }
    return buffer.text;
}

/* ------------------------------------------------------------------- JSON */

static cJSON *ratio_json(unsigned long numerator, unsigned long denominator) {
    cJSON *object = cJSON_CreateObject();
    if (object && !(plan_json_add(object, "num", json_number((long)numerator)) &&
                    plan_json_add(object, "den", json_number((long)denominator)))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

static cJSON *candidate_json(const ExplainCandidate *candidate, bool runner_up) {
    cJSON *object = cJSON_CreateObject();
    bool ok = object && plan_json_add(object, "operator", cJSON_CreateString(candidate->operator_name)) &&
              plan_json_add(object, "label", cJSON_CreateString(candidate->label)) &&
              plan_json_add(object, "score", ratio_json(candidate->score_numerator, candidate->score_denominator));
    ok = ok && (!candidate->styled ||
                plan_json_add(object, "style", ratio_json(candidate->style_numerator, candidate->style_denominator)));
    ok = ok && (!runner_up || plan_json_add(object, "behind", ratio_json(candidate->behind_numerator, candidate->behind_denominator)));
    ok = ok && (!candidate->rejected || plan_json_add(object, "reason", cJSON_CreateString(candidate->reason)));
    ok = ok && (!not_held_of(candidate) || plan_json_add(object, "not_held", cJSON_CreateString(not_held_of(candidate))));
    if (!ok) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

static cJSON *target_json(const ExplainTarget *target) {
    cJSON *object = cJSON_CreateObject();
    cJSON *place = cJSON_CreateObject();
    cJSON *runners = cJSON_CreateArray();
    cJSON *rejected = cJSON_CreateArray();
    bool ok = object && place && runners && rejected && plan_json_add(object, "step", json_number((long)target->step)) &&
              plan_json_add(place, "kind", cJSON_CreateString(target->kind == FILL_TARGET_NODE ? "node" : "lane_span")) &&
              plan_json_add(place, "id", json_number((long)target->id)) &&
              plan_json_add(place, "name", cJSON_CreateString(target->name)) &&
              plan_json_add(place, "section", target->letter[0] ? cJSON_CreateString(target->letter) : cJSON_CreateNull());
    if (ok && target->kind == FILL_TARGET_LANE_SPAN) {
        ok = plan_json_add(place, "lane", cJSON_CreateString(target->lane)) &&
             plan_json_add(place, "voice", json_number(target->voice));
    }
    if (ok && target->first_bar > 0) {
        cJSON *bars = cJSON_CreateObject();
        ok = bars && plan_json_add(bars, "first", json_number((long)target->first_bar)) &&
             plan_json_add(bars, "last", json_number((long)target->last_bar)) && plan_json_add(place, "bars", bars);
        if (!ok) {
            cJSON_Delete(bars);
        }
    }
    if (ok) {
        ok = plan_json_add(object, "target", place);
        place = NULL;
    }
    ok = ok && plan_json_add(object, "outcome", cJSON_CreateString(OUTCOME_NAMES[target->outcome])) &&
         plan_json_add(object, "considered", json_number((long)target->considered)) &&
         plan_json_add(object, "chosen", target->has_chosen ? candidate_json(&target->chosen, false) : cJSON_CreateNull());
    for (size_t i = 0; ok && i < target->runner_up_count; i++) {
        ok = plan_json_append(runners, candidate_json(&target->runners_up[i], true));
    }
    for (size_t i = 0; ok && i < target->rejected_count; i++) {
        ok = plan_json_append(rejected, candidate_json(&target->rejected[i], false));
    }
    if (ok) {
        ok = plan_json_add(object, "runners_up", runners);
        runners = NULL;
    }
    if (ok) {
        ok = plan_json_add(object, "rejected", rejected);
        rejected = NULL;
    }
    if (!ok) {
        cJSON_Delete(object);
        cJSON_Delete(place);
        cJSON_Delete(runners);
        cJSON_Delete(rejected);
        return NULL;
    }
    return object;
}

static cJSON *item_json(const AnalyzerCheckItem *item) {
    cJSON *object = cJSON_CreateObject();
    bool ok = object && plan_json_add(object, "kind", cJSON_CreateString(KIND_NAMES[item->kind])) &&
              plan_json_add(object, "bar", json_number((long)item->bar + 1)) &&
              plan_json_add(object, "planned", cJSON_CreateString(item->planned)) &&
              plan_json_add(object, "heard", cJSON_CreateString(item->heard)) &&
              plan_json_add(object, "agreed", cJSON_CreateBool(item->agreed));
    if (!ok) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

static cJSON *section_json(const ExplainSection *section) {
    cJSON *object = cJSON_CreateObject();
    cJSON *heard = cJSON_CreateObject();
    cJSON *items = cJSON_CreateArray();
    bool ok = object && heard && items && plan_json_add(object, "name", cJSON_CreateString(section->name)) &&
              plan_json_add(object, "letter", section->letter[0] ? cJSON_CreateString(section->letter) : cJSON_CreateNull()) &&
              plan_json_add(heard, "heard", cJSON_CreateBool(section->heard)) &&
              plan_json_add(heard, "form", cJSON_CreateString(section->heard_form)) &&
              plan_json_add(heard, "requested", cJSON_CreateString(section->requested_form)) &&
              plan_json_add(heard, "as_requested", cJSON_CreateBool(section->heard_as_requested));
    if (ok) {
        ok = plan_json_add(object, "form", heard);
        heard = NULL;
    }
    if (ok && section->checked) {
        cJSON *check = cJSON_CreateObject();
        ok = check && plan_json_add(check, "contradicts", cJSON_CreateBool(section->check.contradicts)) &&
             plan_json_add(check, "planned", json_number((long)section->check.planned)) &&
             plan_json_add(check, "agreed", json_number((long)section->check.agreed));
        for (size_t i = 0; ok && i < section->check.item_count; i++) {
            ok = plan_json_append(items, item_json(&section->check.items[i]));
        }
        if (ok) {
            ok = plan_json_add(check, "items", items);
            items = NULL;
        }
        ok = ok && plan_json_add(object, "check", check);
        if (!ok) {
            cJSON_Delete(check);
        }
    } else if (ok) {
        ok = plan_json_add(object, "check", cJSON_CreateNull());
    }
    if (!ok) {
        cJSON_Delete(object);
        cJSON_Delete(heard);
        cJSON_Delete(items);
        return NULL;
    }
    return object;
}

static cJSON *rejection_json(const ComposeRejection *rejection) {
    cJSON *object = cJSON_CreateObject();
    cJSON *items = cJSON_CreateArray();
    char seed[32];
    snprintf(seed, sizeof(seed), "%lu", rejection->seed);
    bool ok = object && items && plan_json_add(object, "round", json_number((long)rejection->round)) &&
              plan_json_add(object, "seed", cJSON_CreateString(seed)) &&
              plan_json_add(object, "stage", cJSON_CreateString(rejection->stage)) &&
              plan_json_add(object, "reason", cJSON_CreateString(rejection->reason)) &&
              plan_json_add(object, "detail", cJSON_CreateString(rejection->detail));
    for (size_t i = 0; ok && i < rejection->item_count; i++) {
        cJSON *item = item_json(&rejection->items[i].item);
        ok = item && plan_json_add(item, "section", cJSON_CreateString(rejection->items[i].section)) && plan_json_append(items, item);
    }
    if (ok) {
        ok = plan_json_add(object, "items", items);
        items = NULL;
    }
    if (!ok) {
        cJSON_Delete(object);
        cJSON_Delete(items);
        return NULL;
    }
    return object;
}

char *explain_to_json(const Explanation *explanation) {
    if (!explanation) {
        return NULL;
    }
    cJSON *document = cJSON_CreateObject();
    cJSON *stages = cJSON_CreateArray();
    cJSON *sections = cJSON_CreateArray();
    cJSON *rejections = cJSON_CreateArray();
    bool ok = document && stages && sections && rejections && plan_json_add(document, "schema", cJSON_CreateString("explanation")) &&
              plan_json_add(document, "version", json_number(1));
    cJSON *stage = NULL;
    cJSON *targets = NULL;
    for (size_t t = 0; ok && t < explanation->target_count; t++) {
        const ExplainTarget *target = &explanation->targets[t];
        /* The targets of a stage are together: a new stage object when the name changes. */
        if (!stage || strcmp(target->stage, explanation->targets[t - 1].stage) != 0) {
            stage = cJSON_CreateObject();
            targets = cJSON_CreateArray();
            ok = stage && targets && plan_json_add(stage, "stage", cJSON_CreateString(target->stage));
            if (ok) {
                ok = plan_json_add(stage, "targets", targets);
                if (ok) {
                    ok = plan_json_append(stages, stage);
                }
            } else {
                cJSON_Delete(stage);
                cJSON_Delete(targets);
                stage = NULL;
                targets = NULL;
            }
            if (!ok) {
                break;
            }
        }
        ok = plan_json_append(targets, target_json(target));
    }
    if (ok) {
        ok = plan_json_add(document, "stages", stages);
        stages = NULL;
    }
    for (size_t s = 0; ok && s < explanation->section_count; s++) {
        ok = plan_json_append(sections, section_json(&explanation->sections[s]));
    }
    if (ok) {
        ok = plan_json_add(document, "sections", sections);
        sections = NULL;
    }
    for (size_t r = 0; ok && r < explanation->rejection_count; r++) {
        ok = plan_json_append(rejections, rejection_json(&explanation->rejections[r]));
    }
    if (ok) {
        ok = plan_json_add(document, "rejections", rejections);
        rejections = NULL;
    }
    char *printed = ok ? cJSON_Print(document) : NULL;
    cJSON_Delete(document);
    cJSON_Delete(stages);
    cJSON_Delete(sections);
    cJSON_Delete(rejections);
    char *text = printed ? json_indent_with_spaces(printed) : NULL;
    cJSON_free(printed);
    return text;
}
