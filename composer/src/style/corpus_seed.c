#include <ursatz_composer/corpus_seed.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>

#include <ursatz_analyzer/similar_material.h>
#include <ursatz_analyzer/style_labels.h>

#include <ursatz_composer/fill_rng.h>
#include <ursatz_composer/harmony_phrase.h>
#include "util/util.h"

/* How many matches a progression search looks through for one the plan can place. */
#define PROGRESSION_CANDIDATES 100

/* The beats of a document's locations are compared as doubles; two are the same place within this. */
#define BEAT_EPSILON 1e-6

const char *corpus_seed_result_name(CorpusSeedResult result) {
    switch (result) {
    case CORPUS_SEED_OK:
        return "ok";
    case CORPUS_SEED_ERR_ARGUMENT:
        return "invalid argument";
    case CORPUS_SEED_ERR_QUERY:
        return "the passage cannot be read as a query";
    case CORPUS_SEED_ERR_CORPUS:
        return "the corpus cannot be read";
    case CORPUS_SEED_ERR_NO_MATCH:
        return "no passage of the corpus is similar enough";
    case CORPUS_SEED_ERR_ALLOC:
        return "out of memory";
    }
    return "unknown";
}

void corpus_seed_describe(const CorpusSeed *seed, char *out, size_t size) {
    snprintf(out, size, "m.%d b.%g - m.%d b.%g", seed->start_measure, seed->start_beat, seed->end_measure,
             seed->end_beat);
}

/* --- reading an analysis document ------------------------------------------------- */

static cJSON *read_document(const char *corpus_output_dir, const char *document) {
    char path[2048];
    if (snprintf(path, sizeof(path), "%s/%s", corpus_output_dir, document) >= (int)sizeof(path)) {
        return NULL;
    }
    char *text;
    if (!util_read_file(path, &text, NULL)) {
        return NULL;
    }
    cJSON *parsed = cJSON_Parse(text);
    free(text);
    return parsed;
}

static const cJSON *member(const cJSON *object, const char *name) {
    return object ? cJSON_GetObjectItemCaseSensitive(object, name) : NULL;
}

/* A location edge of a chord entry as a measure and a beat plus the fraction of it, as the Analyzer reads it. */
static bool location_of(const cJSON *entry, const char *edge, int *measure, double *beat) {
    const cJSON *position = member(member(entry, "location"), edge);
    const cJSON *m = member(position, "measure");
    const cJSON *b = member(position, "beat");
    const cJSON *subdivision = member(position, "subdivision");
    const cJSON *n = member(subdivision, "num");
    const cJSON *d = member(subdivision, "den");
    if (!cJSON_IsNumber(m) || !cJSON_IsNumber(b) || !cJSON_IsNumber(n) || !cJSON_IsNumber(d) || d->valuedouble <= 0) {
        return false;
    }
    *measure = (int)m->valuedouble;
    *beat = b->valuedouble + n->valuedouble / d->valuedouble;
    return true;
}

static bool root_pitch_class(const char *name, int *out) {
    static const int letters[7] = {9, 11, 0, 2, 4, 5, 7}; /* A to G */
    if (!name || name[0] < 'A' || name[0] > 'G') {
        return false;
    }
    int pc = letters[name[0] - 'A'];
    for (const char *c = name + 1; *c; c++) {
        if (*c != '#' && *c != 'b') {
            return false;
        }
        pc += *c == '#' ? 1 : -1;
    }
    *out = ((pc % 12) + 12) % 12;
    return true;
}

typedef struct {
    char *numeral;
    int measure;
    double beat;
    char key_label[24];
} DocumentChord;

static void chords_free(DocumentChord *chords, size_t count) {
    for (size_t i = 0; i < count; i++) {
        free(chords[i].numeral);
    }
    free(chords);
}

