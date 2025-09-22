#include <ursatz_composer/variant_registry.h>

#include <stdio.h>
#include <string.h>

#include <ursatz_composer/motive_analysis.h>
#include <ursatz_composer/motive_sketch_bridge.h>

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
    {"tonal", VARIANT_PARAM_BOOL, 0, 1, "move in steps of the key's scale instead of semitones", false, false, false, false},
    {"by", VARIANT_PARAM_INT, -12, 12, "how far to move: semitones, or steps when tonal; not 0", false, false, false, false},
    {"degree", VARIANT_PARAM_INT, 1, 7, "move the first note to this scale degree instead of using by; tonal", true,
     false, false, false}};

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
    {"axis", VARIANT_PARAM_PITCH, 0, 127, "the pitch mirrored about; the first note when not set", false, false, false, false},
    {"tonal", VARIANT_PARAM_BOOL, 0, 1, "mirror in steps of the key's scale instead of semitones", false, false, false, false}};

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
    {"interval", VARIANT_PARAM_INT, 0, 16, "the 1-based interval changed; 0 for every one", false, true, false, false},
    {"amount", VARIANT_PARAM_INT, -6, 6, "semitones to widen by, negative to narrow; not 0", false, false, false, false},
    {"keep-contour", VARIANT_PARAM_BOOL, 0, 1, "refuse a result that changes a direction", false, false, false, false}};

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
     false, false, false},
    {"length", VARIANT_PARAM_INT, 1, 16, "how many notes the fragment has", false, true, false, false},
    {"min", VARIANT_PARAM_INT, 1, 16, "the fewest notes the result may have; 2 when not set", true, false, false, false}};

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
    {"drop", VARIANT_PARAM_INT, 1, 16, "how many notes to drop, the weakest first", false, true, false, false},
    {"keep-rhythm", VARIANT_PARAM_BOOL, 0, 1, "leave a dropped note's time as a rest", false, false, false, false},
    {"min", VARIANT_PARAM_INT, 1, 16, "the fewest notes the result may have; 2 when not set", true, false, false, false}};

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
     false, false, false}};

static const VariantParam DIMINISH_PARAMS[] = {
    {"ratio", VARIANT_PARAM_RATIO, 1, 3, "the factor durations are multiplied by, n:m below 1; 1:2 when not set", false,
     false, true, false}};

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

/* ------------------------------------------------------- library relations */

static MotiveDevResult relation_status(MotiveRelationResult result) {
    switch (result) {
    case MOTIVE_RELATION_OK:
        return MOTIVE_DEV_OK;
    case MOTIVE_RELATION_UNCHANGED:
        return MOTIVE_DEV_ERR_UNCHANGED;
    case MOTIVE_RELATION_RANGE:
        return MOTIVE_DEV_ERR_RANGE;
    case MOTIVE_RELATION_NOT_EXPRESSIBLE:
        return MOTIVE_DEV_ERR_NOT_EXPRESSIBLE;
    case MOTIVE_RELATION_NO_SCALE:
        return MOTIVE_DEV_ERR_NO_KEY;
    case MOTIVE_RELATION_INVALID:
        return MOTIVE_DEV_ERR_INVALID;
    default:
        return MOTIVE_DEV_ERR_NOT_APPLICABLE;
    }
}

/* The relations that change the harmony: the four harmony rows, passing-harmony and the dominant form. */
static bool changes_harmony(MotiveRelationKind kind) {
    return (kind >= MOTIVE_RELATION_HARMONY_INVERSION && kind <= MOTIVE_RELATION_PASSING_HARMONY) ||
           kind == MOTIVE_RELATION_DOMINANT_FORM;
}

/*
 * Applies the library relation of the same name to source: the motive as a sketch, the relation with the row's
 * parameters in the relation's order (a parameter not set is 0), and the result back as a motive in the source's meter
 * and key.
 */
