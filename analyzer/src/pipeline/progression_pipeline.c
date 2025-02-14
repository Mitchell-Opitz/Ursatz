#include "pipeline/progression_pipeline.h"

#include <stdlib.h>
#include <string.h>

/* Ursatz's corpus_stats_chord_progressions joins a pattern's numerals with this. */
#define PROGRESSION_SEPARATOR " > "

/* One chord of a progression: the entries first..last (a held chord is several) with one base numeral. */
typedef struct {
    size_t first;
    size_t last;
    const char *base;
} Chord;

/* A maximal stretch of chords[first..first + count) in one key region with no unlabelled chord between. */
typedef struct {
    size_t first;
    size_t count;
} Block;

/* A run of `length` chords starting at chords[start]. */
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

typedef struct {
    Chord *chords;
    size_t chord_count;
    Run *runs;
    size_t run_count;
} Chords;

static bool labelled(const AnalysisReportEntry *entry) {
    return entry->in_region && entry->roman.labelled && entry->roman.base_numeral != NULL;
}

/* The report's labelled chords as chords, held chords merged, and the blocks they fall in. Returns false only on
   allocation failure. */
static bool build_chords(const AnalysisReport *report, Chord **chords, size_t *chord_count, Block **blocks,
                         size_t *block_count) {
    size_t total = report->entry_count;
    *chords = calloc(total > 0 ? total : 1, sizeof(Chord));
    *blocks = calloc(total > 0 ? total : 1, sizeof(Block));
    *chord_count = 0;
    *block_count = 0;
    if (!*chords || !*blocks) {
        return false;
    }
    for (size_t i = 0; i < total; i++) {
        const AnalysisReportEntry *entry = &report->entries[i];
        if (!labelled(entry)) {
            continue;
        }
        bool continues = i > 0 && labelled(&report->entries[i - 1]) && report->entries[i - 1].region == entry->region;
        if (continues && strcmp((*chords)[*chord_count - 1].base, entry->roman.base_numeral) == 0) {
            (*chords)[*chord_count - 1].last = i; /* a held chord */
            continue;
        }
        if (!continues) {
            (*blocks)[(*block_count)++] = (Block){*chord_count, 0};
        }
        (*chords)[(*chord_count)++] = (Chord){i, i, entry->roman.base_numeral};
        (*blocks)[*block_count - 1].count++;
    }
    return true;
}

/* The runs index into this array while qsort and same_pattern compare them. */
static const Chord *sort_chords;

/* Shorter patterns first, then by the numerals. */
static int compare_pattern_of_runs(const Run *left, const Run *right) {
    if (left->length != right->length) {
        return left->length < right->length ? -1 : 1;
    }
    for (size_t n = 0; n < left->length; n++) {
        int order = strcmp(sort_chords[left->start + n].base, sort_chords[right->start + n].base);
        if (order != 0) {
            return order;
        }
    }
    return 0;
}

/* The runs by pattern, then by position. */
static int compare_runs(const void *a, const void *b) {
    const Run *left = a;
    const Run *right = b;
    int order = compare_pattern_of_runs(left, right);
    if (order != 0) {
        return order;
    }
    return left->start < right->start ? -1 : (left->start > right->start ? 1 : 0);
}

/* Whether the pattern of runs[inner] appears inside the pattern of runs[outer], which is longer. */
static bool contained_in(const Run *inner, const Run *outer) {
    for (size_t offset = 0; offset + inner->length <= outer->length; offset++) {
        bool same = true;
        for (size_t n = 0; same && n < inner->length; n++) {
            same = strcmp(sort_chords[inner->start + n].base, sort_chords[outer->start + offset + n].base) == 0;
        }
        if (same) {
            return true;
        }
    }
    return false;
}

/* Most occurrences first, then the longest, then the earliest. */
static int compare_patterns(const void *a, const void *b) {
    const Pattern *left = a;
    const Pattern *right = b;
    if (left->count != right->count) {
        return left->count > right->count ? -1 : 1;
    }
    if (left->length != right->length) {
        return left->length > right->length ? -1 : 1;
    }
    return left->start < right->start ? -1 : (left->start > right->start ? 1 : 0);
}

static void progression_free(AnalysisProgression *progression) {
    for (size_t n = 0; n < progression->length; n++) {
        free(progression->pattern[n]);
    }
    free(progression->pattern);
    free(progression->occurrences);
}

