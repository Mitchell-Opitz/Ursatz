#include <ursatz_analyzer/style_profile.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>

#include <ursatz_analyzer/analysis_document.h>

#include "pipeline/analysis_document_internal.h"
#include "pipeline/analysis_result_internal.h"
#include "pipeline/corpus_walk.h"
#include "style/style_profile_internal.h"
#include "util/json_util.h"

#define SYNCOPATION_CAP 4

typedef struct {
    char *category;
    char *label;
    unsigned long count;
} ProfileEntry;

struct StyleProfile {
    ProfileEntry *entries; /* sorted by category, then label */
    size_t count;
    size_t capacity;
    size_t pieces;
    char *training_corpus;
};

StyleProfile *style_profile_create(void) {
    StyleProfile *profile = calloc(1, sizeof *profile);
    if (!profile) {
        return NULL;
    }
    profile->training_corpus = calloc(1, 1);
    if (!profile->training_corpus) {
        free(profile);
        return NULL;
    }
    return profile;
}

void style_profile_destroy(StyleProfile *profile) {
    if (!profile) {
        return;
    }
    for (size_t i = 0; i < profile->count; i++) {
        free(profile->entries[i].category);
        free(profile->entries[i].label);
    }
    free(profile->entries);
    free(profile->training_corpus);
    free(profile);
}

size_t style_profile_piece_count(const StyleProfile *profile) {
    return profile ? profile->pieces : 0;
}

const char *style_profile_training_corpus(const StyleProfile *profile) {
    return profile ? profile->training_corpus : "";
}

static int compare_key(const char *category, const char *label, const ProfileEntry *entry) {
    int c = strcmp(category, entry->category);
    return c != 0 ? c : strcmp(label, entry->label);
}

/* Adds one to (category, label). false on allocation failure. */
static bool count_label(StyleProfile *profile, const char *category, const char *label) {
    size_t low = 0, high = profile->count;
    while (low < high) {
        size_t middle = low + (high - low) / 2;
        int c = compare_key(category, label, &profile->entries[middle]);
        if (c == 0) {
            profile->entries[middle].count++;
            return true;
        }
        if (c < 0) {
            high = middle;
        } else {
            low = middle + 1;
        }
    }
    if (profile->count == profile->capacity) {
        size_t capacity = profile->capacity ? profile->capacity * 2 : 64;
        ProfileEntry *grown = realloc(profile->entries, capacity * sizeof *grown);
        if (!grown) {
            return false;
        }
        profile->entries = grown;
        profile->capacity = capacity;
    }
    char *category_copy = strdup(category);
    char *label_copy = strdup(label);
    if (!category_copy || !label_copy) {
        free(category_copy);
        free(label_copy);
        return false;
    }
    memmove(&profile->entries[low + 1], &profile->entries[low], (profile->count - low) * sizeof *profile->entries);
    profile->entries[low] = (ProfileEntry){category_copy, label_copy, 1};
    profile->count++;
    return true;
}

static bool count_number(StyleProfile *profile, const char *category, long value) {
    char label[32];
    snprintf(label, sizeof label, "%ld", value);
    return count_label(profile, category, label);
}

/* ---- reading the document ---- */

static bool fraction_member(const cJSON *object, const char *name, long *numerator, long *denominator) {
    const cJSON *fraction = json_member(object, name);
    return json_number_member(fraction, "num", numerator) && json_number_member(fraction, "den", denominator) && *denominator > 0 &&
           *numerator >= 0;
}