static MotiveDevResult relation_apply(MotiveRelationKind kind, const Motive *source, const VariantValue *values,
                                      Motive *out) {
    MotiveSketch sketch;
    MotiveSketch result;
    if (!motive_to_sketch(source, &sketch)) {
        return MOTIVE_DEV_ERR_INVALID;
    }
    MotiveRelationStep step;
    memset(&step, 0, sizeof(step));
    step.kind = kind;
    size_t count = motive_relation_param_count(kind);
    for (size_t i = 0; i < count && i < MOTIVE_RELATION_MAX_PARAMS; i++) {
        if (values[i].set) {
            step.value[i] = values[i].value;
            step.den[i] = values[i].den;
        }
    }
    MotiveRelationResult applied = motive_relation_apply(&sketch, &step, &result);
    if (applied == MOTIVE_RELATION_NO_FEATURE && source->feature_count == 0) {
        return MOTIVE_DEV_ERR_NO_FEATURE;
    }
    MotiveDevResult status = relation_status(applied);
    if (status != MOTIVE_DEV_OK) {
        return status;
    }
    /* A harmony row's chords are the variant's own, even a single one that the new melody also implies, which the
       bridge would leave out of a template without harmony (a template that has some keeps every chord). */
    Motive template_motive = *source;
    if (changes_harmony(kind) && template_motive.harmony_count == 0) {
        memset(&template_motive.harmony[0], 0, sizeof(template_motive.harmony[0]));
        template_motive.harmony[0].degree = 1;
        template_motive.harmony_count = 1;
    }
    return motive_from_sketch(&result, &template_motive, out) ? MOTIVE_DEV_OK : MOTIVE_DEV_ERR_NOT_EXPRESSIBLE;
}

static const char *const ORDINALS[] = {"first", "second", "third",   "fourth", "fifth",
                                       "sixth", "seventh", "eighth", "ninth",  "tenth"};

/* "third" for the 0-based index 2; "note 12" style numbers are the caller's for indexes past the table. */
static const char *ordinal(long index) {
    return index >= 0 && index < (long)(sizeof(ORDINALS) / sizeof(ORDINALS[0])) ? ORDINALS[index] : NULL;
}

/* "the third note", or "note 12". */
static void note_name(long index, char *out, size_t size) {
    const char *word = ordinal(index);
    if (word) {
        snprintf(out, size, "the %s note", word);
    } else {
        snprintf(out, size, "note %ld", index + 1);
    }
}

/* A part of a whole note as plain words: "a quarter note", "3:8 of a whole note". */
static void whole_note_part(const VariantValue *value, char *out, size_t size) {
    if (value->value == 1 && value->den == 4) {
        snprintf(out, size, "a quarter note");
    } else if (value->value == 1 && value->den == 8) {
        snprintf(out, size, "an eighth note");
    } else if (value->value == 1 && value->den == 2) {
        snprintf(out, size, "a half note");
    } else {
        snprintf(out, size, "%ld:%ld of a whole note", value->value, value->den);
    }
}

/* ----------------------------------------------------------------- rhythm */

enum { NOTE_REPEAT_NOTE, NOTE_REPEAT_COUNT };

static const VariantParam NOTE_REPEAT_PARAMS[] = {
    {"note", VARIANT_PARAM_INT, 0, 16, "the 0-based note split into repeated notes", false, true, false, false},
    {"count", VARIANT_PARAM_INT, 2, 4, "how many equal notes it becomes", false, false, false, false}};

static MotiveDevResult apply_note_repeat(const Motive *source, const VariantValue *values, Motive *out) {
    return relation_apply(MOTIVE_RELATION_NOTE_REPEAT, source, values, out);
}

static void describe_note_repeat(const VariantValue *values, char *out, size_t size) {
    char name[24];
    note_name(value_or(&values[NOTE_REPEAT_NOTE], 0), name, sizeof(name));
    long count = value_or(&values[NOTE_REPEAT_COUNT], 2);
    char times[16] = "twice";
    if (count != 2) {
        snprintf(times, sizeof(times), "%ld times", count);
    }
    snprintf(out, size, "repeated %s %s", name, times);
}

