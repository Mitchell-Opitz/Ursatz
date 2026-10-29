#include "analysis/progression_patterns.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/* One chord: the entries first..last (a held chord is several) with one label. */
typedef struct {
    size_t first;
    size_t last;
    const char *label;
} Chord;

/* A maximal stretch of chords[first..first + count) in one group with no unlabelled entry between. */
typedef struct {
    size_t first;
    size_t count;
} Block;

/* A run of length chords starting at chords[start]. */
typedef struct {
    size_t start;
    size_t length;
} Run;

/* A pattern that occurs at least twice: runs[first..first + count) are its occurrences, in time order. */
typedef struct {
    size_t first;
    size_t count;
    size_t length;
    size_t start; /* the first occurrence's start chord, for ranking */
    bool dropped;
} Pattern;

typedef int (*ContextCompare)(const void *left, const void *right, const void *context);

/* Stable merge sort with a comparison context. Returns false when the scratch buffer cannot be allocated. */
static bool sort_with_context(void *base, size_t count, size_t size, ContextCompare compare, const void *context) {
    if (count < 2) {
        return true;
    }
    unsigned char *from = base;
    unsigned char *scratch = malloc(count * size);
    if (scratch == NULL) {
        return false;
    }
    unsigned char *to = scratch;
    for (size_t width = 1; width < count; width *= 2) {
        for (size_t low = 0; low < count; low += 2 * width) {
            size_t mid = low + width < count ? low + width : count;
            size_t high = low + 2 * width < count ? low + 2 * width : count;
            size_t l = low;
            size_t r = mid;
            size_t out = low;
            while (l < mid && r < high) {
                if (compare(from + r * size, from + l * size, context) < 0) {
                    memcpy(to + out++ * size, from + r++ * size, size);
                } else {
                    memcpy(to + out++ * size, from + l++ * size, size);
                }
            }
            while (l < mid) {
                memcpy(to + out++ * size, from + l++ * size, size);
            }
            while (r < high) {
                memcpy(to + out++ * size, from + r++ * size, size);
            }
        }
        unsigned char *swap = from;
        from = to;
        to = swap;
    }
    if (from != (unsigned char *)base) {
        memcpy(base, from, count * size);
    }
    free(scratch);
    return true;
}

static bool labelled(const ProgressionEntry *entry) {
    return entry->label != NULL && entry->group >= 0;
}

/* The labelled chords, held chords merged, and the blocks they fall in. false only on allocation failure. */
static bool build_chords(const ProgressionEntry *entries, size_t total, Chord *chords, size_t *chord_count,
                         Block *blocks, size_t *block_count) {
    *chord_count = 0;
    *block_count = 0;
    for (size_t i = 0; i < total; i++) {
        if (!labelled(&entries[i])) {
            continue;
        }
        bool continues = i > 0 && labelled(&entries[i - 1]) && entries[i - 1].group == entries[i].group;
        if (continues && strcmp(chords[*chord_count - 1].label, entries[i].label) == 0) {
            chords[*chord_count - 1].last = i; /* a held chord */
            continue;
        }
        if (!continues) {
            blocks[*block_count].first = *chord_count;
            blocks[*block_count].count = 0;
            (*block_count)++;
        }
        chords[*chord_count].first = i;
        chords[*chord_count].last = i;
        chords[*chord_count].label = entries[i].label;
        (*chord_count)++;
        blocks[*block_count - 1].count++;
    }
    return true;
}

/* Shorter patterns first, then by the labels. */
static int compare_pattern_of_runs(const Run *left, const Run *right, const Chord *chords) {
    if (left->length != right->length) {
        return left->length < right->length ? -1 : 1;
    }
    for (size_t n = 0; n < left->length; n++) {
        int order = strcmp(chords[left->start + n].label, chords[right->start + n].label);
        if (order != 0) {
            return order;
        }
    }
    return 0;
}

/* The runs by pattern, then by position. */
static int compare_runs(const void *a, const void *b, const void *context) {
    const Run *left = a;
    const Run *right = b;
    int order = compare_pattern_of_runs(left, right, context);
    if (order != 0) {
        return order;
    }
    return left->start < right->start ? -1 : (left->start > right->start ? 1 : 0);
}

/* Whether the pattern of inner appears inside the pattern of outer, which is longer. */
static bool contained_in(const Run *inner, const Run *outer, const Chord *chords) {
    for (size_t offset = 0; offset + inner->length <= outer->length; offset++) {
        bool same = true;
        for (size_t n = 0; same && n < inner->length; n++) {
            same = strcmp(chords[inner->start + n].label, chords[outer->start + offset + n].label) == 0;
        }
        if (same) {
            return true;
        }
    }
    return false;
}

/* Most occurrences first, then the longest, then the earliest. */
static int compare_patterns(const void *a, const void *b, const void *context) {
    const Pattern *left = a;
    const Pattern *right = b;
    (void)context;
    if (left->count != right->count) {
        return left->count > right->count ? -1 : 1;
    }
    if (left->length != right->length) {
        return left->length > right->length ? -1 : 1;
    }
    return left->start < right->start ? -1 : (left->start > right->start ? 1 : 0);
}

void progression_patterns_destroy(ProgressionPatterns *patterns) {
    if (patterns == NULL) {
        return;
    }
    for (size_t p = 0; p < patterns->count; p++) {
        free((void *)patterns->items[p].labels);
        free(patterns->items[p].occurrences);
    }
    free(patterns->items);
    patterns->items = NULL;
    patterns->count = 0;
}

