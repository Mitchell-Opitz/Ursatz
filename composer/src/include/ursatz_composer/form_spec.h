#ifndef URSATZ_COMPOSER_FORM_SPEC_H
#define URSATZ_COMPOSER_FORM_SPEC_H

#include <stdbool.h>
#include <stddef.h>

#include <ursatz_composer/constraint.h>

/*
 * A form as a document (R-416): the sections of a short form in order, at each size it comes in, with the keys a piece
 * may move to, the cadences, the returns and the checks that apply. The document is JSON ("schema" "ursatz-form",
 * "version" 1, schema/form.v1.schema.json, docs/forms.md); form_spec_parse reads it and refuses anything it does not
 * know, naming the member. A form is built into the library from forms/<name>.json (form_builtin.h) and one builder,
 * form_build_plan (piece.h), makes the plan of every form from its document.
 */

#define FORM_SPEC_MAX_REGIONS 4
#define FORM_SPEC_MAX_SIZES 8
/* The most sections a size has: a theme and eight variations. */
#define FORM_SPEC_MAX_SECTIONS 9
#define FORM_SPEC_MAX_DEGREES 8
/* A name is [a-z0-9-], 1 to FORM_SPEC_NAME_MAX - 1 characters. */
#define FORM_SPEC_NAME_MAX 32
#define FORM_SPEC_LABEL_MAX 64
/* A section name ("variation 1", "layer 1", "A'"). */
#define FORM_SPEC_SECTION_NAME_MAX 32
/* A section that returns none. */
#define FORM_SPEC_NO_RETURN ((size_t)-1)
/* A key that is the home key. */
#define FORM_SPEC_HOME (-1)
/* The size of a cadence vocabulary name. */
#define FORM_SPEC_CADENCE_NAME_MAX 32

/*
 * What a section is in the form. The theme-like roles (theme, contrast, return, variation, layer) are the sections
 * grow-theme builds; the others (intro, body, turnaround, ending) are for forms whose sections are free phrases
 * (P46 to P53 add rows).
 */
typedef enum {
    FORM_ROLE_THEME,
    FORM_ROLE_CONTRAST,
    FORM_ROLE_RETURN,
    FORM_ROLE_VARIATION,
    FORM_ROLE_LAYER,
    FORM_ROLE_INTRO,
    FORM_ROLE_BODY,
    FORM_ROLE_TURNAROUND,
    FORM_ROLE_ENDING
} FormRole;

#define FORM_ROLE_COUNT 9

/* "theme", "contrast", "return", "variation", "layer", "intro", "body", "turnaround", "ending"; NULL for an invalid role. */
const char *form_role_name(FormRole role);
/* False for a name that is none of these. */
bool form_role_find(const char *name, FormRole *out);
/* True for the roles of a section grow-theme builds (the first five). */
bool form_role_is_theme(FormRole role);

/* The checks of a compose run a form may switch off; a form that names none runs every one. */
typedef enum {
    FORM_CHECK_SECTIONS = 1u << 0, /* each theme section is heard as its shape */
    FORM_CHECK_RETURNS = 1u << 1,  /* each returning section is heard as its source */
    FORM_CHECK_KEYS = 1u << 2,     /* the keys the plan asks for sound */
    FORM_CHECK_CADENCES = 1u << 3, /* the cadences across the sections */
    FORM_CHECK_SEAM = 1u << 4      /* the loop's end leads back to its start */
} FormCheck;

#define FORM_CHECK_ALL (FORM_CHECK_SECTIONS | FORM_CHECK_RETURNS | FORM_CHECK_KEYS | FORM_CHECK_CADENCES | FORM_CHECK_SEAM)

/* The name a check goes by in a document ("sections", "returns", "keys", "cadences", "seam"); NULL for none. */
const char *form_check_name(FormCheck check);

