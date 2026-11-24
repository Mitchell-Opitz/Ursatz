#include "api_grow.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <analysis/dimension/dimension.h>
#include <theory/form/phrase_beginning.h>
#include <theory/form/phrase_ending.h>
#include <theory/form/phrase_level_form.h>
#include <theory/vocabulary/texture_rules.h>

#include <ursatz_composer/analyzer_check.h>
#include <ursatz_composer/compose.h>
#include <ursatz_composer/form_hear.h>
#include <ursatz_composer/grow.h>
#include <ursatz_composer/harmony_phrase.h>
#include <ursatz_composer/rejection.h>
#include <ursatz_composer/theme_shape.h>

#include "api_construction.h"
#include "api_critique.h"
#include "api_internal.h"
#include "api_seed.h"
#include "api_style.h"
#include "api_vocabulary.h"
#include "grow/grow_internal.h"
#include "structure/plan_util.h"
#include "structure/request_parse.h"
#include "util/util.h"

#define MAX_CHORDS 96
#define MAX_LANE_SPANS 128
#define MAX_DEVELOPMENTS 8
#define SLOT_ROWS 16

/* --- describing a theme ------------------------------------------------------------------------------ */

static const char *const KIND_NAMES[] = {"form", "section", "phrase", "subphrase", "idea", "motive"};

static double in_bars(Rational time, Rational bar) {
    return ((double)time.num / (double)time.den) / ((double)bar.num / (double)bar.den);
}

/* A time as bars from the start of the theme. */
static double offset_in_bars(Rational time, PlanSpan theme, Rational bar) {
    Rational offset;
    offset.num = time.num * (long)theme.start.den - theme.start.num * (long)time.den;
    offset.den = time.den * theme.start.den;
    return in_bars(offset, bar);
}

/* A number as the commands print it with %g: six digits. */
static const char *relation_word(const PlanMotiveRef *ref) {
    if (!ref->has_step) {
        return "none";
    }
    switch (ref->step.kind) {
    case MOTIVE_RELATION_TRANSPOSE:
        /* A transposition by steps of the key is a sequence (ch. V). */
        return ref->step.value[0] != 0 ? "sequence" : "transposed";
    case MOTIVE_RELATION_INVERT:
        return "inverted";
    case MOTIVE_RELATION_RETROGRADE:
        return "retrograde";
    case MOTIVE_RELATION_AUGMENT:
        return "augmented";
    case MOTIVE_RELATION_DIMINISH:
        return "diminished";
    case MOTIVE_RELATION_FRAGMENT:
        return "fragmented";
    case MOTIVE_RELATION_EXACT:
        return "restated";
    case MOTIVE_RELATION_INTERVAL_CHANGE:
        return "interval changed";
    case MOTIVE_RELATION_LIQUIDATE:
        return "liquidated";
    case MOTIVE_RELATION_DOMINANT_FORM:
        return "dominant form";
    default:
        return "none";
    }
}

/* "restated, exact", "restated, 1 step up (tonal)", "fragmented, leading, 3 notes": the development the ref asks for. */
static void describe_ref(const PlanMotiveRef *ref, char *out, size_t size) {
    static const char *const POSITIONS[] = {"", ", leading", ", trailing", ", middle"};
    const long *v = ref->step.value;
    MotiveRelationKind kind = ref->step.kind;
    bool has = ref->has_step;
    bool tonal = has && ((kind == MOTIVE_RELATION_TRANSPOSE && v[0] != 0) || (kind == MOTIVE_RELATION_INVERT && v[1] != 0));
    int steps = has && kind == MOTIVE_RELATION_TRANSPOSE && v[0] != 0 ? (int)v[1] : 0;
    int semitones = has && kind == MOTIVE_RELATION_TRANSPOSE && v[0] == 0 ? (int)v[1] : 0;
    size_t used = (size_t)snprintf(out, size, "%s", relation_word(ref));
    char *end = out + size;
    bool moved = false;
    if (steps != 0 && used < size) {
        used += (size_t)snprintf(out + used, (size_t)(end - out) - used, ", %d step%s %s%s", steps > 0 ? steps : -steps,
                                 steps == 1 || steps == -1 ? "" : "s", steps > 0 ? "up" : "down",
                                 tonal ? " (tonal)" : "");
        moved = true;
    } else if (semitones != 0 && used < size) {
        used += (size_t)snprintf(out + used, (size_t)(end - out) - used, ", %+d semitones", semitones);
        moved = true;
    }
    if (has && kind == MOTIVE_RELATION_EXACT && !moved && used < size) {
        used += (size_t)snprintf(out + used, (size_t)(end - out) - used, ", exact");
    }
    if (tonal && !moved && used < size) {
        used += (size_t)snprintf(out + used, (size_t)(end - out) - used, ", tonal");
    }
    if (has && kind == MOTIVE_RELATION_FRAGMENT && used < size) {
        used += (size_t)snprintf(out + used, (size_t)(end - out) - used, "%s", POSITIONS[v[0] >= 0 && v[0] < 4 ? v[0] : 0]);
        if (v[1] > 0 && used < size) {
            used += (size_t)snprintf(out + used, (size_t)(end - out) - used, ", %d notes", (int)v[1]);
        }
    }
    if (has && kind == MOTIVE_RELATION_LIQUIDATE && v[0] > 0 && used < size) {
        used += (size_t)snprintf(out + used, (size_t)(end - out) - used, ", drop %d", (int)v[0]);
    }
    if (has && kind == MOTIVE_RELATION_INTERVAL_CHANGE && v[1] != 0 && used < size) {
        snprintf(out + used, (size_t)(end - out) - used, ", %+d semitones on interval %d", (int)v[1], (int)v[0]);
    }
}

