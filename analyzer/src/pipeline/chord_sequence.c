#include <ursatz_analyzer/chord_sequence.h>

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>

#include <ursatz_analyzer/analysis_document.h>
#include <ursatz_analyzer/analysis_report.h>
#include <ursatz_analyzer/style_labels.h>

#include "pipeline/chord_sequence_internal.h"
#include "pitch/pitch_arithmetic.h"
#include "util/json_util.h"

/* ---- the join rule ---- */

typedef struct {
    AnalyzerChord *chords;
    size_t count;
    size_t capacity;
    long previous_region; /* the key region of the last entry that was a chord */
    bool joined;          /* the last chord can take the next entry */
} Joiner;

typedef enum { JOIN_OK, JOIN_ALLOC } JoinResult;

static void joiner_init(Joiner *joiner, size_t capacity) {
    *joiner = (Joiner){NULL, 0, capacity, -1, false};
}

/* An entry that is not a chord (no numeral, root or key region): it ends the run. */
static void joiner_skip(Joiner *joiner) {
    joiner->joined = false;
}

static bool root_pitch_class(const char *name, int *out) {
    PitchLetter letter;
    int accidental;
    if (!name || !pitch_spelling_parse_name(name, &letter, &accidental, NULL)) {
        return false;
    }
    *out = pitch_class_of_letter(letter, accidental);
    return true;
}

/* Takes the entry as a chord, or joins it to the chord before it. base is the report's root-position numeral and root
   the chord's root name; an entry without either is skipped. */
static JoinResult joiner_add(Joiner *joiner, size_t entry, const char *base, const char *root_name, long region,
                             Rational start, Rational end) {
    int root;
    if (!base || !root_pitch_class(root_name, &root)) {
        joiner_skip(joiner);
        return JOIN_OK;
    }
    char *numeral = style_label_numeral(base);
    if (!numeral) {
        joiner_skip(joiner);
        return JOIN_OK;
    }
    if (region != joiner->previous_region) {
        joiner->joined = false;
    }
    joiner->previous_region = region;
    if (joiner->joined) {
        AnalyzerChord *last = &joiner->chords[joiner->count - 1];
        if (strcmp(last->numeral, numeral) == 0 && last->root_pitch_class == root) {
            last->last_entry = entry;
            last->end = end;
            free(numeral);
            return JOIN_OK;
        }
    }
    if (joiner->count == joiner->capacity) {
        size_t capacity = joiner->capacity ? joiner->capacity * 2 : 16;
        AnalyzerChord *grown = realloc(joiner->chords, capacity * sizeof *grown);
        if (!grown) {
            free(numeral);
            return JOIN_ALLOC;
        }
        joiner->chords = grown;
        joiner->capacity = capacity;
    }
    joiner->chords[joiner->count++] = (AnalyzerChord){numeral, root, region, entry, entry, start, end};
    joiner->joined = true;
    return JOIN_OK;
}

static void joiner_fail(Joiner *joiner) {
    analyzer_chord_sequence_free(joiner->chords, joiner->count);
    joiner->chords = NULL;
    joiner->count = 0;
}

static void joiner_finish(Joiner *joiner, AnalyzerChord **out, size_t *count) {
    if (joiner->count == 0) {
        free(joiner->chords);
        joiner->chords = NULL;
    }
    *out = joiner->chords;
    *count = joiner->count;
}

/* ---- from a result ---- */

