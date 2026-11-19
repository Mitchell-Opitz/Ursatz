#ifndef URSATZ_COMPOSER_API_PIECE_H
#define URSATZ_COMPOSER_API_PIECE_H

#include <stdbool.h>
#include <stddef.h>

#include <cJSON.h>
#include <theory/tonality/common_practice_materials.h>

#include <ursatz_composer/composer_api.h>
#include <ursatz_composer/compose.h>
#include <ursatz_composer/export.h>
#include <ursatz_composer/form_spec.h>
#include <ursatz_composer/fill_target.h>
#include <ursatz_composer/grow_form.h>
#include <plan/plan_lanes.h>
#include <ursatz_composer/motive.h>
#include <ursatz_composer/piece.h>
#include <ursatz_composer/piece_perform.h>
#include <ursatz_composer/style.h>

/*
 * The engine behind the piece API entries (docs/api.md, docs/piece-request.md): the piece request read from its JSON, the
 * loop version and the ending version composed or regenerated from one seed after another, the result document. Private to
 * the library.
 */

#define PIECE_NUMERALS_MAX 32
#define PIECE_POOL_MAX 64
#define PIECE_WITHOUT_MAX 8
/* The ending version is composed from the loop's seed plus this (R-172). */
#define PIECE_ENDING_SEED_OFFSET 20UL
/* How many seeds are tried, one after another from the one given, before a piece is given up. */
#define PIECE_SEED_TRIES 16
#define PIECE_SEED_MAX 4294967295UL
#define PIECE_ROWS_MAX (PIECE_STRUCTURE_MAX_SECTIONS + 1)
#define PIECE_LANES_MAX 4
#define PIECE_VARIATIONS_MAX 8
/* As many seeds are tried as it takes to find the variations asked for, at most this many for each. */
#define PIECE_SEEDS_PER_VARIATION 4

/*
 * The versions a piece of the form has, as names and labels, into names and labels (at most capacity); the count. A form whose
 * sections the request gives and that has a loop gives "loop" ("Loop version") then "ending" ("Ending version"); every other
 * form gives "piece" ("Piece").
 */
size_t api_piece_version_names(const FormSpec *form, const char **names, const char **labels, size_t capacity);

/* The form of a version 1 piece request: the game loop, whose sections the request gives (forms/game-loop.json). */
#define API_PIECE_FORM "game-loop"

/* The piece request, read: the request of the form, the brief, every motive and theme plan. */
typedef struct {
    PieceRequest request; /* its form is form->name; the sections of a form that takes them from the request are in `sections` */
    FormSpec *form;       /* owned: the form the request names (API_PIECE_FORM for schema version 1) */
    PieceSection sections[PIECE_STRUCTURE_MAX_SECTIONS + 1];
    const cJSON *motive_json[PIECE_STRUCTURE_MAX_SECTIONS + 1]; /* a motif fill's own motive */
    const cJSON *forms_json[PIECE_STRUCTURE_MAX_SECTIONS + 1];  /* a motif fill's saved forms */
    Motive *section_motive[PIECE_STRUCTURE_MAX_SECTIONS + 1];   /* owned */
    Plan *theme_plan[PIECE_STRUCTURE_MAX_SECTIONS + 1];    /* owned */
    char source[PIECE_STRUCTURE_MAX_SECTIONS + 1][PLAN_STYLE_PATH_MAX]; /* where a section's material came from */
    const cJSON *piece_motive_json;
    Motive *piece_motive; /* owned */
    const char *key_text;
    const char *material_text;   /* "material" or the old "mode": a material word, or "style" */
    const char *material_member; /* which of the two the request wrote */
    const char *meter_text;
    const char *style_path;
    Rational weight;
    const char *without[PIECE_WITHOUT_MAX];
    size_t without_count;
    ComposerStyle *style; /* owned */
    Rational tempo;  /* a denominator of 0 for none */
    const char *tempo_from;
    const char *mode_from;
    GrowPoolForm *pool;   /* owned */
    Motive *pool_motives; /* owned */
    size_t pool_count;
    const Motive *basic;  /* the motive the motives stage compares with, NULL when it is invented */
    PlanExportOptions export_options;
    const UrsatzJob *job;
    const KnowledgeSnapshot *knowledge; /* the call context's; set by api_piece_read_request, must outlive the request */
    const CommonPracticeMaterials *materials; /* its common-practice materials, resolved once for the call; as long-lived */
} PieceSpec;

/*
 * Reads the piece request object root (docs/piece-request.md). base is its JSON Pointer in the API request, which the error
 * paths start with. style_path, when not NULL, replaces the request's "style". The caller destroys the request, also on
 * failure. URSATZ_ERR_REQUEST names a member; URSATZ_ERR_UNSATISFIABLE is a style that cannot be read.
 */