static cJSON *structure_json(const Plan *plan, NodeId theme_node, Rational bar) {
    cJSON *list = cJSON_CreateArray();
    /* A depth-first walk with an explicit stack (the plan is a tree of a handful of levels). */
    struct {
        NodeId node;
        size_t next;
    } stack[16];
    size_t depth = 0;
    PlanSpan theme = plan_node_span(plan, theme_node);
    stack[0].node = theme_node;
    stack[0].next = 0;
    bool entered = false;
    while (list) {
        if (!entered) {
            NodeId node = stack[depth].node;
            PlanSpan span = plan_node_span(plan, node);
            Rational end;
            rational_add(span.start, span.length, &end);
            const char *name = plan_node_name(plan, node);
            cJSON *item = cJSON_CreateObject();
            if (!item || !cJSON_AddStringToObject(item, "name", name ? name : "") ||
                !cJSON_AddStringToObject(item, "kind", KIND_NAMES[plan_node_kind(plan, node)]) ||
                !cJSON_AddItemToObject(item, "start", api_number_g(offset_in_bars(span.start, theme, bar))) ||
                !cJSON_AddItemToObject(item, "end", api_number_g(offset_in_bars(end, theme, bar))) ||
                !cJSON_AddItemToArray(list, item)) {
                cJSON_Delete(item);
                cJSON_Delete(list);
                return NULL;
            }
            entered = true;
        }
        if (stack[depth].next < plan_node_child_count(plan, stack[depth].node) && depth + 1 < 16) {
            NodeId child = plan_node_child_at(plan, stack[depth].node, stack[depth].next++);
            depth++;
            stack[depth].node = child;
            stack[depth].next = 0;
            entered = false;
            continue;
        }
        if (depth == 0) {
            break;
        }
        depth--;
    }
    return list;
}

static cJSON *chords_json(const Plan *plan, NodeId theme_node, const PlanMeter *meter) {
    cJSON *list = cJSON_CreateArray();
    PlanSpan span = plan_node_span(plan, theme_node);
    Rational bar;
    plan_meter_bar_length(meter, &bar);
    LaneSpanId ids[MAX_CHORDS];
    size_t count = plan_lane_find(plan, PLAN_LANE_HARMONY, 0, span, ids, MAX_CHORDS);
    if (count > MAX_CHORDS) {
        count = MAX_CHORDS;
    }
    for (size_t i = 0; list && i < count; i++) {
        PlanSpan chord = plan_lane_span_span(plan, ids[i]);
        const PlanHarmony *harmony = &plan_lane_span_payload(plan, ids[i])->harmony;
        /* The offset from the start of the theme, as a fraction of a whole note, split into bars and the rest. */
        long off_num = chord.start.num * (long)span.start.den - span.start.num * (long)chord.start.den;
        unsigned long off_den = chord.start.den * span.start.den;
        long bars = (off_num * (long)bar.den) / ((long)off_den * bar.num);
        long rest_num = off_num * (long)bar.den - bars * bar.num * (long)off_den;
        unsigned long rest_den = off_den * bar.den;
        char bass[48] = "";
        LaneSpanId basses[2];
        if (plan_lane_find(plan, PLAN_LANE_BASS, 0, chord, basses, 2) == 1) {
            const PlanNotes *notes = &plan_lane_span_payload(plan, basses[0])->notes;
            size_t used = 0;
            for (size_t n = 0; n < notes->note_count && used + 8 < sizeof bass; n++) {
                char name[16];
                util_pitch_name(notes->notes[n].midi, false, name, sizeof(name));
                used += (size_t)snprintf(bass + used, sizeof bass - used, n ? " %s" : "%s", name);
            }
        }
        cJSON *item = cJSON_CreateObject();
        if (!item || !cJSON_AddNumberToObject(item, "bar", (double)((int)bars + 1)) ||
            !cJSON_AddNumberToObject(item, "beat", (double)((int)(rest_num * meter->beat_unit / (long)rest_den) + 1)) ||
            !cJSON_AddStringToObject(item, "chord", harmony->roman_numeral) ||
            !cJSON_AddStringToObject(item, "function", util_function_name(harmony->has_function, harmony->function)) ||
            (harmony->borrowed_from && !cJSON_AddStringToObject(item, "borrowed_from", harmony->borrowed_from->id)) ||
            !cJSON_AddStringToObject(item, "bass", bass) || !cJSON_AddItemToArray(list, item)) {
            cJSON_Delete(item);
            cJSON_Delete(list);
            return NULL;
        }
    }
    return list;
}

typedef struct {
    char motive[16];
    const char *idea;
    char how[96];
    char segment[PLAN_SEGMENT_LABEL_MAX + 1]; /* the segment the idea is made from, empty for none */
    char summary[192];
    bool measured;  /* the idea has a melody span whose fidelity was measured (R-332) */
    double fidelity; /* the score as a fraction, 3 decimals */
    double minimum;  /* the least the melody stage accepts for the idea's role and setting; 0 for an idea its ref makes differ */
    char heard[96];  /* the relation the recognizer hears between the idea and its source */
    bool agrees;     /* the heard relation is the one the ref intended */
} Development;

static cJSON *string_or_null(const char *text);