AnalyzerStatus analyzer_chord_sequence(const AnalyzerResult *result, AnalyzerChord **out, size_t *count) {
    if (!out || !count) {
        return ANALYZER_ERR_ARGUMENT;
    }
    *out = NULL;
    *count = 0;
    if (!result) {
        return ANALYZER_ERR_ARGUMENT;
    }
    const AnalysisReport *report = analyzer_result_report(result);
    Joiner joiner;
    joiner_init(&joiner, report->entry_count);
    if (report->entry_count > 0) {
        joiner.chords = malloc(report->entry_count * sizeof *joiner.chords);
        if (!joiner.chords) {
            return ANALYZER_ERR_ALLOC;
        }
    }
    for (size_t i = 0; i < report->entry_count; i++) {
        const AnalysisReportEntry *entry = &report->entries[i];
        if (!entry->roman.labelled || !(entry->in_region && entry->region < report->key_region_count)) {
            joiner_skip(&joiner);
            continue;
        }
        Rational start, end;
        if (rational_make(entry->time_numerator, (long long)entry->time_denominator, &start) != RATIONAL_OK ||
            rational_make(entry->end_numerator, (long long)entry->end_denominator, &end) != RATIONAL_OK) {
            joiner_fail(&joiner);
            return ANALYZER_ERR_ARGUMENT;
        }
        if (joiner_add(&joiner, i, entry->roman.base_numeral, entry->chord.root, (long)entry->region, start, end) !=
            JOIN_OK) {
            joiner_fail(&joiner);
            return ANALYZER_ERR_ALLOC;
        }
    }
    joiner_finish(&joiner, out, count);
    return ANALYZER_OK;
}

/* ---- from a document ---- */

static bool fraction_member(const cJSON *object, const char *name, Rational *out) {
    const cJSON *fraction = json_member(object, name);
    long n, d;
    return json_number_member(fraction, "num", &n) && json_number_member(fraction, "den", &d) && d > 0 && n >= 0 &&
           rational_make(n, d, out) == RATIONAL_OK;
}

AnalyzerStatus analyzer_chord_sequence_from_document(const char *document_json, AnalyzerChord **out, size_t *count) {
    if (!out || !count) {
        return ANALYZER_ERR_ARGUMENT;
    }
    *out = NULL;
    *count = 0;
    if (!document_json) {
        return ANALYZER_ERR_ARGUMENT;
    }
    cJSON *document = cJSON_Parse(document_json);
    long version;
    if (!document || !json_number_member(document, "schema_version", &version) ||
        version != URSATZ_ANALYZER_DOCUMENT_SCHEMA_VERSION) {
        cJSON_Delete(document);
        return ANALYZER_ERR_ARGUMENT;
    }
    AnalyzerStatus status = chord_sequence_from_tree(document, out, count);
    cJSON_Delete(document);
    return status;
}

AnalyzerStatus chord_sequence_from_tree(const cJSON *document, AnalyzerChord **out, size_t *count) {
    *out = NULL;
    *count = 0;
    const cJSON *entries = json_member(document, "chords");
    Joiner joiner;
    joiner_init(&joiner, cJSON_IsArray(entries) ? (size_t)cJSON_GetArraySize(entries) : 0);
    if (joiner.capacity > 0) {
        joiner.chords = malloc(joiner.capacity * sizeof *joiner.chords);
        if (!joiner.chords) {
            return ANALYZER_ERR_ALLOC;
        }
    }
    size_t index = 0;
    const cJSON *entry;
    cJSON_ArrayForEach(entry, entries) {
        const cJSON *roman = json_member(entry, "roman_numeral");
        const cJSON *base = json_member(roman, "base_numeral");
        const cJSON *root_name = json_member(entry, "root");
        long region;
        if (!cJSON_IsString(base) || !cJSON_IsString(root_name) || !json_number_member(roman, "key_region", &region)) {
            joiner_skip(&joiner);
            index++;
            continue;
        }
        Rational start, end;
        if (!fraction_member(json_member(entry, "span"), "start", &start) ||
            !fraction_member(json_member(entry, "span"), "end", &end)) {
            joiner_fail(&joiner);
            return ANALYZER_ERR_ARGUMENT;
        }
        if (joiner_add(&joiner, index, base->valuestring, root_name->valuestring, region, start, end) != JOIN_OK) {
            joiner_fail(&joiner);
            return ANALYZER_ERR_ALLOC;
        }
        index++;
    }
    joiner_finish(&joiner, out, count);
    return ANALYZER_OK;
}

void analyzer_chord_sequence_free(AnalyzerChord *chords, size_t count) {
    for (size_t i = 0; chords && i < count; i++) {
        free(chords[i].numeral);
    }
    free(chords);
}