/*
 * The document's chords in time order with a root-position numeral, joined as the Analyzer's search joins them (the
 * same numeral and root one after the other in one key region are one chord), so an index here is an index there.
 * *out is NULL when there are none. false on allocation failure.
 */
static bool read_chords(const cJSON *document, DocumentChord **out, size_t *count) {
    const cJSON *entries = member(document, "chords");
    size_t capacity = cJSON_IsArray(entries) ? (size_t)cJSON_GetArraySize(entries) : 0;
    *out = NULL;
    *count = 0;
    if (capacity == 0) {
        return true;
    }
    DocumentChord *chords = calloc(capacity, sizeof(*chords));
    if (!chords) {
        return false;
    }
    size_t n = 0;
    int previous_root = -1;
    double previous_region = -1;
    bool joined = false;
    const cJSON *entry;
    cJSON_ArrayForEach(entry, entries) {
        const cJSON *roman = member(entry, "roman_numeral");
        const cJSON *base = member(roman, "base_numeral");
        const cJSON *root_name = member(entry, "root");
        const cJSON *region = member(roman, "key_region");
        int root;
        if (!cJSON_IsString(base) || !cJSON_IsString(root_name) || !root_pitch_class(root_name->valuestring, &root) ||
            !cJSON_IsNumber(region)) {
            joined = false;
            continue;
        }
        char *numeral = style_label_numeral(base->valuestring);
        if (!numeral) {
            joined = false;
            continue;
        }
        int measure;
        double beat;
        if (!location_of(entry, "start", &measure, &beat)) {
            free(numeral);
            joined = false;
            continue;
        }
        if (region->valuedouble != previous_region) {
            joined = false;
        }
        previous_region = region->valuedouble;
        if (joined && strcmp(chords[n - 1].numeral, numeral) == 0 && previous_root == root) {
            free(numeral);
            continue;
        }
        const cJSON *key = member(entry, "key_label");
        chords[n].numeral = numeral;
        chords[n].measure = measure;
        chords[n].beat = beat;
        snprintf(chords[n].key_label, sizeof(chords[n].key_label), "%s", cJSON_IsString(key) ? key->valuestring : "");
        n++;
        previous_root = root;
        joined = true;
    }
    if (n == 0) {
        free(chords);
        return true;
    }
    *out = chords;
    *count = n;
    return true;
}

static bool same_place(int measure_a, double beat_a, int measure_b, double beat_b) {
    return measure_a == measure_b && fabs(beat_a - beat_b) < BEAT_EPSILON;
}

/* --- the key the source was heard in ------------------------------------------------ */

/* The key of the last chord that starts at or before the passage, else of the first chord; false when no chord names one. */
static bool source_key(const DocumentChord *chords, size_t count, int measure, double beat, PlanKey *out) {
    const DocumentChord *found = NULL;
    for (size_t i = 0; i < count; i++) {
        bool before = chords[i].measure < measure || same_place(chords[i].measure, chords[i].beat, measure, beat) ||
                      (chords[i].measure == measure && chords[i].beat < beat);
        if (before || !found) {
            found = &chords[i];
        }
        if (!before) {
            break;
        }
    }
    return found && motive_parse_key(found->key_label, out);
}

/* --- the motive ------------------------------------------------------------------- */

static const int MAJOR_SCALE[7] = {0, 2, 4, 5, 7, 9, 11};
static const int MINOR_SCALE[7] = {0, 2, 3, 5, 7, 8, 10};

/*
 * How far to move a melody that starts on pitch so the note keeps its scale degree: the degree of the note in the
 * source key (a note off the scale keeps its distance above the degree below it), placed in the target key, with the
 * move from the pitch within a tritone either way so the melody stays where it was heard.
 */
