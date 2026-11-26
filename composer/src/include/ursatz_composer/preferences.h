#ifndef URSATZ_COMPOSER_PREFERENCES_H
#define URSATZ_COMPOSER_PREFERENCES_H

#include <stdbool.h>
#include <stddef.h>

#include <cJSON.h>
#include <ursatz_composer/critic.h>
#include <ursatz_composer/grow.h>
#include <ursatz_composer/harmony_chords.h>
#include <ursatz_composer/melody_fill.h>
#include <ursatz_composer/motive_blueprint.h>
#include <ursatz_composer/motive_random.h>

/*
 * The composer's settings (R-411): the numbers that shape what it makes, held in one document so that a listening
 * session can change one and compose again without a code change or a rebuild. Every member of the document is
 * optional and its default is the value the Composer used before the document existed, so with no preferences every
 * output is byte-identical to what it was (R-413). docs/preferences.md lists every key, its default and its limit.
 *
 * ComposerPreferences holds the candidates group (CandidatePreferences, R-673) and the melody group, the settings of the melody stage (MelodyFillParams): the range, the
 * non-chord tones, the density, where the climax falls and how it is rated, the search (attempts and keep), and the
 * weight each group of soft rules counts in a melody's score. The motif group is the settings of the segments motive
 * generator (MotiveRandomPreferences): the bars, pickup, shapes, chord plans, features, leap, rung moves, decorations and
 * durations it draws from. The development group is the fidelity minimum table of the theme's development
 * (GrowFidelityTable), the continuation settings (GrowContinuation), basic_as_written, the contrast settings (GrowContrast) and the functions settings (GrowFunctions).
 * The critic group is the critic's settings (CriticSettings, R-722): rank, candidates, keep_rejected, reverse and the weight of each evaluator.
 */
/*
 * The candidates group (R-673): how a set of random motives is chosen. variety 0 to 20 (default 0) is how much a motive
 * is chosen for being unlike the ones before it (0 leaves the draw as it was); pool 2 to 8 (default 4) is how many
 * candidates are drawn for each motive that is chosen among. Off by default.
 */
typedef struct {
    unsigned variety;
    unsigned pool;
} CandidatePreferences;

/*
 * The harmony group (P69): the settings of the phrase model's chords. mixture (R-729) is how often a predominant chord is borrowed
 * from another mode: share 0 to 1 as "n/d" (default 0/1, nothing drawn) and from, the relation words (1 to 6) whose materials it is
 * borrowed from, in order (default "parallel", "one step darker", "one step brighter"). color (R-727) is how often a chord of the phrase gets
 * a color type: share 0 to 1 as "n/d" (default 0/1) and types, the K2 ids (1 to 12) the type is drawn from (default the ten persichetti types).
 */
typedef struct {
    ComposerMixture mixture;
    ComposerColor color;
} HarmonyPreferences;

typedef struct {
    MelodyFillParams melody;
    MotiveRandomPreferences motif;
    GrowFidelityTable development;
    GrowContinuation continuation; /* the development group's continuation settings (phase L3, off by default) */
    bool basic_as_written;         /* the development group's basic_as_written (phase L3, off by default) */
    GrowContrast contrast;         /* the development group's contrast settings (phase L4, R-475: similar 1, the rest 0) */
    GrowFunctions functions;       /* the development group's functions settings (P67, R-710 to R-712: all off) */
    CandidatePreferences candidates; /* the candidates group (R-673, variety 0 and pool 4 by default) */
    CriticSettings critic;           /* the critic group (R-722): weights 1, nothing reversed, rank off */
    HarmonyPreferences harmony;      /* the harmony group (P69): mixture off */
    BlueprintPreferences blueprint;  /* the blueprint group (P69c, R-750): the weights of the blueprint generator */
} ComposerPreferences;

/*
 * The limits of the settings (docs/preferences.md), which the reader enforces and the API's vocabulary shows a client:
 * weights and weighted-list entries 0 to WEIGHT_MAX; climax_position and climax_slack 0 to SCALE_MAX; ratios n/d with
 * d up to RATIO_DENOMINATOR_MAX (the fidelity minimums up to FIDELITY_DENOMINATOR_MAX); attempts 1 to ATTEMPTS_MAX (keep
 * is 1 to MELODY_KEEP_MAX and at most attempts); range.low RANGE_LOW_MIN to RANGE_LOW_MAX, range.high at least
 * range.low + RANGE_SPAN_MIN and at most RANGE_HIGH_MAX; density 0 to DENSITY_MAX; climax_rise CLIMAX_RISE_MIN to
 * CLIMAX_RISE_MAX; pickup_one_in 0 to PICKUP_ONE_IN_MAX; accented_one_in 0 to ACCENTED_ONE_IN_MAX; long_nct_one_in 0 to LONG_NCT_ONE_IN_MAX; leaps_per_motif 0 to
 * LEAPS_PER_MOTIF_MAX; leap.low and leap.high LEAP_MIN to LEAP_MAX; syncopation_one_in 0 to SYNCOPATION_ONE_IN_MAX;
 * skips_per_motif 0 to SKIPS_PER_MOTIF_MAX; plan 1 to PLAN_MAX chords, each with a bar 1 or 2 and a beat 1 to PLAN_BEAT_MAX;
 * rhythm_repeat_bars 0 to RHYTHM_REPEAT_BARS_MAX (the L4 weights, closure, leaps, register and contrast, are weights).
 */
