#include <ursatz_composer/variant_registry.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/fill_rng.h>

#include <ursatz_composer/motive_analysis.h>

#include <analysis/motive_relation.h>

#include "motive/motive_internal.h"
#include "motive/variant_internal.h"
#include "plan/plan_json_internal.h"
#include "util/plan_util.h"

/* How many draws of its open parameters a refused step gets. */
#define VARIANT_DRAWS 16

const char *variant_kind_name(VariantKind kind) {
    return kind == VARIANT_KIND_EXACT ? "exact" : "motive-form";
}

const char *variant_result_name(VariantResult result) {
    static const char *const NAMES[] = {"ok",   "null",           "empty chain", "unknown operation",
                                        "form", "bad parameter", "refused", "out of memory"};
    return (size_t)result < sizeof(NAMES) / sizeof(NAMES[0]) ? NAMES[result] : "unknown";
}

/* ------------------------------------------------------------------- steps */

bool variant_step_init(VariantStep *step, const char *op) {
    if (!step || !variant_registry_find(op)) {
        return false;
    }
    memset(step, 0, sizeof(*step));
    snprintf(step->op, sizeof(step->op), "%s", op);
    return true;
}

/* Whether value (and den, for a ratio) is inside the parameter's range. */
static bool value_in_range(const VariantParam *param, long value, long den) {
    switch (param->kind) {
    case VARIANT_PARAM_RATIO:
        return value >= param->min && value <= param->max && den >= param->min && den <= param->max;
    case VARIANT_PARAM_BOOL:
        return value == 0 || value == 1;
    default:
        return value >= param->min && value <= param->max;
    }
}

/* The parameter at index of the operation (all zero for one it does not have). */
static VariantParam param_at(const VariantOperation *operation, size_t index) {
    VariantParam param;
    if (!variant_operation_param(operation, index, &param)) {
        memset(&param, 0, sizeof(param));
    }
    return param;
}

bool variant_step_set(VariantStep *step, const char *param, long value, long den) {
    const VariantOperation *operation = step ? variant_registry_find(step->op) : NULL;
    int index = variant_operation_param_index(operation, param);
    if (index < 0) {
        return false;
    }
    VariantParam described = param_at(operation, (size_t)index);
    if (!value_in_range(&described, value, den)) {
        return false;
    }
    step->values[index].set = true;
    step->values[index].value = value;
    step->values[index].den = described.kind == VARIANT_PARAM_RATIO ? den : 0;
    return true;
}

/* ----------------------------------------------------------------- drawing */

/* A uniform whole number from min to max, never 0 when the range crosses it. */
static long draw_int(FillRng *rng, long min, long max) {
    if (max <= min) {
        return min;
    }
    if (min < 0 && max > 0) {
        long r = min + (long)fill_rng_below(rng, (uint64_t)(max - min));
        return r >= 0 ? r + 1 : r;
    }
    return min + (long)fill_rng_below(rng, (uint64_t)(max - min + 1));
}

/* The largest value drawn for an int parameter of a step applied to current. */
static long param_max(const VariantParam *param, const Motive *current) {
    long max = param->max;
    if (param->notes_limit && (long)current->note_count - 1 < max) {
        max = (long)current->note_count - 1;
    }
    if (param->features_limit && (long)current->feature_count - 1 < max) {
        max = (long)current->feature_count - 1;
    }
    if (param->chords_limit) {
        long chords = current->chord_count > 0 ? (long)current->chord_count : 1;
        max = chords - 1 < max ? chords - 1 : max;
    }
    return max;
}

