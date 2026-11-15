#include <ursatz_composer/variant_registry.h>

#include <stdio.h>
#include <string.h>

#include <ursatz_composer/motive_analysis.h>
#include <ursatz_composer/motive_tones.h>

#include <analysis/motive_relation.h>

#include "motive/motive_internal.h"
#include "plan/plan_json_internal.h"
#include "util/plan_util.h"

/*
 * The registry table. A row names its library relation (analysis/motive_relation.h); its parameters are the relation's,
 * in its order, and their name, kind and description are the library's (motive_relation_param_info). The row keeps only
 * what the relation does not say, one VariantDrawn for each parameter: the range a chain draws open values from and a
 * value is checked against (a composing choice: the relation accepts more), and how the draw treats the parameter. The
 * constants below name the indexes.
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

static void step_init(MotiveRelationStep *step, MotiveRelationKind kind) {
    memset(step, 0, sizeof(*step));
    step->kind = kind;
}

/* ----------------------------------------------------------------- exact */

static MotiveDevResult apply_exact(const Motive *source, const VariantValue *values, Motive *out) {
    (void)values;
    MotiveRelationStep step;
    step_init(&step, MOTIVE_RELATION_EXACT);
    return motive_develop(source, &step, out);
}

static void describe_exact(const VariantValue *values, char *out, size_t size) {
    (void)values;
    snprintf(out, size, "restated exactly");
}

/* ------------------------------------------------------------- transpose */

enum { TRANSPOSE_TONAL, TRANSPOSE_BY, TRANSPOSE_DEGREE };

static const VariantDrawn TRANSPOSE_DRAWN[] = {
    {.min = 0, .max = 1},
    {.min = -12, .max = 12},
    {.min = 1, .max = 7, .never_drawn = true}};

