#ifndef URSATZ_COMPOSER_API_VOCABULARY_H
#define URSATZ_COMPOSER_API_VOCABULARY_H

#include <stdbool.h>
#include <stddef.h>

#include <cJSON.h>

#include <ursatz_composer/composer_api.h>
#include <ursatz_composer/grow.h>
#include <ursatz_composer/constraint.h>

/*
 * The named tables of the words a request is written in (R-267, R-270). Each table ends with an entry whose value is NULL.
 * The request readers and composer_api_vocabulary use these tables, so a list is written once. Private: not installed.
 */

typedef struct {
    const char *value; /* as a request writes it */
    const char *label; /* as an application shows it */
} ApiChoice;

/* The number of entries before the NULL one. */
size_t api_choice_count(const ApiChoice *table);
/* The index of the entry whose value is text (exactly), or -1. */
int api_choice_find(const ApiChoice *table, const char *text);
/* As api_choice_find, ignoring the case of text (how the readers compare a mode, an archetype, a pattern). */
int api_choice_find_ignore_case(const ApiChoice *table, const char *text);
/* The values as a message lists them: "a, b or c". */
void api_choice_list(const ApiChoice *table, char *out, size_t size);
/* The values joined for a message: separator between entries, last_separator before the last ("a, b or c"). */
void api_choice_join(const ApiChoice *table, const char *separator, const char *last_separator, char *out, size_t size);

extern const ApiChoice API_MODES[];            /* major, minor */
extern const ApiChoice API_PIECE_MODES[];      /* major, minor, style */
extern const ApiChoice API_METERS[];           /* 2/4, 3/4, 4/4, 6/8 */
extern const ApiChoice API_FUNCTIONS[];        /* beginning, continuation, contrast, cadential */
extern const ApiChoice API_DISTANCES[];        /* close, medium, far */
extern const ApiChoice API_THEME_POOLS[];      /* approved-and-unflagged, approved, none */
extern const ApiChoice API_ACCOMPANIMENT_PATTERNS[];
extern const ApiChoice API_PIECE_INTOS[];      /* sentence, period, hybrid */
extern const ApiChoice API_PICKUPS[];          /* the pickups an application offers, in beats */
extern const ApiChoice API_THEME_BARS[];       /* 3 to 16 */
extern const ApiChoice API_STYLE_WEIGHTS[];    /* 1/4, 1/2, 1: the weights offered (any fraction the readers accept still works) */

/* A beginning ("presentation") or an ending ("continuation") of a theme, with the ideas it is made of, in GrowIdea order. */
typedef struct {
    const char *value;
    const char *label;
    GrowIdea ideas[3];
    size_t idea_count;
} ApiThemePart;

extern const ApiThemePart API_THEME_BEGINNINGS[]; /* presentation, antecedent, cbi: in PlanThemeBeginning order, NULL ends */
extern const ApiThemePart API_THEME_ENDINGS[];    /* continuation, consequent, cadential: in PlanThemeEnding order, NULL ends */

/* The limits the readers hold and the vocabulary shows. */
#define API_TEMPO_MIN 20UL
#define API_TEMPO_MAX 400UL
#define API_PIECE_BEATS_MAX 4096UL
#define API_VARIANTS_COUNT_MAX 16
#define API_VARIANTS_DRAWS_PER_COUNT 4 /* with "functions" or "distance", up to this many chains for each variant asked for */
#define API_VARIANTS_SEED_MAX (4294967295UL - API_VARIANTS_COUNT_MAX * API_VARIANTS_DRAWS_PER_COUNT)

/* COMPOSER_ERR_REQUEST at path: the meter is not one of API_METERS ("meter '3/8' is not supported; use 2/4, 3/4, 4/4 or 6/8"). */
ComposerStatus api_fail_meter(ComposerError *error, const char *path, const char *text);

/* True for a cadence type a theme can end in (PAC, IAC or HC). */
bool api_theme_cadence_ok(int cadence_type);

/* Writes the vocabulary document into root (composer_api_vocabulary's json); false on out of memory. */
bool api_vocabulary_build(cJSON *root);

#endif
