#ifndef URSATZ_COMPOSER_API_PIECE_H
#define URSATZ_COMPOSER_API_PIECE_H

#include <stdbool.h>
#include <stddef.h>

#include <cJSON.h>

#include <ursatz_composer/composer_api.h>
#include <ursatz_composer/compose.h>
#include <ursatz_composer/export.h>
#include <ursatz_composer/fill_target.h>
#include <ursatz_composer/grow_form.h>
#include <ursatz_composer/lanes.h>
#include <ursatz_composer/motive.h>
#include <ursatz_composer/piece.h>
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

/* The form every piece is: the game loop, whose sections the piece request gives (forms/game-loop.json). */
#define API_PIECE_FORM "game-loop"

/* The piece request, read: the request of the form, the brief, every motive and theme plan. */
typedef struct {
    PieceRequest request; /* form API_PIECE_FORM, its sections in `sections` */
    PieceSection sections[PIECE_STRUCTURE_MAX_SECTIONS + 1];
    const cJSON *motive_json[PIECE_STRUCTURE_MAX_SECTIONS + 1]; /* a motif fill's own motive */
    const cJSON *forms_json[PIECE_STRUCTURE_MAX_SECTIONS + 1];  /* a motif fill's saved forms */
    Motive *section_motive[PIECE_STRUCTURE_MAX_SECTIONS + 1];   /* owned */
    PiecePlan *theme_plan[PIECE_STRUCTURE_MAX_SECTIONS + 1];    /* owned */
    char *numeral_text[PIECE_STRUCTURE_MAX_SECTIONS + 1];       /* owned; the numerals point into it */
    const char *numerals[PIECE_STRUCTURE_MAX_SECTIONS + 1][PIECE_NUMERALS_MAX];
    char source[PIECE_STRUCTURE_MAX_SECTIONS + 1][PLAN_STYLE_PATH_MAX]; /* where a section's material came from */
    const cJSON *piece_motive_json;
    Motive *piece_motive; /* owned */
    const char *key_text;
    const char *mode_text;
    const char *meter_text;
    const char *style_path;
    ComposerRatio weight;
    const char *without[PIECE_WITHOUT_MAX];
    size_t without_count;
    ComposerStyle *style; /* owned */
    ComposerRatio tempo;  /* a denominator of 0 for none */
    const char *tempo_from;
    const char *mode_from;
    GrowPoolForm *pool;   /* owned */
    Motive *pool_motives; /* owned */
    size_t pool_count;
    const Motive *basic;  /* the motive the motives stage compares with, NULL when it is invented */
    PlanExportOptions export_options;
    const ComposerJob *job;
} PieceSpec;

/*
 * Reads the piece request object root (docs/piece-request.md). base is its JSON Pointer in the API request, which the error
 * paths start with. style_path, when not NULL, replaces the request's "style". The caller destroys the request, also on
 * failure. COMPOSER_ERR_REQUEST names a member; COMPOSER_ERR_UNSATISFIABLE is a style that cannot be read.
 */
ComposerStatus api_piece_read_request(PieceSpec *request, const cJSON *root, const char *base, const char *style_path,
                                      bool plain_midi, const ComposerJob *job, ComposerError *error);
void api_piece_request_destroy(PieceSpec *request);

/*
 * Reads the piece request root without composing and writes the defaults into filled (a copy of root): the cadence of every
 * section that has none (piece_default_cadences) and the default ending section when there is none
 * (piece_default_ending).
 */
ComposerStatus api_piece_validate(const cJSON *root, cJSON *filled, ComposerError *error);

/* A version of the piece: its plan and what composing it reported. */
typedef struct {
    PiecePlan *plan;
    ComposeOutcome outcome;
    bool composed;
} PieceVersion;

typedef struct {
    PieceVersion loop;
    PieceVersion ending;
    unsigned long seed;
} PieceMade;

void api_piece_version_destroy(PieceVersion *version);
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
 * whole section. path is the member's JSON Pointer. COMPOSER_ERR_REQUEST for another word.
 */
ComposerStatus api_piece_read_lanes(const cJSON *lanes, const char *path, PieceEdit *edit, ComposerError *error);

/* The lanes as the document names them: "all", or the lane words joined with "+". */
void api_piece_lanes_name(const PieceEdit *edit, char *out, size_t size);

/*
 * The loop plan written again in the section of edit, with seed (the plan kept when edit has no section). COMPOSER_OK
 * with PIECE_ERR_STUCK in *result is a stuck run, which the next seed may mend; a refused edit is the status it names. A
 * cancel is COMPOSER_ERR_CANCELLED.
 */
ComposerStatus api_piece_regenerate_loop(const PieceSpec *request, const PieceEdit *edit, bool check, unsigned long seed,
                                         PieceVersion *loop, PieceResult *result, ComposerError *error);

/* The ending version built from the loop plan with seed + 20 (compose_run on the plan piece_build_ending makes). */
PieceResult api_piece_compose_ending(const PieceSpec *request, bool check, unsigned long seed, PieceMade *piece);

/* The version as the document has it: {"plan": null, "sections": [...], "score", "seam", "analyzer_check", "style_distance"}. */
cJSON *api_piece_version_json(const PieceSpec *request, const PieceVersion *version);

/* The loop's region: from the start of the first body section to the end of the last section of the loop. */
bool api_piece_loop_region(const PiecePlan *plan, Rational *start, Rational *end);
/* {"start": "2/1", "end": "14/1"} of the region of the plan, or NULL when it has none. */
cJSON *api_piece_loop_json(const PiecePlan *plan);

/*
 * The MusicXML of a version's plan, made from the plan in memory (its span ids are the ones the notes' xml:id carry; a plan
 * file renumbers them). The caller frees *out.
 */
ComposerStatus api_piece_musicxml(const PieceSpec *request, const PiecePlan *plan, char **out, ComposerError *error);

/* The plan as a cJSON object, or NULL when it cannot be written. */
cJSON *api_piece_plan_object(const PiecePlan *plan);

/* The failure of a piece no seed composed: what the last try ran into. */
ComposerStatus api_piece_fail_compose(const PieceSpec *request, PieceResult result, const PieceMade *last,
                                      unsigned long first_seed, unsigned long last_seed, ComposerError *error);

#endif