enum { RHYTHM_REPEAT_FIRST, RHYTHM_REPEAT_LENGTH, RHYTHM_REPEAT_TARGET };

static const VariantParam RHYTHM_REPEAT_PARAMS[] = {
    {"first", VARIANT_PARAM_INT, 0, 16, "the 0-based first note whose durations are copied", false, true, false, false},
    {"length", VARIANT_PARAM_INT, 1, 16, "how many notes' durations are copied", false, true, false, false},
    {"target", VARIANT_PARAM_INT, 0, 16, "the 0-based note the copied durations start on", false, true, false, false}};

static MotiveDevResult apply_rhythm_repeat(const Motive *source, const VariantValue *values, Motive *out) {
    return relation_apply(MOTIVE_RELATION_RHYTHM_REPEAT, source, values, out);
}

static void describe_rhythm_repeat(const VariantValue *values, char *out, size_t size) {
    long first = value_or(&values[RHYTHM_REPEAT_FIRST], 0) + 1;
    long length = value_or(&values[RHYTHM_REPEAT_LENGTH], 1);
    long target = value_or(&values[RHYTHM_REPEAT_TARGET], 0) + 1;
    if (length == 1) {
        snprintf(out, size, "gave note %ld the rhythm of note %ld", target, first);
    } else {
        snprintf(out, size, "gave the notes from %ld the rhythm of notes %ld to %ld", target, first, first + length - 1);
    }
}

static const VariantParam RHYTHM_SHIFT_PARAMS[] = {
    {"offset", VARIANT_PARAM_RATIO, 1, 4, "how far later in the bar the motive starts, in whole notes, n:m below 1",
     false, false, true, false}};

static MotiveDevResult apply_rhythm_shift(const Motive *source, const VariantValue *values, Motive *out) {
    return relation_apply(MOTIVE_RELATION_RHYTHM_SHIFT, source, values, out);
}

static void describe_rhythm_shift(const VariantValue *values, char *out, size_t size) {
    VariantValue offset = values[0].set ? values[0] : (VariantValue){true, 1, 4};
    char part[32];
    whole_note_part(&offset, part, sizeof(part));
    snprintf(out, size, "shifted %s later in the bar", part);
}

enum { UPBEAT_COUNT, UPBEAT_APPROACH };

static const VariantParam ADD_UPBEAT_PARAMS[] = {
    {"count", VARIANT_PARAM_INT, 1, 3, "how many half-beat notes lead into the first note", false, false, false, false},
    {"approach", VARIANT_PARAM_INT, -1, 1, "1 stepping up from below the first note, -1 down from above", false, false,
     false, false}};

static MotiveDevResult apply_add_upbeat(const Motive *source, const VariantValue *values, Motive *out) {
    return relation_apply(MOTIVE_RELATION_ADD_UPBEAT, source, values, out);
}

static void describe_add_upbeat(const VariantValue *values, char *out, size_t size) {
    long count = value_or(&values[UPBEAT_COUNT], 1);
    snprintf(out, size, "added %ld upbeat note%s stepping in from %s", count, count == 1 ? "" : "s",
             value_or(&values[UPBEAT_APPROACH], 1) < 0 ? "above" : "below");
}

enum { CHANGE_METER_NUM, CHANGE_METER_DEN };

static const VariantParam CHANGE_METER_PARAMS[] = {
    {"num", VARIANT_PARAM_INT, 1, 16, "the new meter's beats per bar", false, false, false, false},
    {"den", VARIANT_PARAM_INT, 1, 16, "the new meter's beat unit: 1, 2, 4, 8 or 16", false, false, false, false}};

static MotiveDevResult apply_change_meter(const Motive *source, const VariantValue *values, Motive *out) {
    return relation_apply(MOTIVE_RELATION_CHANGE_METER, source, values, out);
}

static void describe_change_meter(const VariantValue *values, char *out, size_t size) {
    snprintf(out, size, "put into %ld/%ld", value_or(&values[CHANGE_METER_NUM], 4), value_or(&values[CHANGE_METER_DEN], 4));
}

