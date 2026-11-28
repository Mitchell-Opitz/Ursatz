#ifndef URSATZ_COMPOSER_API_GROW_H
#define URSATZ_COMPOSER_API_GROW_H

#include <stdbool.h>
#include <stddef.h>

#include <cJSON.h>
#include <theory/tonality/common_practice_materials.h>

#include <ursatz_composer/analyzer_check.h>
#include <ursatz_composer/composer_api.h>
#include <ursatz_composer/critic.h>
#include <ursatz_composer/corpus_seed.h>
#include <ursatz_composer/explain.h>
#include <ursatz_composer/export.h>
#include <ursatz_composer/given.h>
#include <ursatz_composer/grow.h>
#include "api_seed.h"
#include "api_style.h"
#include "grow/grow_internal.h"
#include <ursatz_composer/grow_form.h>
#include <ursatz_composer/motive.h>
#include <plan/plan.h>
#include <ursatz_composer/style.h>

/*
 * The engine behind the theme API entry (docs/api.md): the options of a theme run, the seeds tried one after another, the
 * result document. Private to the library and the command line.
 */

/* At most four chords a bar in a theme of eight bars (the request's chords), of sixteen bars (a blueprint's, R-743). */
#define GROW_REQUEST_CHORDS 32
#define GROW_EXT_CHORDS 64

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
    const char *material;        /* a material word or "style"; NULL for none */
    const char *material_member; /* "material" or the old "mode", for the path of a refusal */
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
    SectionPairing pairing;
    SpelledKey key;
    PlanMeter meter;
    size_t bars;
    bool has_construction;                   /* a construction was given (api_grow_read_construction), reason GIVEN */
    SectionConstruction construction;
    CadenceType cadence;
    CadenceType first_cadence;
    bool has_accompaniment;
    PlanAccompaniment accompaniment;
    bool has_voicing;
    PlanVoicing voicing;
    Rational style_weight;
    GrowDevelopment development;
    const char *preset;                      /* the preset the request names (R-754), echoed in the brief; NULL for none; not owned */
    bool generate_blueprint;                 /* development "blueprint" (R-752): the Composer writes the theme's blueprint from the motif (R-751) */
    const BlueprintPreferences *blueprint_preferences; /* NULL for the defaults; not owned */
    const GrowContinuation *continuation;    /* NULL for the defaults; not owned */
    bool basic_as_written;                   /* a motif as long as the basic idea is it as written, over its own chords */
    const GrowContrast *contrast;            /* NULL for the defaults; not owned */
    const GrowFunctions *functions;          /* NULL for all off; not owned */
    const CriticSettings *critic;            /* NULL for no ranking; with critic->rank the critic chooses the themes (R-722); not owned */
    const MotiveRandomPreferences *motif;    /* NULL for the defaults; not owned */
    const Motive *motive;                    /* the basic motive */
    const Motive *accompaniment_motive;      /* NULL for none */
    const GrowMaterial *material;            /* NULL for grow itself */
    const struct GrowBlueprint *blueprint;   /* the theme request's blueprint (R-743), NULL for none; not owned */
    ComposerStyle *style;                    /* NULL for none; not owned */
    Rational tempo;                     /* a denominator of 0 for none */
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
    const MelodyFillParams *melody;          /* NULL for the defaults; not owned */
    const ComposerMixture *mixture;          /* the harmony group's mixture settings; NULL for the defaults; not owned */
    const ComposerColor *color;              /* the harmony group's color settings; NULL for the defaults; not owned */
    const VoicingEntry *block_voicing;       /* the accompaniment's voicing (R-731); NULL for none; not owned */
    const UrsatzJob *job;
    const KnowledgeSnapshot *knowledge;      /* the call context's; required, must outlive the run */
    const CommonPracticeMaterials *materials; /* its materials, resolved once for the call; read by api_grow_read_brief */
} GrowSetup;

/* The word of the setup's development as a brief and an explanation give it: "blueprint" when the Composer writes the blueprint, else the grow development's. */
const char *api_setup_development_name(const GrowSetup *setup);