/* Counts the chords: numerals and progressions within one key region. */
static bool count_harmony(StyleProfile *profile, const cJSON *document) {
    char *previous = NULL, *before_previous = NULL; /* the last two numerals of the current run, collapsed */
    long run_region = -1;
    bool ok = true;
    const cJSON *chord;
    cJSON_ArrayForEach(chord, json_member(document, "chords")) {
        const cJSON *roman = json_member(chord, "roman_numeral");
        const char *base = json_string_member(roman, "base_numeral");
        long region = 0;
        if (!base || !json_number_member(roman, "key_region", &region)) {
            free(previous);
            free(before_previous);
            previous = before_previous = NULL;
            run_region = -1;
            continue;
        }
        char *numeral = style_label_numeral(base);
        if (!numeral) {
            continue;
        }
        ok = count_label(profile, STYLE_CATEGORY_CHORD_NUMERAL, numeral);
        if (region != run_region) {
            free(previous);
            free(before_previous);
            previous = before_previous = NULL;
            run_region = region;
        }
        if (ok && !(previous && strcmp(previous, numeral) == 0)) {
            if (previous) {
                const char *pair[2] = {previous, numeral};
                char *label = style_label_progression(pair, 2);
                ok = label && count_label(profile, STYLE_CATEGORY_CHORD_PROGRESSION_2, label);
                free(label);
            }
            if (ok && previous && before_previous) {
                const char *triple[3] = {before_previous, previous, numeral};
                char *label = style_label_progression(triple, 3);
                ok = label && count_label(profile, STYLE_CATEGORY_CHORD_PROGRESSION_3, label);
                free(label);
            }
            free(before_previous);
            before_previous = previous;
            previous = numeral;
        } else {
            free(numeral);
        }
        if (!ok) {
            break;
        }
    }
    free(previous);
    free(before_previous);
    if (!ok) {
        return false;
    }
    const cJSON *cadence;
    cJSON_ArrayForEach(cadence, json_member(document, "cadences")) {
        const char *type = json_string_member(cadence, "type");
        if (type && !count_label(profile, STYLE_CATEGORY_CADENCE_TYPE, type)) {
            return false;
        }
    }
    const cJSON *measure;
    cJSON_ArrayForEach(measure, json_member(json_member(document, "harmonic_rhythm"), "per_measure")) {
        long changes;
        if (json_number_member(measure, "changes", &changes) && !count_number(profile, STYLE_CATEGORY_HARMONIC_RHYTHM, changes)) {
            return false;
        }
    }
    return true;
}

static bool count_time_signature(StyleProfile *profile, const cJSON *signature) {
    long numerator, denominator;
    if (!json_number_member(signature, "numerator", &numerator) || !json_number_member(signature, "denominator", &denominator)) {
        return true;
    }
    char label[32];
    snprintf(label, sizeof label, "%ld/%ld", numerator, denominator);
    return count_label(profile, STYLE_CATEGORY_METER, label);
}

static bool count_rhythm(StyleProfile *profile, const cJSON *document) {
    const cJSON *meter = json_member(document, "meter");
    if (!count_time_signature(profile, json_member(meter, "time_signature"))) {
        return false;
    }
    const cJSON *change;
    cJSON_ArrayForEach(change, json_member(meter, "changes")) {
        if (!count_time_signature(profile, json_member(change, "time_signature"))) {
            return false;
        }
    }
    const cJSON *tempo = json_member(json_member(json_member(document, "metadata"), "input"), "tempo");
    const cJSON *declared = json_member(tempo, "declared");
    const cJSON *quarters = json_member(tempo, "quarters_per_minute");
    if (cJSON_IsTrue(declared) && cJSON_IsNumber(quarters)) {
        char label[32];
        style_label_tempo(quarters->valuedouble, label, sizeof label);
        if (!count_label(profile, STYLE_CATEGORY_TEMPO, label)) {
            return false;
        }
    }
    const cJSON *rhythm_claim;
    cJSON_ArrayForEach(rhythm_claim, json_member(document, "rhythm")) {
        const char *claim_label = json_string_member(rhythm_claim, "label");
        const char *claim_line = json_string_member(rhythm_claim, "line");
        const cJSON *mean = json_member(json_member(rhythm_claim, "attacks_per_beat"), "mean");
        if (claim_label && claim_line && strcmp(claim_label, "density") == 0 && strcmp(claim_line, "all") == 0 &&
            cJSON_IsNumber(mean)) {
            char label[32];
            style_label_density(mean->valuedouble, label, sizeof label);
            if (!count_label(profile, STYLE_CATEGORY_NOTE_DENSITY, label)) {
                return false;
            }
        }
    }
    const cJSON *phrase;
    cJSON_ArrayForEach(phrase, json_member(document, "phrases")) {
        long first, last;
        const cJSON *measures = json_member(phrase, "measures");
        if (!json_number_member(measures, "first", &first) || !json_number_member(measures, "last", &last)) {
            continue;
        }
        long syncopations = 0;
        const cJSON *claim;
        cJSON_ArrayForEach(claim, json_member(document, "rhythm")) {
            const char *label = json_string_member(claim, "label");
            const char *line = json_string_member(claim, "line");
            long measure;
            if (label && line && strcmp(label, "syncopation") == 0 && strcmp(line, "soprano") == 0 &&
                json_number_member(claim, "measure", &measure) && measure >= first && measure <= last) {
                syncopations++;
            }
        }
        char label[32];
        if (syncopations >= SYNCOPATION_CAP) {
            snprintf(label, sizeof label, "%d+", SYNCOPATION_CAP);
        } else {
            snprintf(label, sizeof label, "%ld", syncopations);
        }
        if (!count_label(profile, STYLE_CATEGORY_SYNCOPATION, label)) {
            return false;
        }
    }
    return true;
}

