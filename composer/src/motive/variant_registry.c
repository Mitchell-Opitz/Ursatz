#include <ursatz_composer/variant_registry.h>

#include <stdio.h>
#include <string.h>

#include <ursatz_composer/motive_analysis.h>

#include "analysis/motive_relation.h"

#include "motive/motive_internal.h"
#include "plan/plan_json_internal.h"

/*
 * The registry table. Parameters are in the order the operation's apply and describe functions read them; the
 * constants below name the indexes. The ranges are what a chain draws open parameters from (variant_chain.c) and what
 * a value is checked against.
 */

const char *variant_category_name(VariantCategory category) {
    static const char *const NAMES[] = {"exact", "rhythm", "intervals", "harmony", "melody-adaptation"};
    return (size_t)category < sizeof(NAMES) / sizeof(NAMES[0]) ? NAMES[category] : "unknown";
}

const char *variant_param_kind_name(VariantParamKind kind) {
    static const char *const NAMES[] = {"int", "bool", "ratio", "pitch", "position"};
    return (size_t)kind < sizeof(NAMES) / sizeof(NAMES[0]) ? NAMES[kind] : "unknown";
}

/* ------------------------------------------------------------ value helpers */

static long value_or(const VariantValue *value, long fallback) {
    return value->set ? value->value : fallback;
}

static void spec_init(MotiveDevSpec *spec, MotiveDevKind kind) {
    memset(spec, 0, sizeof(*spec));
    spec->kind = kind;
}

static const char *position_name(long position) {
    static const char *const NAMES[] = {"any", "leading", "trailing", "middle"};
    return position >= 0 && position <= 3 ? NAMES[position] : "any";
}

/* ----------------------------------------------------------------- exact */

static MotiveDevResult apply_exact(const Motive *source, const VariantValue *values, Motive *out) {
    (void)values;
    MotiveDevSpec spec;
    spec_init(&spec, MOTIVE_DEV_EXACT);
    return motive_develop(source, &spec, out);
}

static void describe_exact(const VariantValue *values, char *out, size_t size) {
    (void)values;
    snprintf(out, size, "restated exactly");
}

/* ------------------------------------------------------------- transpose */

enum { TRANSPOSE_TONAL, TRANSPOSE_BY, TRANSPOSE_DEGREE };

static const VariantParam TRANSPOSE_PARAMS[] = {
    {"tonal", VARIANT_PARAM_BOOL, 0, 1, "move in steps of the key's scale instead of semitones", false, false, false},
    {"by", VARIANT_PARAM_INT, -12, 12, "how far to move: semitones, or steps when tonal; not 0", false, false, false},
    {"degree", VARIANT_PARAM_INT, 1, 7, "move the first note to this scale degree instead of using by; tonal", true,
     false, false}};

static MotiveDevResult apply_transpose(const Motive *source, const VariantValue *values, Motive *out) {
    MotiveDevSpec spec;
    spec_init(&spec, MOTIVE_DEV_TRANSPOSE);
    spec.tonal = value_or(&values[TRANSPOSE_TONAL], 0) != 0;
    spec.amount = (int)value_or(&values[TRANSPOSE_BY], 0);
    if (values[TRANSPOSE_DEGREE].set) {
        MotiveAnalysis analysis;
        if (source->key_source == MOTIVE_KEY_UNSPECIFIED || !motive_analyze(source, &analysis) ||
            !analysis.degrees_known) {
            return MOTIVE_DEV_ERR_NO_KEY;
        }
        /* The nearest way to the degree, up to three steps either way. */
        int steps = (int)((values[TRANSPOSE_DEGREE].value - analysis.degrees[0].degree + 7) % 7);
        spec.tonal = true;
        spec.amount = steps > 3 ? steps - 7 : steps;
    }
    return motive_develop(source, &spec, out);
}

