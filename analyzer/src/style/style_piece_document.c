#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>

#include <ursatz_analyzer/analysis_document.h>

#include "style/style_profile_internal.h"
#include "util/json_util.h"

static bool fraction_member(const cJSON *object, const char *name, long *numerator, long *denominator) {
    const cJSON *fraction = json_member(object, name);
    return json_number_member(fraction, "num", numerator) && json_number_member(fraction, "den", denominator) && *denominator > 0 &&
           *numerator >= 0;
}

static bool read_harmony(StylePiece *piece, const cJSON *document) {
    const cJSON *chord;
    cJSON_ArrayForEach(chord, json_member(document, "chords")) {
        const cJSON *roman = json_member(chord, "roman_numeral");
        const char *base = json_string_member(roman, "base_numeral");
        long region = 0;
        bool has_region = json_number_member(roman, "key_region", &region);
        if (!style_piece_add_chord(piece, base, has_region, region)) {
            return false;
        }
    }
    const cJSON *cadence;
    cJSON_ArrayForEach(cadence, json_member(document, "cadences")) {
        const char *type = json_string_member(cadence, "type");
        if (type && !style_piece_add_cadence_type(piece, type)) {
            return false;
        }
    }
    const cJSON *measure;
    cJSON_ArrayForEach(measure, json_member(json_member(document, "harmonic_rhythm"), "per_measure")) {
        long changes;
        if (json_number_member(measure, "changes", &changes) && !style_piece_add_harmonic_rhythm(piece, changes)) {
            return false;
        }
    }
    return true;
}

static bool read_time_signature(StylePiece *piece, const cJSON *signature) {
    long numerator, denominator;
    if (!json_number_member(signature, "numerator", &numerator) || !json_number_member(signature, "denominator", &denominator)) {
        return true;
    }
    return style_piece_add_time_signature(piece, numerator, denominator);
}

static bool read_rhythm(StylePiece *piece, const cJSON *document) {
    const cJSON *meter = json_member(document, "meter");
    if (!read_time_signature(piece, json_member(meter, "time_signature"))) {
        return false;
    }
    const cJSON *change;
    cJSON_ArrayForEach(change, json_member(meter, "changes")) {
        if (!read_time_signature(piece, json_member(change, "time_signature"))) {
            return false;
        }
    }
    const cJSON *tempo = json_member(json_member(json_member(document, "metadata"), "input"), "tempo");
    const cJSON *declared = json_member(tempo, "declared");
    const cJSON *quarters = json_member(tempo, "quarters_per_minute");
    if (cJSON_IsTrue(declared) && cJSON_IsNumber(quarters)) {
        style_piece_set_tempo(piece, true, quarters->valuedouble);
    }
    const cJSON *claim;
    cJSON_ArrayForEach(claim, json_member(document, "rhythm")) {
        const char *label = json_string_member(claim, "label");
        const char *line = json_string_member(claim, "line");
        if (!label || !line) {
            continue;
        }
        if (strcmp(label, "density") == 0 && strcmp(line, "all") == 0) {
            const cJSON *mean = json_member(json_member(claim, "attacks_per_beat"), "mean");
            if (cJSON_IsNumber(mean) && !style_piece_add_density(piece, mean->valuedouble)) {
                return false;
            }
        }
        long measure;
        if (strcmp(label, "syncopation") == 0 && strcmp(line, "soprano") == 0 &&
            json_number_member(claim, "measure", &measure) && !style_piece_add_syncopation(piece, measure)) {
            return false;
        }
    }
    const cJSON *phrase;
    cJSON_ArrayForEach(phrase, json_member(document, "phrases")) {
        long first, last, length;
        const cJSON *measures = json_member(phrase, "measures");
        if (json_number_member(measures, "first", &first) && json_number_member(measures, "last", &last) &&
            json_number_member(phrase, "length", &length) && !style_piece_add_phrase(piece, first, last, length)) {
            return false;
        }
    }
    return true;
}

static bool read_soprano(StylePiece *piece, const cJSON *document) {
    const cJSON *entry;
    cJSON_ArrayForEach(entry, json_member(json_member(document, "outer_voices"), "soprano")) {
        const char *status = json_string_member(entry, "status");
        long id, pitch;
        StylePieceSopranoNote note = {0};
        if (!status || strcmp(status, "present") != 0 || !json_number_member(entry, "note_id", &id) ||
            !json_number_member(json_member(entry, "pitch"), "midi", &pitch)) {
            continue;
        }
        long numerator, denominator;
        note.present = true;
        note.note_id = id;
        note.midi = pitch;
        if (fraction_member(json_member(entry, "duration"), "whole_notes", &numerator, &denominator)) {
            note.has_duration = true;
            note.duration = (Rational){numerator, denominator};
        }
        if (!style_piece_add_soprano_note(piece, &note)) {
            return false;
        }
    }
    return true;
}

/* The strings of a JSON array into a new array of count entries (NULL for an entry that is not a string); NULL with
   *count 0 for an empty array, and *failed on allocation failure. Free with free_strings. */
static char **read_strings(const cJSON *array, size_t *count, bool *failed) {
    int size = cJSON_GetArraySize(array);
    *count = 0;
    if (size <= 0) {
        return NULL;
    }
    char **strings = calloc((size_t)size, sizeof *strings);
    if (!strings) {
        *failed = true;
        return NULL;
    }
    int n = 0;
    const cJSON *item;
    cJSON_ArrayForEach(item, array) {
        if (cJSON_IsString(item) && !(strings[n] = strdup(item->valuestring))) {
            *failed = true;
        }
        n++;
    }
    *count = (size_t)size;
    return strings;
}

static void free_strings(char **strings, size_t count) {
    for (size_t i = 0; strings && i < count; i++) {
        free(strings[i]);
    }
    free(strings);
}