/* -------------------------------------------------------------- intervals */

enum { ADD_INTERVAL_WHICH, ADD_INTERVAL_STEPS };

static const VariantParam ADD_INTERVAL_PARAMS[] = {
    {"interval", VARIANT_PARAM_INT, 1, 16, "the 1-based interval a note is added in", false, true, false, false},
    {"steps", VARIANT_PARAM_INT, -7, 7, "scale steps from the note before the interval; not 0", false, false, false, false}};

static MotiveDevResult apply_add_interval(const Motive *source, const VariantValue *values, Motive *out) {
    return relation_apply(MOTIVE_RELATION_ADD_INTERVAL, source, values, out);
}

static void describe_add_interval(const VariantValue *values, char *out, size_t size) {
    long steps = value_or(&values[ADD_INTERVAL_STEPS], 1);
    long count = steps < 0 ? -steps : steps;
    snprintf(out, size, "added a note %ld scale step%s %s inside interval %ld", count, count == 1 ? "" : "s",
             steps < 0 ? "down" : "up", value_or(&values[ADD_INTERVAL_WHICH], 1));
}

static const VariantParam OMIT_INTERVAL_PARAMS[] = {
    {"interval", VARIANT_PARAM_INT, 1, 16, "the 1-based interval whose second note goes", false, true, false, false}};

static MotiveDevResult apply_omit_interval(const Motive *source, const VariantValue *values, Motive *out) {
    return relation_apply(MOTIVE_RELATION_OMIT_INTERVAL, source, values, out);
}

static void describe_omit_interval(const VariantValue *values, char *out, size_t size) {
    snprintf(out, size, "left out the note after interval %ld", value_or(&values[0], 1));
}

enum { FILL_WHICH, FILL_KIND };

static const VariantParam ANCILLARY_FILL_PARAMS[] = {
    {"interval", VARIANT_PARAM_INT, 1, 16, "the 1-based interval that is filled", false, true, false, false},
    {"kind", VARIANT_PARAM_INT, 0, 1, "0 for passing notes, 1 for a neighbor note", false, false, false, false}};

static MotiveDevResult apply_ancillary_fill(const Motive *source, const VariantValue *values, Motive *out) {
    return relation_apply(MOTIVE_RELATION_ANCILLARY_FILL, source, values, out);
}

static void describe_ancillary_fill(const VariantValue *values, char *out, size_t size) {
    long which = value_or(&values[FILL_WHICH], 1);
    if (value_or(&values[FILL_KIND], 0) != 0) {
        snprintf(out, size, "added a neighbor note after note %ld", which);
    } else {
        snprintf(out, size, "filled the leap after note %ld with passing notes", which);
    }
}

enum { CONDENSE_FIRST, CONDENSE_LAST, CONDENSE_RATIO };

static const VariantParam CONDENSE_PARAMS[] = {
    {"first", VARIANT_PARAM_INT, 0, 16, "the 0-based first note compressed", false, true, false, false},
    {"last", VARIANT_PARAM_INT, 0, 16, "the 0-based last note compressed", false, true, false, false},
    {"ratio", VARIANT_PARAM_RATIO, 1, 3, "the factor their time is multiplied by, n:m below 1", false, false, true, false}};

static MotiveDevResult apply_condense(const Motive *source, const VariantValue *values, Motive *out) {
    return relation_apply(MOTIVE_RELATION_CONDENSE, source, values, out);
}

static void describe_condense(const VariantValue *values, char *out, size_t size) {
    long first = value_or(&values[CONDENSE_FIRST], 0) + 1;
    long last = value_or(&values[CONDENSE_LAST], 0) + 1;
    snprintf(out, size, "condensed notes %ld to %ld to %ld:%ld of their length", first, last,
             values[CONDENSE_RATIO].set ? values[CONDENSE_RATIO].value : 1,
             values[CONDENSE_RATIO].set ? values[CONDENSE_RATIO].den : 2);
}

