#ifndef URSATZ_COMPOSER_FORM_SPEC_H
#define URSATZ_COMPOSER_FORM_SPEC_H

#include <stdbool.h>
#include <stddef.h>

#include <plan/section.h>

#include <ursatz_composer/constraint.h>
#include <ursatz_composer/piece_plan.h>

/*
 * A form as a document (R-416): the sections of a short form in order, at each size it comes in, with the keys a piece
 * may move to, the cadences, the returns and the checks that apply. The document is JSON ("schema" "ursatz-form",
 * "version" 2, schema/form.v2.schema.json, docs/forms.md); form_spec_parse reads it and refuses anything it does not
 * know, naming the member. A form is built into the library from forms/<name>.json (form_builtin.h) and one builder,
 * form_build_plan (piece.h), makes the plan of every form from its document.
 */

#define FORM_SPEC_MAX_REGIONS 4
#define FORM_SPEC_MAX_SIZES 8
/* The API's API_MAX_SECTIONS; the large and sonata rondos have 13 and 14, R-515. */
#define FORM_SPEC_MAX_SECTIONS 16
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
/* The most roles a form whose sections the request gives lists (intro, body, turnaround, ending), and the most cadence rules. */
#define FORM_SPEC_MAX_PARTS 4
#define FORM_SPEC_MAX_CADENCE_RULES 4
/* The size of a section id, with its terminator: the id the request gives a section (piece.h PIECE_SECTION_ID_MAX). */
#define FORM_SPEC_ID_MAX 16
/* The most meters a form lists. */
#define FORM_SPEC_MAX_METERS 4

/* The checks of a compose run a form may switch off; a form that names none runs every one. */
typedef enum {
    FORM_CHECK_SECTIONS = 1u << 0, /* each theme section is heard as its shape */
    FORM_CHECK_RETURNS = 1u << 1,  /* each returning section is heard as its source */
    FORM_CHECK_KEYS = 1u << 2,     /* the keys the plan asks for sound */
    FORM_CHECK_CADENCES = 1u << 3, /* the cadences across the sections */
    FORM_CHECK_SEAM = 1u << 4      /* the loop's end leads back to its start */
} FormCheck;

#define FORM_CHECK_ALL (FORM_CHECK_SECTIONS | FORM_CHECK_RETURNS | FORM_CHECK_KEYS | FORM_CHECK_CADENCES | FORM_CHECK_SEAM)


/*
 * A key a piece may move to: the relations of the knowledge snapshot (K6, `omt.relation.*`) it is reached by from the home key,
 * one list for a major and one for a minor home key; one is drawn from the list (R-606).
 */
typedef struct {
    char name[FORM_SPEC_NAME_MAX];
    char major[FORM_SPEC_MAX_DEGREES][SECTION_WORD_MAX];
    size_t major_count;
    char minor[FORM_SPEC_MAX_DEGREES][SECTION_WORD_MAX];
    size_t minor_count;
} FormRegion;

/* One size of a form. */
typedef struct {
    size_t bars;      /* the sum of its sections' bars */
    size_t count;     /* the variations or layers it has; 0 for a form with no count */
    bool is_default;  /* the size a request gets when it asks for none */
    SectionDescription sections[FORM_SPEC_MAX_SECTIONS]; /* a form's section use of the description (library plan/section.h) */
    size_t section_count;
} FormSpecSize;

/* A cadence rule of a form whose sections the request gives: the last section whose role is one of `roles` ends in `cadence`. */
typedef struct {
    SectionRole roles[FORM_SPEC_MAX_PARTS];
    size_t role_count;
    CadenceType cadence;                           /* the core type: PAC, IAC or HC */
    char cadence_name[FORM_SPEC_CADENCE_NAME_MAX]; /* its name in the standard-harmony vocabulary */
} FormCadenceRule;

/* The section a request that has no ending gets (R-169 defaults it to two empty bars closing in a PAC). */
typedef struct {
    char id[FORM_SPEC_ID_MAX];
    char letter[PLAN_SECTION_LETTER_MAX];
    SectionRole role;
    size_t bars;
    CadenceType cadence;
    char cadence_name[FORM_SPEC_CADENCE_NAME_MAX];
} FormDefaultEnding;

/* A group of consecutive sections a form repeats (R-485): section indexes of a size, the same in every size. */
typedef struct {
    size_t first;
    size_t count;
} FormRepeat;

/* The most parts a compound form has. */
#define FORM_SPEC_MAX_PARTS_OF 4

/*
 * One part of a compound form (R-485): a piece of a built-in form with sizes, composed on its own and performed in order,
 * or (again) an earlier part performed once more.
 */
typedef struct {
    char name[FORM_SPEC_SECTION_NAME_MAX]; /* "Minuet", "Trio", "Minuet da capo": the prefix of its sections' names after the first part */
    char form[FORM_SPEC_NAME_MAX];         /* the built-in form it is a piece of; empty for an again part */
    int key;                               /* FORM_SPEC_HOME, or the index of the region its home key is drawn from */
    bool new_motive;                       /* it has a motive of its own, invented; else it has the piece's motive */
    size_t bars;                           /* the size of the part's form it is composed at; 0: the compound size's bars (R-495) */
    size_t again;                          /* FORM_SPEC_NO_RETURN, or the index of the earlier part it performs again */
    bool repeats;                          /* its repeats are written out (true by default, false for an again part) */
} FormPart;