static int transposition(int pitch, const PlanKey *source, const PlanKey *target) {
    const int *from = source->mode == PLAN_MODE_MINOR ? MINOR_SCALE : MAJOR_SCALE;
    const int *to = target->mode == PLAN_MODE_MINOR ? MINOR_SCALE : MAJOR_SCALE;
    int above = (((pitch - source->tonic_pitch_class) % 12) + 12) % 12;
    int degree = 0;
    for (int d = 0; d < 7; d++) {
        if (from[d] <= above) {
            degree = d;
        }
    }
    int landed = (target->tonic_pitch_class + to[degree] + (above - from[degree])) % 12;
    int shift = ((landed - (pitch % 12)) % 12 + 12) % 12;
    return shift > 6 ? shift - 12 : shift;
}

static unsigned long long gcd_ull(unsigned long long a, unsigned long long b) {
    while (b) {
        unsigned long long t = a % b;
        a = b;
        b = t;
    }
    return a;
}

static bool plan_time_of(unsigned long long numerator, unsigned long long denominator, PlanTime *out) {
    unsigned long long divisor = gcd_ull(numerator, denominator);
    if (denominator == 0 || divisor == 0) {
        return false;
    }
    numerator /= divisor;
    denominator /= divisor;
    if (numerator > 0x7fffffffUL || denominator > 0xffffffffUL) {
        return false;
    }
    return plan_time_make((long)numerator, (unsigned long)denominator, out) == PLAN_OK;
}

/*
 * One bar of a matched melody as a motive: the notes that start in the first bar, the last shortened to the barline,
 * moved by shift semitones. false when fewer than two notes are left or one does not fit a PlanTime.
 */
static bool motive_from_passage(const SimilarMatch *match, const PlanMeter *meter, const PlanKey *key, int shift,
                                Motive *out) {
    unsigned long long bar_n = (unsigned long long)meter->beats_per_bar;
    unsigned long long bar_d = (unsigned long long)meter->beat_unit;
    memset(out, 0, sizeof(*out));
    out->meter = *meter;
    out->key = *key;
    out->key_source = MOTIVE_KEY_GIVEN;
    for (size_t i = 0; i < match->note_count && out->note_count < MOTIVE_MAX_NOTES; i++) {
        const SimilarNote *note = &match->notes[i];
        unsigned long long on_n = note->onset_numerator, on_d = note->onset_denominator;
        /* The note starts in the bar when onset < bar, that is on_n / on_d < bar_n / bar_d. */
        if (on_n * bar_d >= bar_n * on_d) {
            break;
        }
        unsigned long long end_n = on_n * note->duration_denominator + note->duration_numerator * on_d;
        unsigned long long end_d = on_d * note->duration_denominator;
        unsigned long long length_n = note->duration_numerator, length_d = note->duration_denominator;
        if (end_n * bar_d > bar_n * end_d) {
            /* Across the barline: the note lasts until it, bar - onset. */
            length_n = bar_n * on_d - on_n * bar_d;
            length_d = bar_d * on_d;
        }
        PlanNote *placed = &out->notes[out->note_count];
        int pitch = note->pitch + shift;
        if (pitch < 0 || pitch > 127 || !plan_time_of(on_n, on_d, &placed->offset) ||
            !plan_time_of(length_n, length_d, &placed->duration)) {
            return false;
        }
        placed->midi_pitch = pitch;
        out->note_count++;
    }
    if (out->note_count < 2) {
        return false;
    }
    const PlanNote *last = &out->notes[out->note_count - 1];
    return plan_time_add(last->offset, last->duration, &out->length) == PLAN_OK;
}

/* Whether a score is within 1/10 of the best: 10 * score >= 10 * best - 1, in fractions. */
static bool near_best(const SimilarityScore *score, const SimilarityScore *best) {
    long double s = (long double)score->numerator / (long double)score->denominator;
    long double b = (long double)best->numerator / (long double)best->denominator;
    return s >= b - 0.1L - 1e-12L;
}