/* Soprano durations, melodic intervals and contour. A note held across several voice segments (one entry per melody note) is one note. */
static bool count_soprano(StyleProfile *profile, const cJSON *document) {
    bool have_previous = false;
    long previous_id = 0;
    long previous_pitch = 0;
    const cJSON *entry;
    cJSON_ArrayForEach(entry, json_member(json_member(document, "outer_voices"), "soprano")) {
        const char *status = json_string_member(entry, "status");
        long id, pitch, numerator, denominator;
        if (!status || strcmp(status, "present") != 0 || !json_number_member(entry, "note_id", &id) ||
            !json_number_member(json_member(entry, "pitch"), "midi", &pitch)) {
            continue;
        }
        if (have_previous && id == previous_id) {
            continue;
        }
        if (fraction_member(json_member(entry, "duration"), "whole_notes", &numerator, &denominator)) {
            char label[48];
            style_label_duration((unsigned long)numerator, (unsigned long)denominator, label, sizeof label);
            if (!count_label(profile, STYLE_CATEGORY_DURATION, label)) {
                return false;
            }
        }
        if (have_previous) {
            int step = (int)(pitch - previous_pitch);
            char label[16];
            style_label_interval(step, label, sizeof label);
            if (!count_label(profile, STYLE_CATEGORY_MELODIC_INTERVAL, label) ||
                !count_label(profile, STYLE_CATEGORY_CONTOUR, style_label_contour(step))) {
                return false;
            }
        }
        have_previous = true;
        previous_id = id;
        previous_pitch = pitch;
    }
    return true;
}

/* The strings of a JSON array joined by single spaces into a new string, or NULL when an entry is not a string or on
   allocation failure (*failed). An empty array gives NULL without *failed. */
static char *join_strings(const cJSON *array, bool *failed) {
    int count = cJSON_GetArraySize(array);
    if (count <= 0) {
        return NULL;
    }
    const char **parts = malloc((size_t)count * sizeof *parts);
    if (!parts) {
        *failed = true;
        return NULL;
    }
    int n = 0;
    const cJSON *item;
    cJSON_ArrayForEach(item, array) {
        if (!cJSON_IsString(item)) {
            free(parts);
            return NULL;
        }
        parts[n++] = item->valuestring;
    }
    char *joined = style_label_join(parts, (size_t)n);
    free(parts);
    *failed = *failed || !joined;
    return joined;
}

/* The signed semitones of a JSON array as a motive-intervals label. NULL on a non-number entry, an empty array or
   allocation failure (*failed). */
static char *join_intervals(const cJSON *array, bool *failed) {
    int count = cJSON_GetArraySize(array);
    if (count <= 0) {
        return NULL;
    }
    int *steps = malloc((size_t)count * sizeof *steps);
    if (!steps) {
        *failed = true;
        return NULL;
    }
    int n = 0;
    const cJSON *item;
    cJSON_ArrayForEach(item, array) {
        if (!cJSON_IsNumber(item)) {
            free(steps);
            return NULL;
        }
        steps[n++] = (int)item->valuedouble;
    }
    char *joined = style_label_motive_intervals(steps, (size_t)n);
    free(steps);
    *failed = *failed || !joined;
    return joined;
}