/* What the development blueprint made for a theme and the theme keeps (R-751): the blueprint, its chords read, the run's melody range and the rhythms drawn. */
typedef struct GrowGenerated {
    GrowBlueprint blueprint;
    GivenChord chords[GROW_EXT_CHORDS];
    char numerals[GROW_EXT_CHORDS][PLAN_ROMAN_NUMERAL_MAX];
    size_t chord_count;
    MelodyFillParams melody;
    GrowBlueprintLine rhythm[GROW_BLUEPRINT_BARS];
    bool known[GROW_BLUEPRINT_BARS];
} GrowGenerated;

/* A theme made, with what the output of it needs. */
typedef struct {
    Plan *plan;
    GrowOutcome outcome;
    unsigned long seed;
    char *signature;
    AnalyzerCheck check; /* with check, the Analyzer's check of the theme; empty otherwise */
    bool checked;
    size_t passed_before; /* how many of the seeds passed over came before this theme */
    bool judged;          /* the run ranked: the critic's judgment of the theme and its observations */
    CriticJudgment judgment;
    GrowGenerated *generated; /* the blueprint the Composer wrote for the theme (development blueprint), else NULL; owned */
} GrowTheme;

typedef struct {
    GrowTheme themes[GROW_MAX_SUGGESTIONS];
    size_t made;
    ComposeRejection *passed; /* the seeds passed over, in the order tried (explain.h), kept for the explanation */
    size_t passed_count;
    long chord_bar;           /* the bar of the first target a stuck run left open when it holds given chords, else -1 */
    GrowRefusal refusal;      /* why the plan was refused before seeding, when it was */
    unsigned long last_seed;
    CriticRejected rejected[GROW_MAX_SUGGESTIONS]; /* with critic ranking: the candidates not kept, at most keep_rejected, in rank order */
    size_t rejected_count;
    bool ranked;                                   /* the run chose among candidates by the critic */
} GrowRun;

/*
 * The chords of a blueprint's bars (R-744), each bar's text read as an entry of the request's "chords" is, into out (`capacity` of them,
 * numerals alongside) and *count; a refusal names the bar's "chords" at /blueprint/bars/<bar>/chords. Shared by a request's blueprint and
 * the generated one (R-751).
 */
UrsatzStatus api_blueprint_chords(const ComposerChords *chords, const struct GrowBlueprint *blueprint, const PlanMeter *meter,
                                  const SpelledKey *key, size_t bars, GivenChord *out, char numerals[][PLAN_ROMAN_NUMERAL_MAX],
                                  size_t capacity, size_t *count, UrsatzError *error);

/*
 * Sets the tempo (from "option_tempo", else the style) and the mode of the key (mode "style" takes the style's) in out,
 * after the style is loaded. option_tempo has the tempo given with the request or NULL.
 */
UrsatzStatus api_grow_set_tempo_and_mode(GrowSetup *out, const Rational *option_tempo, UrsatzError *error);

/*
 * Tries the seeds one after another: a stuck run is passed over, and a theme that is the same as one already made is not
 * counted. URSATZ_ERR_UNSATISFIABLE when none can be made, URSATZ_ERR_CANCELLED when the job says so. The run holds
 * what was made, also on failure; destroy it either way.
 */
UrsatzStatus api_grow_run(const GrowSetup *setup, GrowRun *run, UrsatzError *error);

/*
 * The GrowRequest the theme is built from for a seed (the plan does not depend on it today): the setup's brief and its
 * material's motives and chords. The request points into the setup and the material, which must outlive it.
 */
void api_grow_request_for(const GrowSetup *setup, unsigned long seed, GrowRequest *request);
void api_grow_run_destroy(GrowRun *run);

/* --- seeding from a corpus ---------------------------------------------------------------------------- */

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

/* The brief of a setup, for the result and the text output. */
void api_grow_brief(const GrowSetup *setup, GrowBrief *out);

#endif