enum { REPEAT_FEATURE_WHICH, REPEAT_FEATURE_TIMES, REPEAT_FEATURE_STEPS };

static const VariantParam REPEAT_FEATURE_PARAMS[] = {
    {"feature", VARIANT_PARAM_INT, 0, 7, "the 0-based labelled feature repeated", false, false, false, true},
    {"times", VARIANT_PARAM_INT, 1, 2, "how many times it is repeated after itself", false, false, false, false},
    {"steps", VARIANT_PARAM_INT, -7, 7, "scale steps each repeat is moved by", false, false, false, false}};

static MotiveDevResult apply_repeat_feature(const Motive *source, const VariantValue *values, Motive *out) {
    return relation_apply(MOTIVE_RELATION_REPEAT_FEATURE, source, values, out);
}

static void describe_repeat_feature(const VariantValue *values, char *out, size_t size) {
    long times = value_or(&values[REPEAT_FEATURE_TIMES], 1);
    long steps = value_or(&values[REPEAT_FEATURE_STEPS], 0);
    char moved[48] = "";
    if (steps != 0) {
        long count = steps < 0 ? -steps : steps;
        snprintf(moved, sizeof(moved), ", each %ld step%s %s", count, count == 1 ? "" : "s", steps < 0 ? "lower" : "higher");
    }
    snprintf(out, size, "repeated feature %ld %s%s", value_or(&values[REPEAT_FEATURE_WHICH], 0) + 1,
             times == 1 ? "once" : "twice", moved);
}

enum { SHIFT_FEATURE_WHICH, SHIFT_FEATURE_OFFSET };

static const VariantParam SHIFT_FEATURE_PARAMS[] = {
    {"feature", VARIANT_PARAM_INT, 0, 7, "the 0-based labelled feature moved later", false, false, false, true},
    {"offset", VARIANT_PARAM_RATIO, 1, 4, "how far later, in whole notes, n:m below 1", false, false, true, false}};

static MotiveDevResult apply_shift_feature(const Motive *source, const VariantValue *values, Motive *out) {
    return relation_apply(MOTIVE_RELATION_SHIFT_FEATURE, source, values, out);
}

static void describe_shift_feature(const VariantValue *values, char *out, size_t size) {
    VariantValue offset = values[SHIFT_FEATURE_OFFSET].set ? values[SHIFT_FEATURE_OFFSET] : (VariantValue){true, 1, 4};
    char part[32];
    whole_note_part(&offset, part, sizeof(part));
    snprintf(out, size, "shifted feature %ld %s later", value_or(&values[SHIFT_FEATURE_WHICH], 0) + 1, part);
}

/* ---------------------------------------------------------------- harmony */

static const char *const ROMAN[] = {"", "I", "II", "III", "IV", "V", "VI", "VII"};

static const char *roman_of(long degree) {
    return degree >= 1 && degree <= 7 ? ROMAN[degree] : "I";
}

/* "the first chord", or "chord 12". */
static void chord_name(long index, char *out, size_t size) {
    const char *word = ordinal(index);
    if (word) {
        snprintf(out, size, "the %s chord", word);
    } else {
        snprintf(out, size, "chord %ld", index + 1);
    }
}

enum { HARMONY_INVERSION_CHORD, HARMONY_INVERSION_INVERSION };

static const VariantParam HARMONY_INVERSION_PARAMS[] = {
    {"chord", VARIANT_PARAM_INT, 0, 7, "the 0-based chord that is inverted", false, false, false, false},
    {"inversion", VARIANT_PARAM_INT, 0, 3, "0 root position, 1 first, 2 second, 3 third (a seventh chord)", false, false,
     false, false}};

static MotiveDevResult apply_harmony_inversion(const Motive *source, const VariantValue *values, Motive *out) {
    return relation_apply(MOTIVE_RELATION_HARMONY_INVERSION, source, values, out);
}

