#ifndef MUSIC_ANALYSIS_PROGRESSION_PATTERNS_H
#define MUSIC_ANALYSIS_PROGRESSION_PATTERNS_H

#include <stddef.h>

/*
 * ProgressionPatterns (L8 analysis, theory-neutral): the chord progressions a piece repeats. It compares
 * labels as text and knows no numeral, key or chord: the caller names each chord entry (a Roman numeral, a
 * function, any string) and the group it belongs to (a key region).
 *
 * Input is one entry per chord in time order. An entry with a NULL label, or a group of -1, belongs to no
 * block and ends the block before it. Consecutive entries with the same label and group are one held chord.
 * A block is a maximal stretch of chords in one group with no such entry between. The patterns are the runs of
 * min_length to max_length chords inside blocks that occur at least twice. A pattern is dropped when a longer
 * one has the same number of occurrences and contains it. They are ordered by most occurrences, then longest,
 * then earliest first occurrence.
 */

typedef struct {
    const char *label; /* borrowed; NULL for an unlabelled entry */
    long group;        /* the key region; -1 for none */
} ProgressionEntry;

/* Entry indices of one occurrence; a held chord spans several entries. */
typedef struct {
    size_t first;
    size_t last;
} ProgressionOccurrence;

typedef struct {
    const char **labels;                /* owned array of length borrowed strings (the entries' labels) */
    size_t length;
    ProgressionOccurrence *occurrences; /* owned; in time order */
    size_t occurrence_count;
} ProgressionPattern;

typedef struct {
    ProgressionPattern *items; /* owned; NULL when count is 0 */
    size_t count;
} ProgressionPatterns;

typedef enum {
    PROGRESSION_PATTERNS_OK = 0,
    PROGRESSION_PATTERNS_INVALID,  /* NULL out, NULL entries with a count, or a length range outside 1 <= min <= max */
    PROGRESSION_PATTERNS_NO_MEMORY
} ProgressionPatternsResult;

#define PROGRESSION_PATTERN_MIN_LENGTH 2
#define PROGRESSION_PATTERN_MAX_LENGTH 6

/*
 * Finds the repeated progressions. The pattern labels borrow from entries, which must outlive *out. *out is
 * written on PROGRESSION_PATTERNS_OK only.
 */
ProgressionPatternsResult progression_patterns_find(const ProgressionEntry *entries, size_t count, size_t min_length,
                                                    size_t max_length, ProgressionPatterns *out);

/* Frees the pattern arrays (not the borrowed labels) and zeroes *patterns. NULL is a no-op. */
void progression_patterns_destroy(ProgressionPatterns *patterns);

/* The pattern's labels joined with separator (owned, free it); NULL for a NULL argument or no memory. */
char *progression_pattern_label(const ProgressionPattern *pattern, const char *separator);

#endif
