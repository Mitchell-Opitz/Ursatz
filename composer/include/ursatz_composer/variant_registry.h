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
    /* A ratio: drawn below 1 instead of above it. */
    bool shrinks;
    /* A 0-based index into the motive's labelled features: the largest value drawn is the feature count less one. */
    bool features_limit;
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
    /*
     * The library relation (a MotiveRelationKind of analysis/motive_relation.h) this row maps to: the relation of the
     * same name, with the row's parameters in the relation's order.
     */
    int relation;
};

#define VARIANT_FORM_BIT(form) (1u << (unsigned)(form))

size_t variant_registry_count(void);
/* NULL for an index past the end. */
const VariantOperation *variant_registry_at(size_t index);
/* NULL for a name that is not in the registry. */
const VariantOperation *variant_registry_find(const char *name);

/*
 * The registry as JSON, 2-space indent, trailing newline; NULL on failure, caller frees:
 *   {"operations": [{"name", "label", "category", "citation",
 *                    "relation", "params": [{"name", "kind", "min", "max", "description", "optional"}],
 *                    "forms": ["full", "rhythm", "contour"]}]}
 */
char *variant_registry_to_json(void);

/* The index of a parameter of the operation by name, or -1. */
int variant_operation_param_index(const VariantOperation *operation, const char *name);

/* --- chains ------------------------------------------------------------- */

#define VARIANT_CHAIN_MAX 4
#define VARIANT_ERROR_MAX 160
#define VARIANT_DERIVATION_MAX 512
#define VARIANT_HARMONY_MAX 256

/* One operation and the parameters Michelle (or a chain file) set. A parameter not set is open. */
typedef struct {
    char op[VARIANT_NAME_MAX];
    VariantValue values[VARIANT_PARAM_MAX]; /* indexed like the operation's params */
} VariantStep;

typedef struct {
    VariantStep steps[VARIANT_CHAIN_MAX];
    size_t step_count;
} VariantChain;

/* Sets *step to the operation with every parameter open. False for a name that is not in the registry. */
bool variant_step_init(VariantStep *step, const char *op);
/*
 * Sets a parameter of a step made by variant_step_init. den is the denominator of a ratio and ignored for the other
 * kinds. False for an unknown parameter or a value outside the parameter's range.
 */
bool variant_step_set(VariantStep *step, const char *param, long value, long den);

/* What a step resolved to: every parameter, and which of them the chain left open (drawn from the seed). */
typedef struct {
    char op[VARIANT_NAME_MAX];
    size_t param_count;
    VariantValue values[VARIANT_PARAM_MAX];
    bool open[VARIANT_PARAM_MAX];
} VariantStepRecord;

/* The dimensions a variant keeps or changes (R-134), in this order: "rhythm", "intervals", "contour", "harmony". */
#define VARIANT_DIMENSIONS 4

/* exact: nothing changed. motive-form: something did. The GUI sets variant, never the Composer. */
typedef enum {
    VARIANT_KIND_EXACT,
    VARIANT_KIND_MOTIVE_FORM
} VariantKind;

/* "exact" or "motive-form". */
const char *variant_kind_name(VariantKind kind);

/*
 * How a variant was made. The same chain and seed give the same variant, so a reroll is the same chain with another
 * seed. On a failed chain, failed_step is the 0-based step and error says why in one sentence.
 */
typedef struct {
    unsigned long seed;
    VariantStepRecord steps[VARIANT_CHAIN_MAX];
    size_t step_count;
    /*
     * What the result kept and changed against the source. A dimension is kept when
     *   rhythm     the durations and onsets are the source's times one common factor (rhythm_scaled when it is not 1)
     *   intervals  every diatonic step (semitones without a key) is the source's
     *   contour    every direction is the source's
     *   harmony    the best implied chord's degree is the source's (a full motive with a key only)
     * and changed otherwise; one the motive's form does not have is in neither list.
     */
    VariantKind kind;
    const char *kept[VARIANT_DIMENSIONS];
    size_t kept_count;
    const char *changed[VARIANT_DIMENSIONS];
    size_t changed_count;
    bool rhythm_scaled;
    /* The result's own harmony as text ("V I@2"), or "" when it has none. */
    char harmony[VARIANT_HARMONY_MAX];
    /* motive_recognizability of the result against the source, the total. */
    MotiveScore recognizability;
    /*
     * How it was made in plain words: each step's phrase from the registry, joined by "then", and what was kept and
     * changed. "Inverted about D4 in the key's scale, then augmented 2:1. Kept the rhythm's proportions; changed
     * the intervals and the contour."
     */
    char derivation[VARIANT_DERIVATION_MAX];
    size_t failed_step;
    char error[VARIANT_ERROR_MAX];
} VariantRecord;

typedef enum {
    VARIANT_OK = 0,
    VARIANT_ERR_NULL,
    VARIANT_ERR_EMPTY,      /* a chain with no steps, or more than VARIANT_CHAIN_MAX */
    VARIANT_ERR_UNKNOWN_OP, /* a step names an operation that is not in the registry */
    VARIANT_ERR_FORM,       /* the operation does not apply to the source's form */
    VARIANT_ERR_PARAM,      /* a set parameter is outside its range */
    VARIANT_ERR_REFUSED     /* the operation refused the motive, on every draw of its open parameters */
} VariantResult;

/* "ok", "empty chain" and so on. */
const char *variant_result_name(VariantResult result);

/*
 * Applies the steps in order, each to the result of the one before. The open parameters of every step are drawn from
 * one FillRng seeded once with seed, in step order and in the order of the operation's params; a step an operation
 * refuses is drawn again, up to 16 draws, and fails if the step has no open parameter to draw. record may be NULL.
 * *out and *record are written on VARIANT_OK; on any other result *out is untouched and the record says which step
 * failed and why.
 */
VariantResult variant_apply_chain(const Motive *source, const VariantChain *chain, unsigned long seed, Motive *out,
                                  VariantRecord *record);

/*
 * A record as JSON, 2-space indent, trailing newline; NULL on failure, caller frees. A parameter's value is a boolean,
 * a number (an int or a MIDI pitch), a position name ("leading") or a ratio text ("2:1"):
 *   {"seed", "chain": {"steps": [{"op", "params": {"tonal": true}, "open": ["by"]}]},
 *    "kind", "kept": [], "changed": [], "harmony": "V I@2", "functions": [], "derivation", "recognizability": 0.8571}
 * params holds every resolved parameter that has a value; open names those the seed drew. harmony is the result's own
 * harmony as text, written only when the result has chords of its own. functions stays [] until the function tags of
 * P14.
 */
char *variant_record_to_json(const VariantRecord *record);

/*
 * A random chain for source: 1 or 2 steps, each a different operation drawn from those the source's form takes (never
 * exact), every parameter open. Drawn from seed, so the same source and seed give the same chain. False for NULL
 * arguments.
 */
bool variant_random_chain(const Motive *source, unsigned long seed, VariantChain *out);

#endif