/* Draws one open parameter for a step that is applied to current. */
static VariantValue draw_param(FillRng *rng, const VariantParam *param, const Motive *current) {
    VariantValue value = {true, 0, 0};
    switch (param->kind) {
    case VARIANT_PARAM_BOOL:
        value.value = (long)fill_rng_below(rng, 2);
        /* A tonal step needs a key; without one the draw is spent and the step is not tonal. */
        if (strcmp(param->name, "tonal") == 0 && current->key_source == MOTIVE_SKETCH_KEY_UNSPECIFIED) {
            value.value = 0;
        }
        break;
    case VARIANT_PARAM_PITCH:
        /* An axis is one of the motive's own pitches. */
        value.value = motive_pitch(current, fill_rng_below(rng, current->note_count));
        break;
    case VARIANT_PARAM_RATIO: {
        /* A factor in lowest terms from min:max to max:min, above 1 or, when the parameter shrinks, below it. */
        long nums[16];
        long dens[16];
        size_t pairs = 0;
        for (long a = param->min; a <= param->max && pairs < 16; a++) {
            for (long b = param->min; b < a && pairs < 16; b++) {
                long x = a;
                long y = b;
                while (y != 0) {
                    long t = x % y;
                    x = y;
                    y = t;
                }
                if (x == 1) {
                    nums[pairs] = a;
                    dens[pairs++] = b;
                }
            }
        }
        size_t pick = pairs == 0 ? 0 : (size_t)fill_rng_below(rng, pairs);
        long up = pairs == 0 ? 2 : nums[pick];
        long down = pairs == 0 ? 1 : dens[pick];
        value.value = param->shrinks ? down : up;
        value.den = param->shrinks ? up : down;
        break;
    }
    default: {
        long max = param_max(param, current);
        value.value = draw_int(rng, param->min, max < param->min ? param->min : max);
        break;
    }
    }
    return value;
}

/* ------------------------------------------------------------- applicable draws */

/* The most combinations of open parameters tried at once for one step. */
#define COMBINATIONS_MAX 512

/* A row that names a figure on a note: its open parameters are drawn from values the motive accepts. */
static bool names_figure(const VariantOperation *operation) {
    return operation->relation == MOTIVE_RELATION_ADD_NON_CHORD_TONE ||
           operation->relation == MOTIVE_RELATION_REMOVE_NON_CHORD_TONE ||
           operation->relation == MOTIVE_RELATION_CHANGE_NON_CHORD_TONE;
}

/* A row that changes the chords: its open parameters are drawn, where they can be, so that a note moves. */
static bool changes_chords(const VariantOperation *operation) {
    return (operation->relation >= MOTIVE_RELATION_HARMONY_INVERSION &&
            operation->relation <= MOTIVE_RELATION_PASSING_HARMONY) ||
           operation->relation == MOTIVE_RELATION_REHARMONIZE_FRAME;
}

/* Whether the step with these values changes at least one pitch of current (motive_relation_harmony_changes_notes). */
static bool moves_notes(const MotiveSketch *sketch, const VariantOperation *operation, const VariantValue *values) {
    MotiveRelationStep step;
    memset(&step, 0, sizeof(step));
    step.kind = (MotiveRelationKind)operation->relation;
    for (size_t p = 0; p < operation->param_count && p < MOTIVE_RELATION_MAX_PARAMS; p++) {
        step.value[p] = values[p].value;
        step.den[p] = values[p].den;
    }
    return motive_relation_harmony_changes_notes(sketch, &step);
}

/*
 * Draws the open parameters of a step from the combinations the operation accepts for current: every combination of the
 * open int and bool parameters is tried, and one of those that apply is chosen. A row that changes the chords chooses
 * among the combinations that move a note when there are any. False, resolved untouched in its open values, when none
 * applies or there are too many to try.
 */
static bool draw_accepted(FillRng *rng, const VariantOperation *operation, VariantStepRecord *resolved,
                          const Motive *current) {
    size_t open[VARIANT_PARAM_MAX];
    long span[VARIANT_PARAM_MAX];
    size_t open_count = 0;
    size_t total = 1;
    for (size_t p = 0; p < operation->param_count; p++) {
        VariantParam described = param_at(operation, p);
        if (resolved->values[p].set || described.optional) {
            continue;
        }
        const VariantParam *param = &described;
        if (param->kind != VARIANT_PARAM_INT && param->kind != VARIANT_PARAM_BOOL) {
            return false;
        }
        long max = param_max(param, current);
        long width = max < param->min ? 1 : max - param->min + 1;
        if (total * (size_t)width > COMBINATIONS_MAX) {
            return false;
        }
        open[open_count] = p;
        span[open_count++] = width;
        total *= (size_t)width;
    }
    size_t accepted[COMBINATIONS_MAX];
    size_t accepted_count = 0;
    size_t moving[COMBINATIONS_MAX];
    size_t moving_count = 0;
    bool prefer_moves = changes_chords(operation) && motive_sketch_validate(current);
    for (size_t combination = 0; combination < total; combination++) {
        VariantValue values[VARIANT_PARAM_MAX];
        memcpy(values, resolved->values, sizeof(values));
        size_t rest = combination;
        for (size_t k = 0; k < open_count; k++) {
            values[open[k]] = (VariantValue){true, param_at(operation, open[k]).min + (long)(rest % (size_t)span[k]), 0};
            rest /= (size_t)span[k];
        }
        Motive result;
        if (operation->apply(current, values, &result) == MOTIVE_DEV_OK) {
            accepted[accepted_count++] = combination;
            if (prefer_moves && moves_notes(current, operation, values)) {
                moving[moving_count++] = combination;
            }
        }
    }
    if (accepted_count == 0) {
        return false;
    }
    size_t rest = moving_count > 0 ? moving[fill_rng_below(rng, moving_count)] : accepted[fill_rng_below(rng, accepted_count)];
    for (size_t k = 0; k < open_count; k++) {
        resolved->values[open[k]] =
            (VariantValue){true, param_at(operation, open[k]).min + (long)(rest % (size_t)span[k]), 0};
        resolved->open[open[k]] = true;
        rest /= (size_t)span[k];
    }
    return true;
}