static bool read_occurrence(StylePieceMotive *family, const cJSON *occurrence) {
    const cJSON *ids = json_member(occurrence, "note_ids");
    const cJSON *degrees = json_member(occurrence, "degrees");
    int count = cJSON_GetArraySize(ids);
    if (!degrees || cJSON_GetArraySize(degrees) != count || count <= 0) {
        return true;
    }
    long *note_ids = calloc((size_t)count, sizeof *note_ids);
    bool failed = !note_ids;
    size_t degree_count = 0;
    char **labels = failed ? NULL : read_strings(degrees, &degree_count, &failed);
    for (int k = 0; !failed && k < count; k++) {
        const cJSON *id = cJSON_GetArrayItem(ids, k);
        if (!cJSON_IsNumber(id)) {
            /* a document's note ids are numbers; one that is not leaves the occurrence out */
            free(note_ids);
            free_strings(labels, degree_count);
            return true;
        }
        note_ids[k] = (long)id->valuedouble;
    }
    bool ok = !failed && style_piece_motive_add_occurrence(family, note_ids, (const char *const *)labels, (size_t)count);
    free(note_ids);
    free_strings(labels, degree_count);
    return ok;
}

static bool read_motives(StylePiece *piece, const cJSON *document) {
    const cJSON *non_chord_tone;
    cJSON_ArrayForEach(non_chord_tone, json_member(document, "non_chord_tones")) {
        const char *type = json_string_member(non_chord_tone, "type");
        const cJSON *ids = json_member(non_chord_tone, "note_ids");
        const cJSON *id;
        if (!type) {
            continue;
        }
        cJSON_ArrayForEach(id, ids) {
            if (cJSON_IsNumber(id) && !style_piece_add_non_chord_tone(piece, (long)id->valuedouble, type)) {
                return false;
            }
        }
    }
    const cJSON *family;
    cJSON_ArrayForEach(family, json_member(document, "motives")) {
        const cJSON *features = json_member(family, "features");
        StylePieceMotive *motive = style_piece_add_motive(piece);
        if (!motive) {
            return false;
        }
        if (!features) {
            continue;
        }
        bool failed = false;
        size_t degree_count = 0, rhythm_count = 0;
        char **degrees = read_strings(json_member(features, "degrees"), &degree_count, &failed);
        char **rhythm = failed ? NULL : read_strings(json_member(features, "rhythm"), &rhythm_count, &failed);
        const cJSON *interval_array = json_member(features, "intervals");
        int interval_size = cJSON_GetArraySize(interval_array);
        int *intervals = NULL;
        size_t interval_count = 0;
        bool intervals_valid = true;
        if (!failed && interval_size > 0) {
            intervals = malloc((size_t)interval_size * sizeof *intervals);
            failed = !intervals;
            const cJSON *item;
            cJSON_ArrayForEach(item, interval_array) {
                if (failed) {
                    break;
                }
                if (!cJSON_IsNumber(item)) {
                    intervals_valid = false;
                    break;
                }
                intervals[interval_count++] = (int)item->valuedouble;
            }
        }
        bool ok = !failed;
        if (ok) {
            /* an interval array that holds a non-number gives no label: the intervals are left empty */
            ok = style_piece_motive_set_features(motive, (const char *const *)degrees, degree_count, intervals,
                                                 intervals_valid ? interval_count : 0, (const char *const *)rhythm,
                                                 rhythm_count);
        }
        free_strings(degrees, degree_count);
        free_strings(rhythm, rhythm_count);
        free(intervals);
        if (!ok) {
            return false;
        }
        const cJSON *occurrence;
        cJSON_ArrayForEach(occurrence, json_member(family, "occurrences")) {
            if (!read_occurrence(motive, occurrence)) {
                return false;
            }
        }
    }
    return true;
}

static bool read_melody(StylePiece *piece, const cJSON *document) {
    if (!read_soprano(piece, document) || !read_motives(piece, document)) {
        return false;
    }
    const cJSON *phrase;
    cJSON_ArrayForEach(phrase, json_member(document, "melody")) {
        long semitones;
        if (json_number_member(json_member(phrase, "range"), "semitones", &semitones) &&
            !style_piece_add_melody_range(piece, semitones)) {
            return false;
        }
    }
    return true;
}

static bool read_form(StylePiece *piece, const cJSON *document) {
    const cJSON *theme;
    cJSON_ArrayForEach(theme, json_member(document, "themes")) {
        const char *form = json_string_member(theme, "label");
        if (form && !style_piece_add_theme_label(piece, form)) {
            return false;
        }
    }
    const cJSON *section;
    cJSON_ArrayForEach(section, json_member(json_member(document, "form"), "sections")) {
        const char *letter = json_string_member(section, "label");
        if (letter && !style_piece_add_section_label(piece, letter)) {
            return false;
        }
    }
    const char *home = json_string_member(json_member(json_member(document, "keys"), "home_key"), "label");
    return !home || style_piece_set_home_key_label(piece, home);
}

StyleProfileResult style_piece_from_document(const cJSON *document, StylePiece *piece) {
    long version;
    if (!document || !piece || !json_number_member(document, "schema_version", &version) ||
        version != URSATZ_ANALYZER_DOCUMENT_SCHEMA_VERSION) {
        return STYLE_PROFILE_ERR_FORMAT;
    }
    memset(piece, 0, sizeof *piece);
    if (!read_harmony(piece, document) || !read_rhythm(piece, document) || !read_melody(piece, document) ||
        !read_form(piece, document)) {
        style_piece_destroy(piece);
        return STYLE_PROFILE_ERR_ALLOC;
    }
    return STYLE_PROFILE_OK;
}