static void describe_transpose(const VariantValue *values, char *out, size_t size) {
    if (values[TRANSPOSE_DEGREE].set) {
        snprintf(out, size, "moved so the first note sits on scale degree %ld", values[TRANSPOSE_DEGREE].value);
    } else if (value_or(&values[TRANSPOSE_TONAL], 0) != 0) {
        long by = value_or(&values[TRANSPOSE_BY], 0);
        snprintf(out, size, "transposed %+ld step%s of the key's scale", by, by == 1 || by == -1 ? "" : "s");
    } else {
        snprintf(out, size, "transposed %+ld semitones", value_or(&values[TRANSPOSE_BY], 0));
    }
}

/* ---------------------------------------------------------------- invert */

enum { INVERT_AXIS, INVERT_TONAL };

static const VariantParam INVERT_PARAMS[] = {
    {"axis", VARIANT_PARAM_PITCH, 0, 127, "the pitch mirrored about; the first note when not set", false, false, false},
    {"tonal", VARIANT_PARAM_BOOL, 0, 1, "mirror in steps of the key's scale instead of semitones", false, false, false}};

static MotiveDevResult apply_invert(const Motive *source, const VariantValue *values, Motive *out) {
    MotiveDevSpec spec;
    spec_init(&spec, MOTIVE_DEV_INVERT);
    spec.tonal = value_or(&values[INVERT_TONAL], 0) != 0;
    spec.has_axis = values[INVERT_AXIS].set;
    spec.axis = (int)values[INVERT_AXIS].value;
    return motive_develop(source, &spec, out);
}

static void describe_invert(const VariantValue *values, char *out, size_t size) {
    char axis[16] = "the first note";
    if (values[INVERT_AXIS].set) {
        motive_pitch_name((int)values[INVERT_AXIS].value, axis, sizeof(axis));
    }
    snprintf(out, size, "inverted about %s%s", axis,
             value_or(&values[INVERT_TONAL], 0) != 0 ? " in the key's scale" : "");
}

/* ------------------------------------------------------------ retrograde */

static MotiveDevResult apply_retrograde(const Motive *source, const VariantValue *values, Motive *out) {
    (void)values;
    MotiveDevSpec spec;
    spec_init(&spec, MOTIVE_DEV_RETROGRADE);
    return motive_develop(source, &spec, out);
}

static void describe_retrograde(const VariantValue *values, char *out, size_t size) {
    (void)values;
    snprintf(out, size, "put in retrograde");
}

/* ------------------------------------------------------- interval change */

enum { INTERVAL_WHICH, INTERVAL_AMOUNT, INTERVAL_KEEP_CONTOUR };

static const VariantParam INTERVAL_PARAMS[] = {
    {"interval", VARIANT_PARAM_INT, 0, 16, "the 1-based interval changed; 0 for every one", false, true, false},
    {"amount", VARIANT_PARAM_INT, -6, 6, "semitones to widen by, negative to narrow; not 0", false, false, false},
    {"keep-contour", VARIANT_PARAM_BOOL, 0, 1, "refuse a result that changes a direction", false, false, false}};

static MotiveDevResult apply_interval_change(const Motive *source, const VariantValue *values, Motive *out) {
    MotiveDevSpec spec;
    spec_init(&spec, MOTIVE_DEV_INTERVAL_CHANGE);
    spec.interval = (int)value_or(&values[INTERVAL_WHICH], 0);
    spec.amount = (int)value_or(&values[INTERVAL_AMOUNT], 0);
    spec.keep_contour = value_or(&values[INTERVAL_KEEP_CONTOUR], 0) != 0;
    return motive_develop(source, &spec, out);
}

static void describe_interval_change(const VariantValue *values, char *out, size_t size) {
    long which = value_or(&values[INTERVAL_WHICH], 0);
    long amount = value_or(&values[INTERVAL_AMOUNT], 0);
    char target[32] = "every interval";
    if (which != 0) {
        snprintf(target, sizeof(target), "interval %ld", which);
    }
    snprintf(out, size, "%s %s by %ld semitone%s%s", amount < 0 ? "narrowed" : "widened", target,
             amount < 0 ? -amount : amount, amount == 1 || amount == -1 ? "" : "s",
             value_or(&values[INTERVAL_KEEP_CONTOUR], 0) != 0 ? ", keeping every direction" : "");
}