/* ------------------------------------------------------------------- forms */

/*
 * A contour motive is only its ranks, sounded as degree r + 1 in quarters. Inverting or cutting it leaves pitches that
 * are no longer those, so the result is read back from its own contour.
 */
static bool normalize_contour(const Motive *source, Motive *result) {
    MotiveSketch sketch;
    if (!motive_sketch_validate(result)) {
        return false;
    }
    sketch = *result;
    if (!motive_sketch_contour_normalize(&sketch, &sketch)) {
        return false;
    }
    /* Harmony, features and role are the sketch's; the meter, the key and the form are the source's. */
    return motive_overlay_sketch(&sketch, source, result);
}

/* ------------------------------------------------------------ kept, changed */

/* Whether a equals b times fnum/fden. */
static bool scaled_equal(Rational a, Rational b, long long fnum, long long fden) {
    return (long long)a.num * (long long)b.den * fden == (long long)b.num * (long long)a.den * fnum;
}

/* The result's times are the source's times one common factor; *scaled says whether that factor is not 1. */
static bool rhythm_kept(const Motive *source, const Motive *result, bool *scaled) {
    *scaled = false;
    if (source->note_count != result->note_count) {
        return false;
    }
    long long fnum = (long long)result->notes[0].duration.num * (long long)source->notes[0].duration.den;
    long long fden = (long long)result->notes[0].duration.den * (long long)source->notes[0].duration.num;
    for (size_t i = 0; i < source->note_count; i++) {
        if (!scaled_equal(result->notes[i].duration, source->notes[i].duration, fnum, fden) ||
            !scaled_equal(result->notes[i].onset, source->notes[i].onset, fnum, fden)) {
            return false;
        }
    }
    *scaled = fnum != fden;
    return true;
}

static bool intervals_kept(const MotiveAnalysis *a, const MotiveAnalysis *b) {
    if (a->interval_count != b->interval_count) {
        return false;
    }
    for (size_t i = 0; i < a->interval_count; i++) {
        bool same = a->intervals[i].has_steps && b->intervals[i].has_steps
                        ? a->intervals[i].steps == b->intervals[i].steps
                        : a->intervals[i].semitones == b->intervals[i].semitones;
        if (!same) {
            return false;
        }
    }
    return true;
}

static bool contour_kept(const MotiveAnalysis *a, const MotiveAnalysis *b) {
    if (a->interval_count != b->interval_count) {
        return false;
    }
    for (size_t i = 0; i < a->interval_count; i++) {
        if (a->directions[i] != b->directions[i]) {
            return false;
        }
    }
    return true;
}

static bool harmony_kept(const MotiveAnalysis *a, const MotiveAnalysis *b) {
    if (a->candidate_count == 0 || b->candidate_count == 0) {
        return a->candidate_count == b->candidate_count;
    }
    return strcmp(a->candidates[0].roman, b->candidates[0].roman) == 0;
}