/* The motive the ideas are made from, in words: a segment, a slot's own motif, else the basic idea. */
static void source_words(const Plan *plan, const PlanMotiveRef *ref, char *out, size_t size) {
    static const char *const SLOT_WORDS[] = {"basic idea", "contrasting motif", "continuation motif", "cadential motif"};
    const char *name = plan_has_node(plan, ref->motive) ? plan_node_name(plan, ref->motive) : NULL;
    if (ref->segment[0] != '\0') {
        snprintf(out, size, "segment %s", ref->segment);
    } else if (name && strncmp(name, "distant for m", 13) == 0) {
        /* A distant contrast made for the idea (R-475): its own rhythm, the contour changed. */
        snprintf(out, size, "a distant variant");
    } else if (name && strcmp(name, "new material") == 0) {
        /* The contrasting slot's motive made for the theme (R-477). */
        snprintf(out, size, "new material");
    } else if (ref->steps > 0 || !name) {
        snprintf(out, size, "basic idea");
    } else if (name[0] == 's' && name[1] >= '1' && name[1] <= '3' && name[2] == '\0') {
        snprintf(out, size, "%s", SLOT_WORDS[name[1] - '0']);
    } else if (name[0] == 'i' && name[1] >= '0' && name[1] <= '9' && name[2] == '\0') {
        snprintf(out, size, "its own motif");
    } else {
        snprintf(out, size, "basic idea");
    }
}

/*
 * One line naming an idea, its source and its operation (ch. VIII): "repetition = basic idea, dominant form",
 * "fragment 2 = segment b, 1 step down (tonal)", "cadential = basic idea, segments liquidated, 2 steps". The idea is named
 * as the plan names it; `how` is describe_ref's text, without the word it starts with when the operation is a restatement or
 * a sequence (the operation says it).
 */
static void summarize_development(const Plan *plan, const PlanMotiveRef *ref, const char *idea, const char *how,
                                  char *out, size_t size) {
    char source[48];
    char operation[96];
    source_words(plan, ref, source, sizeof(source));
    if (ref->steps > 0) {
        snprintf(operation, sizeof(operation), "segments liquidated, %zu step%s", ref->steps, ref->steps == 1 ? "" : "s");
    } else if (strncmp(how, "restated, ", 10) == 0 || strncmp(how, "sequence, ", 10) == 0) {
        snprintf(operation, sizeof(operation), "%s", how + 10);
    } else {
        snprintf(operation, sizeof(operation), "%s", how);
    }
    snprintf(out, size, "%s = %s, %s", idea[0] != '\0' ? idea : "(unused)", source, operation);
}

static double score_fraction(MotiveScore score) {
    double value = (double)score.numerator / (double)(score.denominator > 0 ? score.denominator : 1);
    return (double)(long)(value * 1000.0 + 0.5) / 1000.0;
}

/*
 * The idea's fidelity to its source and the relation heard (R-332): the same measure the melody stage ranks and refuses by
 * (grow_idea_fidelity), with the minimum of the idea's role under the development setting. An idea whose ref makes it differ
 * from its source (grow_ref_departs) has no minimum: it is ranked, never refused. Left unmeasured when the plan cannot say.
 */
static void measure_development(const Plan *plan, LaneSpanId span, const Motive *basic, const GrowFidelityTable *table,
                                GrowDevelopment development,
                                Development *out) {
    MotiveFidelity fidelity;
    Motive source;
    PlanMotiveRef ref;
    bool from_segment = false;
    out->measured = false;
    if (!grow_idea_fidelity(plan, span, basic, &fidelity) ||
        !grow_idea_fidelity_source(plan, span, basic, &source, &ref, &from_segment)) {
        return;
    }
    MotiveScore minimum = {0, 1};
    if (!grow_ref_departs(&ref, from_segment)) {
        minimum = grow_fidelity_minimum_in(table, development, grow_idea_role(out->idea));
    }
    out->measured = true;
    out->fidelity = score_fraction(fidelity.score);
    out->minimum = score_fraction(minimum);
    snprintf(out->heard, sizeof(out->heard), "%s", fidelity.heard);
    out->agrees = fidelity.agrees;
}

/*
 * The motives made from m0 (every motive node with a motive_ref) and the idea each one is the melody of, with the idea's
 * fidelity when it has a melody span (basic is the motive the run was made from, development its setting).
 */
static size_t collect_developments(const Plan *plan, NodeId theme, const Motive *basic, const GrowFidelityTable *fidelity,
                                   GrowDevelopment development,
                                   Development *out) {
    size_t count = 0;
    LaneSpanId melodies[MAX_LANE_SPANS];
    size_t melody_count = plan_lane_find(plan, PLAN_LANE_MELODY, 0, plan_node_span(plan, theme), melodies, MAX_LANE_SPANS);
    for (NodeId id = 1; id <= plan_node_count(plan) && count < MAX_DEVELOPMENTS; id++) {
        const char *name = plan_node_name(plan, id);
        if (plan_node_kind(plan, id) != PLAN_NODE_MOTIVE || !name || strcmp(name, "m0") == 0) {
            continue;
        }
        const PlanConstraint *ref = NULL;
        for (size_t c = 0; c < plan_node_constraint_count(plan, id) && !ref; c++) {
            const PlanConstraint *constraint = plan_node_constraint_at(plan, id, c);
            ref = constraint->kind == PLAN_CONSTRAINT_MOTIVE_REF ? constraint : NULL;
        }
        if (!ref) {
            continue;
        }
        snprintf(out[count].motive, sizeof(out[count].motive), "%s", name);
        out[count].idea = "";
        out[count].measured = false;
        LaneSpanId idea_span = 0;
        bool has_span = false;
        for (size_t m = 0; m < melody_count && m < MAX_LANE_SPANS; m++) {
            for (size_t c = 0; c < plan_lane_span_constraint_count(plan, melodies[m]); c++) {
                const PlanConstraint *constraint = plan_lane_span_constraint_at(plan, melodies[m], c);
                if (constraint->kind == PLAN_CONSTRAINT_MOTIVE_REF && constraint->params.motive.motive == id) {
                    const char *idea = plan_node_name(plan, plan_lane_span_owner(plan, melodies[m]));
                    out[count].idea = idea ? idea : "";
                    idea_span = melodies[m];
                    has_span = true;
                }
            }
        }
        if (has_span) {
            measure_development(plan, idea_span, basic, fidelity, development, &out[count]);
        }
        describe_ref(&ref->params.motive, out[count].how, sizeof(out[count].how));
        snprintf(out[count].segment, sizeof(out[count].segment), "%s", ref->params.motive.segment);
        summarize_development(plan, &ref->params.motive, out[count].idea, out[count].how, out[count].summary,
                              sizeof(out[count].summary));
        count++;
    }
    return count;
}