UrsatzStatus api_piece_read_request(PieceSpec *request, const KnowledgeSnapshot *knowledge, const CommonPracticeMaterials *materials,
                                      const cJSON *root, const char *base, const char *style_path, bool plain_midi,
                                      const UrsatzJob *job, UrsatzError *error);
void api_piece_request_destroy(PieceSpec *request);

/* The name of the first version of the request's form: "loop" for the game loop, "piece" for a form with sizes. */
const char *api_piece_first_version_name(const PieceSpec *request);

/*
 * Reads the piece request root without composing and writes the defaults into filled (a copy of root): the cadence of every
 * section that has none (piece_default_cadences) and the default ending section when there is none
 * (piece_default_ending).
 */
UrsatzStatus api_piece_validate(const KnowledgeSnapshot *knowledge, const CommonPracticeMaterials *materials, const cJSON *root,
                                  cJSON *filled, UrsatzError *error);

/* A version of the piece: its plan and what composing it reported. */
typedef struct {
    Plan *plan;
    ComposeOutcome outcome;
    bool composed;
    /* A form with repeats or parts (form_spec_performed): the plan performed in order, what is rendered and answered (R-485). */
    PiecePerformance performance;
    bool performed;
    /*
     * A compound form (R-485): the composed parts the performance is made of, which the piece (PieceMade) owns. The version's
     * plan is NULL and its outcome empty (composed is true); the rows read each part's plan and outcome. NULL for any other form.
     */
    const PieceParts *parts;
} PieceVersion;

/* The plan a version is rendered and written out from: its performance when it is performed, else its plan. */
const Plan *api_piece_version_output_plan(const PieceVersion *version);

/* The most versions a piece has: the loop and the ending of a game loop (api_piece_version_names). */
#define API_PIECE_VERSIONS_MAX 2

/* The versions of a piece, in the order of api_piece_version_names: index 0 is the first (the loop of a game loop). */
typedef struct {
    PieceVersion versions[API_PIECE_VERSIONS_MAX];
    size_t version_count;
    unsigned long seed;
    PieceParts parts; /* a compound form: its parts composed (versions[0].parts points here) */
} PieceMade;

void api_piece_version_destroy(PieceVersion *version);

/*
 * A performed form is composed whole (R-485): regenerating a section and varying one are Later. URSATZ_ERR_REQUEST at
 * /request/form when the request's form is performed (form_spec_performed), else URSATZ_OK.
 */
UrsatzStatus api_piece_refuse_performed(const PieceSpec *request, UrsatzError *error);
void api_piece_made_destroy(PieceMade *piece);

/* The lanes of a section to write again; none means the whole section. */
typedef struct {
    const char *path;    /* the JSON Pointer of the member that names the section and the lanes: "/regenerate" or "/variations" */
    const char *text;    /* the loop plan as JSON text */
    const char *section; /* the request's section id, or NULL for none: only the ending version is rebuilt */
    PlanLaneKind lanes[PIECE_LANES_MAX];
    size_t lane_count;
} PieceEdit;

/*
 * The lanes of "lanes" (a list of harmony, bass, melody, inner, or all) into edit; an empty or absent list, or "all", is the
 * whole section. path is the member's JSON Pointer. URSATZ_ERR_REQUEST for another word.
 */
UrsatzStatus api_piece_read_lanes(const cJSON *lanes, const char *path, PieceEdit *edit, UrsatzError *error);

/* The lanes as the document names them: "all", or the lane words joined with "+". */
void api_piece_lanes_name(const PieceEdit *edit, char *out, size_t size);

/*
 * The loop plan written again in the section of edit, with seed (the plan kept when edit has no section). URSATZ_OK
 * with PIECE_ERR_STUCK in *result is a stuck run, which the next seed may mend; a refused edit is the status it names. A
 * cancel is URSATZ_ERR_CANCELLED.
 */
UrsatzStatus api_piece_regenerate_loop(const PieceSpec *request, const PieceEdit *edit, bool check, unsigned long seed,
                                         PieceVersion *loop, PieceResult *result, UrsatzError *error);

/* The ending version built from the loop plan with seed + 20 (compose_run on the plan piece_build_ending makes). */
PieceResult api_piece_compose_ending(const PieceSpec *request, bool check, unsigned long seed, PieceMade *piece);

/* The version as the document has it: {"plan": null, "sections": [...], "score", "seam", "analyzer_check", "style_distance"}. */
cJSON *api_piece_version_json(const PieceSpec *request, const PieceVersion *version);

/* The loop's region: from the start of the first body section to the end of the last section of the loop. */
bool api_piece_loop_region(const Plan *plan, Rational *start, Rational *end);
/* {"start": "2/1", "end": "14/1"} of the region of the plan, or NULL when it has none. */
cJSON *api_piece_loop_json(const Plan *plan);