/* -------------------------------------------------------------- fragment */

enum { FRAGMENT_POSITION, FRAGMENT_LENGTH, FRAGMENT_MIN };

static const VariantParam FRAGMENT_PARAMS[] = {
    {"position", VARIANT_PARAM_POSITION, 1, 3, "where the fragment is taken from: leading, trailing or middle", false,
     false, false},
    {"length", VARIANT_PARAM_INT, 1, 16, "how many notes the fragment has", false, true, false},
    {"min", VARIANT_PARAM_INT, 1, 16, "the fewest notes the result may have; 2 when not set", true, false, false}};

static MotiveDevResult apply_fragment(const Motive *source, const VariantValue *values, Motive *out) {
    MotiveDevSpec spec;
    spec_init(&spec, MOTIVE_DEV_FRAGMENT);
    spec.position = (PlanFragmentPosition)value_or(&values[FRAGMENT_POSITION], PLAN_FRAGMENT_LEADING);
    spec.length = (size_t)value_or(&values[FRAGMENT_LENGTH], 0);
    spec.min_length = (size_t)value_or(&values[FRAGMENT_MIN], 0);
    return motive_develop(source, &spec, out);
}

static void describe_fragment(const VariantValue *values, char *out, size_t size) {
    long length = value_or(&values[FRAGMENT_LENGTH], 0);
    snprintf(out, size, "cut to the %s %ld note%s", position_name(value_or(&values[FRAGMENT_POSITION], 0)), length,
             length == 1 ? "" : "s");
}

/* ------------------------------------------------------------- liquidate */

enum { LIQUIDATE_DROP, LIQUIDATE_KEEP_RHYTHM, LIQUIDATE_MIN };

static const VariantParam LIQUIDATE_PARAMS[] = {
    {"drop", VARIANT_PARAM_INT, 1, 16, "how many notes to drop, the weakest first", false, true, false},
    {"keep-rhythm", VARIANT_PARAM_BOOL, 0, 1, "leave a dropped note's time as a rest", false, false, false},
    {"min", VARIANT_PARAM_INT, 1, 16, "the fewest notes the result may have; 2 when not set", true, false, false}};

static MotiveDevResult apply_liquidate(const Motive *source, const VariantValue *values, Motive *out) {
    MotiveDevSpec spec;
    spec_init(&spec, MOTIVE_DEV_LIQUIDATE);
    spec.drop = (size_t)value_or(&values[LIQUIDATE_DROP], 0);
    spec.keep_rhythm = value_or(&values[LIQUIDATE_KEEP_RHYTHM], 0) != 0;
    spec.min_length = (size_t)value_or(&values[LIQUIDATE_MIN], 0);
    return motive_develop(source, &spec, out);
}

static void describe_liquidate(const VariantValue *values, char *out, size_t size) {
    long drop = value_or(&values[LIQUIDATE_DROP], 0);
    snprintf(out, size, "liquidated, dropping the %ld weakest note%s%s", drop, drop == 1 ? "" : "s",
             value_or(&values[LIQUIDATE_KEEP_RHYTHM], 0) != 0 ? " and keeping their time as rests" : "");
}

/* -------------------------------------------------------- augment, diminish */

static const VariantParam AUGMENT_PARAMS[] = {
    {"ratio", VARIANT_PARAM_RATIO, 1, 3, "the factor durations are multiplied by, n:m above 1; 2:1 when not set", false,
     false, false}};

static const VariantParam DIMINISH_PARAMS[] = {
    {"ratio", VARIANT_PARAM_RATIO, 1, 3, "the factor durations are multiplied by, n:m below 1; 1:2 when not set", false,
     false, true}};

static MotiveDevResult apply_ratio(MotiveDevKind kind, const Motive *source, const VariantValue *values, Motive *out) {
    MotiveDevSpec spec;
    spec_init(&spec, kind);
    if (values[0].set) {
        spec.ratio_num = (unsigned long)values[0].value;
        spec.ratio_den = (unsigned long)values[0].den;
    }
    return motive_develop(source, &spec, out);
}