static cJSON *developments_json(const Plan *plan, NodeId theme, const Motive *basic, const GrowFidelityTable *fidelity,
                                GrowDevelopment development) {
    Development developments[MAX_DEVELOPMENTS];
    size_t count = collect_developments(plan, theme, basic, fidelity, development, developments);
    cJSON *list = cJSON_CreateArray();
    for (size_t i = 0; list && i < count; i++) {
        cJSON *item = cJSON_CreateObject();
        if (!item || !cJSON_AddStringToObject(item, "motive", developments[i].motive) ||
            !cJSON_AddStringToObject(item, "idea", developments[i].idea) ||
            !cJSON_AddStringToObject(item, "development", developments[i].how) ||
            !cJSON_AddItemToObject(item, "segment", string_or_null(developments[i].segment)) ||
            !cJSON_AddStringToObject(item, "summary", developments[i].summary) ||
            !cJSON_AddItemToObject(item, "fidelity",
                                   developments[i].measured ? cJSON_CreateNumber(developments[i].fidelity) : cJSON_CreateNull()) ||
            !cJSON_AddItemToObject(item, "minimum",
                                   developments[i].measured ? cJSON_CreateNumber(developments[i].minimum) : cJSON_CreateNull()) ||
            !cJSON_AddItemToObject(item, "heard", developments[i].measured ? cJSON_CreateString(developments[i].heard) : cJSON_CreateNull()) ||
            !cJSON_AddItemToObject(item, "heard_agrees",
                                   developments[i].measured ? cJSON_CreateBool(developments[i].agrees) : cJSON_CreateNull()) ||
            !cJSON_AddItemToArray(list, item)) {
            cJSON_Delete(item);
            cJSON_Delete(list);
            return NULL;
        }
    }
    return list;
}

/* "hybrid (compound basic idea + continuation)" for a hybrid with its beginning and ending, else the form's name. */
static void form_label(int form, int beginning, int ending, char *out, size_t size) {
    if (form == PHRASE_LEVEL_FORM_HYBRID && beginning >= 0 && ending >= 0) {
        snprintf(out, size, "%s (%s + %s)", form_hear_form_name(form), phrase_beginning_name((PhraseBeginning)beginning),
                 phrase_ending_name((PhraseEnding)ending));
    } else {
        snprintf(out, size, "%s", form >= 0 ? form_hear_form_name(form) : "none");
    }
}

/* What was heard, in words; heard_form and requested are the labels of the two forms. */
static void heard_text(const GrowOutcome *outcome, char *heard_form, size_t form_size, char *requested, size_t requested_size,
                       char *out, size_t size) {
    form_label(outcome->requested_form, outcome->requested_beginning, outcome->requested_ending, requested, requested_size);
    heard_form[0] = '\0';
    if (outcome->heard_result != FORM_HEAR_OK) {
        snprintf(out, size, "not heard (%s)", form_hear_result_name(outcome->heard_result));
    } else if (outcome->heard.count == 0) {
        snprintf(out, size, "no theme heard");
    } else {
        const FormHeardTheme *first = &outcome->heard.themes[0];
        form_label(first->form, first->beginning, first->ending, heard_form, form_size);
        if (outcome->heard_as_requested) {
            snprintf(out, size, "heard as %s", heard_form);
        } else {
            snprintf(out, size, "heard as %s, not %s", heard_form, requested);
        }
    }
}

/* --- the slots of a theme (the theme command's) ------------------------------------------------------- */

/* The slot an idea has the function of, by the idea's name (grow_form.c has the same table). */
static GrowSlot idea_slot(const char *name) {
    for (int idea = 0; name && idea < GROW_IDEA_COUNT; idea++) {
        if (strcmp(name, grow_idea_name((GrowIdea)idea)) == 0) {
            return theme_shape_idea_slot((GrowIdea)idea);
        }
    }
    return GROW_SLOT_BASIC;
}

typedef struct {
    const char *idea;
    GrowSlot slot;
    GrowSlot origin; /* the slot whose motive the idea is made from */
    const char *source;
    int own_idea; /* the GrowIdea whose own motive the idea is, else -1 */
    char item[64];
    char chain[256];
    long connection; /* percent, -1 for none */
} SlotRow;

/* The percent in a form's label ("... 87%: ..."), else -1; and where the text before it ends. */
static long label_percent(const char *label, size_t *before) {
    const char *mark = strstr(label, "%:");
    if (!mark) {
        return -1;
    }
    const char *start = mark;
    while (start > label && start[-1] >= '0' && start[-1] <= '9') {
        start--;
    }
    if (start == mark) {
        return -1;
    }
    *before = (size_t)(start - label);
    while (*before > 0 && label[*before - 1] == ' ') {
        (*before)--;
    }
    return strtol(start, NULL, 10);
}