#define COMPOSER_PREFERENCES_WEIGHT_MAX 20
#define COMPOSER_PREFERENCES_SCALE_MAX 1000
#define COMPOSER_PREFERENCES_RATIO_DENOMINATOR_MAX 16
#define COMPOSER_PREFERENCES_ATTEMPTS_MAX 256
#define COMPOSER_PREFERENCES_RANGE_LOW_MIN 36
#define COMPOSER_PREFERENCES_RANGE_LOW_MAX 96
#define COMPOSER_PREFERENCES_RANGE_HIGH_MAX 108
#define COMPOSER_PREFERENCES_RANGE_SPAN_MIN 12
#define COMPOSER_PREFERENCES_DENSITY_MAX 4
#define COMPOSER_PREFERENCES_CLIMAX_RISE_MIN 1
#define COMPOSER_PREFERENCES_CLIMAX_RISE_MAX 12
#define COMPOSER_PREFERENCES_PICKUP_ONE_IN_MAX 64
#define COMPOSER_PREFERENCES_ACCENTED_ONE_IN_MAX 64
#define COMPOSER_PREFERENCES_LONG_NCT_ONE_IN_MAX 64
#define COMPOSER_PREFERENCES_SYNCOPATION_ONE_IN_MAX 64
#define COMPOSER_PREFERENCES_SKIPS_PER_MOTIF_MAX 11
#define COMPOSER_PREFERENCES_PLAN_MAX MOTIVE_RANDOM_PLAN_MAX
#define COMPOSER_PREFERENCES_PLAN_BEAT_MAX 12
#define COMPOSER_PREFERENCES_LEAPS_PER_MOTIF_MAX 11
#define COMPOSER_PREFERENCES_LEAP_MIN 3
#define COMPOSER_PREFERENCES_LEAP_MAX 12
#define COMPOSER_PREFERENCES_FIDELITY_DENOMINATOR_MAX 1000
#define COMPOSER_PREFERENCES_RHYTHM_REPEAT_BARS_MAX 16
#define COMPOSER_PREFERENCES_POOL_MIN 2
#define COMPOSER_PREFERENCES_POOL_MAX 8
#define COMPOSER_PREFERENCES_KEEP_REJECTED_MAX 16
#define COMPOSER_PREFERENCES_BLUEPRINT_RANGE_MIN 12
#define COMPOSER_PREFERENCES_BLUEPRINT_RANGE_MAX 24
#define COMPOSER_PREFERENCES_BLUEPRINT_CANDIDATES_MAX 8

#define COMPOSER_PREFERENCES_PATH_MAX 128
#define COMPOSER_PREFERENCES_MESSAGE_MAX 256

/* Writes the defaults, every setting the Composer used before preferences existed, into *out. */
void composer_preferences_default(ComposerPreferences *out);

/*
 * Reads the JSON object into *out. *out starts from the defaults and every member present is applied over them. A member
 * that is unknown, of the wrong type or outside its limit makes it return false, leaving *out as the defaults
 * with the members read so far, with path set to where it is, relative to the object (for example "/melody/keep"), and
 * message naming the limit. path and message may be NULL when the details are not wanted. Members of the document that
 * hold a null are refused like any other wrong type.
 */
bool composer_preferences_read(const cJSON *object, ComposerPreferences *out, char *path, size_t path_size, char *message,
                               size_t message_size);

/*
 * As composer_preferences_read, over the settings *out already holds instead of the defaults: every member of the object is applied as it
 * is, the members it leaves out keep their values (a preset's document over the defaults, a request's over a preset's, R-754). The same
 * refusals; on a refusal *out holds what was read before it.
 */
bool composer_preferences_apply(const cJSON *object, ComposerPreferences *out, char *path, size_t path_size, char *message,
                                size_t message_size);

/*
 * The full document for the preferences, every member present, in the shape docs/preferences.md describes: ratios as
 * "n/d" text and nct_types in the order passing, neighbor, appoggiatura, suspension, anticipation. The caller deletes
 * it with cJSON_Delete. NULL when memory runs out.
 */
cJSON *composer_preferences_write(const ComposerPreferences *preferences);

/*
 * The setting keys (R-672), for an application that edits settings by their dotted key ("melody.weights.melodic"). One
 * row per leaf of the default document, in document order, and motif.plan last of the motif group's (the one setting the
 * default document does not hold). type says how the value reads: an integer from min to max, a boolean, a ratio "n/d"
 * (max is its denominator limit), a list of words (words, NULL-ended, the names it may hold) or a chord plan. hard is
 * true for a setting that changes what a request may ask for or what a candidate may be, false for a preference (R-671).
 */
typedef enum { COMPOSER_KEY_INTEGER, COMPOSER_KEY_BOOLEAN, COMPOSER_KEY_RATIO, COMPOSER_KEY_WORDS, COMPOSER_KEY_PLAN } ComposerKeyType;

typedef struct {
    const char *key;
    ComposerKeyType type;
    int min;
    int max;
    bool hard;
    const char *const *words;
} ComposerPreferenceKey;

/* The key table, and in *count the number of rows. Static: the caller never frees it. */
const ComposerPreferenceKey *composer_preferences_keys(size_t *count);

/*
 * The rules a setting can override (R-672): the 13 soft groups of the melody stage, then its 7 hard rules, in enum
 * order, then the 8 evaluators of the critic. key is the setting that holds the rule: the weight of a soft group or of an
 * evaluator (critic.weights.<code>), melody.waive for a hard rule.
 */
typedef struct {
    const char *name;
    bool hard;
    const char *key;
} ComposerRule;

/* The rule table, and in *count the number of rows. Static: the caller never frees it. */
const ComposerRule *composer_preferences_rules(size_t *count);

#endif