/*
 * The MusicXML of a version's plan, made from the plan in memory (its span ids are the ones the notes' xml:id carry; a plan
 * file renumbers them). The caller frees *out.
 */
UrsatzStatus api_piece_musicxml(const KnowledgeSnapshot *knowledge, const PieceSpec *request, const Plan *plan, char **out, UrsatzError *error);

/* The plan as a cJSON object, or NULL when it cannot be written. */
cJSON *api_piece_plan_object(const KnowledgeSnapshot *knowledge, const Plan *plan);

/* The failure of a piece no seed composed: what the last try ran into. */
UrsatzStatus api_piece_fail_compose(const PieceSpec *request, PieceResult result, const PieceMade *last,
                                      unsigned long first_seed, unsigned long last_seed, UrsatzError *error);

/* --- shared by the files of the piece entry (api_piece.c reads the request, the others compose, regenerate, read the plans and write the result) --- */

typedef struct {
    const PieceSection *section;     /* the section the request gave, for a form whose sections it gives */
    const SectionDescription *spec;     /* the section of the form's size, for a form with sizes */
    const FormSpecSize *size;
    const char *source;
    Rational start;
    Rational length;
    bool has_cadence;
    CadenceType cadence;
    const char *heard; /* the cadence the Analyzer heard, NULL without a check */
    const Plan *plan; /* a form with sizes: the plan and the section's node, for the pedal and the changes */
    NodeId node;
    char key_heard[16]; /* the key the Analyzer heard at the section's start over the whole piece, "" when none */
    char key[16];       /* the key the plan gives the section's start (R-493), "" when the plan has none */
    char region[128];   /* the region as the request asked for it or the form names it (R-610), "" for none */
    char harmony[320];  /* the harmony as the request gave it (R-610): the numerals, "schema <name>" or the chord bars; "" for none */
    bool key_flats;     /* that key is spelled with flats: the figure's skeleton names its tones so */
    bool pedal;        /* a harmony span in the section's first half was written over a pedal */
    bool kept;         /* a regenerate left the section as it was: it was neither heard nor checked */
    const char *part;      /* a performed compound form: the name of the part the section is in, else NULL */
    const char *repeat_of; /* a performed section that writes another out again: the first time's name, else NULL */
    bool has_upbeat;       /* the section leads into the next: its last chord and the next section's first (R-486) */
    char upbeat_chord[PLAN_ROMAN_NUMERAL_MAX];
    char upbeat_next[PLAN_ROMAN_NUMERAL_MAX];
    int connective;        /* -1 the section has no CONNECTIVE constraint, else whether it was written */
} ApiPieceRow;

typedef struct {
    bool midi;
    bool musicxml;
    bool ending_musicxml;
    bool musicxml_versions;
    bool layers; /* the realization and the reading of version 0's plan */
    int explain; /* 0 none, 1 text, 2 json */
} ApiPieceOutputs;

UrsatzStatus api_piece_fail_cancelled(UrsatzError *error);
size_t api_piece_version_count(const PieceSpec *request);
const char *api_piece_version_name(const PieceSpec *request, size_t index);
void api_piece_make_config(const PieceSpec *request, bool check, unsigned long seed, ComposeConfig *config);
PieceResult api_piece_compose(PieceSpec *request, bool check, unsigned long seed, PieceMade *piece);
bool api_piece_cancelled(const PieceMade *piece);
NodeId api_piece_sized_section_find(const Plan *plan, const char *name);
UrsatzStatus api_piece_regenerate(const PieceSpec *request, const PieceEdit *edit, bool check, unsigned long seed,
                                       PieceMade *piece, PieceResult *result, UrsatzError *error);
cJSON *api_piece_time_json(Rational time);
void api_piece_planned_key_at(const Plan *plan, PlanSpan section, char *out, size_t size, bool *flats);
size_t api_piece_collect_rows(const PieceSpec *request, const PieceVersion *version, ApiPieceRow *rows);
double api_piece_span_beats(const PieceSpec *request, Rational length);
cJSON *api_piece_document(const PieceSpec *request, const PieceMade *piece, const PieceEdit *edit,
                             const cJSON *regenerate_section, bool from);
UrsatzStatus api_piece_render_loop(const KnowledgeSnapshot *knowledge, const PieceSpec *request, const PieceMade *piece, const ApiPieceOutputs *outputs,
                                  ComposerResponse *out);
UrsatzStatus api_piece_add_outputs(const KnowledgeSnapshot *knowledge, cJSON *document, const PieceSpec *request, const PieceMade *piece, const ApiPieceOutputs *outputs,
                                  ComposerResponse *out);

#endif