/* The step of the motives stage that wrote the motive node, else NULL. */
static const FillConsidered *chosen_for(const GrowOutcome *outcome, NodeId node) {
    const FillRecord *record = outcome->records[GROW_STAGE_MOTIVES];
    for (size_t i = 0; record && i < record->step_count; i++) {
        const FillStep *step = &record->steps[i];
        if (step->target.kind == FILL_TARGET_NODE && step->target.id == node && step->chosen >= 0) {
            return &step->considered[step->chosen];
        }
    }
    return NULL;
}

static void describe_row(const GrowOutcome *outcome, NodeId motive_node, SlotRow *row) {
    const FillConsidered *chosen = chosen_for(outcome, motive_node);
    row->source = "motive";
    row->connection = -1;
    if (!chosen) {
        return;
    }
    size_t before = 0;
    row->connection = label_percent(chosen->label, &before);
    if (strcmp(chosen->operator_name, GROW_FORM_OPERATOR_NAME) != 0) {
        row->source = "operator";
        snprintf(row->chain, sizeof(row->chain), "%s", chosen->label);
    } else if (strncmp(chosen->label, "exact ", 6) == 0) {
        row->source = "motive";
    } else {
        row->source = "registry";
        snprintf(row->chain, sizeof(row->chain), "%.*s", (int)(before < sizeof(row->chain) - 1 ? before : sizeof(row->chain) - 1),
                 chosen->label);
    }
}

/* The ideas of the theme, in order, with the motive each is made from and how its motive was made. */
static size_t collect_slot_rows(const Plan *plan, const GrowOutcome *outcome, SlotRow *rows) {
    NodeId theme = api_find_node(plan, "theme");
    size_t count = 0;
    if (theme == PLAN_NODE_NONE) {
        return 0;
    }
    PlanSpan span = plan_node_span(plan, theme);
    for (NodeId id = 1; id <= plan_node_count(plan) && count < SLOT_ROWS; id++) {
        if (plan_node_kind(plan, id) != PLAN_NODE_IDEA || !plan_span_contains(span, plan_node_span(plan, id))) {
            continue;
        }
        LaneSpanId lanes[16];
        size_t found = plan_node_lane_spans(plan, id, lanes, 16);
        for (size_t i = 0; i < found && i < 16; i++) {
            if (plan_lane_span_lane(plan, lanes[i]) != PLAN_LANE_MELODY || plan_lane_span_voice(plan, lanes[i]) != 0) {
                continue;
            }
            for (size_t k = 0; k < plan_lane_span_constraint_count(plan, lanes[i]); k++) {
                const PlanConstraint *constraint = plan_lane_span_constraint_at(plan, lanes[i], k);
                if (constraint->kind != PLAN_CONSTRAINT_MOTIVE_REF) {
                    continue;
                }
                SlotRow *row = &rows[count];
                memset(row, 0, sizeof(*row));
                row->idea = plan_node_name(plan, id);
                row->own_idea = -1;
                row->slot = idea_slot(row->idea);
                row->origin = GROW_SLOT_BASIC;
                /* The idea's melody asks for its motive node (m1 to m4); that node is made from m0 or a slot's node. */
                NodeId made_from = constraint->params.motive.motive;
                for (size_t c = 0; c < plan_node_constraint_count(plan, made_from); c++) {
                    const PlanConstraint *ref = plan_node_constraint_at(plan, made_from, c);
                    if (ref->kind == PLAN_CONSTRAINT_MOTIVE_REF) {
                        made_from = ref->params.motive.motive;
                    }
                }
                const char *node_name = plan_node_name(plan, made_from);
                /* The slot motives are the material nodes s1 (contrasting), s2 (continuation) and s3 (cadential). */
                for (GrowSlot own = GROW_SLOT_CONTRASTING; node_name && own < GROW_SLOT_COUNT; own++) {
                    if (node_name[0] == 's' && node_name[1] == (char)('0' + own) && node_name[2] == '\0') {
                        row->origin = own;
                    }
                }
                const char *direct = plan_node_name(plan, constraint->params.motive.motive);
                if (direct && direct[0] == 'i' && direct[1] >= '0' && direct[1] <= '9' && direct[2] == '\0') {
                    /* The idea has a motive of its own: the node "i<k>" of the material. */
                    row->own_idea = direct[1] - '0';
                    row->source = "idea";
                    row->connection = -1;
                } else if (direct && strncmp(direct, "distant for m", 13) == 0) {
                    /* A distant contrast made for the idea (R-475). */
                    row->source = "distant";
                    row->connection = -1;
                } else if (node_name && strcmp(node_name, "new material") == 0) {
                    /* The idea is the contrasting slot's new material, or is made from it (R-477). */
                    row->source = "new";
                    row->connection = -1;
                } else if (direct && strcmp(direct, "m0") == 0) {
                    row->source = "motive"; /* the basic idea is the basic motive */
                    row->connection = -1;
                } else {
                    describe_row(outcome, constraint->params.motive.motive, row);
                }
                count++;
                break;
            }
            break;
        }
    }
    return count;
}

static cJSON *string_or_null(const char *text) {
    return text && text[0] ? cJSON_CreateString(text) : cJSON_CreateNull();
}

