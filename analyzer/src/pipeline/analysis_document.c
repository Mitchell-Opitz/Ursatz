#include <ursatz_analyzer/analysis_document.h>

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "framework.h"
#include "pipeline/path_util.h"
#include "theory/framework.h"
#include "time/duration.h"
#include "time/musical_time.h"
#include "time/temporal_anchor.h"

#ifndef URSATZ_ANALYZER_VERSION
#define URSATZ_ANALYZER_VERSION "unknown"
#endif
#ifndef URSATZ_ANALYZER_URSATZ_COMMIT
#define URSATZ_ANALYZER_URSATZ_COMMIT "unknown"
#endif

/* Source id for a key read from the MIDI key signature meta-event: no
   framework Interpretation is involved there. */
static const char KEY_SIGNATURE_SOURCE[] = "midi-key-signature";

/* ---- Rational time (whole notes) ---- */

typedef struct {
    long long num;
    unsigned long long den;
} Rational;

static unsigned long long gcd_ull(unsigned long long a, unsigned long long b) {
    while (b != 0) {
        unsigned long long t = a % b;
        a = b;
        b = t;
    }
    return a;
}

static Rational rational_make(long long num, unsigned long long den) {
    unsigned long long magnitude = num < 0 ? (unsigned long long)(-num) : (unsigned long long)num;
    unsigned long long divisor = gcd_ull(magnitude, den);
    if (divisor > 1) {
        num /= (long long)divisor;
        den /= divisor;
    }
    return (Rational){num, den};
}

static bool rational_less(Rational a, Rational b) {
    return a.num * (long long)b.den < b.num * (long long)a.den;
}

static Rational note_onset(const NoteEvent *event) {
    const MusicalTime *onset = temporal_anchor_position(note_event_temporal_anchor(event));
    return rational_make(musical_time_numerator(onset), musical_time_denominator(onset));
}

static Rational note_end(const NoteEvent *event) {
    Rational onset = note_onset(event);
    const Duration *duration = note_event_duration(event);
    long long duration_num = duration_numerator(duration);
    unsigned long long duration_den = duration_denominator(duration);
    return rational_make(onset.num * (long long)duration_den + duration_num * (long long)onset.den,
                         onset.den * duration_den);
}

/* Earliest onset to latest note end over the given notes (ids NULL means
   every imported note). */
static void notes_span(const ImportedNotes *notes, const size_t *ids, size_t id_count, Rational *start,
                       Rational *end) {
    *start = (Rational){0, 1};
    *end = (Rational){0, 1};
    for (size_t i = 0; i < id_count; i++) {
        const NoteEvent *event = notes->events[ids ? ids[i] : i];
        Rational onset = note_onset(event);
        Rational finish = note_end(event);
        if (i == 0 || rational_less(onset, *start)) {
            *start = onset;
        }
        if (i == 0 || rational_less(*end, finish)) {
            *end = finish;
        }
    }
}

/* ---- Document construction ---- */

/* Adds item under key, taking ownership; false (item freed) on failure. */
static bool add(cJSON *object, const char *key, cJSON *item) {
    if (!item) {
        return false;
    }
    if (!cJSON_AddItemToObject(object, key, item)) {
        cJSON_Delete(item);
        return false;
    }
    return true;
}

static bool append(cJSON *array, cJSON *item) {
    if (!item) {
        return false;
    }
    if (!cJSON_AddItemToArray(array, item)) {
        cJSON_Delete(item);
        return false;
    }
    return true;
}

