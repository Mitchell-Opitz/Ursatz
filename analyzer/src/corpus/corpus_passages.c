#include <ursatz_analyzer/analyzer_api.h>
#include <ursatz_analyzer_internal/similar_material.h>

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>


#include "corpus/chord_sequence_internal.h"
#include <common/files.h>
#include <common/json.h>

/* The passage's minimum similarity, the same as the Composer's seeding has always asked for. */
#define MINIMUM_NUMERATOR 1UL
#define MINIMUM_DENOMINATOR 2UL

static void passage_clear(AnalyzerCorpusPassage *passage) {
    free(passage->piece);
    free(passage->notes);
    for (size_t i = 0; i < passage->chord_count; i++) {
        free(passage->chords[i].numeral);
        free(passage->chords[i].key_label);
    }
    free(passage->chords);
    memset(passage, 0, sizeof(*passage));
}

void analyzer_corpus_passages_destroy(AnalyzerCorpusPassages *passages) {
    if (!passages) {
        return;
    }
    for (size_t i = 0; i < passages->count; i++) {
        passage_clear(&passages->passages[i]);
    }
    free(passages->passages);
    memset(passages, 0, sizeof(*passages));
}

/* A location edge of a chord entry as a measure and a beat plus the fraction of it. */
static bool location_of(const cJSON *entry, const char *edge, int *measure, double *beat) {
    const cJSON *position = common_json_member(common_json_member(entry, "location"), edge);
    const cJSON *m = common_json_member(position, "measure");
    const cJSON *b = common_json_member(position, "beat");
    long long n, d;
    if (!cJSON_IsNumber(m) || !cJSON_IsNumber(b) ||
        !common_json_rational_read(common_json_member(position, "subdivision"), COMMON_RATIONAL_OBJECT, &n, &d)) {
        return false;
    }
    *measure = (int)m->valuedouble;
    *beat = b->valuedouble + (double)n / (double)d;
    return true;
}

/*
 * The chords of a document in time order with a root-position numeral: the joined chord sequence, so an index here is an
 * index there. The place and key of a chord are those of the entry it starts at; one whose place cannot be read is left out.
 * false on allocation failure.
 */
static bool read_chords(const cJSON *document, AnalyzerPassageChord **out, size_t *count) {
    *out = NULL;
    *count = 0;
    AnalyzerChord *sequence = NULL;
    size_t sequence_count = 0;
    UrsatzStatus status = chord_sequence_from_tree(document, &sequence, &sequence_count);
    if (status != URSATZ_OK) {
        return status != URSATZ_ERR_ALLOC;
    }
    if (sequence_count == 0) {
        return true;
    }
    const cJSON *entries = common_json_member(document, "chords");
    AnalyzerPassageChord *chords = calloc(sequence_count, sizeof(*chords));
    bool ok = chords != NULL;
    size_t n = 0;
    for (size_t i = 0; ok && i < sequence_count; i++) {
        const cJSON *entry = cJSON_GetArrayItem(entries, (int)sequence[i].first_entry);
        const char *key = common_json_string_or_null(entry, "key_label");
        AnalyzerPassageChord *chord = &chords[n];
        if (!location_of(entry, "start", &chord->measure, &chord->beat)) {
            continue;
        }
        chord->numeral = strdup(sequence[i].numeral);
        chord->key_label = strdup(key ? key : "");
        ok = chord->numeral && chord->key_label;
        n++;
    }
    analyzer_chord_sequence_free(sequence, sequence_count);
    if (!ok || n == 0) {
        for (size_t i = 0; i < n; i++) {
            free(chords[i].numeral);
            free(chords[i].key_label);
        }
        free(chords);
        return n == 0 && ok;
    }
    *out = chords;
    *count = n;
    return true;
}

