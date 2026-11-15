#include <ursatz_analyzer/similar_material.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>

#include <ursatz_analyzer/analysis_document.h>
#include <ursatz_analyzer/analysis_pipeline.h>
#include <ursatz_analyzer/chord_sequence.h>
#include <ursatz_analyzer/input_pipeline.h>
#include <ursatz_analyzer/style_labels.h>

#include "core/rational.h"
#include "pipeline/chord_sequence_internal.h"
#include "pipeline/corpus_walk.h"
#include "pipeline/run_file.h"
#include "query/similarity_model.h"
#include "standard-tonality/roman_numeral_grammar.h"
#include "util/json_util.h"

/* ---- reading the document ---- */

static bool fraction_member(const cJSON *object, const char *name, unsigned long *numerator, unsigned long *denominator) {
    const cJSON *fraction = json_member(object, name);
    long n, d;
    if (!json_number_member(fraction, "num", &n) || !json_number_member(fraction, "den", &d) || d <= 0 || n < 0) {
        return false;
    }
    *numerator = (unsigned long)n;
    *denominator = (unsigned long)d;
    return true;
}

/* A position as a measure and a beat (the beat plus the fraction of it). */
static bool location_member(const cJSON *entry, const char *edge, int *measure, double *beat) {
    const cJSON *position = json_member(json_member(entry, "location"), edge);
    long m, b, n, d;
    if (!json_number_member(position, "measure", &m) || !json_number_member(position, "beat", &b) ||
        !json_number_member(json_member(position, "subdivision"), "num", &n) ||
        !json_number_member(json_member(position, "subdivision"), "den", &d) || d <= 0) {
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
 * The soprano's notes in time order. The document has one soprano entry per voice segment (one per melody note), so a
 * note held across several segments is one note and its end is the end of its last segment. Entries that are not present are skipped.
 * *out is NULL when *count is 0. false on allocation failure or an entry that cannot be read.
 */
static bool read_notes(const cJSON *document, DocNote **out, size_t *count) {
    const cJSON *soprano = json_member(json_member(document, "outer_voices"), "soprano");
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
        const cJSON *status = json_member(entry, "status");
        long id, pitch;
        if (!cJSON_IsString(status) || strcmp(status->valuestring, "present") != 0 || !json_number_member(entry, "note_id", &id) ||
            !json_number_member(json_member(entry, "pitch"), "midi", &pitch)) {
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
        const cJSON *onset = json_member(entry, "onset");
        if (!fraction_member(onset, "time", &note->onset_n, &note->onset_d) ||
            !fraction_member(json_member(entry, "duration"), "whole_notes", &note->duration_n, &note->duration_d) ||
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
    AnalyzerResult *result = NULL;
    RunFileResult ran = analyzer_run_file(path, INPUT_FORMAT_AUTO, NULL, &imported, &result, NULL, NULL);
    if (ran != RUN_FILE_OK) {
        return ran == RUN_FILE_ERR_IMPORT ? SIMILAR_ERR_IO : SIMILAR_ERR_ALLOC;
    }
    AnalysisDocument *built = analyzer_run_file_document(path, &imported, result);
    char *json = built ? analysis_document_to_json(built) : NULL;
    analysis_document_destroy(built);
    if (!json) {
        return SIMILAR_ERR_ALLOC;
    }
    cJSON *document = cJSON_Parse(json);
    free(json);
    long version;
    if (!document || !json_number_member(document, "schema_version", &version) || version != URSATZ_ANALYZER_DOCUMENT_SCHEMA_VERSION) {
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

/* ---- chords ---- */

/* A chord of a document with a root-position numeral, at its absolute position, from the joined chord sequence
   (chord_sequence.h). */
typedef struct {
    char *numeral;
    int root;   /* pitch class */
    bool minor; /* the mode of its key region */
    long run;   /* chords of one run follow each other within one key region */
    unsigned long start_n, start_d, end_n, end_d;
    int start_measure, end_measure;
    double start_beat, end_beat;
} DocChord;

static void chords_free(DocChord *chords, size_t count) {
    for (size_t i = 0; i < count; i++) {
        free(chords[i].numeral);
    }
    free(chords);
}

/* The entry of the document's "chords" at an index, walking forward from a cursor (indices only grow). */
static const cJSON *entry_at(const cJSON *entries, const cJSON **cursor, size_t *cursor_index, size_t index) {
    if (!*cursor || index < *cursor_index) {
        *cursor = entries ? entries->child : NULL;
        *cursor_index = 0;
    }
    while (*cursor && *cursor_index < index) {
        *cursor = (*cursor)->next;
        (*cursor_index)++;
    }
    return *cursor;
}

/* The chords in time order, from the joined chord sequence (chord_sequence.h), which is the one owner of the join rule.
   *out is NULL when *count is 0. false on allocation failure or an entry that cannot be read. */
static bool read_chords(const cJSON *document, DocChord **out, size_t *count) {
    AnalyzerChord *sequence = NULL;
    size_t sequence_count = 0;
    *out = NULL;
    *count = 0;
    if (chord_sequence_from_tree(document, &sequence, &sequence_count) != ANALYZER_OK) {
        return false;
    }
    if (sequence_count == 0) {
        return true;
    }
    DocChord *chords = malloc(sequence_count * sizeof *chords);
    if (!chords) {
        analyzer_chord_sequence_free(sequence, sequence_count);
        return false;
    }
    const cJSON *entries = json_member(document, "chords");
    const cJSON *first_cursor = NULL, *last_cursor = NULL;
    size_t first_index = 0, last_index = 0;
    long run = 0;
    size_t n = 0;
    for (size_t i = 0; i < sequence_count; i++) {
        const AnalyzerChord *chord = &sequence[i];
        /* chords of one run follow each other, entry after entry, within one key region */
        if (i == 0 || chord->key_region != sequence[i - 1].key_region || chord->first_entry != sequence[i - 1].last_entry + 1) {
            run++;
        }
        const cJSON *first = entry_at(entries, &first_cursor, &first_index, chord->first_entry);
        const cJSON *last = entry_at(entries, &last_cursor, &last_index, chord->last_entry);
        const cJSON *key = json_member(first, "key_label");
        int start_measure, end_measure;
        double start_beat, end_beat;
        if (!location_member(first, "start", &start_measure, &start_beat) ||
            !location_member(last, "end", &end_measure, &end_beat)) {
            chords_free(chords, n);
            analyzer_chord_sequence_free(sequence, sequence_count);
            return false;
        }
        size_t key_length = cJSON_IsString(key) ? strlen(key->valuestring) : 0;
        chords[n++] = (DocChord){chord->numeral,
                                 chord->root_pitch_class,
                                 key_length >= 5 && strcmp(key->valuestring + key_length - 5, "minor") == 0,
                                 run,
                                 (unsigned long)chord->start.num,
                                 (unsigned long)chord->start.den,
                                 (unsigned long)chord->end.num,
                                 (unsigned long)chord->end.den,
                                 start_measure,
                                 end_measure,
                                 start_beat,
                                 end_beat};
        sequence[i].numeral = NULL; /* now owned by the chord */
    }
    free(sequence);
    *out = chords;
    *count = n;
    return true;
}

/* ---- numerals as root steps ---- */

/* The pitch class of a numeral's root above its tonic, in a major or a minor key; false for text that is not a numeral.
 */
static bool numeral_root_offset(const char *numeral, bool minor, int *out) {
    StandardTonalityNumeralText parsed;
    return standard_tonality_numeral_parse(numeral, &parsed) &&
           standard_tonality_numeral_root_semitone(
               &parsed, minor ? STANDARD_TONALITY_MODE_NATURAL_MINOR : STANDARD_TONALITY_MODE_MAJOR, out);
}

/* The directed semitone step from one pitch class to another, -6 to +5. */
static long root_step(int from, int to) {
    int step = ((to - from) % 12 + 12) % 12;
    return step > 5 ? step - 12 : step;
}

/* ---- options and scoring ---- */

SimilarOptions similar_options_default(void) {
    return (SimilarOptions){1, 2, 10, {2, 1, 1}};
}

void similar_results_destroy(SimilarResults *results) {
    if (!results) {
        return;
    }
    for (size_t i = 0; i < results->count; i++) {
        free(results->matches[i].piece);
        free(results->matches[i].document);
        free(results->matches[i].notes);
    }
    free(results->matches);
    *results = (SimilarResults){0};
}

typedef struct {
    size_t first;
    SimilarityBreakdown score;
    size_t numeral_matches;
} Candidate;

static int compare_candidates(const void *x, const void *y) {
    const Candidate *a = x, *b = y;
    int order = rational_cmp_unsigned(b->score.similarity.numerator, b->score.similarity.denominator,
                                      a->score.similarity.numerator, a->score.similarity.denominator);
    if (order != 0) {
        return order;
    }
    return a->first < b->first ? -1 : (a->first > b->first ? 1 : 0);
}

typedef struct {
    const SimilarQuery *query;
    const SimilarOptions *options;
    SimilarityModel *model;
    size_t size;                 /* the query's notes or numerals */
    long *intervals[2];          /* a motive: its steps, in [0]; a progression: its root steps in a major, then a minor key */
    ContourDirection *directions; /* a motive only */
    SimilarityDuration *durations;
} Search;

static void search_free(Search *search) {
    similarity_model_destroy(search->model);
    free(search->intervals[0]);
    free(search->intervals[1]);
    free(search->directions);
    free(search->durations);
}

static SimilarResult search_prepare(Search *search, const SimilarQuery *query, const SimilarOptions *options) {
    *search = (Search){.query = query, .options = options};
    SimilarityDimensionWeight weights[3];
    size_t weight_count = 0;
    bool motive = query->kind == SIMILAR_QUERY_MOTIVE;
    if (options->weights.interval) {
        weights[weight_count++] = (SimilarityDimensionWeight){SIMILARITY_DIMENSION_INTERVAL, (long)options->weights.interval, 1};
    }
    if (motive && options->weights.contour) {
        weights[weight_count++] = (SimilarityDimensionWeight){SIMILARITY_DIMENSION_CONTOUR, (long)options->weights.contour, 1};
    }
    if (options->weights.rhythm) {
        weights[weight_count++] = (SimilarityDimensionWeight){SIMILARITY_DIMENSION_RHYTHM, (long)options->weights.rhythm, 1};
    }
    if (similarity_model_create(weights, weight_count, &search->model) != SIMILARITY_MODEL_OK) {
        return SIMILAR_ERR_ARGUMENT;
    }
    search->size = motive ? query->note_count : query->numeral_count;
    search->durations = malloc(search->size * sizeof *search->durations);
    search->intervals[0] = malloc(search->size * sizeof(long));
    search->intervals[1] = malloc(search->size * sizeof(long));
    if (motive) {
        search->directions = malloc(search->size * sizeof *search->directions);
    }
    if (!search->durations || !search->intervals[0] || !search->intervals[1] || (motive && !search->directions)) {
        return SIMILAR_ERR_ALLOC;
    }
    if (motive) {
        for (size_t i = 0; i < search->size; i++) {
            const SimilarNote *note = &query->notes[i];
            if (note->duration_numerator == 0 || note->duration_denominator == 0) {
                return SIMILAR_ERR_QUERY;
            }
            search->durations[i] = (SimilarityDuration){note->duration_numerator, note->duration_denominator};
            if (i > 0) {
                search->intervals[0][i - 1] = (long)note->pitch - (long)query->notes[i - 1].pitch;
            }
        }
        similarity_directions_from_intervals(search->intervals[0], search->size - 1, search->directions);
        return SIMILAR_OK;
    }
    for (int mode = 0; mode < 2; mode++) {
        int previous = 0;
        for (size_t i = 0; i < search->size; i++) {
            int offset;
            if (!numeral_root_offset(query->numerals[i], mode == 1, &offset)) {
                return SIMILAR_ERR_QUERY;
            }
            if (i > 0) {
                search->intervals[mode][i - 1] = root_step(previous, offset);
            }
            previous = offset;
        }
    }
    for (size_t i = 0; i < search->size; i++) {
        search->durations[i] = (SimilarityDuration){1, 1};
    }
    return SIMILAR_OK;
}

typedef struct {
    Candidate *items;
    size_t count, capacity;
} Candidates;

/* Scores one window against the query, using the query's steps given, and keeps it when it reaches the minimum. false
   on allocation failure or a score that cannot be computed. */
static bool consider_window(const Search *search, const long *query_intervals, size_t first, const long *intervals,
                            const SimilarityDuration *durations, size_t numeral_matches, Candidates *candidates) {
    size_t n = search->size;
    bool motive = search->query->kind == SIMILAR_QUERY_MOTIVE;
    ContourDirection *window_directions = NULL;
    if (motive) {
        window_directions = malloc((n - 1) * sizeof *window_directions);
        if (!window_directions) {
            return false;
        }
        similarity_directions_from_intervals(intervals, n - 1, window_directions);
    }
    SimilarityInput query = {query_intervals, n - 1, search->directions, motive ? n - 1 : 0, search->durations, n};
    SimilarityInput window = {intervals, n - 1, window_directions, motive ? n - 1 : 0, durations, n};
    SimilarityBreakdown breakdown;
    SimilarityScoreResult scored = similarity_score(search->model, &query, &window, &breakdown);
    free(window_directions);
    if (scored != SIMILARITY_SCORE_OK) {
        return false;
    }
    if (rational_cmp_unsigned(breakdown.similarity.numerator, breakdown.similarity.denominator,
                              search->options->minimum_numerator, search->options->minimum_denominator) < 0) {
        return true;
    }
    if (candidates->count == candidates->capacity) {
        size_t capacity = candidates->capacity ? candidates->capacity * 2 : 64;
        Candidate *grown = realloc(candidates->items, capacity * sizeof *grown);
        if (!grown) {
            return false;
        }
        candidates->items = grown;
        candidates->capacity = capacity;
    }
    candidates->items[candidates->count++] = (Candidate){first, breakdown, numeral_matches};
    return true;
}

static bool motive_windows(const Search *search, const DocNote *notes, size_t count, Candidates *candidates) {
    size_t n = search->size;
    long *intervals = malloc((n - 1) * sizeof *intervals);
    SimilarityDuration *durations = malloc(n * sizeof *durations);
    bool ok = intervals && durations;
    for (size_t first = 0; ok && first + n <= count; first++) {
        for (size_t i = 0; i < n; i++) {
            durations[i] = (SimilarityDuration){notes[first + i].duration_n, notes[first + i].duration_d};
            if (i > 0) {
                intervals[i - 1] = (long)notes[first + i].pitch - (long)notes[first + i - 1].pitch;
            }
        }
        ok = consider_window(search, search->intervals[0], first, intervals, durations, 0, candidates);
    }
    free(intervals);
    free(durations);
    return ok;
}

static bool progression_windows(const Search *search, const DocChord *chords, size_t count, Candidates *candidates) {
    size_t n = search->size;
    long *intervals = malloc((n - 1) * sizeof *intervals);
    SimilarityDuration *durations = malloc(n * sizeof *durations);
    bool ok = intervals && durations;
    for (size_t first = 0; ok && first + n <= count; first++) {
        if (chords[first].run != chords[first + n - 1].run) {
            continue;
        }
        bool measured = true;
        size_t matches = 0;
        for (size_t i = 0; i < n && measured; i++) {
            const DocChord *chord = &chords[first + i];
            unsigned long length_n, length_d;
            measured = rational_difference(chord->end_n, chord->end_d, chord->start_n, chord->start_d, &length_n, &length_d) &&
                       length_n > 0;
            durations[i] = (SimilarityDuration){length_n, length_d};
            if (i > 0) {
                intervals[i - 1] = root_step(chords[first + i - 1].root, chord->root);
            }
            matches += strcmp(chord->numeral, search->query->numerals[i]) == 0;
        }
        if (measured) {
            ok = consider_window(search, search->intervals[chords[first].minor ? 1 : 0], first, intervals, durations, matches,
                                 candidates);
        }
    }
    free(intervals);
    free(durations);
    return ok;
}

/* Soprano notes that start in [start, end), as a passage. *out is NULL when there are none. false on allocation failure. */
static bool notes_between(const DocNote *notes, size_t count, unsigned long start_n, unsigned long start_d,
                          unsigned long end_n, unsigned long end_d, SimilarNote **out, size_t *found) {
    size_t first = 0;
    while (first < count && rational_cmp_unsigned(notes[first].onset_n, notes[first].onset_d, start_n, start_d) < 0) {
        first++;
    }
    size_t last = first;
    while (last < count && rational_cmp_unsigned(notes[last].onset_n, notes[last].onset_d, end_n, end_d) < 0) {
        last++;
    }
    *out = NULL;
    *found = last - first;
    if (last == first) {
        return true;
    }
    *out = passage_notes(notes, first, last - first);
    return *out != NULL;
}

typedef struct {
    SimilarMatch *items;
    size_t count, capacity;
} Matches;

static bool matches_add(Matches *matches, SimilarMatch match) {
    if (matches->count == matches->capacity) {
        size_t capacity = matches->capacity ? matches->capacity * 2 : 16;
        SimilarMatch *grown = realloc(matches->items, capacity * sizeof *grown);
        if (!grown) {
            return false;
        }
        matches->items = grown;
        matches->capacity = capacity;
    }
    matches->items[matches->count++] = match;
    return true;
}

static int compare_matches(const void *x, const void *y) {
    const SimilarMatch *a = x, *b = y;
    int order = rational_cmp_unsigned(b->score.similarity.numerator, b->score.similarity.denominator,
                                      a->score.similarity.numerator, a->score.similarity.denominator);
    if (order == 0) {
        order = strcmp(a->piece, b->piece);
    }
    if (order == 0) {
        order = a->start_measure - b->start_measure;
    }
    if (order == 0) {
        order = a->start_beat < b->start_beat ? -1 : (a->start_beat > b->start_beat ? 1 : 0);
    }
    return order;
}

static void match_destroy(SimilarMatch *match) {
    free(match->piece);
    free(match->document);
    free(match->notes);
}

/* Searches one analysis document and adds its best non-overlapping windows (at most max_results) to matches. */
static SimilarResult search_document(const Search *search, const char *piece, const char *document_path, const cJSON *document,
                                     Matches *matches) {
    bool motive = search->query->kind == SIMILAR_QUERY_MOTIVE;
    DocNote *notes = NULL;
    DocChord *chords = NULL;
    size_t note_count = 0, chord_count = 0;
    Candidates candidates = {0};
    SimilarResult result = SIMILAR_OK;
    if (!read_notes(document, &notes, &note_count) || (!motive && !read_chords(document, &chords, &chord_count))) {
        result = SIMILAR_ERR_FORMAT;
    } else if (!(motive ? motive_windows(search, notes, note_count, &candidates)
                        : progression_windows(search, chords, chord_count, &candidates))) {
        result = SIMILAR_ERR_ALLOC;
    }
    if (result == SIMILAR_OK && candidates.count > 1) {
        qsort(candidates.items, candidates.count, sizeof *candidates.items, compare_candidates);
    }
    size_t n = search->size;
    size_t kept = 0;
    for (size_t c = 0; result == SIMILAR_OK && c < candidates.count && kept < search->options->max_results; c++) {
        size_t first = candidates.items[c].first;
        bool overlaps = false;
        for (size_t k = 0; k < kept && !overlaps; k++) {
            size_t other = candidates.items[k].first; /* the kept candidates are moved to the front */
            overlaps = first < other + n && other < first + n;
        }
        if (overlaps) {
            continue;
        }
        candidates.items[kept++] = candidates.items[c];
        SimilarMatch match = {0};
        match.score = candidates.items[c].score;
        match.numeral_matches = candidates.items[c].numeral_matches;
        if (motive) {
            match.start_measure = notes[first].start_measure;
            match.start_beat = notes[first].start_beat;
            match.end_measure = notes[first + n - 1].end_measure;
            match.end_beat = notes[first + n - 1].end_beat;
            match.notes = passage_notes(notes, first, n);
            match.note_count = n;
            if (!match.notes) {
                result = SIMILAR_ERR_ALLOC;
            }
        } else {
            const DocChord *begin = &chords[first], *end = &chords[first + n - 1];
            match.start_measure = begin->start_measure;
            match.start_beat = begin->start_beat;
            match.end_measure = end->end_measure;
            match.end_beat = end->end_beat;
            if (!notes_between(notes, note_count, begin->start_n, begin->start_d, end->end_n, end->end_d, &match.notes,
                               &match.note_count)) {
                result = SIMILAR_ERR_ALLOC;
            }
        }
        match.piece = strdup(piece);
        match.document = strdup(document_path);
        if (result == SIMILAR_OK && (!match.piece || !match.document || !matches_add(matches, match))) {
            result = SIMILAR_ERR_ALLOC;
        }
        if (result != SIMILAR_OK) {
            match_destroy(&match);
        }
    }
    free(candidates.items);
    free(notes);
    chords_free(chords, chord_count);
    return result;
}

typedef struct {
    Search *search;
    Matches matches;
    size_t searched;
    SimilarResult result;
} SearchWalk;

static bool search_walk_document(const char *piece_path, const char *document, const char *json,
                                 AnalyzerDocumentStatus status, void *context) {
    (void)status;
    SearchWalk *walk = context;
    cJSON *parsed = cJSON_Parse(json);
    long version;
    if (!parsed || !json_number_member(parsed, "schema_version", &version) ||
        version != URSATZ_ANALYZER_DOCUMENT_SCHEMA_VERSION) {
        cJSON_Delete(parsed);
        walk->result = SIMILAR_ERR_FORMAT;
        return false;
    }
    walk->result = search_document(walk->search, piece_path, document, parsed, &walk->matches);
    cJSON_Delete(parsed);
    if (walk->result != SIMILAR_OK) {
        return false;
    }
    walk->searched++;
    return true;
}

SimilarResult similar_material_find(const char *corpus_output_dir, const SimilarQuery *query, const SimilarOptions *options,
                                    SimilarResults *results) {
    if (!corpus_output_dir || !query || !results) {
        return SIMILAR_ERR_ARGUMENT;
    }
    SimilarOptions defaults = similar_options_default();
    if (!options) {
        options = &defaults;
    }
    size_t size = query->kind == SIMILAR_QUERY_MOTIVE ? query->note_count : query->numeral_count;
    if (size < 2 || (query->kind == SIMILAR_QUERY_MOTIVE ? query->notes == NULL : query->numerals == NULL)) {
        return SIMILAR_ERR_QUERY;
    }
    if (options->minimum_denominator == 0 || options->minimum_numerator > options->minimum_denominator ||
        options->max_results == 0) {
        return SIMILAR_ERR_ARGUMENT;
    }
    Search search;
    SimilarResult result = search_prepare(&search, query, options);
    if (result != SIMILAR_OK) {
        search_free(&search);
        return result;
    }
    char *index_path = malloc(strlen(corpus_output_dir) + 24);
    if (!index_path) {
        search_free(&search);
        return SIMILAR_ERR_ALLOC;
    }
    sprintf(index_path, "%s/corpus-index.json", corpus_output_dir);
    SearchWalk walk = {&search, {0}, 0, SIMILAR_OK};
    CorpusWalkResult walked = corpus_walk_documents(index_path, search_walk_document, &walk, NULL);
    free(index_path);
    Matches matches = walk.matches;
    size_t searched = walk.searched;
    switch (walked) {
    case CORPUS_WALK_OK:
        break;
    case CORPUS_WALK_ERR_ALLOC:
        result = SIMILAR_ERR_ALLOC;
        break;
    case CORPUS_WALK_ERR_IO:
        result = SIMILAR_ERR_IO;
        break;
    case CORPUS_WALK_ERR_FORMAT:
        result = SIMILAR_ERR_FORMAT;
        break;
    case CORPUS_WALK_ERR_VISIT:
        result = walk.result;
        break;
    }
    search_free(&search);
    if (result != SIMILAR_OK) {
        for (size_t i = 0; i < matches.count; i++) {
            match_destroy(&matches.items[i]);
        }
        free(matches.items);
        return result;
    }
    if (matches.count > 1) {
        qsort(matches.items, matches.count, sizeof *matches.items, compare_matches);
    }
    for (size_t i = options->max_results; i < matches.count; i++) {
        match_destroy(&matches.items[i]);
    }
    if (matches.count > options->max_results) {
        matches.count = options->max_results;
    }
    *results = (SimilarResults){matches.items, matches.count, searched};
    return SIMILAR_OK;
}
