#include "pipeline/progression_pipeline.h"

#include <stdlib.h>
#include <string.h>

#include "analysis/progression_patterns.h"

/* Ursatz's corpus_stats_chord_progressions joins a pattern's numerals with this. */
#define PROGRESSION_SEPARATOR " > "

/* The report's chords as the kernel's entries: the base numeral of a labelled chord in a key region, the key region as
   the group. An unlabelled chord, or one outside every region, ends the block before it. */
static ProgressionEntry entry_of(const AnalysisReportEntry *entry) {
    bool labelled = entry->in_region && entry->roman.labelled && entry->roman.base_numeral != NULL;
    return (ProgressionEntry){labelled ? entry->roman.base_numeral : NULL, labelled ? (long)entry->region : -1};
}

static void progression_free(AnalysisProgression *progression) {
    for (size_t n = 0; n < progression->length; n++) {
        free(progression->pattern[n]);
    }
    free(progression->pattern);
    free(progression->occurrences);
}

/* Copies one kernel pattern into out. Returns false only on allocation failure (out then holds what was built). */
static bool progression_make(const ProgressionPattern *pattern, AnalysisProgression *out) {
    *out = (AnalysisProgression){0};
    out->length = pattern->length;
    out->pattern = calloc(pattern->length, sizeof(char *));
    out->occurrences = calloc(pattern->occurrence_count, sizeof(AnalysisProgressionOccurrence));
    if (!out->pattern || !out->occurrences) {
        return false;
    }
    out->occurrence_count = pattern->occurrence_count;
    for (size_t n = 0; n < pattern->length; n++) {
        out->pattern[n] = strdup(pattern->labels[n]);
        if (!out->pattern[n]) {
            return false;
        }
    }
    for (size_t o = 0; o < pattern->occurrence_count; o++) {
        out->occurrences[o] = (AnalysisProgressionOccurrence){pattern->occurrences[o].first, pattern->occurrences[o].last};
    }
    return true;
}

bool progression_stage_collect(AnalysisReport *report) {
    ProgressionEntry *entries = calloc(report->entry_count > 0 ? report->entry_count : 1, sizeof(ProgressionEntry));
    ProgressionPatterns found = {0};
    if (!entries) {
        return false;
    }
    for (size_t i = 0; i < report->entry_count; i++) {
        entries[i] = entry_of(&report->entries[i]);
    }
    bool ok = progression_patterns_find(entries, report->entry_count, PROGRESSION_MIN_LENGTH, PROGRESSION_MAX_LENGTH,
                                        &found) == PROGRESSION_PATTERNS_OK;
    if (ok) {
        report->progressions = calloc(found.count > 0 ? found.count : 1, sizeof(AnalysisProgression));
        ok = report->progressions != NULL;
    }
    for (size_t p = 0; ok && p < found.count; p++) {
        ok = progression_make(&found.items[p], &report->progressions[p]);
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
    progression_patterns_destroy(&found);
    free(entries);
    return ok;
}

char *progression_label(const AnalysisProgression *progression) {
    ProgressionPattern pattern = {(const char **)progression->pattern, progression->length, NULL, 0};
    return progression_pattern_label(&pattern, PROGRESSION_SEPARATOR);
}