static MotiveDevResult apply_transpose(const Motive *source, const VariantValue *values, Motive *out) {
    MotiveRelationStep step;
    step_init(&step, MOTIVE_RELATION_TRANSPOSE);
    step.value[0] = value_or(&values[TRANSPOSE_TONAL], 0) != 0 ? 1 : 0;
    step.value[1] = value_or(&values[TRANSPOSE_BY], 0);
    if (values[TRANSPOSE_DEGREE].set) {
        MotiveAnalysis analysis;
        if (source->key_source == MOTIVE_SKETCH_KEY_UNSPECIFIED || !motive_analyze(source, &analysis) ||
            !analysis.degrees_known) {
            return MOTIVE_DEV_ERR_NO_KEY;
        }
        /* The nearest way to the degree, up to three steps either way. */
        int steps = (int)((values[TRANSPOSE_DEGREE].value - analysis.degrees[0].degree + 7) % 7);
        step.value[0] = 1;
        step.value[1] = steps > 3 ? steps - 7 : steps;
    }
    MotiveDevResult status = motive_develop(source, &step, out);
    if (status == MOTIVE_DEV_OK && step.value[0] != 0) {
        /* A tonal transposition moves the harmony with the melody: each chord's degree by the same scale steps, onsets
           kept. A real (chromatic) transposition keeps the source's chords. */
        for (size_t i = 0; i < out->chord_count; i++) {
            long degree = ((long)out->chords[i].degree - 1 + step.value[1]) % 7;
            out->chords[i].degree = (int)((degree + 7) % 7 + 1);
            out->chords[i].alteration = 0;
        }
    }
    return status;
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

static const VariantDrawn INVERT_DRAWN[] = {
    {.min = 0, .max = 127, .pitch = true},
    {.min = 0, .max = 1}};

static MotiveDevResult apply_invert(const Motive *source, const VariantValue *values, Motive *out) {
    MotiveRelationStep step;
    step_init(&step, MOTIVE_RELATION_INVERT);
    step.value[0] = values[INVERT_AXIS].set ? values[INVERT_AXIS].value : -1;
    step.value[1] = value_or(&values[INVERT_TONAL], 0) != 0 ? 1 : 0;
    return motive_develop(source, &step, out);
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
    MotiveRelationStep step;
    step_init(&step, MOTIVE_RELATION_RETROGRADE);
    return motive_develop(source, &step, out);
}

static void describe_retrograde(const VariantValue *values, char *out, size_t size) {
    (void)values;
    snprintf(out, size, "put in retrograde");
}

/* ------------------------------------------------------- interval change */

enum { INTERVAL_WHICH, INTERVAL_AMOUNT, INTERVAL_KEEP_CONTOUR };

static const VariantDrawn INTERVAL_DRAWN[] = {
    {.min = 0, .max = 16, .notes_limit = true},
    {.min = -6, .max = 6},
    {.min = 0, .max = 1}};

static MotiveDevResult apply_interval_change(const Motive *source, const VariantValue *values, Motive *out) {
    MotiveRelationStep step;
    step_init(&step, MOTIVE_RELATION_INTERVAL_CHANGE);
    step.value[0] = value_or(&values[INTERVAL_WHICH], 0);
    step.value[1] = value_or(&values[INTERVAL_AMOUNT], 0);
    step.value[2] = value_or(&values[INTERVAL_KEEP_CONTOUR], 0) != 0 ? 1 : 0;
    return motive_develop(source, &step, out);
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

static const VariantDrawn FRAGMENT_DRAWN[] = {
    {.min = 1, .max = 3, .position = true},
    {.min = 1, .max = 16, .notes_limit = true},
    {.min = 1, .max = 16, .never_drawn = true}};

static MotiveDevResult apply_fragment(const Motive *source, const VariantValue *values, Motive *out) {
    MotiveRelationStep step;
    step_init(&step, MOTIVE_RELATION_FRAGMENT);
    step.value[0] = value_or(&values[FRAGMENT_POSITION], MOTIVE_FRAGMENT_LEADING);
    step.value[1] = value_or(&values[FRAGMENT_LENGTH], 0);
    step.value[2] = value_or(&values[FRAGMENT_MIN], 0);
    return motive_develop(source, &step, out);
}

static void describe_fragment(const VariantValue *values, char *out, size_t size) {
    long length = value_or(&values[FRAGMENT_LENGTH], 0);
    snprintf(out, size, "cut to the %s %ld note%s", plan_fragment_position_name(value_or(&values[FRAGMENT_POSITION], 0)), length,
             length == 1 ? "" : "s");
}

/* ------------------------------------------------------------- liquidate */

enum { LIQUIDATE_DROP, LIQUIDATE_KEEP_RHYTHM, LIQUIDATE_MIN };

static const VariantDrawn LIQUIDATE_DRAWN[] = {
    {.min = 1, .max = 16, .notes_limit = true},
    {.min = 0, .max = 1},
    {.min = 1, .max = 16, .never_drawn = true}};

static MotiveDevResult apply_liquidate(const Motive *source, const VariantValue *values, Motive *out) {
    MotiveRelationStep step;
    step_init(&step, MOTIVE_RELATION_LIQUIDATE);
    step.value[0] = value_or(&values[LIQUIDATE_DROP], 0);
    step.value[1] = value_or(&values[LIQUIDATE_KEEP_RHYTHM], 0) != 0 ? 1 : 0;
    step.value[2] = value_or(&values[LIQUIDATE_MIN], 0);
    return motive_develop(source, &step, out);
}

static void describe_liquidate(const VariantValue *values, char *out, size_t size) {
    long drop = value_or(&values[LIQUIDATE_DROP], 0);
    snprintf(out, size, "liquidated, dropping the %ld weakest note%s%s", drop, drop == 1 ? "" : "s",
             value_or(&values[LIQUIDATE_KEEP_RHYTHM], 0) != 0 ? " and keeping their time as rests" : "");
}

/* -------------------------------------------------------- augment, diminish */

static const VariantDrawn AUGMENT_DRAWN[] = {
    {.min = 1, .max = 3}};

static const VariantDrawn DIMINISH_DRAWN[] = {
    {.min = 1, .max = 3, .shrinks = true}};

static MotiveDevResult apply_ratio(MotiveRelationKind kind, const Motive *source, const VariantValue *values,
                                   Motive *out) {
    MotiveRelationStep step;
    step_init(&step, kind);
    if (values[0].set) {
        step.value[0] = values[0].value;
        step.den[0] = values[0].den;
    }
    return motive_develop(source, &step, out);
}

static MotiveDevResult apply_augment(const Motive *source, const VariantValue *values, Motive *out) {
    return apply_ratio(MOTIVE_RELATION_AUGMENT, source, values, out);
}

static MotiveDevResult apply_diminish(const Motive *source, const VariantValue *values, Motive *out) {
    return apply_ratio(MOTIVE_RELATION_DIMINISH, source, values, out);
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

/* The relations that give the result chords: the four harmony rows, passing-harmony, the dominant form and liquidation by
   features, which keeps the source's chord under the thinned melody. */
static bool changes_harmony(MotiveRelationKind kind) {
    return (kind >= MOTIVE_RELATION_HARMONY_INVERSION && kind <= MOTIVE_RELATION_PASSING_HARMONY) ||
           kind == MOTIVE_RELATION_DOMINANT_FORM || kind == MOTIVE_RELATION_LIQUIDATE_FEATURES ||
           kind == MOTIVE_RELATION_REHARMONIZE_FRAME;
}

/*
 * Applies the library relation of the same name to source: the motive as a sketch, the relation with the row's
 * parameters in the relation's order (a parameter not set is 0), and the result back as a motive in the source's meter
 * and key.
 */
static MotiveDevResult relation_apply(MotiveRelationKind kind, const Motive *source, const VariantValue *values,
                                      Motive *out) {
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
    MotiveRelationResult applied;
    MotiveDevResult status = motive_relation_develop(source, &step, changes_harmony(kind), out, &applied);
    if (applied == MOTIVE_RELATION_NO_FEATURE && source->feature_count == 0) {
        return MOTIVE_DEV_ERR_NO_FEATURE;
    }
    return status;
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

static const VariantDrawn NOTE_REPEAT_DRAWN[] = {
    {.min = 0, .max = 16, .notes_limit = true},
    {.min = 2, .max = 4}};

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

static const VariantDrawn RHYTHM_REPEAT_DRAWN[] = {
    {.min = 0, .max = 16, .notes_limit = true},
    {.min = 1, .max = 16, .notes_limit = true},
    {.min = 0, .max = 16, .notes_limit = true}};

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

static const VariantDrawn RHYTHM_SHIFT_DRAWN[] = {
    {.min = 1, .max = 4, .shrinks = true}};

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

static const VariantDrawn ADD_UPBEAT_DRAWN[] = {
    {.min = 1, .max = 3},
    {.min = -1, .max = 1}};

static MotiveDevResult apply_add_upbeat(const Motive *source, const VariantValue *values, Motive *out) {
    return relation_apply(MOTIVE_RELATION_ADD_UPBEAT, source, values, out);
}

static void describe_add_upbeat(const VariantValue *values, char *out, size_t size) {
    long count = value_or(&values[UPBEAT_COUNT], 1);
    snprintf(out, size, "added %ld upbeat note%s stepping in from %s", count, count == 1 ? "" : "s",
             value_or(&values[UPBEAT_APPROACH], 1) < 0 ? "above" : "below");
}

enum { CHANGE_METER_NUM, CHANGE_METER_DEN };

static const VariantDrawn CHANGE_METER_DRAWN[] = {
    {.min = 1, .max = 16},
    {.min = 1, .max = 16}};

static MotiveDevResult apply_change_meter(const Motive *source, const VariantValue *values, Motive *out) {
    return relation_apply(MOTIVE_RELATION_CHANGE_METER, source, values, out);
}

static void describe_change_meter(const VariantValue *values, char *out, size_t size) {
    snprintf(out, size, "put into %ld/%ld", value_or(&values[CHANGE_METER_NUM], 4), value_or(&values[CHANGE_METER_DEN], 4));
}

/* -------------------------------------------------------------- intervals */

enum { ADD_INTERVAL_WHICH, ADD_INTERVAL_STEPS };

static const VariantDrawn ADD_INTERVAL_DRAWN[] = {
    {.min = 1, .max = 16, .notes_limit = true},
    {.min = -7, .max = 7}};

static MotiveDevResult apply_add_interval(const Motive *source, const VariantValue *values, Motive *out) {
    return relation_apply(MOTIVE_RELATION_ADD_INTERVAL, source, values, out);
}

static void describe_add_interval(const VariantValue *values, char *out, size_t size) {
    long steps = value_or(&values[ADD_INTERVAL_STEPS], 1);
    long count = steps < 0 ? -steps : steps;
    snprintf(out, size, "added a note %ld scale step%s %s inside interval %ld", count, count == 1 ? "" : "s",
             steps < 0 ? "down" : "up", value_or(&values[ADD_INTERVAL_WHICH], 1));
}

static const VariantDrawn OMIT_INTERVAL_DRAWN[] = {
    {.min = 1, .max = 16, .notes_limit = true}};

static MotiveDevResult apply_omit_interval(const Motive *source, const VariantValue *values, Motive *out) {
    return relation_apply(MOTIVE_RELATION_OMIT_INTERVAL, source, values, out);
}

static void describe_omit_interval(const VariantValue *values, char *out, size_t size) {
    snprintf(out, size, "left out the note after interval %ld", value_or(&values[0], 1));
}

enum { FILL_WHICH, FILL_KIND };

static const VariantDrawn ANCILLARY_FILL_DRAWN[] = {
    {.min = 1, .max = 16, .notes_limit = true},
    {.min = 0, .max = 1}};

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

static const VariantDrawn CONDENSE_DRAWN[] = {
    {.min = 0, .max = 16, .notes_limit = true},
    {.min = 0, .max = 16, .notes_limit = true},
    {.min = 1, .max = 3, .shrinks = true}};

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

static const VariantDrawn REPEAT_FEATURE_DRAWN[] = {
    {.min = 0, .max = 7, .features_limit = true},
    {.min = 1, .max = 2},
    {.min = -7, .max = 7}};

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

static const VariantDrawn SHIFT_FEATURE_DRAWN[] = {
    {.min = 0, .max = 7, .features_limit = true},
    {.min = 1, .max = 4, .shrinks = true}};

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

static const VariantDrawn HARMONY_INVERSION_DRAWN[] = {
    {.min = 0, .max = 7, .chords_limit = true},
    {.min = 0, .max = 3}};

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

static const VariantDrawn HARMONY_APPEND_DRAWN[] = {
    {.min = 1, .max = 7},
    {.min = 0, .max = 1}};

static MotiveDevResult apply_harmony_append(const Motive *source, const VariantValue *values, Motive *out) {
    return relation_apply(MOTIVE_RELATION_HARMONY_APPEND, source, values, out);
}

static void describe_harmony_append(const VariantValue *values, char *out, size_t size) {
    snprintf(out, size, "added %s%s at the end, melody adapted", roman_of(value_or(&values[HARMONY_APPEND_DEGREE], 1)),
             value_or(&values[HARMONY_APPEND_SEVENTH], 0) != 0 ? "7" : "");
}

enum { HARMONY_INSERT_CHORD, HARMONY_INSERT_DEGREE };

static const VariantDrawn HARMONY_INSERT_DRAWN[] = {
    {.min = 0, .max = 7, .chords_limit = true},
    {.min = 1, .max = 7}};

static MotiveDevResult apply_harmony_insert(const Motive *source, const VariantValue *values, Motive *out) {
    return relation_apply(MOTIVE_RELATION_HARMONY_INSERT, source, values, out);
}

static void describe_harmony_insert(const VariantValue *values, char *out, size_t size) {
    char chord[24];
    chord_name(value_or(&values[HARMONY_INSERT_CHORD], 0), chord, sizeof(chord));
    snprintf(out, size, "inserted %s after %s, melody adapted", roman_of(value_or(&values[HARMONY_INSERT_DEGREE], 1)), chord);
}

enum { HARMONY_SUBSTITUTE_CHORD, HARMONY_SUBSTITUTE_DEGREE, HARMONY_SUBSTITUTE_THEN };

static const VariantDrawn HARMONY_SUBSTITUTE_DRAWN[] = {
    {.min = 0, .max = 7, .chords_limit = true},
    {.min = 1, .max = 7},
    {.min = 1, .max = 7, .never_drawn = true}};

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

static const VariantDrawn PASSING_HARMONY_DRAWN[] = {
    {.min = 0, .max = 7, .chords_limit = true}};

static MotiveDevResult apply_passing_harmony(const Motive *source, const VariantValue *values, Motive *out) {
    return relation_apply(MOTIVE_RELATION_PASSING_HARMONY, source, values, out);
}

static void describe_passing_harmony(const VariantValue *values, char *out, size_t size) {
    char chord[24];
    chord_name(value_or(&values[PASSING_HARMONY_CHORD], 0), chord, sizeof(chord));
    snprintf(out, size, "put a passing chord before the change from %s, melody adapted", chord);
}

static MotiveDevResult apply_counter_line(const Motive *source, const VariantValue *values, Motive *out) {
    if (!source->accompaniment) {
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

/* ------------------------------------------------- liquidate by features */

enum { LIQUIDATE_FEATURES_STEPS, LIQUIDATE_FEATURES_SHORTEN };

static const VariantDrawn LIQUIDATE_FEATURES_DRAWN[] = {
    {.min = 1, .max = 3},
    {.min = 0, .max = 1}};

/* A full motive with a key only: the features give way to the chord tone nearest the first note, which a rhythm has not. */
static MotiveDevResult apply_liquidate_features(const Motive *source, const VariantValue *values, Motive *out) {
    return relation_apply(MOTIVE_RELATION_LIQUIDATE_FEATURES, source, values, out);
}

static void describe_liquidate_features(const VariantValue *values, char *out, size_t size) {
    long steps = value_or(&values[LIQUIDATE_FEATURES_STEPS], 1);
    snprintf(out, size, "liquidated %ld characteristic feature%s, each reduced to one chord tone%s", steps, steps == 1 ? "" : "s",
             value_or(&values[LIQUIDATE_FEATURES_SHORTEN], 0) != 0 ? " and shortened by half" : "");
}

/* ------------------------------------------------------ frame and tones */

/* The type a value names, as the motive reports it: "passing", "double-neighbor". */
static const char *type_phrase(long type) {
    const char *name = motive_tone_type_name((NonChordTone)type);
    return name ? name : "passing";
}

static MotiveDevResult apply_reduce_to_frame(const Motive *source, const VariantValue *values, Motive *out) {
    return relation_apply(MOTIVE_RELATION_REDUCE_TO_FRAME, source, values, out);
}

static void describe_reduce_to_frame(const VariantValue *values, char *out, size_t size) {
    (void)values;
    snprintf(out, size, "reduced to its frame of chord tones, every other note taken out");
}

enum { ADD_TONE_AFTER, ADD_TONE_TYPE };

static const VariantDrawn ADD_TONE_DRAWN[] = {
    {.min = 0, .max = 16, .notes_limit = true},
    {.min = 0, .max = 9}};

static MotiveDevResult apply_add_tone(const Motive *source, const VariantValue *values, Motive *out) {
    return relation_apply(MOTIVE_RELATION_ADD_NON_CHORD_TONE, source, values, out);
}

static void describe_add_tone(const VariantValue *values, char *out, size_t size) {
    char note[24];
    note_name(value_or(&values[ADD_TONE_AFTER], 0), note, sizeof(note));
    snprintf(out, size, "added a %s after %s", type_phrase(value_or(&values[ADD_TONE_TYPE], 0)), note);
}

enum { REMOVE_TONE_NOTE };

static const VariantDrawn REMOVE_TONE_DRAWN[] = {
    {.min = 0, .max = 16, .notes_limit = true}};

static MotiveDevResult apply_remove_tone(const Motive *source, const VariantValue *values, Motive *out) {
    return relation_apply(MOTIVE_RELATION_REMOVE_NON_CHORD_TONE, source, values, out);
}

static void describe_remove_tone(const VariantValue *values, char *out, size_t size) {
    char note[24];
    note_name(value_or(&values[REMOVE_TONE_NOTE], 0), note, sizeof(note));
    snprintf(out, size, "took out %s, a non-chord tone", note);
}

enum { CHANGE_TONE_NOTE, CHANGE_TONE_TYPE };

static const VariantDrawn CHANGE_TONE_DRAWN[] = {
    {.min = 0, .max = 16, .notes_limit = true},
    {.min = 0, .max = 9}};

static MotiveDevResult apply_change_tone(const Motive *source, const VariantValue *values, Motive *out) {
    return relation_apply(MOTIVE_RELATION_CHANGE_NON_CHORD_TONE, source, values, out);
}

static void describe_change_tone(const VariantValue *values, char *out, size_t size) {
    char note[24];
    note_name(value_or(&values[CHANGE_TONE_NOTE], 0), note, sizeof(note));
    snprintf(out, size, "made %s a %s", note, type_phrase(value_or(&values[CHANGE_TONE_TYPE], 0)));
}

enum { REHARMONIZE_DEGREE, REHARMONIZE_SEVENTH };

static const VariantDrawn REHARMONIZE_DRAWN[] = {
    {.min = 1, .max = 7},
    {.min = 0, .max = 1}};

static MotiveDevResult apply_reharmonize_frame(const Motive *source, const VariantValue *values, Motive *out) {
    return relation_apply(MOTIVE_RELATION_REHARMONIZE_FRAME, source, values, out);
}

static void describe_reharmonize_frame(const VariantValue *values, char *out, size_t size) {
    snprintf(out, size, "put %s%s under the whole motive, melody adapted", roman_of(value_or(&values[REHARMONIZE_DEGREE], 1)),
             value_or(&values[REHARMONIZE_SEVENTH], 0) != 0 ? "7" : "");
}

/* ----------------------------------------------------------------- table */

#define FORMS_ALL (VARIANT_FORM_BIT(MOTIVE_SKETCH_FORM_FULL) | VARIANT_FORM_BIT(MOTIVE_SKETCH_FORM_RHYTHM) | VARIANT_FORM_BIT(MOTIVE_SKETCH_FORM_CONTOUR))
#define FORMS_FULL VARIANT_FORM_BIT(MOTIVE_SKETCH_FORM_FULL)
#define FORMS_FULL_RHYTHM (VARIANT_FORM_BIT(MOTIVE_SKETCH_FORM_FULL) | VARIANT_FORM_BIT(MOTIVE_SKETCH_FORM_RHYTHM))
#define FORMS_FULL_CONTOUR (VARIANT_FORM_BIT(MOTIVE_SKETCH_FORM_FULL) | VARIANT_FORM_BIT(MOTIVE_SKETCH_FORM_CONTOUR))

#define DRAWN(array) array, sizeof(array) / sizeof((array)[0])

/*
 * One row per operation. The citations are the ones decided for the phase (R-133). A rhythm motive takes exact,
 * retrograde, fragment, liquidate, augment and diminish; a contour motive takes exact, invert, retrograde and
 * fragment. Harmony rows take a full motive with a key.
 */
static const VariantOperation OPERATIONS[] = {
    {"exact", "Exact", VARIANT_CATEGORY_EXACT, "Schoenberg ch. III, Ex. 14", NULL, 0, FORMS_ALL, apply_exact,
     describe_exact, MOTIVE_RELATION_EXACT},
    {"transpose", "Transpose", VARIANT_CATEGORY_MELODY_ADAPTATION, "ch. III, Exs. 14, 28", DRAWN(TRANSPOSE_DRAWN),
     FORMS_FULL, apply_transpose, describe_transpose, MOTIVE_RELATION_TRANSPOSE},
    {"invert", "Invert", VARIANT_CATEGORY_INTERVALS, "ch. III, Exs. 14, 19", DRAWN(INVERT_DRAWN), FORMS_FULL_CONTOUR,
     apply_invert, describe_invert, MOTIVE_RELATION_INVERT},
    {"retrograde", "Retrograde", VARIANT_CATEGORY_INTERVALS, "ch. III, Exs. 14, 19", NULL, 0, FORMS_ALL,
     apply_retrograde, describe_retrograde, MOTIVE_RELATION_RETROGRADE},
    {"interval-change", "Interval change", VARIANT_CATEGORY_INTERVALS, "ch. III, Ex. 21", DRAWN(INTERVAL_DRAWN),
     FORMS_FULL, apply_interval_change, describe_interval_change, MOTIVE_RELATION_INTERVAL_CHANGE},
    {"fragment", "Fragment", VARIANT_CATEGORY_INTERVALS, "ch. III, Ex. 21; ch. VIII, Ex. 52", DRAWN(FRAGMENT_DRAWN),
     FORMS_ALL, apply_fragment, describe_fragment, MOTIVE_RELATION_FRAGMENT},
    {"liquidate", "Liquidate", VARIANT_CATEGORY_INTERVALS, "ch. VIII, Ex. 52", DRAWN(LIQUIDATE_DRAWN),
     FORMS_FULL_RHYTHM, apply_liquidate, describe_liquidate, MOTIVE_RELATION_LIQUIDATE},
    {"augment", "Augment", VARIANT_CATEGORY_RHYTHM, "ch. III, Exs. 14, 17", DRAWN(AUGMENT_DRAWN), FORMS_FULL_RHYTHM,
     apply_augment, describe_augment, MOTIVE_RELATION_AUGMENT},
    {"diminish", "Diminish", VARIANT_CATEGORY_RHYTHM, "ch. III, Exs. 14, 17", DRAWN(DIMINISH_DRAWN), FORMS_FULL_RHYTHM,
     apply_diminish, describe_diminish, MOTIVE_RELATION_DIMINISH},
    {"note-repeat", "Note repeat", VARIANT_CATEGORY_RHYTHM, "ch. III, Exs. 17h, i, k, l, n", DRAWN(NOTE_REPEAT_DRAWN),
     FORMS_FULL_RHYTHM, apply_note_repeat, describe_note_repeat, MOTIVE_RELATION_NOTE_REPEAT},
    {"rhythm-repeat", "Rhythm repeat", VARIANT_CATEGORY_RHYTHM, "ch. III, Exs. 17l, m, 18e", DRAWN(RHYTHM_REPEAT_DRAWN),
     FORMS_FULL_RHYTHM, apply_rhythm_repeat, describe_rhythm_repeat, MOTIVE_RELATION_RHYTHM_REPEAT},
    {"rhythm-shift", "Rhythm shift", VARIANT_CATEGORY_RHYTHM, "ch. III, Ex. 23", DRAWN(RHYTHM_SHIFT_DRAWN),
     FORMS_FULL_RHYTHM, apply_rhythm_shift, describe_rhythm_shift, MOTIVE_RELATION_RHYTHM_SHIFT},
    {"add-upbeat", "Add upbeat", VARIANT_CATEGORY_RHYTHM, "ch. III, Ex. 22", DRAWN(ADD_UPBEAT_DRAWN),
     FORMS_FULL_RHYTHM, apply_add_upbeat, describe_add_upbeat, MOTIVE_RELATION_ADD_UPBEAT},
    {"change-meter", "Change meter", VARIANT_CATEGORY_RHYTHM, "ch. III, Ex. 24", DRAWN(CHANGE_METER_DRAWN),
     FORMS_FULL_RHYTHM, apply_change_meter, describe_change_meter, MOTIVE_RELATION_CHANGE_METER},
    {"add-interval", "Add interval", VARIANT_CATEGORY_INTERVALS, "ch. III, Ex. 21", DRAWN(ADD_INTERVAL_DRAWN),
     FORMS_FULL_CONTOUR, apply_add_interval, describe_add_interval, MOTIVE_RELATION_ADD_INTERVAL},
    {"omit-interval", "Omit interval", VARIANT_CATEGORY_INTERVALS, "ch. III, Ex. 21", DRAWN(OMIT_INTERVAL_DRAWN),
     FORMS_FULL_CONTOUR, apply_omit_interval, describe_omit_interval, MOTIVE_RELATION_OMIT_INTERVAL},
    {"ancillary-fill", "Ancillary fill", VARIANT_CATEGORY_INTERVALS, "ch. III, Exs. 18, 20", DRAWN(ANCILLARY_FILL_DRAWN),
     FORMS_FULL, apply_ancillary_fill, describe_ancillary_fill, MOTIVE_RELATION_ANCILLARY_FILL},
    {"condense", "Condense", VARIANT_CATEGORY_INTERVALS, "ch. III, Ex. 21; ch. VIII, Ex. 52", DRAWN(CONDENSE_DRAWN),
     FORMS_FULL_RHYTHM, apply_condense, describe_condense, MOTIVE_RELATION_CONDENSE},
    {"repeat-feature", "Repeat feature", VARIANT_CATEGORY_INTERVALS, "ch. III, Exs. 20h, 22a, b, d",
     DRAWN(REPEAT_FEATURE_DRAWN), FORMS_FULL_CONTOUR, apply_repeat_feature, describe_repeat_feature,
     MOTIVE_RELATION_REPEAT_FEATURE},
    {"shift-feature", "Shift feature", VARIANT_CATEGORY_INTERVALS, "ch. III, Ex. 23", DRAWN(SHIFT_FEATURE_DRAWN),
     FORMS_FULL, apply_shift_feature, describe_shift_feature, MOTIVE_RELATION_SHIFT_FEATURE},
    {"harmony-inversion", "Harmony inversion", VARIANT_CATEGORY_HARMONY, "ch. III, Exs. 25a, b",
     DRAWN(HARMONY_INVERSION_DRAWN), FORMS_FULL, apply_harmony_inversion, describe_harmony_inversion,
     MOTIVE_RELATION_HARMONY_INVERSION},
    {"harmony-append", "Harmony append", VARIANT_CATEGORY_HARMONY, "ch. III, Exs. 25c to i",
     DRAWN(HARMONY_APPEND_DRAWN), FORMS_FULL, apply_harmony_append, describe_harmony_append,
     MOTIVE_RELATION_HARMONY_APPEND},
    {"harmony-insert", "Harmony insert", VARIANT_CATEGORY_HARMONY, "ch. III, Ex. 26", DRAWN(HARMONY_INSERT_DRAWN),
     FORMS_FULL, apply_harmony_insert, describe_harmony_insert, MOTIVE_RELATION_HARMONY_INSERT},
    {"harmony-substitute", "Harmony substitute", VARIANT_CATEGORY_HARMONY, "ch. III, Exs. 27a to i",
     DRAWN(HARMONY_SUBSTITUTE_DRAWN), FORMS_FULL, apply_harmony_substitute, describe_harmony_substitute,
     MOTIVE_RELATION_HARMONY_SUBSTITUTE},
    {"passing-harmony", "Passing harmony", VARIANT_CATEGORY_MELODY_ADAPTATION, "ch. III, Ex. 29",
     DRAWN(PASSING_HARMONY_DRAWN), FORMS_FULL, apply_passing_harmony, describe_passing_harmony,
     MOTIVE_RELATION_PASSING_HARMONY},
    {"counter-line", "Counter line", VARIANT_CATEGORY_MELODY_ADAPTATION, "ch. III, Ex. 29", NULL, 0, FORMS_FULL,
     apply_counter_line, describe_counter_line, MOTIVE_RELATION_COUNTER_LINE},
    {"dominant-form", "Dominant form", VARIANT_CATEGORY_HARMONY, "ch. V, the dominant form", NULL, 0, FORMS_FULL,
     apply_dominant_form, describe_dominant_form, MOTIVE_RELATION_DOMINANT_FORM},
    {"liquidate-features", "Liquidate features", VARIANT_CATEGORY_INTERVALS, "ch. VIII, Ex. 52",
     DRAWN(LIQUIDATE_FEATURES_DRAWN), FORMS_FULL, apply_liquidate_features, describe_liquidate_features,
     MOTIVE_RELATION_LIQUIDATE_FEATURES},
    {"reduce-to-frame", "Reduce to frame", VARIANT_CATEGORY_MELODY_ADAPTATION, "ch. II and III", NULL, 0, FORMS_FULL,
     apply_reduce_to_frame, describe_reduce_to_frame, MOTIVE_RELATION_REDUCE_TO_FRAME},
    {"add-non-chord-tone", "Add non-chord tone", VARIANT_CATEGORY_MELODY_ADAPTATION, "ch. II and III",
     DRAWN(ADD_TONE_DRAWN), FORMS_FULL, apply_add_tone, describe_add_tone, MOTIVE_RELATION_ADD_NON_CHORD_TONE},
    {"remove-non-chord-tone", "Remove non-chord tone", VARIANT_CATEGORY_MELODY_ADAPTATION, "ch. II and III",
     DRAWN(REMOVE_TONE_DRAWN), FORMS_FULL, apply_remove_tone, describe_remove_tone,
     MOTIVE_RELATION_REMOVE_NON_CHORD_TONE},
    {"change-non-chord-tone", "Change non-chord tone", VARIANT_CATEGORY_MELODY_ADAPTATION, "ch. II and III",
     DRAWN(CHANGE_TONE_DRAWN), FORMS_FULL, apply_change_tone, describe_change_tone,
     MOTIVE_RELATION_CHANGE_NON_CHORD_TONE},
    {"reharmonize-frame", "Reharmonize frame", VARIANT_CATEGORY_MELODY_ADAPTATION, "ch. II and III",
     DRAWN(REHARMONIZE_DRAWN), FORMS_FULL, apply_reharmonize_frame, describe_reharmonize_frame,
     MOTIVE_RELATION_REHARMONIZE_FRAME}};

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

static VariantParamKind param_kind(const MotiveRelationParamInfo *info, const VariantDrawn *drawn) {
    switch (info->kind) {
    case MOTIVE_RELATION_PARAM_BOOL:
        return VARIANT_PARAM_BOOL;
    case MOTIVE_RELATION_PARAM_RATIO:
        return VARIANT_PARAM_RATIO;
    case MOTIVE_RELATION_PARAM_CHOICE:
        return drawn->position ? VARIANT_PARAM_POSITION : VARIANT_PARAM_INT;
    default:
        return drawn->pitch ? VARIANT_PARAM_PITCH : VARIANT_PARAM_INT;
    }
}

bool variant_operation_param(const VariantOperation *operation, size_t index, VariantParam *out) {
    MotiveRelationParamInfo info;
    if (!operation || !out || index >= operation->param_count ||
        !motive_relation_param_info((MotiveRelationKind)operation->relation, index, &info)) {
        return false;
    }
    const VariantDrawn *drawn = &operation->drawn[index];
    *out = (VariantParam){info.name,          param_kind(&info, drawn), drawn->min,         drawn->max,
                          info.description,   drawn->never_drawn,       drawn->notes_limit, drawn->shrinks,
                          drawn->features_limit, drawn->chords_limit};
    return true;
}

int variant_operation_param_index(const VariantOperation *operation, const char *name) {
    if (!operation || !name) {
        return -1;
    }
    for (size_t i = 0; i < operation->param_count; i++) {
        VariantParam param;
        if (variant_operation_param(operation, i, &param) && strcmp(param.name, name) == 0) {
            return (int)i;
        }
    }
    return -1;
}

/* ------------------------------------------------------------------- JSON */

static cJSON *param_json(const VariantParam *param) {
    cJSON *object = cJSON_CreateObject();
    bool ok = object && plan_json_add(object, "name", cJSON_CreateString(param->name)) &&
              plan_json_add(object, "kind", cJSON_CreateString(variant_param_kind_name(param->kind))) &&
              plan_json_add(object, "min", json_number(param->min)) && plan_json_add(object, "max", json_number(param->max)) &&
              plan_json_add(object, "description", cJSON_CreateString(param->description)) &&
              plan_json_add(object, "optional", cJSON_CreateBool(param->optional));
    if (!ok) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* Adds an empty array under key and returns it; the object owns it. NULL on failure. */
static cJSON *add_array(cJSON *object, const char *key) {
    cJSON *array = cJSON_CreateArray();
    return plan_json_add(object, key, array) ? array : NULL;
}

static cJSON *operation_json(const VariantOperation *operation) {
    cJSON *object = cJSON_CreateObject();
    bool ok = object && plan_json_add(object, "name", cJSON_CreateString(operation->name)) &&
              plan_json_add(object, "label", cJSON_CreateString(operation->label)) &&
              plan_json_add(object, "category", cJSON_CreateString(variant_category_name(operation->category))) &&
              plan_json_add(object, "citation", cJSON_CreateString(operation->citation)) &&
              plan_json_add(object, "relation", cJSON_CreateString(motive_relation_name((MotiveRelationKind)operation->relation)));
    cJSON *params = ok ? add_array(object, "params") : NULL;
    cJSON *forms = ok ? add_array(object, "forms") : NULL;
    ok = ok && params && forms;
    for (size_t i = 0; ok && i < operation->param_count; i++) {
        VariantParam param;
        ok = variant_operation_param(operation, i, &param) && plan_json_append(params, param_json(&param));
    }
    for (int form = MOTIVE_SKETCH_FORM_FULL; ok && form <= MOTIVE_SKETCH_FORM_CONTOUR; form++) {
        if ((operation->forms & VARIANT_FORM_BIT(form)) != 0) {
            ok = plan_json_append(forms, cJSON_CreateString(motive_form_name((MotiveSketchForm)form)));
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
        ok = plan_json_append(operations, operation_json(variant_registry_at(i)));
    }
    if (!ok) {
        cJSON_Delete(document);
        return NULL;
    }
    return motive_json_text(document);
}
