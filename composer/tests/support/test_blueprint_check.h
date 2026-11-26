#ifndef URSATZ_COMPOSER_TEST_BLUEPRINT_CHECK_H
#define URSATZ_COMPOSER_TEST_BLUEPRINT_CHECK_H

#include <cJSON.h>
#include <theory/tonality/spelled_key.h>

/*
 * The checks of a theme composed from a blueprint (P69b R-740, P69c R-757), shared by the reference skeleton test and the sweep of the blueprint
 * generator: the theme's melody and chords read from its plan (test_blueprint_read_theme) and every fixed item of the blueprint held
 * (test_blueprint_check_fixed): each bar's chords are the given ones at their beats; a bar with a rhythm has exactly its onsets and durations; an
 * anchor's note has its pitch; an event's note is not a tone of its chord and moves by step; the climax note has its pitch and no other note
 * reaches it; the accompaniment is the given pattern. A check that fails prints the case and the seed and aborts.
 */

#define WHOLE 96 /* ticks a whole note: a sixteenth is 6, a dotted sixteenth 9 */
#define NOTES_MAX 512
#define CHORDS_MAX 64
#define ITEMS_MAX 32

typedef struct {
    long onset; /* ticks from the theme's start */
    long duration;
    int pitch;
} Note;

typedef struct {
    long onset;
    long end;
    char label[32];
} Chord;

/* What the theme is read with: the meter, the key and the plan's melody and chords. */
typedef struct {
    long bar;  /* ticks a bar */
    long beat; /* ticks a beat of the meter's unit (6/8: an eighth) */
    long bars;
    SpelledKey key;
    Note notes[NOTES_MAX];
    size_t note_count;
    Chord chords[CHORDS_MAX];
    size_t chord_count;
} Theme;

typedef struct {
    const char *name;
    unsigned seed;
} Where;

/* Reads the melody (the lane's voice 0 inside the theme) and the given chords of the plan, in time order, for the request's meter, key and bars. */
void test_blueprint_read_theme(const char *plan_json, const cJSON *request, Theme *theme);

/* Every fixed item of the request's `blueprint` member held in the plan (the request needs `meter`, `key`, `material` and `bars` as a theme request has them). */
void test_blueprint_check_fixed(const Where *where, const char *plan_json, const cJSON *request);

#endif
