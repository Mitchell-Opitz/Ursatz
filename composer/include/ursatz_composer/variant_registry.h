#ifndef URSATZ_COMPOSER_VARIANT_REGISTRY_H
#define URSATZ_COMPOSER_VARIANT_REGISTRY_H

#include <stdbool.h>
#include <stddef.h>

#include <ursatz_composer/motive.h>
#include <ursatz_composer/motive_develop.h>

/*
 * The variant operation registry: every way a motive can be made into a variant, one table row each. The 9 rows are
 * the development operations of motive_develop.h, with the category and the citation of Schoenberg's Fundamentals of
 * Musical Composition they belong to. Adding an operation is adding a row to the table in variant_registry.c; nothing
 * else changes (chains, records, JSON and the random chains all read the table).
 *
 * An operation has parameters (the same ones the develop command takes). A parameter's value is a VariantValue; a
 * parameter that is not set is open, and a chain draws it from a seed (variant_chain). Values are indexed like the
 * operation's params.
 */

#define VARIANT_PARAM_MAX 4
#define VARIANT_NAME_MAX 32

typedef enum {
    VARIANT_CATEGORY_EXACT,
    VARIANT_CATEGORY_RHYTHM,
    VARIANT_CATEGORY_INTERVALS,
    VARIANT_CATEGORY_HARMONY,
    VARIANT_CATEGORY_MELODY_ADAPTATION
} VariantCategory;

/* "exact", "rhythm", "intervals", "harmony" or "melody-adaptation". */
const char *variant_category_name(VariantCategory category);

typedef enum {
    VARIANT_PARAM_INT,      /* a whole number from min to max */
    VARIANT_PARAM_BOOL,     /* 0 or 1 */
    VARIANT_PARAM_RATIO,    /* num:den, each from min to max */
    VARIANT_PARAM_PITCH,    /* a MIDI note number */
    VARIANT_PARAM_POSITION  /* a PlanFragmentPosition: 1 leading, 2 trailing, 3 middle */
} VariantParamKind;

/* "int", "bool", "ratio", "pitch" or "position". */
const char *variant_param_kind_name(VariantParamKind kind);

typedef struct {
    const char *name; /* as the develop command's option without the dashes: "tonal", "keep-contour" */
    VariantParamKind kind;
    long min;
    long max;
    const char *description;
    /* Never drawn: an open optional parameter stays unset and the operation does without it. */
    bool optional;
    /* A count of the source's notes: the largest value drawn is the source's note count less one. */
    bool notes_limit;
} VariantParam;

/*
 * A parameter's value. set is false for an open parameter. For a ratio, value is the numerator and den the
 * denominator; for the other kinds den is 0.
 */
typedef struct {
    bool set;
    long value;
    long den;
} VariantValue;

typedef struct VariantOperation VariantOperation;

/*
 * Applies the operation to source with every non-optional parameter set in values (indexed like params), writing
 * the result to *out. The motive_develop result is returned as it is: MOTIVE_DEV_OK, or why the operation was refused.
 */
typedef MotiveDevResult (*VariantApplyFn)(const Motive *source, const VariantValue *values, Motive *out);

/* One plain phrase for a step with its parameters: "inverted about D4 in the key's scale". Lower case. */
typedef void (*VariantDescribeFn)(const VariantValue *values, char *out, size_t size);

struct VariantOperation {
    const char *name;     /* "invert", "interval-change" */
    const char *label;    /* "Invert", "Interval change" */
    VariantCategory category;
    const char *citation; /* "Schoenberg ch. III, Ex. 14" */
    const VariantParam *params;
    size_t param_count;
    unsigned forms; /* a bit mask of 1u << MotiveForm: the forms the operation applies to */
    VariantApplyFn apply;
    VariantDescribeFn describe;
};

#define VARIANT_FORM_BIT(form) (1u << (unsigned)(form))

size_t variant_registry_count(void);
/* NULL for an index past the end. */
const VariantOperation *variant_registry_at(size_t index);
/* NULL for a name that is not in the registry. */
const VariantOperation *variant_registry_find(const char *name);

/* The index of a parameter of the operation by name, or -1. */
int variant_operation_param_index(const VariantOperation *operation, const char *name);

#endif