/* The non-chord-tone type the document names for a note, or NULL. The first claim of a note wins. */
static const char *note_nct_type(const cJSON *claims, long note_id) {
    const cJSON *claim;
    cJSON_ArrayForEach(claim, claims) {
        const char *type = json_string_member(claim, "type");
        const cJSON *id;
        if (!type) {
            continue;
        }
        cJSON_ArrayForEach(id, json_member(claim, "note_ids")) {
            if (cJSON_IsNumber(id) && (long)id->valuedouble == note_id) {
                return type;
            }
        }
    }
    return NULL;
}

typedef struct {
    long *ids;
    size_t count;
    size_t capacity;
} SeenNotes;

/* Records the note; false when it was already there or on allocation failure (*failed). */
static bool see_note(SeenNotes *seen, long id, bool *failed) {
    for (size_t i = 0; i < seen->count; i++) {
        if (seen->ids[i] == id) {
            return false;
        }
    }
    if (seen->count == seen->capacity) {
        size_t capacity = seen->capacity ? seen->capacity * 2 : 64;
        long *grown = realloc(seen->ids, capacity * sizeof *grown);
        if (!grown) {
            *failed = true;
            return false;
        }
        seen->ids = grown;
        seen->capacity = capacity;
    }
    seen->ids[seen->count++] = id;
    return true;
}

static const char *degree_at(const cJSON *degrees, int index) {
    const cJSON *item = index >= 0 ? cJSON_GetArrayItem(degrees, index) : NULL;
    return cJSON_IsString(item) ? item->valuestring : "-";
}

/* One label per non-chord tone on a note of an occurrence of a family with features: the previous note's degree, the
   note's degree and type joined by ":", the next note's degree ("1 b6:appoggiatura 5"), a dash for a neighbour the
   occurrence does not have. A note counts once for the piece, whichever occurrence names it first. The family's
   degrees, intervals and rhythm are one label each. */
static bool count_motives(StyleProfile *profile, const cJSON *document) {
    const cJSON *claims = json_member(document, "non_chord_tones");
    SeenNotes seen = {NULL, 0, 0};
    bool ok = true;
    bool failed = false;
    const cJSON *family;
    cJSON_ArrayForEach(family, json_member(document, "motives")) {
        const cJSON *features = json_member(family, "features");
        if (!ok || !features) {
            continue;
        }
        char *degrees = join_strings(json_member(features, "degrees"), &failed);
        char *intervals = failed ? NULL : join_intervals(json_member(features, "intervals"), &failed);
        char *rhythm = failed ? NULL : join_strings(json_member(features, "rhythm"), &failed);
        ok = !failed && (!degrees || count_label(profile, STYLE_CATEGORY_MOTIVE_DEGREES, degrees)) &&
             (!intervals || count_label(profile, STYLE_CATEGORY_MOTIVE_INTERVALS, intervals)) &&
             (!rhythm || count_label(profile, STYLE_CATEGORY_MOTIVE_RHYTHM, rhythm));
        free(degrees);
        free(intervals);
        free(rhythm);
        const cJSON *occurrence;
        cJSON_ArrayForEach(occurrence, json_member(family, "occurrences")) {
            const cJSON *ids = json_member(occurrence, "note_ids");
            const cJSON *occurrence_degrees = json_member(occurrence, "degrees");
            int count = cJSON_GetArraySize(ids);
            if (!ok || !occurrence_degrees || cJSON_GetArraySize(occurrence_degrees) != count) {
                continue;
            }
            for (int k = 0; ok && k < count; k++) {
                const cJSON *id = cJSON_GetArrayItem(ids, k);
                const char *type = cJSON_IsNumber(id) ? note_nct_type(claims, (long)id->valuedouble) : NULL;
                if (!type || !see_note(&seen, (long)id->valuedouble, &failed)) {
                    ok = !failed;
                    continue;
                }
                const char *current = degree_at(occurrence_degrees, k);
                size_t length = strlen(current) + strlen(type) + 2 * 16 + 4;
                char *label = malloc(length);
                if (!label) {
                    ok = false;
                    break;
                }
                style_label_nct_figure(degree_at(occurrence_degrees, k - 1), current, type,
                                       k + 1 < count ? degree_at(occurrence_degrees, k + 1) : NULL, label, length);
                ok = count_label(profile, STYLE_CATEGORY_MOTIVE_NCT_FIGURE, label);
                free(label);
            }
        }
    }
    free(seen.ids);
    return ok && !failed;
}

