#include <ursatz_analyzer/analysis_document.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "beat_grid.h"
#include "cJSON.h"
#include "framework.h"
#include "metric_position_rules.h"
#include "pipeline/analysis_document_internal.h"
#include "pipeline/path_util.h"
#include "theory/framework.h"
#include "time/duration.h"
#include "time/meter_map.h"
#include "time/metrical_position.h"
#include "time/musical_time.h"
#include "time/temporal_anchor.h"
#include "time/time_signature.h"

#ifndef URSATZ_ANALYZER_VERSION
#define URSATZ_ANALYZER_VERSION "unknown"
#endif
#ifndef URSATZ_ANALYZER_URSATZ_COMMIT
#define URSATZ_ANALYZER_URSATZ_COMMIT "unknown"
#endif

/* Source id for a key read from the MIDI key signature meta-event: no
   framework Interpretation is involved there. */
static const char KEY_SIGNATURE_SOURCE[] = "midi-key-signature";

/* Source ids for the meter: a time signature the MIDI file declares, and
   the 4/4 the library assumes when it declares none. */
static const char TIME_SIGNATURE_SOURCE[] = "midi-time-signature";
static const char DEFAULT_METER_SOURCE[] = "ursatz-default-meter";

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

/* Measure and beat of one time on the piece's measure grid:
   {"measure", "beat", "subdivision"}. Measures are numbered as the
   library numbers them (the first full measure is 1, a pickup is 0).
   "beat" is 1-based; "subdivision" is the exact fraction of that beat
   past its start, as a rational (0/1 on the beat). Null when the
   position cannot be read, e.g. a signature with no beat grid. */
