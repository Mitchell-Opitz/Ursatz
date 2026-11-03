#ifndef URSATZ_COMPOSER_API_VOCABULARY_H
#define URSATZ_COMPOSER_API_VOCABULARY_H

#include <stdbool.h>
#include <stddef.h>

#include <cJSON.h>

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
/* The values joined for a message: separator between entries, last_separator before the last ("a, b or c"). */
void api_choice_join(const ApiChoice *table, const char *separator, const char *last_separator, char *out, size_t size);

extern const ApiChoice API_MODES[];            /* major, minor */
extern const ApiChoice API_PIECE_MODES[];      /* major, minor, style */
extern const ApiChoice API_METERS[];           /* 2/4, 3/4, 4/4, 6/8 */
extern const ApiChoice API_FUNCTIONS[];        /* beginning, continuation, contrast, cadential */
extern const ApiChoice API_DISTANCES[];        /* close, medium, far */
extern const ApiChoice API_THEME_SLOTS[];      /* basic, contrasting, continuation, cadential */
extern const ApiChoice API_THEME_POOLS[];      /* approved-and-unflagged, approved, none */
extern const ApiChoice API_ACCOMPANIMENT_PATTERNS[];
extern const ApiChoice API_PIECE_INTOS[];      /* sentence, period, hybrid */
extern const ApiChoice API_PICKUPS[];          /* the pickups an application offers, in beats */

/* The ideas a theme form has: the name, and the index in API_THEME_SLOTS of the slot whose motive the idea is made from. */
typedef struct {
    const char *name;
    int slot;
} ApiThemeIdea;
extern const ApiThemeIdea API_THEME_IDEAS[]; /* ends with a NULL name */

/* True for a cadence type a theme can end in (PAC, IAC or HC). */
bool api_theme_cadence_ok(int cadence_type);

/* Writes the vocabulary document into root (composer_api_vocabulary's json); false on out of memory. */
bool api_vocabulary_build(cJSON *root);

#endif