static void describe_harmony_inversion(const VariantValue *values, char *out, size_t size) {
    static const char *const POSITIONS[] = {"root position", "first inversion", "second inversion", "third inversion"};
    char chord[24];
    chord_name(value_or(&values[HARMONY_INVERSION_CHORD], 0), chord, sizeof(chord));
    long inversion = value_or(&values[HARMONY_INVERSION_INVERSION], 1);
    snprintf(out, size, "put %s in %s, melody adapted", chord, POSITIONS[inversion >= 0 && inversion <= 3 ? inversion : 1]);
}

enum { HARMONY_APPEND_DEGREE, HARMONY_APPEND_SEVENTH };

static const VariantParam HARMONY_APPEND_PARAMS[] = {
    {"degree", VARIANT_PARAM_INT, 1, 7, "the scale degree of the chord added at the end", false, false, false, false},
    {"seventh", VARIANT_PARAM_BOOL, 0, 1, "make the added chord a seventh chord", false, false, false, false}};

static MotiveDevResult apply_harmony_append(const Motive *source, const VariantValue *values, Motive *out) {
    return relation_apply(MOTIVE_RELATION_HARMONY_APPEND, source, values, out);
}

static void describe_harmony_append(const VariantValue *values, char *out, size_t size) {
    snprintf(out, size, "added %s%s at the end, melody adapted", roman_of(value_or(&values[HARMONY_APPEND_DEGREE], 1)),
             value_or(&values[HARMONY_APPEND_SEVENTH], 0) != 0 ? "7" : "");
}

enum { HARMONY_INSERT_CHORD, HARMONY_INSERT_DEGREE };

static const VariantParam HARMONY_INSERT_PARAMS[] = {
    {"chord", VARIANT_PARAM_INT, 0, 7, "the 0-based chord the new one is inserted after", false, false, false, false},
    {"degree", VARIANT_PARAM_INT, 1, 7, "the scale degree of the inserted chord", false, false, false, false}};

static MotiveDevResult apply_harmony_insert(const Motive *source, const VariantValue *values, Motive *out) {
    return relation_apply(MOTIVE_RELATION_HARMONY_INSERT, source, values, out);
}

static void describe_harmony_insert(const VariantValue *values, char *out, size_t size) {
    char chord[24];
    chord_name(value_or(&values[HARMONY_INSERT_CHORD], 0), chord, sizeof(chord));
    snprintf(out, size, "inserted %s after %s, melody adapted", roman_of(value_or(&values[HARMONY_INSERT_DEGREE], 1)), chord);
}

enum { HARMONY_SUBSTITUTE_CHORD, HARMONY_SUBSTITUTE_DEGREE, HARMONY_SUBSTITUTE_THEN };

static const VariantParam HARMONY_SUBSTITUTE_PARAMS[] = {
    {"chord", VARIANT_PARAM_INT, 0, 7, "the 0-based chord that is replaced", false, false, false, false},
    {"degree", VARIANT_PARAM_INT, 1, 7, "the scale degree of the chord put in its place", false, false, false, false},
    {"then", VARIANT_PARAM_INT, 1, 7, "a second degree that follows it in the same time; not set for none", true, false,
     false, false}};

static MotiveDevResult apply_harmony_substitute(const Motive *source, const VariantValue *values, Motive *out) {
    return relation_apply(MOTIVE_RELATION_HARMONY_SUBSTITUTE, source, values, out);
}

static void describe_harmony_substitute(const VariantValue *values, char *out, size_t size) {
    char chord[24];
    chord_name(value_or(&values[HARMONY_SUBSTITUTE_CHORD], 0), chord, sizeof(chord));
    char second[24] = "";
    if (values[HARMONY_SUBSTITUTE_THEN].set) {
        snprintf(second, sizeof(second), " then %s", roman_of(values[HARMONY_SUBSTITUTE_THEN].value));
    }
    snprintf(out, size, "substituted %s%s for %s, melody adapted", roman_of(value_or(&values[HARMONY_SUBSTITUTE_DEGREE], 1)),
             second, chord);
}

/* ------------------------------------------------------ melody adaptation */

