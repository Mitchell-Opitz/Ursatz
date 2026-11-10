#ifndef URSATZ_COMPOSER_API_GROW_H
#define URSATZ_COMPOSER_API_GROW_H

#include <stdbool.h>
#include <stddef.h>

#include <cJSON.h>

#include <ursatz_composer/analyzer_check.h>
#include <ursatz_composer/composer_api.h>
#include <ursatz_composer/corpus_seed.h>
#include <ursatz_composer/explain.h>
#include <ursatz_composer/export.h>
#include <ursatz_composer/given.h>
#include <ursatz_composer/grow.h>
#include <ursatz_composer/grow_form.h>
#include <ursatz_composer/motive.h>
#include <ursatz_composer/piece_plan.h>
#include <ursatz_composer/style.h>

/*
 * The engine behind the theme API entry (docs/api.md): the options of a theme run, the seeds tried one after another, the
 * result document. Private to the library and the command line.
 */

#define GROW_MAX_SUGGESTIONS 16
#define GROW_MAX_SEED_TRIES 16
#define GROW_SEED_MAX 4294967295UL
/* At most two chords a bar in a theme of eight bars. */
#define GROW_EXT_CHORDS 16

/* What the brief of a run was. */
typedef struct {
    ComposerRatio tempo;      /* a denominator of 0 for none */
    const char *tempo_from;   /* "option", "style" or "none" */
    bool minor;               /* the key's mode */
    const char *mode_from;    /* "request" or "style" */
    bool has_density;         /* the style's density (attacks per beat) */
    ComposerRatio density;
    const char *const *without;
    size_t without_count;
    const char *development;  /* "melody", "balanced" or "theme" for a theme; NULL for none (not echoed) */
} GrowBrief;

/* "melody", "balanced" or "theme". */
const char *api_development_name(GrowDevelopment development);

/* The motives of a theme run (the `theme` request's), besides the basic one. */
typedef struct {
    Motive basic;
    bool has_slot[GROW_SLOT_COUNT]; /* the slots with a motive of their own (not basic) */
    Motive slot_motives[GROW_SLOT_COUNT];
    bool has_idea[GROW_IDEA_COUNT]; /* the ideas with a motive of their own (not the basic idea) */
    Motive idea_motives[GROW_IDEA_COUNT];
    bool has_accompaniment_motive;
    Motive accompaniment_motive;
    /* The chords Given over the theme (GrowRequest.chords), numerals written in chord_numerals. */
    GivenChord chords[GROW_EXT_CHORDS];
    char chord_numerals[GROW_EXT_CHORDS][PLAN_ROMAN_NUMERAL_MAX];
    size_t chord_count;
    const GrowPoolForm *pool;
    size_t pool_count;
} GrowMaterial;

/* The text of the brief, as the command line or a request gives it; NULL for a member not given. */
typedef struct {
    const char *into;
    const char *hybrid;
    const char *key;
    const char *mode;
    const char *meter;
    const char *bars;
    const char *cadence;
    const char *first_cadence;
    const char *accompaniment;
    const char *texture;
    const char *style_weight;
    const char *development; /* "melody", "balanced" or "theme"; NULL for balanced */
    bool voicing;
    bool accompaniment_motive; /* an accompaniment motive is given (a file for grow, a member of the request for theme) */
    bool has_style;
    size_t without_count;
} GrowBriefText;

typedef enum { GROW_INTO_SENTENCE, GROW_INTO_PERIOD, GROW_INTO_HYBRID } GrowInto;

/* Everything a run needs, as read from the text of the brief by api_grow_read_brief and the caller. */
typedef struct {
    const GrowBriefText *text; /* kept for the messages; must outlive the run */
    GrowInto into;
    PlanThemePairing pairing;
    AnalysisKey key;
    PlanMeter meter;
    size_t bars;
    CadenceType cadence;
    CadenceType first_cadence;
    bool has_accompaniment;
    PlanAccompaniment accompaniment;
    bool has_voicing;
    PlanVoicing voicing;
    ComposerRatio style_weight;
    GrowDevelopment development;
    const Motive *motive;                    /* the basic motive */
    const Motive *accompaniment_motive;      /* NULL for none */
    const GrowMaterial *material;            /* NULL for grow itself */
    ComposerStyle *style;                    /* NULL for none; not owned */
    ComposerRatio tempo;                     /* a denominator of 0 for none */
    const char *tempo_from;                  /* "option", "style" or "none" */
    const char *mode_from;                   /* "request" or "style" */
    const char *const *without;              /* the categories the style is read without */
    size_t without_count;
    bool seeded;                             /* the motive is a passage of the corpus, kept in seed_found */
    CorpusSeed seed_found;
    PlanSource source;                       /* seed_found as the plan records it */
    bool check;
    bool explain;                            /* keep what the explanation needs */
    unsigned long seed;
    unsigned long suggest;
    bool suggested;                          /* "suggest" was given: the text output numbers the themes */
    const GrowFidelityTable *fidelity;       /* NULL for the defaults; not owned */
    const ComposerJob *job;
} GrowSetup;

/* A theme made, with what the output of it needs. */
typedef struct {
    PiecePlan *plan;
    GrowOutcome outcome;
    unsigned long seed;
    char *signature;
    AnalyzerCheck check; /* with check, the Analyzer's check of the theme; empty otherwise */
    bool checked;
    size_t passed_before; /* how many of the seeds passed over came before this theme */
} GrowTheme;

typedef struct {
    GrowTheme themes[GROW_MAX_SUGGESTIONS];
    size_t made;
    ComposeRejection *passed; /* the seeds passed over, in the order tried (explain.h), kept for the explanation */
    size_t passed_count;
    long chord_bar;           /* the bar of the first target a stuck run left open when it holds given chords, else -1 */
    GrowRefusal refusal;      /* why the plan was refused before seeding, when it was */
    unsigned long last_seed;
} GrowRun;