/* Own harmony, when either motive has it: kept when every chord's degree, alteration, inversion, seventh and onset match. */
static bool own_harmony_kept(const Motive *a, const Motive *b) {
    if (a->chord_count != b->chord_count) {
        return false;
    }
    for (size_t i = 0; i < a->chord_count; i++) {
        if (a->chords[i].degree != b->chords[i].degree || a->chords[i].alteration != b->chords[i].alteration ||
            a->chords[i].inversion != b->chords[i].inversion || a->chords[i].seventh != b->chords[i].seventh ||
            rational_cmp(a->chords[i].onset, b->chords[i].onset) != 0) {
            return false;
        }
    }
    return true;
}

static void note_dimension(VariantRecord *record, const char *name, bool kept) {
    if (kept) {
        record->kept[record->kept_count++] = name;
    } else {
        record->changed[record->changed_count++] = name;
    }
}

/* Fills the record's kind, kept, changed and recognizability from the source and the finished variant. */
void variant_record_difference(const Motive *source, const Motive *result, VariantRecord *record) {
    MotiveAnalysis before;
    MotiveAnalysis after;
    record->kept_count = 0;
    record->changed_count = 0;
    if (motive_analyze(source, &before) && motive_analyze(result, &after)) {
        if (before.rhythm_known) {
            note_dimension(record, "rhythm", rhythm_kept(source, result, &record->rhythm_scaled));
        }
        if (before.intervals_known) {
            note_dimension(record, "intervals", intervals_kept(&before, &after));
        }
        if (before.contour_known) {
            note_dimension(record, "contour", contour_kept(&before, &after));
        }
        if (source->chord_count > 0 || result->chord_count > 0) {
            note_dimension(record, "harmony", own_harmony_kept(source, result));
        } else if (before.harmony_known && after.harmony_known) {
            note_dimension(record, "harmony", harmony_kept(&before, &after));
        }
    }
    record->kind = record->changed_count == 0 ? VARIANT_KIND_EXACT : VARIANT_KIND_MOTIVE_FORM;
    MotiveRecognizability score;
    if (motive_recognizability(source, result, NULL, &score)) {
        record->recognizability = score.total;
    }
}

/* ------------------------------------------------------------------- marks */

static size_t unresolved_count(const MotiveAnalysis *analysis) {
    size_t count = 0;
    for (size_t i = 0; analysis->tones_known && i < analysis->note_count; i++) {
        count += analysis->tones[i].unresolved ? 1 : 0;
    }
    return count;
}

bool variant_leaves_harmony(const Motive *source, const Motive *variant) {
    MotiveAnalysis after;
    MotiveAnalysis before;
    if (!variant || !motive_analyze(variant, &after) || !after.tones_known) {
        return false;
    }
    size_t strong = 0;
    size_t off = 0;
    for (size_t i = 0; i < after.note_count; i++) {
        if (after.rhythm[i].on_beat) {
            strong++;
            off += after.tones[i].role == MOTIVE_TONE_CHORD ? 0 : 1;
        }
    }
    size_t was = source && motive_analyze(source, &before) ? unresolved_count(&before) : 0;
    return off * 2 > strong || unresolved_count(&after) > was;
}

/* ---------------------------------------------------------------- derivation */

static void append(char *out, size_t size, const char *text) {
    size_t used = strlen(out);
    if (used < size) {
        snprintf(out + used, size - used, "%s", text);
    }
}

/* "a", "a and b", "a, b and c"; "nothing" for none. */
static void append_list(char *out, size_t size, const char *const *names, size_t count, bool rhythm_scaled) {
    if (count == 0) {
        append(out, size, "nothing");
        return;
    }
    for (size_t i = 0; i < count; i++) {
        append(out, size, i == 0 ? "" : (i + 1 == count ? " and " : ", "));
        append(out, size, "the ");
        append(out, size, names[i]);
        if (strcmp(names[i], "rhythm") == 0 && rhythm_scaled) {
            append(out, size, "'s proportions");
        }
    }
}