static MotiveDevResult apply_augment(const Motive *source, const VariantValue *values, Motive *out) {
    return apply_ratio(MOTIVE_DEV_AUGMENT, source, values, out);
}

static MotiveDevResult apply_diminish(const Motive *source, const VariantValue *values, Motive *out) {
    return apply_ratio(MOTIVE_DEV_DIMINISH, source, values, out);
}

static void describe_ratio(const char *verb, long default_num, long default_den, const VariantValue *values, char *out,
                           size_t size) {
    snprintf(out, size, "%s %ld:%ld", verb, values[0].set ? values[0].value : default_num,
             values[0].set ? values[0].den : default_den);
}

static void describe_augment(const VariantValue *values, char *out, size_t size) {
    describe_ratio("augmented", 2, 1, values, out, size);
}

static void describe_diminish(const VariantValue *values, char *out, size_t size) {
    describe_ratio("diminished", 1, 2, values, out, size);
}

/* ----------------------------------------------------------------- table */

#define FORMS_ALL (VARIANT_FORM_BIT(MOTIVE_FORM_FULL) | VARIANT_FORM_BIT(MOTIVE_FORM_RHYTHM) | VARIANT_FORM_BIT(MOTIVE_FORM_CONTOUR))
#define FORMS_FULL VARIANT_FORM_BIT(MOTIVE_FORM_FULL)
#define FORMS_FULL_RHYTHM (VARIANT_FORM_BIT(MOTIVE_FORM_FULL) | VARIANT_FORM_BIT(MOTIVE_FORM_RHYTHM))
#define FORMS_FULL_CONTOUR (VARIANT_FORM_BIT(MOTIVE_FORM_FULL) | VARIANT_FORM_BIT(MOTIVE_FORM_CONTOUR))

#define PARAMS(array) array, sizeof(array) / sizeof((array)[0])

/*
 * One row per operation. The citations are the ones decided for the phase (R-133). A rhythm motive takes exact,
 * retrograde, fragment, liquidate, augment and diminish; a contour motive takes exact, invert, retrograde and
 * fragment. Harmony has no row until P14.
 */
static const VariantOperation OPERATIONS[] = {
    {"exact", "Exact", VARIANT_CATEGORY_EXACT, "Schoenberg ch. III, Ex. 14", NULL, 0, FORMS_ALL, apply_exact,
     describe_exact, MOTIVE_RELATION_EXACT},
    {"transpose", "Transpose", VARIANT_CATEGORY_MELODY_ADAPTATION, "ch. III, Exs. 14, 28", PARAMS(TRANSPOSE_PARAMS),
     FORMS_FULL, apply_transpose, describe_transpose, MOTIVE_RELATION_TRANSPOSE},
    {"invert", "Invert", VARIANT_CATEGORY_INTERVALS, "ch. III, Exs. 14, 19", PARAMS(INVERT_PARAMS), FORMS_FULL_CONTOUR,
     apply_invert, describe_invert, MOTIVE_RELATION_INVERT},
    {"retrograde", "Retrograde", VARIANT_CATEGORY_INTERVALS, "ch. III, Exs. 14, 19", NULL, 0, FORMS_ALL,
     apply_retrograde, describe_retrograde, MOTIVE_RELATION_RETROGRADE},
    {"interval-change", "Interval change", VARIANT_CATEGORY_INTERVALS, "ch. III, Ex. 21", PARAMS(INTERVAL_PARAMS),
     FORMS_FULL, apply_interval_change, describe_interval_change, MOTIVE_RELATION_INTERVAL_CHANGE},
    {"fragment", "Fragment", VARIANT_CATEGORY_INTERVALS, "ch. III, Ex. 21; ch. VIII, Ex. 52", PARAMS(FRAGMENT_PARAMS),
     FORMS_ALL, apply_fragment, describe_fragment, MOTIVE_RELATION_FRAGMENT},
    {"liquidate", "Liquidate", VARIANT_CATEGORY_INTERVALS, "ch. VIII, Ex. 52", PARAMS(LIQUIDATE_PARAMS),
     FORMS_FULL_RHYTHM, apply_liquidate, describe_liquidate, MOTIVE_RELATION_LIQUIDATE},
    {"augment", "Augment", VARIANT_CATEGORY_RHYTHM, "ch. III, Exs. 14, 17", PARAMS(AUGMENT_PARAMS), FORMS_FULL_RHYTHM,
     apply_augment, describe_augment, MOTIVE_RELATION_AUGMENT},
    {"diminish", "Diminish", VARIANT_CATEGORY_RHYTHM, "ch. III, Exs. 14, 17", PARAMS(DIMINISH_PARAMS), FORMS_FULL_RHYTHM,
     apply_diminish, describe_diminish, MOTIVE_RELATION_DIMINISH}};