/* The brief of the text: sentence, period or hybrid with its pairing, the key, the meter, the bars, the cadences, the
   accompaniment and the voicing, into *out (not out->style, out->motive...). Messages name the member. */
ComposerStatus api_grow_read_brief(const GrowBriefText *text, GrowSetup *out, ComposerError *error);

/* "any", "block", "arpeggiated", "alberti", "oom-pah", "motive". */
const char *api_accompaniment_name(PlanAccompanimentPattern pattern);
/* The accompaniment of the two texts (either may be NULL) for the meter. */
ComposerStatus api_read_accompaniment(const char *pattern_text, const char *texture_text, const char *meter_text,
                                      const PlanMeter *meter, PlanAccompaniment *out, ComposerError *error);
/* The blend weights from --blend-weight / --blend-weights texts (any NULL). */
ComposerStatus api_read_blend(bool has_style, const char *blend_path, const char *all_text, const char *list_text,
                              ComposerStyleWeights *out, ComposerError *error);
/* Loads the style at path, blended with the one at blend_path when that is not NULL. */
ComposerStatus api_load_blend(const char *style_path, const char *blend_path, const ComposerStyleWeights *weights,
                              const char *const *without, size_t without_count, ComposerStyle **out,
                              ComposerError *error);

/*
 * Sets the tempo (from "option_tempo", else the style) and the mode of the key (mode "style" takes the style's) in out,
 * after the style is loaded. option_tempo has the tempo given with the request or NULL.
 */
ComposerStatus api_grow_set_tempo_and_mode(GrowSetup *out, const ComposerRatio *option_tempo, ComposerError *error);

/*
 * Tries the seeds one after another: a stuck run is passed over, and a theme that is the same as one already made is not
 * counted. COMPOSER_ERR_UNSATISFIABLE when none can be made, COMPOSER_ERR_CANCELLED when the job says so. The run holds
 * what was made, also on failure; destroy it either way.
 */
ComposerStatus api_grow_run(const GrowSetup *setup, GrowRun *run, ComposerError *error);

/*
 * The GrowRequest the theme is built from for a seed (the plan does not depend on it today): the setup's brief and its
 * material's motives and chords. The request points into the setup and the material, which must outlive it.
 */
void api_grow_request_for(const GrowSetup *setup, unsigned long seed, GrowRequest *request);
void api_grow_run_destroy(GrowRun *run);

/*
 * The refusal of a motif longer than the basic idea it fills, at path: "the motif is 3 bars; the basic idea of an 8-bar
 * theme is 2". The motif's bars are those it takes from its start, a pickup included.
 */
ComposerStatus api_fail_motif_long(ComposerError *error, const char *path, const Motive *motive, const PlanMeter *meter,
                                   size_t theme_bars, size_t idea_bars);

/* COMPOSER_OK when name is a style category a style can be read without, else a request error at /style_without. */
ComposerStatus api_style_category_check(const char *name, ComposerError *error);

/* The names of the style categories a style can be read without: adds name to names (at most capacity; a repeat is ignored). */
ComposerStatus api_style_without_add(const char **names, size_t *count, size_t capacity, const char *name,
                                     ComposerError *error);

/* --- seeding from a corpus ---------------------------------------------------------------------------- */

/* The motive of a corpus passage like the passage file like_path, in the meter and key. */
ComposerStatus api_seed_motive(const char *corpus_dir, const char *like_path, const PlanMeter *meter, const AnalysisKey *key,
                               unsigned long seed, CorpusSeed *out, ComposerError *error);
/* The seed as the plan records it. */
ComposerStatus api_seed_source(const CorpusSeed *seed, PlanSource *out, ComposerError *error);
/* "3 stale documents in '<dir>' (analyzed by another version; run the corpus again)", or "" when none is stale. */
void api_seed_stale_notice(const CorpusSeed *seed, const char *corpus_dir, char *out, size_t size);

/* The first node of the plan with the name, or PLAN_NODE_NONE. */
NodeId api_find_node(const PiecePlan *plan, const char *name);

/* --- the result ------------------------------------------------------------------------------------ */

/* What a `theme` request adds to the result: the motive of each slot and idea by id, the chords as the request gave them. */
typedef struct {
    const char *slot_motive_id[GROW_SLOT_COUNT]; /* NULL for none */
    const char *idea_motive_id[GROW_IDEA_COUNT];
    const cJSON *chords; /* the request's "chords" array, or NULL */
} GrowThemeInfo;

/*
 * The result document: {key, meter, bars, into, pairing, cadence, ..., tempo, [brief,] themes: [{seed, structure, chords,
 * developments, [slots,] score, requested, heard, heard_as_requested, heard_text, [style_distance]}]}. info is NULL for
 * grow. The members after "tempo" (brief and chords) are the theme command's, with info.
 */
cJSON *api_grow_result_json(const GrowSetup *setup, const GrowRun *run, const GrowThemeInfo *info);

/* The Analyzer's distance from the style as {dimension: value, ..., overall}, values to four places. */
cJSON *api_style_distance_json(const StyleComparison *comparison);
/* The corpus passage a motive was taken from, as {piece, start, end, score}. */
cJSON *api_seed_json(const CorpusSeed *seed);
/* {"tempo":..., "mode":..., "density":..., "without": [...]} of a brief. */
cJSON *api_brief_json(const GrowBrief *brief);

/* The brief of a setup, for the result and the text output. */
void api_grow_brief(const GrowSetup *setup, GrowBrief *out);

#endif