static cJSON *rational_json(Rational value) {
    cJSON *object = cJSON_CreateObject();
    if (object && !(add(object, "num", cJSON_CreateNumber((double)value.num)) &&
                    add(object, "den", cJSON_CreateNumber((double)value.den)))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

static cJSON *span_json(Rational start, Rational end) {
    cJSON *object = cJSON_CreateObject();
    if (object && !(add(object, "start", rational_json(start)) && add(object, "end", rational_json(end)))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

static cJSON *note_ids_json(const size_t *ids, size_t id_count) {
    cJSON *array = cJSON_CreateArray();
    if (!array) {
        return NULL;
    }
    for (size_t i = 0; i < id_count; i++) {
        if (!append(array, cJSON_CreateNumber((double)(ids ? ids[i] : i)))) {
            cJSON_Delete(array);
            return NULL;
        }
    }
    return array;
}

static cJSON *uncertainty_json(AnalysisConfidence confidence) {
    if (!confidence.present || confidence.denominator == 0) {
        return cJSON_CreateNull();
    }
    return cJSON_CreateNumber((double)confidence.numerator / (double)confidence.denominator);
}

/* The shared claim fields, in schema order. ids NULL means every
   imported note. */
static cJSON *claim_json(const char *framework, const ImportedNotes *notes, const size_t *ids, size_t id_count,
                         const char *label, AnalysisConfidence confidence) {
    Rational start;
    Rational end;
    notes_span(notes, ids, id_count, &start, &end);

    cJSON *claim = cJSON_CreateObject();
    if (claim && !(add(claim, "framework", cJSON_CreateString(framework)) &&
                   add(claim, "span", span_json(start, end)) &&
                   add(claim, "note_ids", note_ids_json(ids, id_count)) &&
                   add(claim, "label", cJSON_CreateString(label)) &&
                   add(claim, "uncertainty", uncertainty_json(confidence)))) {
        cJSON_Delete(claim);
        return NULL;
    }
    return claim;
}

static const char *library_framework_name(void) {
    const Framework *framework = common_practice_minimal_framework();
    return framework ? framework_name(framework) : "unknown";
}

static const char *key_method_name(AnalysisKeyMethod method) {
    switch (method) {
        case ANALYSIS_KEY_METHOD_KEY_SIGNATURE:
            return "key_signature";
        case ANALYSIS_KEY_METHOD_BASS_FALLBACK:
            return "bass_fallback";
        case ANALYSIS_KEY_METHOD_PITCH_CLASS_SCAN:
            return "pitch_class_scan";
        default:
            return NULL;
    }
}

static cJSON *metadata_json(const char *input_path, const ImportedNotes *notes, const AnalysisReport *report) {
    cJSON *metadata = cJSON_CreateObject();
    if (metadata &&
        !(add(metadata, "source_file", cJSON_CreateString(path_basename(input_path))) &&
          add(metadata, "analyzer_version", cJSON_CreateString(URSATZ_ANALYZER_VERSION)) &&
          add(metadata, "ursatz_commit", cJSON_CreateString(URSATZ_ANALYZER_URSATZ_COMMIT)) &&
          add(metadata, "note_count", cJSON_CreateNumber((double)notes->count)) &&
          add(metadata, "ticks_per_quarter", notes->ticks_per_quarter > 0
                                                 ? cJSON_CreateNumber((double)notes->ticks_per_quarter)
                                                 : cJSON_CreateNull()) &&
          add(metadata, "key_found", cJSON_CreateBool(report->key_found)))) {
        cJSON_Delete(metadata);
        return NULL;
    }
    return metadata;
}

/* One whole-piece key claim, or an empty array when no key was found. */
static cJSON *keys_json(const ImportedNotes *notes, const AnalysisReport *report) {
    cJSON *keys = cJSON_CreateArray();
    if (!keys) {
        return NULL;
    }
    const char *method = key_method_name(report->key_method);
    if (!report->key_found || report->entry_count == 0 || notes->count == 0 || !method) {
        return keys;
    }

    const char *framework = report->key_method == ANALYSIS_KEY_METHOD_KEY_SIGNATURE ? KEY_SIGNATURE_SOURCE
                                                                                    : library_framework_name();
    cJSON *claim = claim_json(framework, notes, NULL, notes->count, report->entries[0].key_label,
                              report->key_confidence);
    if (!(claim && add(claim, "method", cJSON_CreateString(method)))) {
        cJSON_Delete(claim);
        cJSON_Delete(keys);
        return NULL;
    }
    if (!append(keys, claim)) {
        cJSON_Delete(keys);
        return NULL;
    }
    return keys;
}

/* One chord claim per report entry, in time order. */
static cJSON *chords_json(const ImportedNotes *notes, const AnalysisReport *report) {
    cJSON *chords = cJSON_CreateArray();
    if (!chords) {
        return NULL;
    }
    const char *framework = library_framework_name();
    for (size_t i = 0; i < report->entry_count; i++) {
        const AnalysisReportEntry *entry = &report->entries[i];
        cJSON *claim = claim_json(framework, notes, entry->note_ids, entry->note_id_count, entry->chord_label,
                                  entry->chord_confidence);
        if (!(claim && add(claim, "key_label", cJSON_CreateString(entry->key_label)))) {
            cJSON_Delete(claim);
            cJSON_Delete(chords);
            return NULL;
        }
        if (!append(chords, claim)) {
            cJSON_Delete(chords);
            return NULL;
        }
    }
    return chords;
}

static cJSON *document_json(const char *input_path, const ImportedNotes *notes, const AnalysisReport *report) {
    cJSON *document = cJSON_CreateObject();
    if (document &&
        !(add(document, "schema_version", cJSON_CreateNumber(URSATZ_ANALYZER_DOCUMENT_SCHEMA_VERSION)) &&
          add(document, "metadata", metadata_json(input_path, notes, report)) &&
          add(document, "meter", cJSON_CreateNull()) && add(document, "keys", keys_json(notes, report)) &&
          add(document, "chords", chords_json(notes, report)) && add(document, "cadences", cJSON_CreateArray()) &&
          add(document, "phrases", cJSON_CreateArray()) && add(document, "form", cJSON_CreateNull()) &&
          add(document, "motives", cJSON_CreateArray()) && add(document, "melody", cJSON_CreateArray()) &&
          add(document, "rhythm", cJSON_CreateArray()) && add(document, "texture", cJSON_CreateArray()) &&
          add(document, "voice_leading", cJSON_CreateArray()) && add(document, "schemas", cJSON_CreateArray()))) {
        cJSON_Delete(document);
        return NULL;
    }
    return document;
}

/* ---- Pretty printing (2-space indent; cJSON does all escaping) ---- */

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} Buffer;

static bool buffer_append(Buffer *buffer, const char *text) {
    size_t text_len = strlen(text);
    if (buffer->length + text_len + 1 > buffer->capacity) {
        size_t capacity = buffer->capacity ? buffer->capacity : 1024;
        while (buffer->length + text_len + 1 > capacity) {
            capacity *= 2;
        }
        char *data = realloc(buffer->data, capacity);
        if (!data) {
            return false;
        }
        buffer->data = data;
        buffer->capacity = capacity;
    }
    memcpy(buffer->data + buffer->length, text, text_len + 1);
    buffer->length += text_len;
    return true;
}

static bool buffer_indent(Buffer *buffer, int depth) {
    for (int i = 0; i < depth; i++) {
        if (!buffer_append(buffer, "  ")) {
            return false;
        }
    }
    return true;
}

static bool print_scalar(Buffer *buffer, const cJSON *item) {
    char *text = cJSON_PrintUnformatted(item);
    if (!text) {
        return false;
    }
    bool ok = buffer_append(buffer, text);
    cJSON_free(text);
    return ok;
}

static bool print_key(Buffer *buffer, const char *key) {
    cJSON *key_item = cJSON_CreateString(key);
    if (!key_item) {
        return false;
    }
    bool ok = print_scalar(buffer, key_item) && buffer_append(buffer, ": ");
    cJSON_Delete(key_item);
    return ok;
}

static bool is_container(const cJSON *item) {
    return cJSON_IsObject(item) || cJSON_IsArray(item);
}

/* Objects print one member per line. Arrays of scalars (note_ids) print
   on one line; arrays holding objects print one element per line. */
static bool print_value(Buffer *buffer, const cJSON *item, int depth) {
    if (!is_container(item)) {
        return print_scalar(buffer, item);
    }
    bool is_object = cJSON_IsObject(item);
    const char *close = is_object ? "}" : "]";
    if (!item->child) {
        return buffer_append(buffer, is_object ? "{" : "[") && buffer_append(buffer, close);
    }

    bool multiline = is_object;
    for (const cJSON *child = item->child; child && !multiline; child = child->next) {
        multiline = is_container(child);
    }
    if (!multiline) {
        if (!buffer_append(buffer, "[")) {
            return false;
        }
        for (const cJSON *child = item->child; child; child = child->next) {
            if ((child != item->child && !buffer_append(buffer, ", ")) || !print_scalar(buffer, child)) {
                return false;
            }
        }
        return buffer_append(buffer, "]");
    }

    if (!buffer_append(buffer, is_object ? "{\n" : "[\n")) {
        return false;
    }
    for (const cJSON *child = item->child; child; child = child->next) {
        if (!buffer_indent(buffer, depth + 1) || (is_object && !print_key(buffer, child->string)) ||
            !print_value(buffer, child, depth + 1) || !buffer_append(buffer, child->next ? ",\n" : "\n")) {
            return false;
        }
    }
    return buffer_indent(buffer, depth) && buffer_append(buffer, close);
}

char *analysis_document_render(const char *input_path, const ImportedNotes *notes, const AnalysisReport *report) {
    cJSON *document = document_json(input_path, notes, report);
    if (!document) {
        return NULL;
    }

    Buffer buffer = {NULL, 0, 0};
    bool ok = print_value(&buffer, document, 0) && buffer_append(&buffer, "\n");
    cJSON_Delete(document);
    if (!ok) {
        free(buffer.data);
        return NULL;
    }
    return buffer.data;
}

char *derive_analysis_document_path(const char *input_path) {
    return path_with_suffix(input_path, ".analysis.json");
}