/* The soprano notes of a match as Rational notes; NULL and 0 when it has none or one does not fit a Rational. false on allocation failure. */
static bool convert_notes(const SimilarMatch *match, AnalyzerPassageNote **out, size_t *count) {
    *out = NULL;
    *count = 0;
    if (match->note_count == 0) {
        return true;
    }
    AnalyzerPassageNote *notes = calloc(match->note_count, sizeof(*notes));
    if (!notes) {
        return false;
    }
    for (size_t i = 0; i < match->note_count; i++) {
        const SimilarNote *note = &match->notes[i];
        notes[i].pitch = note->pitch;
        if (note->onset_denominator > (unsigned long)LLONG_MAX || note->onset_numerator > (unsigned long)LLONG_MAX ||
            note->duration_denominator > (unsigned long)LLONG_MAX || note->duration_numerator > (unsigned long)LLONG_MAX ||
            rational_make((long long)note->onset_numerator, (long long)note->onset_denominator, &notes[i].onset) != RATIONAL_OK ||
            rational_make((long long)note->duration_numerator, (long long)note->duration_denominator, &notes[i].duration) !=
                RATIONAL_OK) {
            free(notes);
            return true;
        }
    }
    *out = notes;
    *count = match->note_count;
    return true;
}

/* One match as a passage: its document is read and parsed once. A document that cannot be read or parsed (the search read it a
   moment ago) gives a passage with no chords that is not stale. false on allocation failure. */
static bool passage_from_match(const char *corpus_dir, const SimilarMatch *match, AnalyzerCorpusPassage *out) {
    memset(out, 0, sizeof(*out));
    out->piece = strdup(match->piece);
    out->start_measure = match->start_measure;
    out->start_beat = match->start_beat;
    out->end_measure = match->end_measure;
    out->end_beat = match->end_beat;
    out->similarity = match->score.similarity;
    if (!out->piece || !convert_notes(match, &out->notes, &out->note_count)) {
        passage_clear(out);
        return false;
    }
    char path[2048];
    char *text = NULL;
    if (snprintf(path, sizeof path, "%s/%s", corpus_dir, match->document) >= (int)sizeof path || common_file_read(path, &text, NULL, NULL) != URSATZ_OK) {
        return true;
    }
    cJSON *document = cJSON_Parse(text);
    if (document) {
        out->stale = analyzer_document_status(text) != ANALYZER_DOCUMENT_CURRENT;
        if (!read_chords(document, &out->chords, &out->chord_count)) {
            cJSON_Delete(document);
            free(text);
            passage_clear(out);
            return false;
        }
        cJSON_Delete(document);
    }
    free(text);
    return true;
}

static UrsatzStatus passages_of(const char *corpus_dir, const KnowledgeSnapshot *knowledge, const SimilarQuery *query,
                                          size_t max, AnalyzerCorpusPassages *out) {
    SimilarOptions options = similar_options_default();
    options.minimum_numerator = MINIMUM_NUMERATOR;
    options.minimum_denominator = MINIMUM_DENOMINATOR;
    options.max_results = max;
    SimilarResults results;
    memset(&results, 0, sizeof(results));
    UrsatzStatus found = similar_material_find(corpus_dir, knowledge, query, &options, &results);
    if (found != URSATZ_OK) {
        similar_results_destroy(&results);
        return found == URSATZ_ERR_ALLOC ? URSATZ_ERR_ALLOC
               : found == URSATZ_ERR_REQUEST ? URSATZ_ERR_REQUEST
               : found == URSATZ_ERR_REQUEST ? URSATZ_ERR_REQUEST
                                           : URSATZ_ERR_NOT_FOUND;
    }
    AnalyzerCorpusPassages made = {0};
    if (results.count > 0) {
        made.passages = calloc(results.count, sizeof(*made.passages));
        if (!made.passages) {
            similar_results_destroy(&results);
            return URSATZ_ERR_ALLOC;
        }
    }
    for (size_t i = 0; i < results.count; i++) {
        if (!passage_from_match(corpus_dir, &results.matches[i], &made.passages[i])) {
            made.count = i;
            analyzer_corpus_passages_destroy(&made);
            similar_results_destroy(&results);
            return URSATZ_ERR_ALLOC;
        }
        made.count = i + 1;
    }
    similar_results_destroy(&results);
    *out = made;
    return URSATZ_OK;
}

UrsatzStatus analyzer_corpus_passages_like(const char *corpus_dir, const KnowledgeSnapshot *knowledge,
                                                     const char *passage_path, size_t max, AnalyzerCorpusPassages *out) {
    if (!out) {
        return URSATZ_ERR_REQUEST;
    }
    memset(out, 0, sizeof(*out));
    if (!corpus_dir || !knowledge || !passage_path || max == 0) {
        return URSATZ_ERR_REQUEST;
    }
    SimilarQuery query;
    UrsatzStatus read = similar_query_from_file(passage_path, knowledge, &query);
    if (read != URSATZ_OK) {
        return read == URSATZ_ERR_ALLOC ? URSATZ_ERR_ALLOC : URSATZ_ERR_REQUEST;
    }
    UrsatzStatus status = passages_of(corpus_dir, knowledge, &query, max, out);
    similar_query_destroy(&query);
    return status;
}