static void fill_match(const SimilarMatch *match, CorpusSeed *out) {
    snprintf(out->piece, sizeof(out->piece), "%s", match->piece);
    out->start_measure = match->start_measure;
    out->start_beat = match->start_beat;
    out->end_measure = match->end_measure;
    out->end_beat = match->end_beat;
    out->score_numerator = match->score.similarity.numerator;
    out->score_denominator = match->score.similarity.denominator;
}

static CorpusSeedResult search(const char *corpus_output_dir, const SimilarQuery *query, SimilarResults *results) {
    SimilarOptions options = similar_options_default();
    options.minimum_numerator = CORPUS_SEED_MINIMUM_NUMERATOR;
    options.minimum_denominator = CORPUS_SEED_MINIMUM_DENOMINATOR;
    options.max_results = 10;
    SimilarResult found = similar_material_find(corpus_output_dir, query, &options, results);
    if (found == SIMILAR_ERR_ALLOC) {
        return CORPUS_SEED_ERR_ALLOC;
    }
    if (found != SIMILAR_OK) {
        return CORPUS_SEED_ERR_CORPUS;
    }
    return results->count == 0 ? CORPUS_SEED_ERR_NO_MATCH : CORPUS_SEED_OK;
}

CorpusSeedResult corpus_seed_motive(const char *corpus_output_dir, const char *passage_path, const PlanMeter *meter,
                                    const PlanKey *key, unsigned long seed, CorpusSeed *out) {
    if (!corpus_output_dir || !passage_path || !meter || !key || !out || meter->beats_per_bar < 1 ||
        meter->beat_unit < 1) {
        return CORPUS_SEED_ERR_ARGUMENT;
    }
    SimilarQuery query;
    SimilarResult read = similar_query_from_file(passage_path, &query);
    if (read == SIMILAR_ERR_ALLOC) {
        return CORPUS_SEED_ERR_ALLOC;
    }
    if (read != SIMILAR_OK) {
        return CORPUS_SEED_ERR_QUERY;
    }
    SimilarResults results;
    memset(&results, 0, sizeof(results));
    CorpusSeedResult status = search(corpus_output_dir, &query, &results);
    similar_query_destroy(&query);
    if (status != CORPUS_SEED_OK) {
        similar_results_destroy(&results);
        return status;
    }

    /* The matches are sorted best first; the ones within 1/10 of the best are the choice. */
    size_t eligible = 0;
    while (eligible < results.count &&
           near_best(&results.matches[eligible].score.similarity, &results.matches[0].score.similarity)) {
        eligible++;
    }
    FillRng rng;
    fill_rng_seed(&rng, seed);
    size_t pick = (size_t)fill_rng_below(&rng, eligible);

    status = CORPUS_SEED_ERR_NO_MATCH;
    for (size_t step = 0; step < eligible && status != CORPUS_SEED_OK; step++) {
        const SimilarMatch *match = &results.matches[(pick + step) % eligible];
        if (match->note_count == 0) {
            continue;
        }
        int shift = 0;
        cJSON *document = read_document(corpus_output_dir, match->document);
        if (document) {
            DocumentChord *chords = NULL;
            size_t chord_count = 0;
            PlanKey from;
            if (read_chords(document, &chords, &chord_count) &&
                source_key(chords, chord_count, match->start_measure, match->start_beat, &from)) {
                shift = transposition(match->notes[0].pitch, &from, key);
            }
            chords_free(chords, chord_count);
            cJSON_Delete(document);
        }
        CorpusSeed seeded;
        memset(&seeded, 0, sizeof(seeded));
        if (motive_from_passage(match, meter, key, shift, &seeded.motive)) {
            fill_match(match, &seeded);
            *out = seeded;
            status = CORPUS_SEED_OK;
        }
    }
    similar_results_destroy(&results);
    return status;
}