/* One sentence for the steps, then one for what was kept and changed. */
void variant_record_derivation(VariantRecord *record) {
    char *text = record->derivation;
    size_t size = sizeof(record->derivation);
    text[0] = '\0';
    for (size_t s = 0; s < record->step_count; s++) {
        const VariantOperation *operation = variant_registry_find(record->steps[s].op);
        char phrase[96] = "";
        if (operation) {
            operation->describe(record->steps[s].values, phrase, sizeof(phrase));
        }
        size_t start = strlen(text);
        append(text, size, s == 0 ? "" : ", then ");
        append(text, size, phrase);
        if (s == 0 && text[start] >= 'a' && text[start] <= 'z') {
            text[start] = (char)(text[start] - 'a' + 'A');
        }
    }
    append(text, size, ". Kept ");
    append_list(text, size, record->kept, record->kept_count, record->rhythm_scaled);
    append(text, size, "; changed ");
    append_list(text, size, record->changed, record->changed_count, false);
    append(text, size, ".");
}

/* -------------------------------------------------------------------- chain */

static void record_error(VariantRecord *record, size_t step, const char *format, ...) __attribute__((format(printf, 3, 4)));

static void record_error(VariantRecord *record, size_t step, const char *format, ...) {
    if (!record) {
        return;
    }
    record->failed_step = step;
    va_list args;
    va_start(args, format);
    vsnprintf(record->error, sizeof(record->error), format, args);
    va_end(args);
}

VariantResult variant_apply_chain(const Motive *source, const VariantChain *chain, unsigned long seed, Motive *out,
                                  VariantRecord *record) {
    if (!source || !chain || !out) {
        return VARIANT_ERR_NULL;
    }
    VariantRecord local;
    VariantRecord *log = record ? record : &local;
    memset(log, 0, sizeof(*log));
    log->seed = seed;
    if (chain->step_count == 0 || chain->step_count > VARIANT_CHAIN_MAX) {
        record_error(log, 0, "a chain has 1 to %d steps", VARIANT_CHAIN_MAX);
        return VARIANT_ERR_EMPTY;
    }

    FillRng rng;
    fill_rng_seed(&rng, seed);
    Motive current = *source;

    for (size_t s = 0; s < chain->step_count; s++) {
        const VariantStep *step = &chain->steps[s];
        const VariantOperation *operation = variant_registry_find(step->op);
        if (!operation) {
            record_error(log, s, "unknown operation '%s'", step->op);
            return VARIANT_ERR_UNKNOWN_OP;
        }
        if ((operation->forms & VARIANT_FORM_BIT(source->form)) == 0) {
            record_error(log, s, "%s does not apply to a %s motive", operation->name, motive_form_name(source->form));
            return VARIANT_ERR_FORM;
        }
        for (size_t p = 0; p < operation->param_count; p++) {
            VariantParam described = param_at(operation, p);
            if (step->values[p].set && !value_in_range(&described, step->values[p].value, step->values[p].den)) {
                record_error(log, s, "%s: %s is outside %ld to %ld", operation->name, described.name, described.min,
                             described.max);
                return VARIANT_ERR_PARAM;
            }
        }

        bool drawn = false;
        for (size_t p = 0; p < operation->param_count; p++) {
            drawn = drawn || (!step->values[p].set && !param_at(operation, p).optional);
        }
        size_t draws = drawn ? VARIANT_DRAWS : 1;

        VariantStepRecord *resolved = &log->steps[s];
        Motive next;
        MotiveDevResult status = MOTIVE_DEV_ERR_INVALID;
        for (size_t attempt = 0; attempt < draws; attempt++) {
            memset(resolved, 0, sizeof(*resolved));
            snprintf(resolved->op, sizeof(resolved->op), "%s", operation->name);
            resolved->param_count = operation->param_count;
            for (size_t p = 0; p < operation->param_count; p++) {
                if (step->values[p].set) {
                    resolved->values[p] = step->values[p];
                }
            }
            bool chosen = attempt == 0 && drawn && (names_figure(operation) || changes_chords(operation)) &&
                          draw_accepted(&rng, operation, resolved, &current);
            for (size_t p = 0; p < operation->param_count; p++) {
                VariantParam described = param_at(operation, p);
                if (!chosen && !step->values[p].set && !described.optional) {
                    resolved->values[p] = draw_param(&rng, &described, &current);
                    resolved->open[p] = true;
                }
            }
            status = operation->apply(&current, resolved->values, &next);
            if (status == MOTIVE_DEV_OK && source->form == MOTIVE_SKETCH_FORM_CONTOUR && !normalize_contour(source, &next)) {
                status = MOTIVE_DEV_ERR_INVALID;
            }
            if (status == MOTIVE_DEV_OK) {
                break;
            }
        }
        if (status != MOTIVE_DEV_OK) {
            if (status == MOTIVE_DEV_ERR_NO_FEATURE) {
                record_error(log, s, "%s needs a labelled feature", operation->name);
            } else if (status == MOTIVE_DEV_ERR_NO_ACCOMPANIMENT) {
                record_error(log, s, "%s needs an accompaniment motive", operation->name);
            } else {
                record_error(log, s, "%s: %s", operation->name, motive_dev_result_name(status));
            }
            return VARIANT_ERR_REFUSED;
        }
        log->step_count = s + 1;
        current = next;
    }

    variant_record_difference(source, &current, log);
    log->leaves_harmony = variant_leaves_harmony(source, &current);
    if (current.chord_count > 0) {
        char *harmony = motive_harmony_to_text(&current);
        if (harmony) {
            snprintf(log->harmony, sizeof(log->harmony), "%s", harmony);
            free(harmony);
        }
    }
    variant_record_derivation(log);
    *out = current;
    return VARIANT_OK;
}