typedef struct {
    char name[FORM_SPEC_NAME_MAX];
    char label[FORM_SPEC_LABEL_MAX];
    char plan_name[FORM_SPEC_NAME_MAX]; /* the plan's form node name: `name` unless the document says otherwise */
    bool closing;                       /* the piece ends with a cadence: the cadences across the sections are planned */
    bool modulates;                     /* the key plan leaves home whether or not the request asks for modulate (R-493) */
    bool accompaniment_required;        /* every section has an accompaniment (pattern ANY when the request has none) */
    FormRegion regions[FORM_SPEC_MAX_REGIONS];
    size_t region_count;
    unsigned checks;                    /* FormCheck bits; FORM_CHECK_ALL when the document names none */
    PlanMeter meters[FORM_SPEC_MAX_METERS]; /* the meters a piece of the form may be in */
    size_t meter_count;                 /* 0: any the builder supports */
    unsigned tempo;                     /* quarter notes a minute when the request and its style give none; 0: none */
    bool request_sections;              /* the piece request gives the sections (no sizes) */
    FormRepeat repeats[FORM_SPEC_MAX_SECTIONS]; /* the groups performed twice in place, in order (R-485) */
    size_t repeat_count;
    /*
     * A compound form (R-485): its parts, composed each as a piece of its own form at the size of the form's bars and
     * performed in order. Its sizes have bars and no sections. 0 parts: not compound.
     */
    FormPart parts[FORM_SPEC_MAX_PARTS_OF];
    size_t part_count;
    /*
     * The members of a form whose sections the request gives (the game loop, R-419); all empty for any other. The roles a
     * section may have, in order (until P45: intro, body, turnaround and ending); the roles that are the loop (the body
     * and the turnaround: the loop's end is held for the seam stage and a seam bar follows it; the ending version
     * replaces the turnaround with the ending); the cadence a section gets when the request names none: default_cadence,
     * else the first rule of last_cadences whose roles hold the last section of that role; the section a request with no
     * ending gets.
     */
    SectionRole roles[FORM_SPEC_MAX_PARTS];
    size_t role_count;
    SectionRole loop_roles[FORM_SPEC_MAX_PARTS];
    size_t loop_role_count;
    CadenceType default_cadence;
    char default_cadence_name[FORM_SPEC_CADENCE_NAME_MAX];
    FormCadenceRule last_cadences[FORM_SPEC_MAX_CADENCE_RULES];
    size_t last_cadence_count;
    bool has_default_ending;
    FormDefaultEnding default_ending;
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

/*
 * The size of the form a request asks for. bars 0 and count 0 give the default size; a count only for a form whose
 * sizes have counts, and it picks the size with that count (bars, when given, must match); bars alone pick the size
 * with those bars (the first, when two have them). NULL when the form has no such size, and for a form whose sections
 * the request gives.
 */
const FormSpecSize *form_spec_size(const FormSpec *form, size_t bars, size_t count);
/* The default size; NULL for a form with no sizes. */
const FormSpecSize *form_spec_default_size(const FormSpec *form);

/* Whether the role is one of the form's `roles` (a form whose sections the request gives). */
bool form_spec_has_role(const FormSpec *form, SectionRole role);
/* Whether the role is one of the form's loop roles. */
bool form_spec_role_in_loop(const FormSpec *form, SectionRole role);

/* Whether a piece of the form may be in the meter: true when the form lists no meters or lists this one. */
bool form_spec_meter_allowed(const FormSpec *form, const PlanMeter *meter);

/* Whether a piece of the form is performed: its composed plan is not what it renders (R-485): the form has repeats or parts. */
bool form_spec_performed(const FormSpec *form);

/* The index of the region with that name, or -1. */
int form_spec_region_find(const FormSpec *form, const char *name);
/* FORM_SPEC_HOME, or the index of the region a section's key region names (a region that is none or home is home). */
int form_spec_region_of(const FormSpec *form, const SectionRegion *region);
/* The index in the size of the section with that name ("" or an unknown name: FORM_SPEC_NO_RETURN). */
size_t form_spec_section_index(const FormSpecSize *size, const char *name);
/* The step a section's motive makes from m0: false when it has none; a lyric theme's is m0 augmented, its open ratio left for the builder. */
bool form_spec_section_motive(const SectionDescription *section, MotiveRelationStep *out);

/*
 * The forms built into the library, one per file in forms/ in the order COMPOSER_FORMS in src/CMakeLists.txt lists
 * them (binary, rounded-binary, ternary, variations, loop, layered, game-loop). The documents are embedded at build time
 * (cmake/EmbedForms.cmake); adding a form is a file in forms/ and a line in COMPOSER_FORMS.
 */
size_t form_spec_builtin_count(void);
/* The document text of the form at index (NUL-terminated), and the name its file goes by; NULL past the end. */
const char *form_spec_builtin_document(size_t index);
const char *form_spec_builtin_name(size_t index);
/* The name the plan's form node of the form at index has (its plan_name, else its name), and the hash of its document: FNV-1a 64
   of the embedded bytes, 16 lowercase hexadecimal digits. NULL past the end. The hash is kept in static storage. */
#define FORM_SPEC_BUILTIN_MAX 32
#define FORM_SPEC_HASH_LEN 16
#define FORM_SPEC_VERSION 2 /* the version of the form documents this library reads and writes */
const char *form_spec_builtin_plan_name(size_t index);
const char *form_spec_builtin_hash(size_t index);
/* Parses the built-in form with this name into *out (on every call: no state is kept); FORM_SPEC_ERR_UNKNOWN for none. */
FormSpecResult form_spec_builtin(const char *name, FormSpec **out);

#endif
