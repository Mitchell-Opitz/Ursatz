#include <ursatz_composer/variant_registry.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/fill_rng.h>

#include <ursatz_composer/motive_analysis.h>

#include "motive/motive_internal.h"

/* How many draws of its open parameters a refused step gets. */
#define VARIANT_DRAWS 16

const char *variant_kind_name(VariantKind kind) {
    return kind == VARIANT_KIND_EXACT ? "exact" : "motive-form";
}

const char *variant_result_name(VariantResult result) {
    static const char *const NAMES[] = {"ok",   "null",           "empty chain", "unknown operation",
                                        "form", "bad parameter", "refused"};
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

bool variant_step_set(VariantStep *step, const char *param, long value, long den) {
    const VariantOperation *operation = step ? variant_registry_find(step->op) : NULL;
    int index = variant_operation_param_index(operation, param);
    if (index < 0 || !value_in_range(&operation->params[index], value, den)) {
        return false;
    }
    step->values[index].set = true;
    step->values[index].value = value;
    step->values[index].den = operation->params[index].kind == VARIANT_PARAM_RATIO ? den : 0;
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

/* Draws one open parameter for a step that is applied to current. */
static VariantValue draw_param(FillRng *rng, const VariantParam *param, const Motive *current) {
    VariantValue value = {true, 0, 0};
    switch (param->kind) {
    case VARIANT_PARAM_BOOL:
        value.value = (long)fill_rng_below(rng, 2);
        /* A tonal step needs a key; without one the draw is spent and the step is not tonal. */
        if (strcmp(param->name, "tonal") == 0 && current->key_source == MOTIVE_KEY_UNSPECIFIED) {
            value.value = 0;
        }
        break;
    case VARIANT_PARAM_PITCH:
        /* An axis is one of the motive's own pitches. */
        value.value = current->notes[fill_rng_below(rng, current->note_count)].midi_pitch;
        break;
    case VARIANT_PARAM_RATIO:
        value.value = draw_int(rng, param->min, param->max);
        value.den = draw_int(rng, param->min, param->max);
        break;
    default: {
        long max = param->max;
        if (param->notes_limit && (long)current->note_count - 1 < max) {
            max = (long)current->note_count - 1;
        }
        value.value = draw_int(rng, param->min, max < param->min ? param->min : max);
        break;
    }
    }
    return value;
}

/* ------------------------------------------------------------------- forms */

/*
 * A contour motive is only its ranks, sounded as degree r + 1 in quarters. Inverting or cutting it leaves pitches that
 * are no longer those, so the result is read back from its own contour.
 */
static bool normalize_contour(const Motive *source, Motive *result) {
    char *text = motive_to_contour_text(result);
    if (!text) {
        return false;
    }
    MotiveOptions options;
    memset(&options, 0, sizeof(options));
    options.has_meter = true;
    options.meter = source->meter;
    if (source->key_source != MOTIVE_KEY_UNSPECIFIED) {
        options.has_key = true;
        options.key = source->key;
    }
    if (source->pickup.den != 0 && source->pickup.num != 0) {
        options.has_pickup = true;
        options.pickup = source->pickup;
    }
    Motive read;
    bool ok = motive_from_contour_text(text, &options, &read, NULL) == MOTIVE_OK;
    free(text);
    if (ok) {
        read.key_source = source->key_source;
        if (source->key_source != MOTIVE_KEY_UNSPECIFIED) {
            read.key = source->key;
        }
        *result = read;
    }
    return ok;
}

/* ------------------------------------------------------------ kept, changed */

/* Whether a equals b times fnum/fden. */
static bool scaled_equal(PlanTime a, PlanTime b, long long fnum, long long fden) {
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
            !scaled_equal(result->notes[i].offset, source->notes[i].offset, fnum, fden)) {
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

static void note_dimension(VariantRecord *record, const char *name, bool kept) {
    if (kept) {
        record->kept[record->kept_count++] = name;
    } else {
        record->changed[record->changed_count++] = name;
    }
}

/* Fills the record's kind, kept, changed and recognizability from the source and the finished variant. */
static void record_difference(const Motive *source, const Motive *result, VariantRecord *record) {
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
        if (before.harmony_known && after.harmony_known) {
            note_dimension(record, "harmony", harmony_kept(&before, &after));
        }
    }
    record->kind = record->changed_count == 0 ? VARIANT_KIND_EXACT : VARIANT_KIND_MOTIVE_FORM;
    MotiveRecognizability score;
    if (motive_recognizability(source, result, NULL, &score)) {
        record->recognizability = score.total;
    }
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
static void record_derivation(VariantRecord *record) {
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
            if (step->values[p].set && !value_in_range(&operation->params[p], step->values[p].value, step->values[p].den)) {
                record_error(log, s, "%s: %s is outside %ld to %ld", operation->name, operation->params[p].name,
                             operation->params[p].min, operation->params[p].max);
                return VARIANT_ERR_PARAM;
            }
        }

        bool drawn = false;
        for (size_t p = 0; p < operation->param_count; p++) {
            drawn = drawn || (!step->values[p].set && !operation->params[p].optional);
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
                } else if (!operation->params[p].optional) {
                    resolved->values[p] = draw_param(&rng, &operation->params[p], &current);
                    resolved->open[p] = true;
                }
            }
            status = operation->apply(&current, resolved->values, &next);
            if (status == MOTIVE_DEV_OK && source->form == MOTIVE_FORM_CONTOUR && !normalize_contour(source, &next)) {
                status = MOTIVE_DEV_ERR_INVALID;
            }
            if (status == MOTIVE_DEV_OK) {
                break;
            }
        }
        if (status != MOTIVE_DEV_OK) {
            record_error(log, s, "%s: %s", operation->name, motive_dev_result_name(status));
            return VARIANT_ERR_REFUSED;
        }
        log->step_count = s + 1;
        current = next;
    }

    record_difference(source, &current, log);
    record_derivation(log);
    *out = current;
    return VARIANT_OK;
}