/* Whether the operation needs two chords to work on: the source's own, or the one implied chord is too few. */
static bool needs_two_chords(const VariantOperation *operation) {
    return strcmp(operation->name, "passing-harmony") == 0;
}

/* Whether the operation works on a motive of the accompaniment role only. */
static bool needs_accompaniment(const VariantOperation *operation) {
    return strcmp(operation->name, "counter-line") == 0;
}

/* Whether the operation works on one of the motive's labelled features. */
static bool takes_feature(const VariantOperation *operation) {
    for (size_t p = 0; p < operation->param_count; p++) {
        if (param_at(operation, p).features_limit) {
            return true;
        }
    }
    return false;
}

/* Whether any note of the motive is a non-chord or unclassified tone against its base harmony. */
static bool has_figure(const Motive *motive) {
    MotiveAnalysis analysis;
    if (!motive_analyze(motive, &analysis) || !analysis.tones_known) {
        return false;
    }
    for (size_t i = 0; i < analysis.note_count; i++) {
        if (analysis.tones[i].role != MOTIVE_TONE_CHORD) {
            return true;
        }
    }
    return false;
}

/* Whether the operation works on a note that is not a chord tone: reduce-to-frame, remove and change. */
static bool needs_figure(const VariantOperation *operation) {
    return operation->relation == MOTIVE_RELATION_REDUCE_TO_FRAME ||
           operation->relation == MOTIVE_RELATION_REMOVE_NON_CHORD_TONE ||
           operation->relation == MOTIVE_RELATION_CHANGE_NON_CHORD_TONE;
}

bool variant_random_chain_of(const Motive *source, unsigned long seed, size_t min_steps, size_t max_steps,
                             VariantChain *out) {
    if (!source || !out || min_steps < 1 || max_steps < min_steps || max_steps > VARIANT_CHAIN_MAX) {
        return false;
    }
    const VariantOperation *candidates[VARIANT_CHAIN_MAX * 4 + 16];
    size_t count = 0;
    bool figure = has_figure(source);
    for (size_t i = 0; i < variant_registry_count() && count < sizeof(candidates) / sizeof(candidates[0]); i++) {
        const VariantOperation *operation = variant_registry_at(i);
        /* Change-meter is seldom usable (ch. III, Ex. 24), so no random chain draws it, and a feature operation needs
           a labelled feature to work on, counter-line the accompaniment role, passing-harmony two chords, and a row
           that takes out or changes a non-chord tone one to work on. */
        if (operation->category != VARIANT_CATEGORY_EXACT && strcmp(operation->name, "change-meter") != 0 &&
            (operation->forms & VARIANT_FORM_BIT(source->form)) != 0 &&
            (source->feature_count > 0 || !takes_feature(operation)) &&
            (source->accompaniment || !needs_accompaniment(operation)) &&
            (source->chord_count >= 2 || !needs_two_chords(operation)) && (figure || !needs_figure(operation))) {
            candidates[count++] = operation;
        }
    }
    memset(out, 0, sizeof(*out));
    if (count == 0) {
        return true;
    }
    FillRng rng;
    fill_rng_seed(&rng, seed);
    size_t steps = min_steps + (max_steps > min_steps ? (size_t)fill_rng_below(&rng, max_steps - min_steps + 1) : 0);
    if (steps > count) {
        steps = count;
    }
    for (size_t s = 0; s < steps; s++) {
        size_t pick = (size_t)fill_rng_below(&rng, count - s);
        variant_step_init(&out->steps[s], candidates[pick]->name);
        candidates[pick] = candidates[count - s - 1];
    }
    out->step_count = steps;
    return true;
}