static cJSON *slots_json(const GrowThemeInfo *info, const Plan *plan, const GrowOutcome *outcome) {
    SlotRow rows[SLOT_ROWS];
    size_t count = collect_slot_rows(plan, outcome, rows);
    cJSON *list = cJSON_CreateArray();
    for (size_t i = 0; list && i < count; i++) {
        const char *motive = rows[i].own_idea >= 0 ? info->idea_motive_id[rows[i].own_idea]
                                                   : info->slot_motive_id[rows[i].origin];
        cJSON *item = cJSON_CreateObject();
        if (!item || !cJSON_AddStringToObject(item, "idea", rows[i].idea ? rows[i].idea : "") ||
            !cJSON_AddStringToObject(item, "slot", grow_slot_name(rows[i].slot)) ||
            !cJSON_AddStringToObject(item, "motive", motive ? motive : "") ||
            !cJSON_AddStringToObject(item, "source", rows[i].source) ||
            !cJSON_AddItemToObject(item, "item", string_or_null(rows[i].item)) ||
            !cJSON_AddItemToObject(item, "chain", string_or_null(rows[i].chain)) ||
            !cJSON_AddItemToObject(item, "connection",
                                   rows[i].connection >= 0 ? cJSON_CreateNumber((double)rows[i].connection)
                                                           : cJSON_CreateNull()) ||
            !cJSON_AddItemToArray(list, item)) {
            cJSON_Delete(item);
            cJSON_Delete(list);
            return NULL;
        }
    }
    return list;
}

/* --- the result -------------------------------------------------------------------------------------- */


void api_grow_brief(const GrowSetup *setup, GrowBrief *out) {
    memset(out, 0, sizeof(*out));
    out->tempo = setup->tempo;
    out->tempo_from = setup->tempo_from;
    out->material = setup->key.material;
    out->mode_from = setup->mode_from;
    out->has_density = composer_style_density(setup->style, &out->density);
    out->without = setup->without;
    out->without_count = setup->without_count;
    out->development = setup->material ? api_development_name(setup->development) : NULL; /* a theme's, not a grow's */
}

/* The construction of the theme (R-468) as the response gives it, null for a theme without one. */
static cJSON *construction_json(const Plan *plan, NodeId node) {
    const SectionDescription *section = plan_section_or_empty(plan, node);
    if (!section->has_construction || !section->has_pairing) {
        return cJSON_CreateNull();
    }
    char text[192];
    grow_section_describe(plan, node, text, sizeof text);
    return api_grow_construction_json(&section->construction, &section->pairing, text);
}

/* --- the measured contrasts (R-710) ---------------------------------------------------------------------- */

#define CONTRAST_NOTES 128

/*
 * The notes of the motive an idea is made from as a span the dimension measures read, in onset order: the contrast target is held to
 * the motives (grow_contrast_met), before the melody stage moves notes to the chords and holds the cadence, so the response measures
 * what the draw held. The span's length is the motive's last note's end rounded up to whole bars. False when the idea has no motive.
 */
static bool idea_span(const Plan *plan, NodeId idea, const PlanMeter *meter, DimensionNote *notes, DimensionSpan *out) {
    LaneSpanId melody;
    LaneSpanId made;
    if (!plan_melody_span_of(plan, idea, &melody)) {
        return false;
    }
    const PlanConstraint *ref = plan_lane_span_find_constraint(plan, melody, PLAN_CONSTRAINT_MOTIVE_REF);
    if (ref == NULL || !plan_has_node(plan, ref->params.motive.motive) || !plan_melody_span_of(plan, ref->params.motive.motive, &made)) {
        return false;
    }
    const PlanNotes *payload = &plan_lane_span_payload(plan, made)->notes;
    Rational bar;
    Rational end = {0, 1};
    if (payload->note_count == 0 || payload->note_count > CONTRAST_NOTES || plan_meter_bar_length(meter, &bar) != PLAN_OK) {
        return false;
    }
    for (size_t n = 0; n < payload->note_count; n++) {
        Rational note_end;
        notes[n].onset = payload->notes[n].offset;
        notes[n].duration = payload->notes[n].duration;
        notes[n].pitch = payload->notes[n].midi;
        if (rational_add(notes[n].onset, notes[n].duration, &note_end) != RATIONAL_OK) {
            return false;
        }
        end = rational_cmp(note_end, end) > 0 ? note_end : end;
    }
    Rational bars;
    long long whole = 0;
    if (rational_div(end, bar, &bars) != RATIONAL_OK || rational_ceil(bars, &whole) != RATIONAL_OK || whole < 1 ||
        rational_mul_int(bar, whole, &out->length) != RATIONAL_OK) {
        return false;
    }
    bool compound = meter->beat_unit == 8 && meter->beats_per_bar >= 6 && meter->beats_per_bar % 3 == 0;
    out->notes = notes;
    out->count = payload->note_count;
    out->beat = compound ? rational_ratio(3, 8) : rational_ratio(1, meter->beat_unit > 0 ? meter->beat_unit : 4);
    return true;
}

/*
 * One row for each idea of the theme that holds a contrast constraint: the idea, the idea it contrasts against and the distance
 * of its motive from that idea's in rhythm, contour, register and density. NULL for a theme with no such idea.
 */