enum { PASSING_HARMONY_CHORD };

static const VariantParam PASSING_HARMONY_PARAMS[] = {
    {"chord", VARIANT_PARAM_INT, 0, 7, "the 0-based chord whose change into the next gets a passing chord", false, false,
     false, false}};

static MotiveDevResult apply_passing_harmony(const Motive *source, const VariantValue *values, Motive *out) {
    return relation_apply(MOTIVE_RELATION_PASSING_HARMONY, source, values, out);
}

static void describe_passing_harmony(const VariantValue *values, char *out, size_t size) {
    char chord[24];
    chord_name(value_or(&values[PASSING_HARMONY_CHORD], 0), chord, sizeof(chord));
    snprintf(out, size, "put a passing chord before the change from %s, melody adapted", chord);
}

static MotiveDevResult apply_counter_line(const Motive *source, const VariantValue *values, Motive *out) {
    if (source->role != MOTIVE_ROLE_ACCOMPANIMENT) {
        return MOTIVE_DEV_ERR_NO_ACCOMPANIMENT;
    }
    return relation_apply(MOTIVE_RELATION_COUNTER_LINE, source, values, out);
}

static void describe_counter_line(const VariantValue *values, char *out, size_t size) {
    (void)values;
    snprintf(out, size, "gave the repeated notes a counter-line moving within each chord");
}

/* ----------------------------------------------------------- dominant form */

static MotiveDevResult apply_dominant_form(const Motive *source, const VariantValue *values, Motive *out) {
    return relation_apply(MOTIVE_RELATION_DOMINANT_FORM, source, values, out);
}