bool variant_random_chain(const Motive *source, unsigned long seed, VariantChain *out) {
    return variant_random_chain_of(source, seed, 1, 2, out);
}

/* ------------------------------------------------------------------- JSON */

static cJSON *value_json(const VariantParam *param, const VariantValue *value) {
    switch (param->kind) {
    case VARIANT_PARAM_BOOL:
        return cJSON_CreateBool(value->value != 0);
    case VARIANT_PARAM_POSITION: {
        return cJSON_CreateString(plan_fragment_position_name(value->value));
    }
    case VARIANT_PARAM_RATIO: {
        char text[32];
        snprintf(text, sizeof(text), "%ld:%ld", value->value, value->den);
        return cJSON_CreateString(text);
    }
    default:
        return json_number(value->value);
    }
}

static cJSON *step_json(const VariantStepRecord *step) {
    const VariantOperation *operation = variant_registry_find(step->op);
    cJSON *object = cJSON_CreateObject();
    bool ok = object && operation && plan_json_add(object, "op", cJSON_CreateString(step->op));
    cJSON *params = ok ? cJSON_CreateObject() : NULL;
    ok = ok && plan_json_add(object, "params", params);
    cJSON *open = ok ? cJSON_CreateArray() : NULL;
    ok = ok && plan_json_add(object, "open", open);
    for (size_t p = 0; ok && p < step->param_count; p++) {
        if (step->values[p].set) {
            VariantParam described = param_at(operation, p);
            ok = plan_json_add(params, described.name, value_json(&described, &step->values[p]));
        }
        if (ok && step->open[p]) {
            ok = plan_json_append(open, cJSON_CreateString(param_at(operation, p).name));
        }
    }
    if (!ok) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

static bool names_json(cJSON *object, const char *key, const char *const *names, size_t count) {
    cJSON *array = cJSON_CreateArray();
    if (!plan_json_add(object, key, array)) {
        return false;
    }
    for (size_t i = 0; i < count; i++) {
        if (!plan_json_append(array, cJSON_CreateString(names[i]))) {
            return false;
        }
    }
    return true;
}

char *variant_record_to_json(const VariantRecord *record) {
    if (!record) {
        return NULL;
    }
    cJSON *document = cJSON_CreateObject();
    cJSON *chain = document ? cJSON_CreateObject() : NULL;
    bool ok = document && plan_json_add(document, "seed", json_number((long)record->seed)) && plan_json_add(document, "chain", chain);
    cJSON *steps = ok ? cJSON_CreateArray() : NULL;
    ok = ok && plan_json_add(chain, "steps", steps);
    for (size_t s = 0; ok && s < record->step_count; s++) {
        ok = plan_json_append(steps, step_json(&record->steps[s]));
    }
    double score = record->recognizability.denominator == 0
                       ? 0.0
                       : (double)record->recognizability.numerator / (double)record->recognizability.denominator;
    ok = ok && plan_json_add(document, "kind", cJSON_CreateString(variant_kind_name(record->kind))) &&
         names_json(document, "kept", record->kept, record->kept_count) &&
         names_json(document, "changed", record->changed, record->changed_count) &&
         (record->harmony[0] == '\0' || plan_json_add(document, "harmony", cJSON_CreateString(record->harmony))) &&
         names_json(document, "functions", record->functions, record->function_count) &&
         plan_json_add(document, "derivation", cJSON_CreateString(record->derivation)) &&
         plan_json_add(document, "recognizability", cJSON_CreateNumber((double)(long)(score * 10000.0 + 0.5) / 10000.0)) &&
         (record->remoteness_den == 0 ||
          plan_json_add(document, "remoteness",
                   cJSON_CreateNumber((double)(long)((double)record->remoteness_num / (double)record->remoteness_den *
                                                         10000.0 + 0.5) / 10000.0)));
    if (!ok) {
        cJSON_Delete(document);
        return NULL;
    }
    return motive_json_text(document);
}