static bool count_melody(StyleProfile *profile, const cJSON *document) {
    if (!count_soprano(profile, document) || !count_motives(profile, document)) {
        return false;
    }
    const cJSON *phrase;
    cJSON_ArrayForEach(phrase, json_member(document, "melody")) {
        long semitones;
        if (json_number_member(json_member(phrase, "range"), "semitones", &semitones) &&
            !count_number(profile, STYLE_CATEGORY_PHRASE_RANGE, semitones)) {
            return false;
        }
    }
    return true;
}

static bool count_form(StyleProfile *profile, const cJSON *document) {
    const cJSON *theme;
    cJSON_ArrayForEach(theme, json_member(document, "themes")) {
        const char *form = json_string_member(theme, "label");
        if (form && !count_label(profile, STYLE_CATEGORY_THEME_FORM, form)) {
            return false;
        }
    }
    const cJSON *phrase;
    cJSON_ArrayForEach(phrase, json_member(document, "phrases")) {
        long length;
        if (json_number_member(phrase, "length", &length) && !count_number(profile, STYLE_CATEGORY_PHRASE_LENGTH, length)) {
            return false;
        }
    }
    const cJSON *sections = json_member(json_member(document, "form"), "sections");
    size_t length = 1;
    const cJSON *section;
    cJSON_ArrayForEach(section, sections) {
        const char *letter = json_string_member(section, "label");
        length += letter ? strlen(letter) + 1 : 0;
    }
    if (length > 1) {
        char *letters = malloc(length);
        if (!letters) {
            return false;
        }
        letters[0] = '\0';
        cJSON_ArrayForEach(section, sections) {
            const char *letter = json_string_member(section, "label");
            if (letter) {
                if (letters[0]) {
                    strcat(letters, " ");
                }
                strcat(letters, letter);
            }
        }
        bool ok = count_label(profile, STYLE_CATEGORY_SECTION_LETTERS, letters);
        free(letters);
        if (!ok) {
            return false;
        }
    }
    const char *home = json_string_member(json_member(json_member(document, "keys"), "home_key"), "label");
    const char *mode = home ? strrchr(home, ' ') : NULL;
    if (mode && mode[1] && !count_label(profile, STYLE_CATEGORY_MODE, mode + 1)) {
        return false;
    }
    return true;
}

/* Counts one document tree (schema version 9) into the profile. */
static StyleProfileResult add_document_tree(StyleProfile *profile, const cJSON *document) {
    long version;
    if (!document || !json_number_member(document, "schema_version", &version) ||
        version != URSATZ_ANALYZER_DOCUMENT_SCHEMA_VERSION) {
        return STYLE_PROFILE_ERR_FORMAT;
    }
    bool ok = count_harmony(profile, document) && count_rhythm(profile, document) && count_melody(profile, document) &&
              count_form(profile, document);
    if (!ok) {
        return STYLE_PROFILE_ERR_ALLOC;
    }
    profile->pieces++;
    return STYLE_PROFILE_OK;
}

StyleProfileResult style_profile_add_document_json(StyleProfile *profile, const char *json) {
    if (!profile || !json) {
        return STYLE_PROFILE_ERR_ARGUMENT;
    }
    cJSON *document = cJSON_Parse(json);
    StyleProfileResult result = add_document_tree(profile, document);
    cJSON_Delete(document);
    return result;
}

