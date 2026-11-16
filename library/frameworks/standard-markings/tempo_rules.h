#ifndef URSATZ_FRAMEWORKS_STANDARD_MARKINGS_TEMPO_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_MARKINGS_TEMPO_RULES_H

#include <stdbool.h>
#include <stddef.h>
#include "time/metronome_marking.h"

/*
 * Tempo rules for standard-markings-v1. Tempo is how fast or slow a
 * composition is performed, given specifically by a metronome marking or
 * less specifically by an Italian word:
 *
 *   TEMPO_GROUPS     fast (allegretto, allegro, vivace, presto), medium
 *                    (moderato, andante), slow (adagio, largo, lento, grave);
 *                    -etto is a suffix meaning "little"
 *   TEMPO_ORDER      slowest to fastest: grave, largo, lento, adagio,
 *                    andante, moderato, allegretto, allegro, vivace, presto
 *   TEMPO_MODIFIERS  words such as assai ("very"), espressivo, and
 *                    cantabile may follow a tempo word ("andante cantabile")
 *   METRONOME        a metronome marking is in beats per minute: quarter =
 *                    60 is sixty quarter notes in a minute, one per second
 *   TEMPO_CHANGE     ritardando (rit.) is a gradual slowing, accelerando
 *                    (accel.) a gradual speeding up; trailing dots mean the
 *                    direction continues
 *   A_TEMPO          "a tempo" returns to the original tempo after a
 *                    ritardando or accelerando
 *
 * The slowest-to-fastest order is the CONVENTIONAL one. The grouping is
 * stated by the theory and the order within and across groups is
 * convention; neighbouring words overlap in real use (adagio and largo,
 * vivace and presto), so this framework asserts an ordinal rank only --
 * no bpm range is attached to any word. Words are matched
 * case-insensitively; anything unrecognized is reported as such.
 */

typedef enum {
    TEMPO_GROUP_UNKNOWN,
    TEMPO_GROUP_SLOW,
    TEMPO_GROUP_MEDIUM,
    TEMPO_GROUP_FAST
} TempoGroup;

typedef enum {
    TEMPO_CHANGE_NONE,
    TEMPO_CHANGE_GRADUALLY_SLOWER, /* ritardando, rit. */
    TEMPO_CHANGE_GRADUALLY_FASTER, /* accelerando, accel. */
    TEMPO_CHANGE_RETURN            /* a tempo */
} TempoChange;

#define STANDARD_MARKINGS_TEMPO_WORD_COUNT 10

/* TEMPO_ORDER: rank of a tempo word, 0 (grave, slowest) through 9
   (presto, fastest). False, leaving *out_rank untouched, for an
   unrecognized word. */
bool standard_markings_tempo_word_rank(const char *word, int *out_rank);

/* TEMPO_GROUPS: the group of a tempo word. */
TempoGroup standard_markings_tempo_group(const char *word);

/* TEMPO_MODIFIERS: assai, espressivo, cantabile. */
bool standard_markings_is_tempo_modifier(const char *word);

/*
 * TEMPO_MODIFIERS: a tempo indication text is a tempo word optionally
 * followed by modifier words ("allegro assai", "adagio espressivo").
 * Returns true and sets *out_rank to the tempo word's rank when text has
 * that shape. A modifier never changes the rank.
 */
bool standard_markings_tempo_text_rank(const char *text, int *out_rank);

/* TEMPO_CHANGE / A_TEMPO: classifies rit./ritardando, accel./accelerando
   (trailing dots and spaces ignored) and "a tempo". */
TempoChange standard_markings_tempo_change(const char *text);

#endif