/* Copies one pattern into out. Returns false only on allocation failure (out then holds what was built). */
static bool progression_make(const Chords *all, const Pattern *pattern, AnalysisProgression *out) {
    *out = (AnalysisProgression){0};
    out->length = pattern->length;
    out->pattern = calloc(pattern->length, sizeof(char *));
    out->occurrences = calloc(pattern->count, sizeof(AnalysisProgressionOccurrence));
    if (!out->pattern || !out->occurrences) {
        return false;
    }
    out->occurrence_count = pattern->count;
    const Run *first = &all->runs[pattern->first];
    for (size_t n = 0; n < pattern->length; n++) {
        out->pattern[n] = strdup(all->chords[first->start + n].base);
        if (!out->pattern[n]) {
            return false;
        }
    }
    for (size_t o = 0; o < pattern->count; o++) {
        const Run *run = &all->runs[pattern->first + o];
        out->occurrences[o] = (AnalysisProgressionOccurrence){all->chords[run->start].first,
                                                              all->chords[run->start + run->length - 1].last};
    }
    return true;
}

bool progression_stage_collect(AnalysisReport *report) {
    Chords all = {0};
    Block *blocks = NULL;
    size_t block_count = 0;
    Pattern *patterns = NULL;
    bool ok = build_chords(report, &all.chords, &all.chord_count, &blocks, &block_count);
    if (ok) {
        all.runs = calloc(all.chord_count * (PROGRESSION_MAX_LENGTH - PROGRESSION_MIN_LENGTH + 1) + 1, sizeof(Run));
        ok = all.runs != NULL;
    }
    for (size_t b = 0; ok && b < block_count; b++) {
        for (size_t length = PROGRESSION_MIN_LENGTH; length <= PROGRESSION_MAX_LENGTH; length++) {
            for (size_t start = blocks[b].first; start + length <= blocks[b].first + blocks[b].count; start++) {
                all.runs[all.run_count++] = (Run){start, length};
            }
        }
    }
    if (ok) {
        sort_chords = all.chords;
        qsort(all.runs, all.run_count, sizeof(Run), compare_runs);
        patterns = calloc(all.run_count + 1, sizeof(Pattern));
        ok = patterns != NULL;
    }
    size_t pattern_count = 0;
    for (size_t r = 0; ok && r < all.run_count;) {
        size_t end = r + 1;
        while (end < all.run_count && compare_pattern_of_runs(&all.runs[r], &all.runs[end]) == 0) {
            end++;
        }
        if (end - r >= 2) {
            patterns[pattern_count++] = (Pattern){r, end - r, all.runs[r].length, all.runs[r].start, false};
        }
        r = end;
    }
    /* A pattern whose occurrences are all inside the occurrences of a longer pattern is part of it. */
    for (size_t p = 0; ok && p < pattern_count; p++) {
        for (size_t q = 0; !patterns[p].dropped && q < pattern_count; q++) {
            patterns[p].dropped = patterns[q].length > patterns[p].length && patterns[q].count == patterns[p].count &&
                                  contained_in(&all.runs[patterns[p].first], &all.runs[patterns[q].first]);
        }
    }
    size_t kept = 0;
    for (size_t p = 0; ok && p < pattern_count; p++) {
        if (!patterns[p].dropped) {
            patterns[kept++] = patterns[p];
        }
    }
    if (ok) {
        qsort(patterns, kept, sizeof(Pattern), compare_patterns);
        report->progressions = calloc(kept > 0 ? kept : 1, sizeof(AnalysisProgression));
        ok = report->progressions != NULL;
    }
    for (size_t p = 0; ok && p < kept; p++) {
        ok = progression_make(&all, &patterns[p], &report->progressions[p]);
        report->progression_count = p + 1;
    }
    if (!ok && report->progressions) {
        for (size_t p = 0; p < report->progression_count; p++) {
            progression_free(&report->progressions[p]);
        }
        free(report->progressions);
        report->progressions = NULL;
        report->progression_count = 0;
    }
    free(patterns);
    free(all.runs);
    free(all.chords);
    free(blocks);
    return ok;
}

char *progression_label(const AnalysisProgression *progression) {
    size_t size = 1;
    for (size_t n = 0; n < progression->length; n++) {
        size += strlen(progression->pattern[n]) + strlen(PROGRESSION_SEPARATOR);
    }
    char *label = malloc(size);
    if (!label) {
        return NULL;
    }
    label[0] = '\0';
    for (size_t n = 0; n < progression->length; n++) {
        if (n > 0) {
            strcat(label, PROGRESSION_SEPARATOR);
        }
        strcat(label, progression->pattern[n]);
    }
    return label;
}