/* Whether the plan can place the numeral under a key of the mode (given.h reads them with these two). */
static bool numeral_placeable(PlanKeyMode mode, const char *numeral) {
    HarmonyChord chord;
    HarmonyAppliedChord applied;
    int inversion;
    return strlen(numeral) < PLAN_ROMAN_NUMERAL_MAX &&
           (harmony_roman_parse(mode, numeral, &chord) || harmony_roman_parse_applied_inverted(mode, numeral, &applied, &inversion));
}

/*
 * The numerals of the passage a match is, read from its document: the chords from the one that starts where the match
 * does. false when the document cannot be read, has no chord there, or a numeral cannot be placed under the mode.
 */
static bool match_numerals(const char *corpus_output_dir, const SimilarMatch *match, size_t wanted, PlanKeyMode mode,
                           CorpusSeed *seeded) {
    bool ok = false;
    cJSON *document = read_document(corpus_output_dir, match->document);
    if (!document) {
        return false;
    }
    DocumentChord *chords = NULL;
    size_t chord_count = 0;
    if (read_chords(document, &chords, &chord_count)) {
        for (size_t first = 0; first < chord_count && !ok; first++) {
            if (!same_place(chords[first].measure, chords[first].beat, match->start_measure, match->start_beat)) {
                continue;
            }
            ok = first + wanted <= chord_count;
            for (size_t i = 0; ok && i < wanted; i++) {
                const char *numeral = chords[first + i].numeral;
                ok = strlen(numeral) < CORPUS_SEED_NUMERAL_MAX && numeral_placeable(mode, numeral);
                if (ok) {
                    snprintf(seeded->numerals[i], CORPUS_SEED_NUMERAL_MAX, "%s", numeral);
                }
            }
            break;
        }
    }
    chords_free(chords, chord_count);
    cJSON_Delete(document);
    return ok;
}

CorpusSeedResult corpus_seed_progression(const char *corpus_output_dir, const char *numerals_text, PlanKeyMode mode,
                                         CorpusSeed *out) {
    if (!corpus_output_dir || !numerals_text || !out) {
        return CORPUS_SEED_ERR_ARGUMENT;
    }
    SimilarQuery query;
    SimilarResult read = similar_query_from_numerals(numerals_text, &query);
    if (read == SIMILAR_ERR_ALLOC) {
        return CORPUS_SEED_ERR_ALLOC;
    }
    if (read != SIMILAR_OK) {
        return CORPUS_SEED_ERR_QUERY;
    }
    if (query.numeral_count > CORPUS_SEED_MAX_NUMERALS) {
        similar_query_destroy(&query);
        return CORPUS_SEED_ERR_QUERY;
    }
    size_t wanted = query.numeral_count;
    SimilarOptions options = similar_options_default();
    options.minimum_numerator = CORPUS_SEED_MINIMUM_NUMERATOR;
    options.minimum_denominator = CORPUS_SEED_MINIMUM_DENOMINATOR;
    options.max_results = PROGRESSION_CANDIDATES;
    SimilarResults results;
    memset(&results, 0, sizeof(results));
    SimilarResult found = similar_material_find(corpus_output_dir, &query, &options, &results);
    similar_query_destroy(&query);
    CorpusSeedResult status = found == SIMILAR_ERR_ALLOC ? CORPUS_SEED_ERR_ALLOC
                              : found != SIMILAR_OK      ? CORPUS_SEED_ERR_CORPUS
                                                         : CORPUS_SEED_ERR_NO_MATCH;
    /* The first match, best first, whose chords the plan can place in the key's mode. */
    for (size_t i = 0; i < results.count && status == CORPUS_SEED_ERR_NO_MATCH; i++) {
        CorpusSeed seeded;
        memset(&seeded, 0, sizeof(seeded));
        if (match_numerals(corpus_output_dir, &results.matches[i], wanted, mode, &seeded)) {
            fill_match(&results.matches[i], &seeded);
            seeded.numeral_count = wanted;
            *out = seeded;
            status = CORPUS_SEED_OK;
        }
    }
    similar_results_destroy(&results);
    return status;
}