size_t variant_registry_count(void) {
    return sizeof(OPERATIONS) / sizeof(OPERATIONS[0]);
}

const VariantOperation *variant_registry_at(size_t index) {
    return index < variant_registry_count() ? &OPERATIONS[index] : NULL;
}

const VariantOperation *variant_registry_find(const char *name) {
    if (!name) {
        return NULL;
    }
    for (size_t i = 0; i < variant_registry_count(); i++) {
        if (strcmp(OPERATIONS[i].name, name) == 0) {
            return &OPERATIONS[i];
        }
    }
    return NULL;
}

int variant_operation_param_index(const VariantOperation *operation, const char *name) {
    if (!operation || !name) {
        return -1;
    }
    for (size_t i = 0; i < operation->param_count; i++) {
        if (strcmp(operation->params[i].name, name) == 0) {
            return (int)i;
        }
    }
    return -1;
}

/* ------------------------------------------------------------------- JSON */

static cJSON *param_json(const VariantParam *param) {
    cJSON *object = cJSON_CreateObject();
    bool ok = object && json_add(object, "name", cJSON_CreateString(param->name)) &&
              json_add(object, "kind", cJSON_CreateString(variant_param_kind_name(param->kind))) &&
              json_add(object, "min", json_number(param->min)) && json_add(object, "max", json_number(param->max)) &&
              json_add(object, "description", cJSON_CreateString(param->description)) &&
              json_add(object, "optional", cJSON_CreateBool(param->optional));
    if (!ok) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* Adds an empty array under key and returns it; the object owns it. NULL on failure. */
static cJSON *add_array(cJSON *object, const char *key) {
    cJSON *array = cJSON_CreateArray();
    return json_add(object, key, array) ? array : NULL;
}

static cJSON *operation_json(const VariantOperation *operation) {
    cJSON *object = cJSON_CreateObject();
    bool ok = object && json_add(object, "name", cJSON_CreateString(operation->name)) &&
              json_add(object, "label", cJSON_CreateString(operation->label)) &&
              json_add(object, "category", cJSON_CreateString(variant_category_name(operation->category))) &&
              json_add(object, "citation", cJSON_CreateString(operation->citation)) &&
              json_add(object, "relation", cJSON_CreateString(motive_relation_name((MotiveRelationKind)operation->relation)));
    cJSON *params = ok ? add_array(object, "params") : NULL;
    cJSON *forms = ok ? add_array(object, "forms") : NULL;
    ok = ok && params && forms;
    for (size_t i = 0; ok && i < operation->param_count; i++) {
        ok = json_append(params, param_json(&operation->params[i]));
    }
    for (int form = MOTIVE_FORM_FULL; ok && form <= MOTIVE_FORM_CONTOUR; form++) {
        if ((operation->forms & VARIANT_FORM_BIT(form)) != 0) {
            ok = json_append(forms, cJSON_CreateString(motive_form_name((MotiveForm)form)));
        }
    }
    if (!ok) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

char *variant_registry_to_json(void) {
    cJSON *document = cJSON_CreateObject();
    cJSON *operations = document ? add_array(document, "operations") : NULL;
    bool ok = operations != NULL;
    for (size_t i = 0; ok && i < variant_registry_count(); i++) {
        ok = json_append(operations, operation_json(variant_registry_at(i)));
    }
    if (!ok) {
        cJSON_Delete(document);
        return NULL;
    }
    return motive_json_text(document);
}
