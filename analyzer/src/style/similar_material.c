#include <ursatz_analyzer/similar_material.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>

#include <ursatz_analyzer/analysis_document.h>
#include <ursatz_analyzer/analysis_pipeline.h>
#include <ursatz_analyzer/input_pipeline.h>
#include <ursatz_analyzer/style_labels.h>

#define DOCUMENT_SCHEMA_VERSION 9

/* ---- reading the document ---- */

static const cJSON *member(const cJSON *object, const char *name) {
    return cJSON_IsObject(object) ? cJSON_GetObjectItemCaseSensitive(object, name) : NULL;
}

static bool number_member(const cJSON *object, const char *name, long *out) {
    const cJSON *item = member(object, name);
    if (!cJSON_IsNumber(item)) {
        return false;
    }
    *out = (long)item->valuedouble;
    return true;
}

static bool fraction_member(const cJSON *object, const char *name, unsigned long *numerator, unsigned long *denominator) {
    const cJSON *fraction = member(object, name);
    long n, d;
    if (!number_member(fraction, "num", &n) || !number_member(fraction, "den", &d) || d <= 0 || n < 0) {
        return false;
    }
    *numerator = (unsigned long)n;
    *denominator = (unsigned long)d;
    return true;
}

/* A position as a measure and a beat (the beat plus the fraction of it). */
static bool location_member(const cJSON *entry, const char *edge, int *measure, double *beat) {
    const cJSON *position = member(member(entry, "location"), edge);
    long m, b, n, d;
    if (!number_member(position, "measure", &m) || !number_member(position, "beat", &b) ||
        !number_member(member(position, "subdivision"), "num", &n) ||
        !number_member(member(position, "subdivision"), "den", &d) || d <= 0) {
        return false;
    }
    *measure = (int)m;
    *beat = (double)b + (double)n / (double)d;
    return true;
}

__extension__ typedef unsigned __int128 WideInt;

static WideInt wide_gcd(WideInt a, WideInt b) {
    while (b) {
        WideInt t = a % b;
        a = b;
        b = t;
    }
    return a;
}

/* a - b as a fraction in lowest terms, for a >= b. false when the result does not fit. */
static bool rational_difference(unsigned long an, unsigned long ad, unsigned long bn, unsigned long bd, unsigned long *n,
                                unsigned long *d) {
    WideInt left = (WideInt)an * bd, right = (WideInt)bn * ad;
    if (left < right) {
        return false;
    }
    WideInt numerator = left - right, denominator = (WideInt)ad * bd;
    WideInt divisor = wide_gcd(numerator, denominator);
    if (divisor > 1) {
        numerator /= divisor;
        denominator /= divisor;
    }
    if (numerator > (unsigned long)-1 || denominator > (unsigned long)-1) {
        return false;
    }
    *n = (unsigned long)numerator;
    *d = (unsigned long)denominator;
    return true;
}

/* A soprano note of a document, at its absolute position. */
typedef struct {
    int pitch;
    unsigned long onset_n, onset_d;
    unsigned long duration_n, duration_d;
    int start_measure, end_measure;
    double start_beat, end_beat;
} DocNote;

/*
 * The soprano's notes in time order. The document has one soprano entry per chord segment, so a note held across
 * several segments is one note and its end is the end of its last segment. Entries that are not present are skipped.
 * *out is NULL when *count is 0. false on allocation failure or an entry that cannot be read.
 */
static bool read_notes(const cJSON *document, DocNote **out, size_t *count) {
    const cJSON *soprano = member(member(document, "outer_voices"), "soprano");
    size_t capacity = cJSON_IsArray(soprano) ? (size_t)cJSON_GetArraySize(soprano) : 0;
    *out = NULL;
    *count = 0;
    if (capacity == 0) {
        return true;
    }
    DocNote *notes = malloc(capacity * sizeof *notes);
    if (!notes) {
        return false;
    }
    size_t n = 0;
    bool have_previous = false;
    long previous_id = 0;
    const cJSON *entry;
    cJSON_ArrayForEach(entry, soprano) {
        const cJSON *status = member(entry, "status");
        long id, pitch;
        if (!cJSON_IsString(status) || strcmp(status->valuestring, "present") != 0 || !number_member(entry, "note_id", &id) ||
            !number_member(member(entry, "pitch"), "midi", &pitch)) {
            continue;
        }
        if (have_previous && id == previous_id) {
            if (!location_member(entry, "end", &notes[n - 1].end_measure, &notes[n - 1].end_beat)) {
                free(notes);
                return false;
            }
            continue;
        }
        DocNote *note = &notes[n];
        const cJSON *onset = member(entry, "onset");
        if (!fraction_member(onset, "time", &note->onset_n, &note->onset_d) ||
            !fraction_member(member(entry, "duration"), "whole_notes", &note->duration_n, &note->duration_d) ||
            note->duration_n == 0 || !location_member(entry, "start", &note->start_measure, &note->start_beat) ||
            !location_member(entry, "end", &note->end_measure, &note->end_beat)) {
            free(notes);
            return false;
        }
        note->pitch = (int)pitch;
        have_previous = true;
        previous_id = id;
        n++;
    }
    if (n == 0) {
        free(notes);
        return true;
    }
    *out = notes;
    *count = n;
    return true;
}