static cJSON *position_json(const MeterMap *map, Rational time) {
    MusicalTime *musical_time = NULL;
    if (musical_time_create((long)time.num, (unsigned long)time.den, &musical_time) != MUSICAL_TIME_OK) {
        return cJSON_CreateNull();
    }
    MetricalPosition *position = NULL;
    MetricalPositionResult result = metrical_position_from_time(map, musical_time, &position);
    musical_time_destroy(musical_time);
    if (result != METRICAL_POSITION_OK) {
        return cJSON_CreateNull();
    }

    int measure = metrical_position_measure(position);
    const TimeSignature *reading = NULL;
    StandardMeterBeatPosition beat;
    bool readable = metrical_position_reading_signature(map, measure, &reading) == METRICAL_POSITION_OK &&
                    standard_meter_beat_position(reading, position, &beat);
    metrical_position_destroy(position);
    if (!readable) {
        return cJSON_CreateNull();
    }

    cJSON *object = cJSON_CreateObject();
    if (object &&
        !(add(object, "measure", cJSON_CreateNumber((double)measure)) &&
          add(object, "beat", cJSON_CreateNumber((double)beat.beat)) &&
          add(object, "subdivision",
              rational_json(rational_make(beat.subdivision_numerator, beat.subdivision_denominator))))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* Where a span starts and ends on the measure grid. The end is the
   position of the span's (exclusive) end time, so a span that ends on a
   barline ends at beat 1 of the next measure. */
static cJSON *location_json(const MeterMap *map, Rational start, Rational end) {
    cJSON *object = cJSON_CreateObject();
    if (object &&
        !(add(object, "start", position_json(map, start)) && add(object, "end", position_json(map, end)))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* The shared claim fields, in schema order. ids NULL means every
   imported note. */
static cJSON *claim_json_at(const char *framework, const ImportedNotes *notes, const size_t *ids,
                            size_t id_count, Rational start, Rational end, const char *label,
                            AnalysisConfidence confidence) {
    cJSON *claim = cJSON_CreateObject();
    if (claim && !(add(claim, "framework", cJSON_CreateString(framework)) &&
                   add(claim, "span", span_json(start, end)) &&
                   add(claim, "location", location_json(notes->meter.meter_map, start, end)) &&
                   add(claim, "note_ids", note_ids_json(ids, id_count)) &&
                   add(claim, "label", cJSON_CreateString(label)) &&
                   add(claim, "uncertainty", uncertainty_json(confidence)))) {
        cJSON_Delete(claim);
        return NULL;
    }
    return claim;
}

static cJSON *claim_json(const char *framework, const ImportedNotes *notes, const size_t *ids, size_t id_count,
                         const char *label, AnalysisConfidence confidence) {
    Rational start;
    Rational end;
    notes_span(notes, ids, id_count, &start, &end);
    return claim_json_at(framework, notes, ids, id_count, start, end, label, confidence);
}

static Rational time_rational(const MusicalTime *time) {
    return rational_make(musical_time_numerator(time), musical_time_denominator(time));
}

static cJSON *time_signature_json(const TimeSignature *time_signature) {
    cJSON *object = cJSON_CreateObject();
    if (object &&
        !(add(object, "numerator", cJSON_CreateNumber((double)time_signature_top_number(time_signature))) &&
          add(object, "denominator", cJSON_CreateNumber((double)time_signature_bottom_number(time_signature))))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* The pickup region's length in whole notes and in beats of the
   signature its position is read in (null beats when that signature has
   no beat grid). */
static cJSON *pickup_json(const MeterMap *map) {
    const MeterMapRegion *region = meter_map_region(map, 0);
    const MusicalTime *end = meter_map_region_end(region);
    const TimeSignature *reading = NULL;
    if (!end || metrical_position_reading_signature(map, 0, &reading) != METRICAL_POSITION_OK) {
        return NULL;
    }
    Rational length = time_rational(end);

    cJSON *beats = NULL;
    Duration *beat = NULL;
    if (standard_meter_beat_start(reading, 1, &beat)) {
        beats = rational_json(rational_make(length.num * (long long)duration_denominator(beat),
                                            length.den * (unsigned long long)duration_numerator(beat)));
        duration_destroy(beat);
    } else {
        beats = cJSON_CreateNull();
    }

    cJSON *pickup = cJSON_CreateObject();
    if (pickup && !(add(pickup, "length", rational_json(length)) && add(pickup, "beats", beats))) {
        cJSON_Delete(pickup);
        return NULL;
    }
    if (!pickup) {
        cJSON_Delete(beats);
    }
    return pickup;
}

/* Every region after the first full measure's, one entry per meter change. */
static cJSON *meter_changes_json(const MeterMap *map, size_t first_index) {
    cJSON *changes = cJSON_CreateArray();
    if (!changes) {
        return NULL;
    }
    for (size_t i = first_index; i < meter_map_region_count(map); i++) {
        const MeterMapRegion *region = meter_map_region(map, i);
        cJSON *change = cJSON_CreateObject();
        if (!(change &&
              add(change, "measure", cJSON_CreateNumber((double)meter_map_region_first_measure_number(region))) &&
              add(change, "start", rational_json(time_rational(meter_map_region_start(region)))) &&
              add(change, "time_signature", time_signature_json(meter_map_region_time_signature(region))) &&
              add(change, "inferred", cJSON_CreateBool(meter_map_region_is_defaulted(region))))) {
            cJSON_Delete(change);
            cJSON_Delete(changes);
            return NULL;
        }
        if (!append(changes, change)) {
            cJSON_Delete(changes);
            return NULL;
        }
    }
    return changes;
}

/* The piece's meter claim. The time signature is that of the first full
   measure; a pickup before it is reported separately, and later signature
   changes under "changes". A file without a time signature gets the
   library's 4/4 default, reported with source "inferred". */
static cJSON *meter_json(const ImportedNotes *notes) {
    const MeterMap *map = notes->meter.meter_map;
    bool inferred = meter_map_is_defaulted(map);
    bool has_pickup = meter_map_has_pickup(map);
    size_t primary_index = has_pickup ? 1 : 0;
    const TimeSignature *time_signature = meter_map_region_time_signature(meter_map_region(map, primary_index));

    size_t measure_count = 0;
    if (meter_map_measure_count(map, notes->meter.end_time, &measure_count) != METER_MAP_OK) {
        return NULL;
    }

    char label[48];
    snprintf(label, sizeof(label), "%u/%u", time_signature_top_number(time_signature),
             time_signature_bottom_number(time_signature));

    cJSON *meter = claim_json(inferred ? DEFAULT_METER_SOURCE : TIME_SIGNATURE_SOURCE, notes, NULL, notes->count,
                              label, (AnalysisConfidence){false, 0, 0});
    if (!(meter && add(meter, "source", cJSON_CreateString(inferred ? "inferred" : "file")) &&
          add(meter, "time_signature", time_signature_json(time_signature)) &&
          add(meter, "measure_count", cJSON_CreateNumber((double)measure_count)) &&
          add(meter, "last_measure",
              measure_count > 0 ? cJSON_CreateNumber((double)(meter_map_first_measure_number(map) +
                                                              (int)measure_count - 1))
                                : cJSON_CreateNull()) &&
          add(meter, "pickup", has_pickup ? pickup_json(map) : cJSON_CreateNull()) &&
          add(meter, "changes", meter_changes_json(map, primary_index + 1)))) {
        cJSON_Delete(meter);
        return NULL;
    }
    return meter;
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

/* The span's length in beats of the signature its start is read in: a rational, or null when the
   position cannot be read or the signature has no beat grid. Segments do not cross a barline, so one
   signature covers the whole span. */
static cJSON *span_beats_json(const MeterMap *map, Rational start, Rational end) {
    MusicalTime *musical_time = NULL;
    if (musical_time_create((long)start.num, (unsigned long)start.den, &musical_time) != MUSICAL_TIME_OK) {
        return cJSON_CreateNull();
    }
    MetricalPosition *position = NULL;
    MetricalPositionResult result = metrical_position_from_time(map, musical_time, &position);
    musical_time_destroy(musical_time);
    if (result != METRICAL_POSITION_OK) {
        return cJSON_CreateNull();
    }
    const TimeSignature *reading = NULL;
    Duration *beat = NULL;
    bool readable = metrical_position_reading_signature(map, metrical_position_measure(position), &reading) ==
                        METRICAL_POSITION_OK &&
                    standard_meter_beat_start(reading, 1, &beat);
    metrical_position_destroy(position);
    if (!readable) {
        return cJSON_CreateNull();
    }

    Rational length = rational_make(end.num * (long long)start.den - start.num * (long long)end.den,
                                    start.den * end.den);
    cJSON *beats = rational_json(rational_make(length.num * (long long)duration_denominator(beat),
                                               length.den * (unsigned long long)duration_numerator(beat)));
    duration_destroy(beat);
    return beats;
}

static cJSON *optional_string_json(const char *value) {
    return value ? cJSON_CreateString(value) : cJSON_CreateNull();
}

/* The runner-up candidate, or null when the library gave none. */
static cJSON *runner_up_json(const AnalysisChordReading *reading) {
    if (!reading->label) {
        return cJSON_CreateNull();
    }
    cJSON *object = cJSON_CreateObject();
    if (object && !(add(object, "label", cJSON_CreateString(reading->label)) &&
                    add(object, "root", cJSON_CreateString(reading->root)) &&
                    add(object, "quality", cJSON_CreateString(reading->quality)) &&
                    add(object, "score", cJSON_CreateNumber(reading->score)) &&
                    add(object, "uncertainty", uncertainty_json(reading->confidence)))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* The chord fields beyond the shared claim: root, quality, bass, inversion, duration, score and runner-up. */
static bool add_chord_fields(cJSON *claim, const ImportedNotes *notes, const AnalysisReportEntry *entry,
                             Rational start, Rational end) {
    const AnalysisChordReading *chord = &entry->chord;
    bool matched = chord->label != NULL;
    cJSON *duration = cJSON_CreateObject();
    if (duration && !(add(duration, "whole_notes",
                          rational_json(rational_make(end.num * (long long)start.den - start.num * (long long)end.den,
                                                      start.den * end.den))) &&
                      add(duration, "beats", span_beats_json(notes->meter.meter_map, start, end)))) {
        cJSON_Delete(duration);
        duration = NULL;
    }
    return add(claim, "root", optional_string_json(chord->root)) &&
           add(claim, "quality", optional_string_json(chord->quality)) &&
           add(claim, "bass", optional_string_json(chord->bass)) &&
           add(claim, "inversion", matched && chord->inversion >= 0 ? cJSON_CreateNumber(chord->inversion)
                                                                    : cJSON_CreateNull()) &&
           add(claim, "duration", duration) &&
           add(claim, "score", matched ? cJSON_CreateNumber(chord->score) : cJSON_CreateNull()) &&
           add(claim, "runner_up", runner_up_json(&entry->runner_up));
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
        Rational start = rational_make(entry->time_numerator, entry->time_denominator);
        Rational end = rational_make(entry->end_numerator, entry->end_denominator);
        cJSON *claim = claim_json_at(framework, notes, entry->note_ids, entry->note_id_count, start, end,
                                     entry->chord_label, entry->chord_confidence);
        if (!(claim && add(claim, "key_label", cJSON_CreateString(entry->key_label)) &&
              add_chord_fields(claim, notes, entry, start, end))) {
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
          add(document, "meter", meter_json(notes)) && add(document, "keys", keys_json(notes, report)) &&
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

AnalysisDocument *analysis_document_build(const char *input_path, const ImportedNotes *notes,
                                          const AnalysisReport *report) {
    AnalysisDocument *document = malloc(sizeof(AnalysisDocument));
    if (!document) {
        return NULL;
    }
    document->root = document_json(input_path, notes, report);
    if (!document->root) {
        free(document);
        return NULL;
    }
    return document;
}

char *analysis_document_to_json(const AnalysisDocument *document) {
    Buffer buffer = {NULL, 0, 0};
    if (!print_value(&buffer, document->root, 0) || !buffer_append(&buffer, "\n")) {
        free(buffer.data);
        return NULL;
    }
    return buffer.data;
}

void analysis_document_destroy(AnalysisDocument *document) {
    if (!document) {
        return;
    }
    cJSON_Delete(document->root);
    free(document);
}

char *analysis_document_render(const char *input_path, const ImportedNotes *notes, const AnalysisReport *report) {
    AnalysisDocument *document = analysis_document_build(input_path, notes, report);
    if (!document) {
        return NULL;
    }
    char *json = analysis_document_to_json(document);
    analysis_document_destroy(document);
    return json;
}

char *derive_analysis_document_path(const char *input_path) {
    return path_with_suffix(input_path, ".analysis.json");
}