StyleProfileResult style_profile_add_result(StyleProfile *profile, const AnalyzerResult *result) {
    if (!profile || !result) {
        return STYLE_PROFILE_ERR_ARGUMENT;
    }
    /* The document is the result's export view (R-252): its in-memory tree is counted, with no text rendered or parsed. */
    AnalysisDocument *document = analysis_document_build_from_result("<fragment>", result);
    if (!document) {
        return analyzer_result_report_complete(result) ? STYLE_PROFILE_ERR_ALLOC : STYLE_PROFILE_ERR_ARGUMENT;
    }
    StyleProfileResult added = add_document_tree(profile, document->root);
    analysis_document_destroy(document);
    return added;
}

/* ---- corpus ---- */

static bool add_training_corpus(StyleProfile *profile, const char *input_dir) {
    if (strcmp(profile->training_corpus, input_dir) == 0) {
        return true;
    }
    size_t old = strlen(profile->training_corpus);
    char *grown = realloc(profile->training_corpus, old + strlen(input_dir) + 2);
    if (!grown) {
        return false;
    }
    profile->training_corpus = grown;
    if (old > 0) {
        grown[old++] = ';';
    }
    strcpy(grown + old, input_dir);
    return true;
}

typedef struct {
    StyleProfile *profile;
    StyleProfileResult result;
} CorpusAdd;

static bool add_corpus_document(const char *piece_path, const char *document, const char *json,
                                AnalyzerDocumentStatus status, void *context) {
    (void)piece_path;
    (void)document;
    (void)status;
    CorpusAdd *add = context;
    add->result = style_profile_add_document_json(add->profile, json);
    return add->result == STYLE_PROFILE_OK;
}

StyleProfileResult style_profile_add_corpus(StyleProfile *profile, const char *output_dir) {
    if (!profile || !output_dir) {
        return STYLE_PROFILE_ERR_ARGUMENT;
    }
    char *index_path = malloc(strlen(output_dir) + 24);
    if (!index_path) {
        return STYLE_PROFILE_ERR_ALLOC;
    }
    sprintf(index_path, "%s/corpus-index.json", output_dir);
    CorpusAdd add = {profile, STYLE_PROFILE_OK};
    char *input_dir = NULL;
    CorpusWalkResult walked = corpus_walk_documents(index_path, add_corpus_document, &add, &input_dir);
    free(index_path);
    StyleProfileResult result = STYLE_PROFILE_OK;
    switch (walked) {
    case CORPUS_WALK_OK:
        if (!add_training_corpus(profile, input_dir)) {
            result = STYLE_PROFILE_ERR_ALLOC;
        }
        break;
    case CORPUS_WALK_ERR_ALLOC:
        result = STYLE_PROFILE_ERR_ALLOC;
        break;
    case CORPUS_WALK_ERR_IO:
        result = STYLE_PROFILE_ERR_IO;
        break;
    case CORPUS_WALK_ERR_FORMAT:
        result = STYLE_PROFILE_ERR_FORMAT;
        break;
    case CORPUS_WALK_ERR_VISIT:
        result = add.result;
        break;
    }
    free(input_dir);
    return result;
}

/* ---- statistics ---- */

StyleProfileResult style_profile_statistics(const StyleProfile *profile, StyleStatistic **out, size_t *count) {
    if (!profile || !out || !count) {
        return STYLE_PROFILE_ERR_ARGUMENT;
    }
    *out = NULL;
    *count = profile->count;
    if (profile->count == 0) {
        return STYLE_PROFILE_OK;
    }
    StyleStatistic *statistics = malloc(profile->count * sizeof *statistics);
    if (!statistics) {
        return STYLE_PROFILE_ERR_ALLOC;
    }
    size_t start = 0;
    for (size_t i = 0; i <= profile->count; i++) {
        if (i == profile->count || strcmp(profile->entries[i].category, profile->entries[start].category) != 0) {
            unsigned long total = 0;
            for (size_t j = start; j < i; j++) {
                total += profile->entries[j].count;
            }
            for (size_t j = start; j < i; j++) {
                statistics[j] = (StyleStatistic){profile->entries[j].category, profile->entries[j].label,
                                                 profile->entries[j].count, total};
            }
            start = i;
        }
    }
    *out = statistics;
    return STYLE_PROFILE_OK;
}