/* The notes first..first+count-1 as a passage: onsets from the first note's onset. NULL on allocation failure or an
   onset that does not fit. */
static SimilarNote *passage_notes(const DocNote *notes, size_t first, size_t count) {
    SimilarNote *out = malloc(count * sizeof *out);
    if (!out) {
        return NULL;
    }
    const DocNote *base = &notes[first];
    for (size_t i = 0; i < count; i++) {
        const DocNote *note = &notes[first + i];
        SimilarNote *target = &out[i];
        if (!rational_difference(note->onset_n, note->onset_d, base->onset_n, base->onset_d, &target->onset_numerator,
                                 &target->onset_denominator)) {
            free(out);
            return NULL;
        }
        if (target->onset_numerator == 0) {
            target->onset_denominator = 1;
        }
        target->pitch = note->pitch;
        target->duration_numerator = note->duration_n;
        target->duration_denominator = note->duration_d;
    }
    return out;
}

/* ---- queries ---- */

void similar_query_destroy(SimilarQuery *query) {
    if (!query) {
        return;
    }
    free(query->notes);
    for (size_t i = 0; i < query->numeral_count; i++) {
        free(query->numerals[i]);
    }
    free(query->numerals);
    *query = (SimilarQuery){0};
}

SimilarResult similar_query_from_file(const char *path, SimilarQuery *query) {
    if (!path || !query) {
        return SIMILAR_ERR_ARGUMENT;
    }
    ImportedNotes imported;
    if (input_pipeline_import_file(path, INPUT_FORMAT_AUTO, &imported) != INPUT_PIPELINE_OK) {
        return SIMILAR_ERR_IO;
    }
    AnalysisReport report;
    if (!run_analysis_pipeline(&imported, &report)) {
        imported_notes_destroy(&imported);
        return SIMILAR_ERR_ALLOC;
    }
    AnalysisDocument *built = analysis_document_build(path, &imported, &report);
    analysis_report_destroy(&report);
    imported_notes_destroy(&imported);
    char *json = built ? analysis_document_to_json(built) : NULL;
    analysis_document_destroy(built);
    if (!json) {
        return SIMILAR_ERR_ALLOC;
    }
    cJSON *document = cJSON_Parse(json);
    free(json);
    long version;
    if (!document || !number_member(document, "schema_version", &version) || version != DOCUMENT_SCHEMA_VERSION) {
        cJSON_Delete(document);
        return SIMILAR_ERR_FORMAT;
    }
    DocNote *notes;
    size_t count;
    bool read = read_notes(document, &notes, &count);
    cJSON_Delete(document);
    if (!read) {
        return SIMILAR_ERR_ALLOC;
    }
    if (count < 2) {
        free(notes);
        return SIMILAR_ERR_QUERY;
    }
    SimilarNote *line = passage_notes(notes, 0, count);
    free(notes);
    if (!line) {
        return SIMILAR_ERR_ALLOC;
    }
    *query = (SimilarQuery){SIMILAR_QUERY_MOTIVE, line, count, NULL, 0};
    return SIMILAR_OK;
}

SimilarResult similar_query_from_numerals(const char *text, SimilarQuery *query) {
    if (!text || !query) {
        return SIMILAR_ERR_ARGUMENT;
    }
    char **numerals = NULL;
    size_t count = 0;
    SimilarResult result = SIMILAR_OK;
    const char *cursor = text;
    while (*cursor && result == SIMILAR_OK) {
        size_t length = strcspn(cursor, " \t-");
        if (length > 0) {
            char *token = strndup(cursor, length);
            char *label = token ? style_label_numeral(token) : NULL;
            free(token);
            char **grown = label ? realloc(numerals, (count + 1) * sizeof *numerals) : NULL;
            if (!label) {
                result = token ? SIMILAR_ERR_QUERY : SIMILAR_ERR_ALLOC;
            } else if (!grown) {
                free(label);
                result = SIMILAR_ERR_ALLOC;
            } else {
                numerals = grown;
                numerals[count++] = label;
            }
        }
        cursor += length;
        if (*cursor) {
            cursor++;
        }
    }
    if (result == SIMILAR_OK && count < 2) {
        result = SIMILAR_ERR_QUERY;
    }
    if (result != SIMILAR_OK) {
        for (size_t i = 0; i < count; i++) {
            free(numerals[i]);
        }
        free(numerals);
        return result;
    }
    *query = (SimilarQuery){SIMILAR_QUERY_PROGRESSION, NULL, 0, numerals, count};
    return SIMILAR_OK;
}