/* A key a piece may move to: the degrees of the home key it is drawn from in a major and in a minor home key. */
typedef struct {
    char name[FORM_SPEC_NAME_MAX];
    int major[FORM_SPEC_MAX_DEGREES];
    size_t major_count;
    int minor[FORM_SPEC_MAX_DEGREES];
    size_t minor_count;
} FormRegion;

/* One section of one size. */
typedef struct {
    char name[FORM_SPEC_SECTION_NAME_MAX]; /* the section node's name: "A", "theme", "variation 1", "layer 1" */
    char letter[PLAN_SECTION_LETTER_MAX];  /* "A", "B", "A'", "T", "V1", "L1" */
    FormRole role;
    size_t bars;                           /* its length in whole bars; 0 when it is given in beats */
    size_t beats;                          /* its length in whole beats of the meter; 0 when it is given in bars */
    PlanThemePairing pairing;
    CadenceType cadence;                   /* the core type of the cadence it ends in: PAC, IAC or HC */
    char cadence_name[FORM_SPEC_CADENCE_NAME_MAX]; /* the cadence's name in the standard-harmony vocabulary */
    bool closed;                           /* it may close in the home key with a PAC before the piece ends */
    int key_start;                         /* FORM_SPEC_HOME, or the index of the region it starts in */
    int key_end;                           /* FORM_SPEC_HOME, or the index of the region it ends in */
    size_t returns;                        /* the index of the section it returns, or FORM_SPEC_NO_RETURN */
    PlanReturnVariation variation;         /* how it varies the section it returns; read only when it returns one */
    bool has_motive;                       /* its motive is made from m0 by `motive`; false for m0 itself */
    MotiveRelationStep motive;             /* how its motive is made from m0 (open parameters are drawn) */
    unsigned layers;                       /* the LAYERS mask (PlanLayers) in a layered form; 0 for none */
} FormSectionSpec;

/* One size of a form. */
typedef struct {
    size_t bars;      /* the sum of its sections' bars */
    size_t count;     /* the variations or layers it has; 0 for a form with no count */
    bool is_default;  /* the size a request gets when it asks for none */
    FormSectionSpec sections[FORM_SPEC_MAX_SECTIONS];
    size_t section_count;
} FormSpecSize;

typedef struct {
    char name[FORM_SPEC_NAME_MAX];
    char label[FORM_SPEC_LABEL_MAX];
    char plan_name[FORM_SPEC_NAME_MAX]; /* the plan's form node name: `name` unless the document says otherwise */
    bool closing;                       /* the piece ends with a cadence: the cadences across the sections are planned */
    bool accompaniment_required;        /* every section has an accompaniment (pattern ANY when the request has none) */
    FormRegion regions[FORM_SPEC_MAX_REGIONS];
    size_t region_count;
    unsigned checks;                    /* FormCheck bits; FORM_CHECK_ALL when the document names none */
    bool request_sections;              /* the piece request gives the sections (no sizes) */
    FormSpecSize sizes[FORM_SPEC_MAX_SIZES];
    size_t size_count;
} FormSpec;

typedef enum {
    FORM_SPEC_OK = 0,
    FORM_SPEC_ERR_NULL,
    FORM_SPEC_ERR_ALLOC,
    FORM_SPEC_ERR_PARSE,   /* not JSON */
    FORM_SPEC_ERR_INVALID, /* JSON that is not a valid form document: error names the member */
    FORM_SPEC_ERR_UNKNOWN  /* no built-in form of that name */
} FormSpecResult;

/* Filled on failure; pass NULL when the details are not wanted. */
typedef struct {
    char path[64];     /* a JSON pointer to the member at fault ("/sizes/0/sections/1/cadence"); "" for the document */
    char message[192];
} FormSpecError;

/* Reads a document. On FORM_SPEC_OK, *out is a form the caller frees with form_spec_destroy; else *out is NULL. */
FormSpecResult form_spec_parse(const char *json, FormSpec **out, FormSpecError *error);
void form_spec_destroy(FormSpec *form);

/* The index of the region with that name, or -1. */
int form_spec_region_find(const FormSpec *form, const char *name);

#endif