static void describe_dominant_form(const VariantValue *values, char *out, size_t size) {
    (void)values;
    snprintf(out, size, "put in the dominant form, harmony and melody moved to the dominant");
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
 * fragment. Harmony rows take a full motive with a key.
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
     apply_diminish, describe_diminish, MOTIVE_RELATION_DIMINISH},
    {"note-repeat", "Note repeat", VARIANT_CATEGORY_RHYTHM, "ch. III, Exs. 17h, i, k, l, n", PARAMS(NOTE_REPEAT_PARAMS),
     FORMS_FULL_RHYTHM, apply_note_repeat, describe_note_repeat, MOTIVE_RELATION_NOTE_REPEAT},
    {"rhythm-repeat", "Rhythm repeat", VARIANT_CATEGORY_RHYTHM, "ch. III, Exs. 17l, m, 18e", PARAMS(RHYTHM_REPEAT_PARAMS),
     FORMS_FULL_RHYTHM, apply_rhythm_repeat, describe_rhythm_repeat, MOTIVE_RELATION_RHYTHM_REPEAT},
    {"rhythm-shift", "Rhythm shift", VARIANT_CATEGORY_RHYTHM, "ch. III, Ex. 23", PARAMS(RHYTHM_SHIFT_PARAMS),
     FORMS_FULL_RHYTHM, apply_rhythm_shift, describe_rhythm_shift, MOTIVE_RELATION_RHYTHM_SHIFT},
    {"add-upbeat", "Add upbeat", VARIANT_CATEGORY_RHYTHM, "ch. III, Ex. 22", PARAMS(ADD_UPBEAT_PARAMS),
     FORMS_FULL_RHYTHM, apply_add_upbeat, describe_add_upbeat, MOTIVE_RELATION_ADD_UPBEAT},
    {"change-meter", "Change meter", VARIANT_CATEGORY_RHYTHM, "ch. III, Ex. 24", PARAMS(CHANGE_METER_PARAMS),
     FORMS_FULL_RHYTHM, apply_change_meter, describe_change_meter, MOTIVE_RELATION_CHANGE_METER},
    {"add-interval", "Add interval", VARIANT_CATEGORY_INTERVALS, "ch. III, Ex. 21", PARAMS(ADD_INTERVAL_PARAMS),
     FORMS_FULL_CONTOUR, apply_add_interval, describe_add_interval, MOTIVE_RELATION_ADD_INTERVAL},
    {"omit-interval", "Omit interval", VARIANT_CATEGORY_INTERVALS, "ch. III, Ex. 21", PARAMS(OMIT_INTERVAL_PARAMS),
     FORMS_FULL_CONTOUR, apply_omit_interval, describe_omit_interval, MOTIVE_RELATION_OMIT_INTERVAL},
    {"ancillary-fill", "Ancillary fill", VARIANT_CATEGORY_INTERVALS, "ch. III, Exs. 18, 20", PARAMS(ANCILLARY_FILL_PARAMS),
     FORMS_FULL, apply_ancillary_fill, describe_ancillary_fill, MOTIVE_RELATION_ANCILLARY_FILL},
    {"condense", "Condense", VARIANT_CATEGORY_INTERVALS, "ch. III, Ex. 21; ch. VIII, Ex. 52", PARAMS(CONDENSE_PARAMS),
     FORMS_FULL_RHYTHM, apply_condense, describe_condense, MOTIVE_RELATION_CONDENSE},
    {"repeat-feature", "Repeat feature", VARIANT_CATEGORY_INTERVALS, "ch. III, Exs. 20h, 22a, b, d",
     PARAMS(REPEAT_FEATURE_PARAMS), FORMS_FULL_CONTOUR, apply_repeat_feature, describe_repeat_feature,
     MOTIVE_RELATION_REPEAT_FEATURE},
    {"shift-feature", "Shift feature", VARIANT_CATEGORY_INTERVALS, "ch. III, Ex. 23", PARAMS(SHIFT_FEATURE_PARAMS),
     FORMS_FULL, apply_shift_feature, describe_shift_feature, MOTIVE_RELATION_SHIFT_FEATURE},
    {"harmony-inversion", "Harmony inversion", VARIANT_CATEGORY_HARMONY, "ch. III, Exs. 25a, b",
     PARAMS(HARMONY_INVERSION_PARAMS), FORMS_FULL, apply_harmony_inversion, describe_harmony_inversion,
     MOTIVE_RELATION_HARMONY_INVERSION},
    {"harmony-append", "Harmony append", VARIANT_CATEGORY_HARMONY, "ch. III, Exs. 25c to i",
     PARAMS(HARMONY_APPEND_PARAMS), FORMS_FULL, apply_harmony_append, describe_harmony_append,
     MOTIVE_RELATION_HARMONY_APPEND},
    {"harmony-insert", "Harmony insert", VARIANT_CATEGORY_HARMONY, "ch. III, Ex. 26", PARAMS(HARMONY_INSERT_PARAMS),
     FORMS_FULL, apply_harmony_insert, describe_harmony_insert, MOTIVE_RELATION_HARMONY_INSERT},
    {"harmony-substitute", "Harmony substitute", VARIANT_CATEGORY_HARMONY, "ch. III, Exs. 27a to i",
     PARAMS(HARMONY_SUBSTITUTE_PARAMS), FORMS_FULL, apply_harmony_substitute, describe_harmony_substitute,
     MOTIVE_RELATION_HARMONY_SUBSTITUTE},
    {"passing-harmony", "Passing harmony", VARIANT_CATEGORY_MELODY_ADAPTATION, "ch. III, Ex. 29",
     PARAMS(PASSING_HARMONY_PARAMS), FORMS_FULL, apply_passing_harmony, describe_passing_harmony,
     MOTIVE_RELATION_PASSING_HARMONY},
    {"counter-line", "Counter line", VARIANT_CATEGORY_MELODY_ADAPTATION, "ch. III, Ex. 29", NULL, 0, FORMS_FULL,
     apply_counter_line, describe_counter_line, MOTIVE_RELATION_COUNTER_LINE},
    {"dominant-form", "Dominant form", VARIANT_CATEGORY_HARMONY, "ch. V, the dominant form", NULL, 0, FORMS_FULL,
     apply_dominant_form, describe_dominant_form, MOTIVE_RELATION_DOMINANT_FORM}};

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