static cJSON *contrasts_json(const Plan *plan, NodeId theme_node, const PlanMeter *meter) {
    PlanSpan span = plan_node_span(plan, theme_node);
    cJSON *list = NULL;
    DimensionNote idea_notes[CONTRAST_NOTES];
    DimensionNote against_notes[CONTRAST_NOTES];
    for (NodeId id = 1; id <= plan_node_count(plan); id++) {
        const PlanConstraint *constraint = plan_node_kind(plan, id) == PLAN_NODE_IDEA && plan_span_contains(span, plan_node_span(plan, id))
                                               ? plan_node_find_constraint(plan, id, PLAN_CONSTRAINT_CONTRAST)
                                               : NULL;
        if (constraint == NULL) {
            continue;
        }
        NodeId against = api_find_node(plan, constraint->params.contrast.against);
        DimensionSpan first;
        DimensionSpan second;
        ContrastVector vector;
        if (against == PLAN_NODE_NONE || !idea_span(plan, id, meter, idea_notes, &second) ||
            !idea_span(plan, against, meter, against_notes, &first) || dimension_contrast(&first, &second, &vector) != DIMENSION_OK) {
            continue;
        }
        cJSON *item = cJSON_CreateObject();
        bool ok = item && cJSON_AddStringToObject(item, "idea", plan_node_name(plan, id)) &&
                  cJSON_AddStringToObject(item, "against", constraint->params.contrast.against);
        for (int aspect = 0; ok && aspect < CONTRAST_ASPECT_COUNT; aspect++) {
            ok = cJSON_AddItemToObject(item, contrast_aspect_name((ContrastAspect)aspect), api_ratio_number(&vector.distance[aspect]));
        }
        list = list ? list : cJSON_CreateArray();
        if (!ok || !list || !cJSON_AddItemToArray(list, item)) {
            cJSON_Delete(item);
            cJSON_Delete(list);
            return NULL;
        }
    }
    return list;
}