/* Fills one pattern. false only on allocation failure (the pattern then owns what was built). */
static bool pattern_make(const Chord *chords, const Run *runs, const Pattern *pattern, ProgressionPattern *out) {
    memset(out, 0, sizeof(*out));
    out->labels = calloc(pattern->length, sizeof(char *));
    out->occurrences = calloc(pattern->count, sizeof(ProgressionOccurrence));
    if (out->labels == NULL || out->occurrences == NULL) {
        return false;
    }
    out->length = pattern->length;
    out->occurrence_count = pattern->count;
    const Run *first = &runs[pattern->first];
    for (size_t n = 0; n < pattern->length; n++) {
        out->labels[n] = chords[first->start + n].label;
    }
    for (size_t o = 0; o < pattern->count; o++) {
        const Run *run = &runs[pattern->first + o];
        out->occurrences[o].first = chords[run->start].first;
        out->occurrences[o].last = chords[run->start + run->length - 1].last;
    }
    return true;
}

ProgressionPatternsResult progression_patterns_find(const ProgressionEntry *entries, size_t count, size_t min_length,
                                                    size_t max_length, ProgressionPatterns *out) {
    if (out == NULL || (entries == NULL && count > 0) || min_length < 1 || min_length > max_length) {
        return PROGRESSION_PATTERNS_INVALID;
    }
    size_t alloc = count > 0 ? count : 1;
    Chord *chords = calloc(alloc, sizeof(Chord));
    Block *blocks = calloc(alloc, sizeof(Block));
    Run *runs = NULL;
    Pattern *patterns = NULL;
    ProgressionPattern *items = NULL;
    size_t chord_count = 0;
    size_t block_count = 0;
    size_t run_count = 0;
    size_t pattern_count = 0;
    size_t kept = 0;
    bool ok = chords != NULL && blocks != NULL;
    if (ok) {
        ok = build_chords(entries, count, chords, &chord_count, blocks, &block_count);
    }
    if (ok) {
        size_t total = 0;
        for (size_t b = 0; b < block_count; b++) {
            for (size_t length = min_length; length <= max_length && length <= blocks[b].count; length++) {
                total += blocks[b].count - length + 1;
            }
        }
        runs = calloc(total + 1, sizeof(Run));
        ok = runs != NULL;
    }
    for (size_t b = 0; ok && b < block_count; b++) {
        for (size_t length = min_length; length <= max_length && length <= blocks[b].count; length++) {
            for (size_t start = blocks[b].first; start + length <= blocks[b].first + blocks[b].count; start++) {
                runs[run_count].start = start;
                runs[run_count].length = length;
                run_count++;
            }
        }
    }
    if (ok) {
        ok = sort_with_context(runs, run_count, sizeof(Run), compare_runs, chords);
    }
    if (ok) {
        patterns = calloc(run_count + 1, sizeof(Pattern));
        ok = patterns != NULL;
    }
    for (size_t r = 0; ok && r < run_count;) {
        size_t end = r + 1;
        while (end < run_count && compare_pattern_of_runs(&runs[r], &runs[end], chords) == 0) {
            end++;
        }
        if (end - r >= 2) {
            patterns[pattern_count].first = r;
            patterns[pattern_count].count = end - r;
            patterns[pattern_count].length = runs[r].length;
            patterns[pattern_count].start = runs[r].start;
            patterns[pattern_count].dropped = false;
            pattern_count++;
        }
        r = end;
    }
    /* A pattern that a longer one with as many occurrences contains is part of it. */
    for (size_t p = 0; ok && p < pattern_count; p++) {
        for (size_t q = 0; !patterns[p].dropped && q < pattern_count; q++) {
            patterns[p].dropped = patterns[q].length > patterns[p].length && patterns[q].count == patterns[p].count &&
                                  contained_in(&runs[patterns[p].first], &runs[patterns[q].first], chords);
        }
    }
    for (size_t p = 0; ok && p < pattern_count; p++) {
        if (!patterns[p].dropped) {
            patterns[kept++] = patterns[p];
        }
    }
    if (ok) {
        ok = sort_with_context(patterns, kept, sizeof(Pattern), compare_patterns, NULL);
    }
    size_t made = 0;
    if (ok && kept > 0) {
        items = calloc(kept, sizeof(ProgressionPattern));
        ok = items != NULL;
    }
    for (size_t p = 0; ok && p < kept; p++) {
        ok = pattern_make(chords, runs, &patterns[p], &items[p]);
        made = p + 1;
    }
    if (ok) {
        out->items = items;
        out->count = kept;
    } else {
        for (size_t p = 0; p < made; p++) {
            free((void *)items[p].labels);
            free(items[p].occurrences);
        }
        free(items);
    }
    free(patterns);
    free(runs);
    free(chords);
    free(blocks);
    return ok ? PROGRESSION_PATTERNS_OK : PROGRESSION_PATTERNS_NO_MEMORY;
}

char *progression_pattern_label(const ProgressionPattern *pattern, const char *separator) {
    if (pattern == NULL || separator == NULL || (pattern->labels == NULL && pattern->length > 0)) {
        return NULL;
    }
    size_t size = 1;
    for (size_t n = 0; n < pattern->length; n++) {
        size += strlen(pattern->labels[n]) + strlen(separator);
    }
    char *label = malloc(size);
    if (label == NULL) {
        return NULL;
    }
    label[0] = '\0';
    for (size_t n = 0; n < pattern->length; n++) {
        if (n > 0) {
            strcat(label, separator);
        }
        strcat(label, pattern->labels[n]);
    }
    return label;
}