UrsatzStatus analyzer_corpus_passages_numerals(const char *corpus_dir, const KnowledgeSnapshot *knowledge,
                                                         const char *numerals, size_t max, AnalyzerCorpusPassages *out) {
    if (!out) {
        return URSATZ_ERR_REQUEST;
    }
    memset(out, 0, sizeof(*out));
    if (!corpus_dir || !knowledge || !numerals || max == 0) {
        return URSATZ_ERR_REQUEST;
    }
    SimilarQuery query;
    UrsatzStatus read = similar_query_from_numerals(numerals, &query);
    if (read != URSATZ_OK) {
        return read == URSATZ_ERR_ALLOC ? URSATZ_ERR_ALLOC : URSATZ_ERR_REQUEST;
    }
    UrsatzStatus status = passages_of(corpus_dir, knowledge, &query, max, out);
    similar_query_destroy(&query);
    return status;
}

/* ---- the request member ---- */

static cJSON *fraction_json(Rational value) {
    return common_json_rational_object(value.num, value.den);
}

static cJSON *place_json(int measure, double beat) {
    cJSON *place = cJSON_CreateObject();
    if (!place || !cJSON_AddNumberToObject(place, "measure", measure) || !cJSON_AddNumberToObject(place, "beat", beat)) {
        cJSON_Delete(place);
        return NULL;
    }
    return place;
}

static cJSON *passage_json(const AnalyzerCorpusPassage *passage) {
    cJSON *object = cJSON_CreateObject();
    cJSON *notes = cJSON_CreateArray();
    cJSON *chords = cJSON_CreateArray();
    bool ok = object && notes && chords && cJSON_AddStringToObject(object, "piece", passage->piece) &&
              common_json_add(object, "start", place_json(passage->start_measure, passage->start_beat)) &&
              common_json_add(object, "end", place_json(passage->end_measure, passage->end_beat)) &&
              common_json_add(object, "similarity", fraction_json(passage->similarity)) &&
              cJSON_AddBoolToObject(object, "stale", passage->stale);
    for (size_t i = 0; ok && i < passage->note_count; i++) {
        cJSON *note = cJSON_CreateObject();
        if (!(note && cJSON_AddNumberToObject(note, "midi", passage->notes[i].pitch) &&
              common_json_add(note, "onset", fraction_json(passage->notes[i].onset)) &&
              common_json_add(note, "duration", fraction_json(passage->notes[i].duration)))) {
            cJSON_Delete(note);
            ok = false;
        } else {
            ok = common_json_append(notes, note);
        }
    }
    for (size_t i = 0; ok && i < passage->chord_count; i++) {
        cJSON *chord = cJSON_CreateObject();
        if (!(chord && cJSON_AddStringToObject(chord, "numeral", passage->chords[i].numeral) &&
              cJSON_AddNumberToObject(chord, "measure", passage->chords[i].measure) &&
              cJSON_AddNumberToObject(chord, "beat", passage->chords[i].beat) &&
              cJSON_AddStringToObject(chord, "key", passage->chords[i].key_label))) {
            cJSON_Delete(chord);
            ok = false;
        } else {
            ok = common_json_append(chords, chord);
        }
    }
    ok = ok && common_json_add(object, "notes", notes) && common_json_add(object, "chords", chords);
    if (!ok) {
        if (object && !cJSON_GetObjectItemCaseSensitive(object, "notes")) {
            cJSON_Delete(notes);
        }
        if (object && !cJSON_GetObjectItemCaseSensitive(object, "chords")) {
            cJSON_Delete(chords);
        }
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

char *analyzer_corpus_passages_to_json(const AnalyzerCorpusPassages *passages) {
    if (!passages) {
        return NULL;
    }
    cJSON *array = cJSON_CreateArray();
    for (size_t i = 0; array && i < passages->count; i++) {
        if (!common_json_append(array, passage_json(&passages->passages[i]))) {
            cJSON_Delete(array);
            return NULL;
        }
    }
    char *text = array ? cJSON_PrintUnformatted(array) : NULL;
    cJSON_Delete(array);
    return text;
}