static cJSON *theme_json(const GrowSetup *setup, const GrowTheme *theme, const GrowThemeInfo *info, Rational bar) {
    const Plan *plan = theme->plan;
    NodeId node = api_find_node(plan, "theme");
    cJSON *object = cJSON_CreateObject();
    char heard_form[96];
    char requested[96];
    char heard[256];
    heard_text(&theme->outcome, heard_form, sizeof(heard_form), requested, sizeof(requested), heard, sizeof(heard));
    cJSON *score = cJSON_CreateObject();
    bool ok = object && score && cJSON_AddNumberToObject(object, "seed", (double)theme->seed) &&
              cJSON_AddItemToObject(object, "structure", structure_json(plan, node, bar)) &&
              cJSON_AddItemToObject(object, "construction", construction_json(plan, node)) &&
              cJSON_AddItemToObject(object, "chords", chords_json(plan, node, &setup->meter)) &&
              cJSON_AddItemToObject(object, "developments",
                                    developments_json(plan, node, setup->motive, setup->fidelity, setup->development));
    if (ok && info) {
        ok = cJSON_AddItemToObject(object, "slots", slots_json(info, plan, &theme->outcome));
    }
    ok = ok && cJSON_AddNumberToObject(score, "numerator", (double)theme->outcome.score.numerator) &&
         cJSON_AddNumberToObject(score, "denominator", (double)theme->outcome.score.denominator) &&
         cJSON_AddItemToObject(object, "score", score);
    if (ok) {
        score = NULL;
    }
    ok = ok && cJSON_AddStringToObject(object, "requested", requested) &&
         cJSON_AddItemToObject(object, "heard", string_or_null(heard_form)) &&
         cJSON_AddBoolToObject(object, "heard_as_requested", theme->outcome.heard_as_requested) &&
         cJSON_AddStringToObject(object, "heard_text", heard);
    if (ok && theme->outcome.style_measured) {
        ok = cJSON_AddItemToObject(object, "style_distance", api_style_distance_json(&theme->outcome.style_distance));
    }
    cJSON *contrasts = ok ? contrasts_json(plan, node, &setup->meter) : NULL;
    if (contrasts && !cJSON_AddItemToObject(object, "contrasts", contrasts)) {
        cJSON_Delete(contrasts);
        ok = false;
    }
    if (ok && theme->judged && setup->critic) {
        ok = cJSON_AddItemToObject(object, "critic", api_critic_json(&theme->judgment, setup->critic));
    }
    if (!ok) {
        cJSON_Delete(score);
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* The construction as the request gave it: {"ideas": {"<idea>": bars}, "extensions": [{"kind", "idea", "bars", "varied"}]}. */
static cJSON *given_construction_json(const SectionConstruction *construction) {
    cJSON *object = cJSON_CreateObject();
    cJSON *ideas = object ? cJSON_AddObjectToObject(object, "ideas") : NULL;
    cJSON *extensions = object ? cJSON_AddArrayToObject(object, "extensions") : NULL;
    bool ok = object && ideas && extensions;
    for (int idea = 0; ok && idea < SECTION_CONSTRUCTION_IDEAS; idea++) {
        ok = construction->idea_bars[idea] == 0 ||
             cJSON_AddNumberToObject(ideas, theme_shape_idea_name((GrowIdea)idea), construction->idea_bars[idea]) != NULL;
    }
    for (size_t k = 0; ok && k < construction->extension_count; k++) {
        const SectionExtension *extension = &construction->extensions[k];
        bool names_idea = extension->kind == SECTION_EXTENSION_REPEAT || extension->kind == SECTION_EXTENSION_STRETCH;
        cJSON *item = cJSON_CreateObject();
        ok = item && cJSON_AddItemToArray(extensions, item) && cJSON_AddStringToObject(item, "kind", API_EXTENSION_KINDS[extension->kind]) &&
             cJSON_AddItemToObject(item, "idea",
                                   names_idea ? cJSON_CreateString(theme_shape_idea_name((GrowIdea)extension->idea)) : cJSON_CreateNull()) &&
             cJSON_AddNumberToObject(item, "bars", extension->bars) && cJSON_AddBoolToObject(item, "varied", extension->varied);
    }
    if (!ok) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

cJSON *api_grow_result_json(const GrowSetup *setup, const GrowRun *run, const GrowThemeInfo *info) {
    const GrowBriefText *text = setup->text;
    char key[24];
    util_key_name(spelled_key_tonic_pitch_class(&setup->key), setup->key.material, key, sizeof key);
    char pairing[64];
    snprintf(pairing, sizeof(pairing), "%s + %s", API_BEGINNING_NAMES[setup->pairing.beginning],
             API_ENDING_NAMES[setup->pairing.ending]);
    char meter[16];
    snprintf(meter, sizeof meter, "%d/%d", setup->meter.beats_per_bar, setup->meter.beat_unit);
    Rational bar;
    plan_meter_bar_length(&setup->meter, &bar);
    cJSON *root = cJSON_CreateObject();
    bool ok = root && cJSON_AddStringToObject(root, "key", key) && cJSON_AddStringToObject(root, "meter", meter) &&
              cJSON_AddNumberToObject(root, "bars", (double)setup->bars) &&
              cJSON_AddStringToObject(root, "into", text->into) && cJSON_AddStringToObject(root, "pairing", pairing) &&
              cJSON_AddStringToObject(root, "cadence", plan_cadence_name(setup->cadence));
    if (ok && setup->pairing.beginning == SECTION_BEGINNING_ANTECEDENT) {
        ok = cJSON_AddStringToObject(root, "first_cadence", plan_cadence_name(setup->first_cadence)) != NULL;
    }
    if (ok && setup->has_construction) {
        ok = cJSON_AddItemToObject(root, "construction", given_construction_json(&setup->construction));
    }
    if (ok && setup->has_accompaniment) {
        ok = cJSON_AddStringToObject(root, "accompaniment", api_accompaniment_name(setup->accompaniment.pattern)) != NULL;
        if (ok && setup->accompaniment.texture >= 0) {
            ok = cJSON_AddStringToObject(root, "texture", standard_texture_name((StandardTexture)setup->accompaniment.texture)) !=
                 NULL;
        }
    }
    if (ok && setup->has_voicing) {
        ok = cJSON_AddStringToObject(root, "voicing", setup->voicing.write_soprano ? "chorale" : "satb") != NULL;
    }
    if (ok && setup->seeded) {
        ok = cJSON_AddItemToObject(root, "source", api_seed_json(&setup->seed_found));
    }
    cJSON *tempo = ok ? cJSON_CreateObject() : NULL;
    ok = tempo && cJSON_AddItemToObject(tempo, "qpm", setup->tempo.den > 0 ? api_ratio_number(&setup->tempo) : cJSON_CreateNull()) &&
         cJSON_AddStringToObject(tempo, "from", setup->tempo_from) && cJSON_AddItemToObject(root, "tempo", tempo);
    if (!ok) {
        if (tempo && !cJSON_GetObjectItem(root, "tempo")) {
            cJSON_Delete(tempo);
        }
        cJSON_Delete(root);
        return NULL;
    }
    if (info) {
        GrowBrief brief;
        api_grow_brief(setup, &brief);
        ok = cJSON_AddItemToObject(root, "brief", api_brief_json(&brief));
        /* The chords the request gave, as read: [{"bar", "text"}]. */
        cJSON *chords = ok ? cJSON_CreateArray() : NULL;
        const cJSON *entry = NULL;
        cJSON_ArrayForEach(entry, info->chords) {
            cJSON *item = cJSON_CreateObject();
            if (!item || !cJSON_AddNumberToObject(item, "bar", cJSON_GetObjectItemCaseSensitive(entry, "bar")->valuedouble) ||
                !cJSON_AddStringToObject(item, "text", cJSON_GetObjectItemCaseSensitive(entry, "text")->valuestring) ||
                !cJSON_AddItemToArray(chords, item)) {
                cJSON_Delete(item);
                ok = false;
                break;
            }
        }
        if (!ok || !chords || !cJSON_AddItemToObject(root, "chords", chords)) {
            if (chords && !cJSON_GetObjectItem(root, "chords")) {
                cJSON_Delete(chords);
            }
            cJSON_Delete(root);
            return NULL;
        }
    }
    cJSON *themes = cJSON_AddArrayToObject(root, "themes");
    for (size_t k = 0; themes && k < run->made; k++) {
        if (!cJSON_AddItemToArray(themes, theme_json(setup, &run->themes[k], info, bar))) {
            themes = NULL;
        }
    }
    cJSON *rejected = themes && run->ranked && setup->critic ? cJSON_AddArrayToObject(root, "rejected") : NULL;
    for (size_t k = 0; rejected && k < run->rejected_count; k++) {
        const CriticRejected *candidate = &run->rejected[k];
        cJSON *item = cJSON_CreateObject();
        bool added = item && cJSON_AddItemToArray(rejected, item) && cJSON_AddNumberToObject(item, "seed", (double)candidate->seed) &&
                     api_add(item, "score", api_critic_ratio_json(candidate->score)) &&
                     api_add(item, "evaluators", api_critic_evaluators_json(candidate->findings, setup->critic));
        if (!added) {
            if (item && cJSON_GetArraySize(rejected) == (int)k) {
                cJSON_Delete(item);
            }
            themes = NULL;
        }
    }
    if (!themes || (run->ranked && setup->critic && !rejected)) {
        cJSON_Delete(root);
        return NULL;
    }
    return root;
}
