#include <ursatz_analyzer/analysis_document.h>
#include <ursatz_analyzer/version.h>

#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "common-practice-minimal/framework.h"
#include "core/rational.h"
#include "pipeline/analysis_document_internal.h"
#include "pipeline/analysis_result_internal.h"
#include "pipeline/path_util.h"
#include "pipeline/pitch_class_util.h"
#include "standard-tonality/tonal_key.h"
#include "pipeline/progression_pipeline.h"
#include "pipeline/voicing_check.h"
#include "standard-form/framework.h"
#include "standard-harmony/harmony_framework.h"
#include "standard-meter/beat_grid.h"
#include "standard-meter/metric_position_rules.h"
#include "theory/framework.h"
#include "time/duration.h"
#include "time/meter_map.h"
#include "time/metrical_position.h"
#include "time/musical_time.h"
#include "time/temporal_anchor.h"
#include "time/time_rational.h"
#include "time/time_signature.h"
#include "util/json_util.h"
#include "util/misc_util.h"

/* How the home key is chosen: the library offers no determination of its own, so it is the key with the
   greatest total duration over all its regions, a tie going to the key that appears first. */
static const char HOME_KEY_RULE[] = "longest_total_duration";

/* Source ids for the meter: a time signature the MIDI file declares or the score notates, and
   the 4/4 the library assumes when it declares none. */
static const char TIME_SIGNATURE_SOURCE[] = "midi-time-signature";
static const char NOTATED_TIME_SIGNATURE_SOURCE[] = "musicxml-time-signature";
static const char DEFAULT_METER_SOURCE[] = "ursatz-default-meter";
/* The meter a notation reading sets, replacing the input's. */
static const char READING_METER_SOURCE[] = "notation-reading-meter";

/* ---- Rational time (whole notes) ---- */

/* The state of one analysis_document_build. overflowed is set when a time value does not fit a Rational; the build then
   fails instead of printing a wrapped time. */
typedef struct {
    bool overflowed;
} DocBuild;

static Rational doc_rational(DocBuild *build, long long num, unsigned long long den) {
    Rational value = {0, 1};
    if (den > (unsigned long long)LLONG_MAX || rational_make(num, (long long)den, &value) != RATIONAL_OK) {
        build->overflowed = true;
        return (Rational){0, 1};
    }
    return value;
}

static Rational doc_add(DocBuild *build, Rational a, Rational b) {
    Rational sum = {0, 1};
    if (rational_add(a, b, &sum) != RATIONAL_OK) {
        build->overflowed = true;
    }
    return sum;
}

static Rational doc_sub(DocBuild *build, Rational a, Rational b) {
    Rational difference = {0, 1};
    if (rational_sub(a, b, &difference) != RATIONAL_OK) {
        build->overflowed = true;
    }
    return difference;
}

/* length measured in beats of the given duration. */
static Rational doc_in_beats(DocBuild *build, Rational length, const Duration *beat) {
    Rational beats = {0, 1};
    if (rational_div(length, doc_rational(build, duration_numerator(beat), duration_denominator(beat)), &beats) !=
        RATIONAL_OK) {
        build->overflowed = true;
    }
    return beats;
}

static bool time_before(Rational a, Rational b) { return rational_cmp(a, b) < 0; }

static Rational time_rational(DocBuild *build, const MusicalTime *time) {
    Rational value = {0, 1};
    if (rational_of_musical_time(time, &value) != RATIONAL_OK) {
        build->overflowed = true;
    }
    return value;
}

static Rational note_onset(DocBuild *build, const NoteEvent *event) {
    return time_rational(build, temporal_anchor_position(note_event_temporal_anchor(event)));
}

static Rational note_end(DocBuild *build, const NoteEvent *event) {
    Rational length = {0, 1};
    if (rational_of_duration(note_event_duration(event), &length) != RATIONAL_OK) {
        build->overflowed = true;
    }
    return doc_add(build, note_onset(build, event), length);
}

/* Earliest onset to latest note end over the given notes (ids NULL means
   every imported note). */
static void notes_span(DocBuild *build, const ImportedNotes *notes, const size_t *ids, size_t id_count, Rational *start, Rational *end) {
    *start = (Rational){0, 1};
    *end = (Rational){0, 1};
    for (size_t i = 0; i < id_count; i++) {
        const NoteEvent *event = notes->events[ids ? ids[i] : i];
        Rational onset = note_onset(build, event);
        Rational finish = note_end(build, event);
        if (i == 0 || time_before(onset, *start)) {
            *start = onset;
        }
        if (i == 0 || time_before(*end, finish)) {
            *end = finish;
        }
    }
}

/* ---- Document construction ---- */

/* Adds item under key, taking ownership; false (item freed) on failure. */
static cJSON *rational_json(Rational value) {
    cJSON *object = cJSON_CreateObject();
    if (object && !(json_add(object, "num", cJSON_CreateNumber((double)value.num)) &&
                    json_add(object, "den", cJSON_CreateNumber((double)value.den)))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

static cJSON *span_json(Rational start, Rational end) {
    cJSON *object = cJSON_CreateObject();
    if (object && !(json_add(object, "start", rational_json(start)) && json_add(object, "end", rational_json(end)))) {
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
        if (!json_append(array, cJSON_CreateNumber((double)(ids ? ids[i] : i)))) {
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
static cJSON *position_json(DocBuild *build, const MeterMap *map, Rational time) {
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
    if (object && !(json_add(object, "measure", cJSON_CreateNumber((double)measure)) &&
                    json_add(object, "beat", cJSON_CreateNumber((double)beat.beat)) &&
                    json_add(object, "subdivision",
                             rational_json(doc_rational(build, beat.subdivision_numerator, beat.subdivision_denominator))))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* Where a span starts and ends on the measure grid. The end is the
   position of the span's (exclusive) end time, so a span that ends on a
   barline ends at beat 1 of the next measure. */
static cJSON *location_json(DocBuild *build, const MeterMap *map, Rational start, Rational end) {
    cJSON *object = cJSON_CreateObject();
    if (object &&
        !(json_add(object, "start", position_json(build, map, start)) && json_add(object, "end", position_json(build, map, end)))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* The shared claim fields, in schema order, with the uncertainty as built by the caller (taken over, freed on
   failure). ids NULL means every imported note. */
static cJSON *claim_json_with(DocBuild *build, const char *framework, const ImportedNotes *notes, const size_t *ids, size_t id_count,
                              Rational start, Rational end, const char *label, cJSON *uncertainty) {
    cJSON *claim = cJSON_CreateObject();
    bool consumed = false; /* json_add() takes the uncertainty over, and frees it itself when it fails */
    if (claim && !(json_add(claim, "framework", cJSON_CreateString(framework)) &&
                   json_add(claim, "span", span_json(start, end)) &&
                   json_add(claim, "location", location_json(build, notes->meter.meter_map, start, end)) &&
                   json_add(claim, "note_ids", note_ids_json(ids, id_count)) &&
                   json_add(claim, "label", cJSON_CreateString(label)) &&
                   (consumed = true, json_add(claim, "uncertainty", uncertainty)))) {
        if (!consumed) {
            cJSON_Delete(uncertainty);
        }
        cJSON_Delete(claim);
        return NULL;
    }
    if (!claim) {
        cJSON_Delete(uncertainty);
    }
    return claim;
}

static cJSON *claim_json_at(DocBuild *build, const char *framework, const ImportedNotes *notes, const size_t *ids, size_t id_count,
                            Rational start, Rational end, const char *label, AnalysisConfidence confidence) {
    return claim_json_with(build, framework, notes, ids, id_count, start, end, label, uncertainty_json(confidence));
}

static cJSON *claim_json(DocBuild *build, const char *framework, const ImportedNotes *notes, const size_t *ids, size_t id_count,
                         const char *label, AnalysisConfidence confidence) {
    Rational start;
    Rational end;
    notes_span(build, notes, ids, id_count, &start, &end);
    return claim_json_at(build, framework, notes, ids, id_count, start, end, label, confidence);
}

static cJSON *time_signature_json(const TimeSignature *time_signature) {
    cJSON *object = cJSON_CreateObject();
    if (object &&
        !(json_add(object, "numerator", cJSON_CreateNumber((double)time_signature_top_number(time_signature))) &&
          json_add(object, "denominator", cJSON_CreateNumber((double)time_signature_bottom_number(time_signature))))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* The pickup region's length in whole notes and in beats of the
   signature its position is read in (null beats when that signature has
   no beat grid). */
static cJSON *pickup_json(DocBuild *build, const MeterMap *map) {
    const MeterMapRegion *region = meter_map_region(map, 0);
    const MusicalTime *end = meter_map_region_end(region);
    const TimeSignature *reading = NULL;
    if (!end || metrical_position_reading_signature(map, 0, &reading) != METRICAL_POSITION_OK) {
        return NULL;
    }
    Rational length = time_rational(build, end);

    cJSON *beats = NULL;
    Duration *beat = NULL;
    if (standard_meter_beat_start(reading, 1, &beat)) {
        beats = rational_json(doc_in_beats(build, length, beat));
        duration_destroy(beat);
    } else {
        beats = cJSON_CreateNull();
    }

    cJSON *pickup = cJSON_CreateObject();
    if (pickup && !(json_add(pickup, "length", rational_json(length)) && json_add(pickup, "beats", beats))) {
        cJSON_Delete(pickup);
        return NULL;
    }
    if (!pickup) {
        cJSON_Delete(beats);
    }
    return pickup;
}

/* Every region after the first full measure's, one entry per meter change. */
static cJSON *meter_changes_json(DocBuild *build, const MeterMap *map, size_t first_index) {
    cJSON *changes = cJSON_CreateArray();
    if (!changes) {
        return NULL;
    }
    for (size_t i = first_index; i < meter_map_region_count(map); i++) {
        const MeterMapRegion *region = meter_map_region(map, i);
        cJSON *change = cJSON_CreateObject();
        if (!(change &&
              json_add(change, "measure", cJSON_CreateNumber((double)meter_map_region_first_measure_number(region))) &&
              json_add(change, "start", rational_json(time_rational(build, meter_map_region_start(region)))) &&
              json_add(change, "time_signature", time_signature_json(meter_map_region_time_signature(region))) &&
              json_add(change, "inferred", cJSON_CreateBool(meter_map_region_is_defaulted(region))))) {
            cJSON_Delete(change);
            cJSON_Delete(changes);
            return NULL;
        }
        if (!json_append(changes, change)) {
            cJSON_Delete(changes);
            return NULL;
        }
    }
    return changes;
}

/* The notation reading applied to the notes: {"scale", "meter_overridden"}, null when none was. */
static cJSON *reading_json(DocBuild *build, const ImportedNotes *notes) {
    if (!notes->source.reading_applied) {
        return cJSON_CreateNull();
    }
    const NotationReading *reading = &notes->source.reading;
    cJSON *object = cJSON_CreateObject();
    if (object && !(json_add(object, "scale",
                             rational_json(doc_rational(build, reading->scale_numerator, reading->scale_denominator))) &&
                    json_add(object, "meter_overridden", cJSON_CreateBool(reading->has_meter)))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* The piece's meter claim. The time signature is that of the first full
   measure; a pickup before it is reported separately, and later signature
   changes under "changes". A file without a time signature gets the
   library's 4/4 default, reported with source "inferred"; a meter set by a
   notation reading is "override". */
static cJSON *meter_json(DocBuild *build, const ImportedNotes *notes) {
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

    bool overridden = notes->source.reading_applied && notes->source.reading.has_meter;
    bool notated = !inferred && !overridden && notes->source.meter_notated;
    const char *claim_source = overridden  ? READING_METER_SOURCE
                               : inferred  ? DEFAULT_METER_SOURCE
                               : (notated ? NOTATED_TIME_SIGNATURE_SOURCE : TIME_SIGNATURE_SOURCE);
    const char *source_name = overridden ? "override" : (inferred ? "inferred" : (notated ? "notated" : "file"));
    cJSON *meter = claim_json(build, claim_source, notes, NULL, notes->count, label, (AnalysisConfidence){false, 0, 0});
    if (!(meter && json_add(meter, "source", cJSON_CreateString(source_name)) &&
          json_add(meter, "time_signature", time_signature_json(time_signature)) &&
          json_add(meter, "measure_count", cJSON_CreateNumber((double)measure_count)) &&
          json_add(meter, "last_measure",
              measure_count > 0 ? cJSON_CreateNumber((double)(meter_map_first_measure_number(map) +
                                                              (int)measure_count - 1))
                                : cJSON_CreateNull()) &&
          json_add(meter, "pickup", has_pickup ? pickup_json(build, map) : cJSON_CreateNull()) &&
          json_add(meter, "changes", meter_changes_json(build, map, primary_index + 1)) &&
          json_add(meter, "reading", reading_json(build, notes)))) {
        cJSON_Delete(meter);
        return NULL;
    }
    return meter;
}

static const char *library_framework_name(void) {
    const Framework *framework = common_practice_minimal_framework();
    return framework ? framework_name(framework) : "unknown";
}

static const char *mode_hint_name(MidiModeHint hint) {
    switch (hint) {
        case MIDI_MODE_HINT_MAJOR:
            return "major";
        case MIDI_MODE_HINT_MINOR:
            return "minor";
        default:
            return "unknown";
    }
}

/* The notated key signatures, as the file declares them: where each sits, its sharps/flats count, the raw
   major/minor hint (unreliable, never used to decide a key), and the major key and relative minor it allows. */
static cJSON *key_signatures_json(DocBuild *build, const ImportedNotes *notes) {
    cJSON *signatures = cJSON_CreateArray();
    if (!signatures) {
        return NULL;
    }
    for (size_t i = 0; i < notes->key_signature_events.count; i++) {
        const MidiKeySignatureEvent *event = &notes->key_signature_events.events[i];
        int major = key_signature_major_tonic(event->sharps_flats);
        char *major_label = key_label_create(major, true);
        char *minor_label = key_label_create((major + 9) % 12, false);
        Rational start = time_rational(build, event->position);
        cJSON *object = cJSON_CreateObject();
        bool ok = major_label && minor_label && object &&
                  json_add(object, "start", rational_json(start)) &&
                  json_add(object, "location", position_json(build, notes->meter.meter_map, start)) &&
                  json_add(object, "sharps_flats", cJSON_CreateNumber(event->sharps_flats)) &&
                  json_add(object, "mode_hint", cJSON_CreateString(mode_hint_name(event->mode_hint))) &&
                  json_add(object, "major_key", cJSON_CreateString(major_label)) &&
                  json_add(object, "minor_key", cJSON_CreateString(minor_label));
        free(major_label);
        free(minor_label);
        if (!ok || !json_append(signatures, object)) {
            if (!ok) {
                cJSON_Delete(object);
            }
            cJSON_Delete(signatures);
            return NULL;
        }
    }
    return signatures;
}

/* How the input knows its meter or key: written in the score, an event embedded in the file, or neither (the
   library's 4/4, or a key found from the notes alone). */
static const char *knowledge_name(bool present, bool notated) {
    return !present ? "inferred" : (notated ? "notated" : "embedded");
}

static cJSON *input_parts_json(const ImportedNotes *notes) {
    cJSON *parts = cJSON_CreateArray();
    if (!parts) {
        return NULL;
    }
    for (size_t i = 0; i < notes->source.part_count; i++) {
        size_t note_count = 0;
        for (size_t n = 0; n < notes->count; n++) {
            note_count += notes->source.part_index[n] == i;
        }
        const ImportedPart *part = &notes->source.parts[i];
        cJSON *object = cJSON_CreateObject();
        if (!(object && json_add(object, "id", cJSON_CreateString(part->id)) &&
              json_add(object, "name", cJSON_CreateString(part->name)) &&
              json_add(object, "staff_count", cJSON_CreateNumber(part->staff_count)) &&
              json_add(object, "note_count", cJSON_CreateNumber((double)note_count)) && json_append(parts, object))) {
            cJSON_Delete(object);
            cJSON_Delete(parts);
            return NULL;
        }
    }
    return parts;
}

static cJSON *input_tempo_json(const ImportedSource *source) {
    cJSON *tempo = cJSON_CreateObject();
    if (tempo && !(json_add(tempo, "declared", cJSON_CreateBool(source->tempo_declared)) &&
                   json_add(tempo, "quarters_per_minute", source->tempo_declared
                                                         ? cJSON_CreateNumber(source->initial_quarters_per_minute)
                                                         : cJSON_CreateNull()) &&
                   json_add(tempo, "marking_count", cJSON_CreateNumber((double)source->tempo_marking_count)))) {
        cJSON_Delete(tempo);
        return NULL;
    }
    return tempo;
}

/* Where the input came from and what it says about itself: the format, whether it is a score or a performance,
   and for each of meter, key and pitch spelling whether the input states it or the pipeline supplied it. The
   score's parts are merged into one texture; the tempo and dynamics markings are carried, not analyzed. */
static cJSON *input_json(const ImportedNotes *notes) {
    const ImportedSource *source = &notes->source;
    bool score = source->format == INPUT_FORMAT_MUSICXML;
    bool meter_stated = !meter_map_is_defaulted(notes->meter.meter_map);
    cJSON *input = cJSON_CreateObject();
    if (input &&
        !(json_add(input, "format", cJSON_CreateString(score ? "musicxml" : "midi")) &&
          json_add(input, "source_type", cJSON_CreateString(score ? "score" : "performance")) &&
          json_add(input, "meter", cJSON_CreateString(knowledge_name(meter_stated, source->meter_notated))) &&
          json_add(input, "key",
              cJSON_CreateString(knowledge_name(notes->key_signature_events.count > 0, source->key_notated))) &&
          json_add(input, "spelling", cJSON_CreateString(source->spelling_notated ? "notated" : "rule")) &&
          json_add(input, "parts", input_parts_json(notes)) && json_add(input, "tempo", input_tempo_json(source)) &&
          json_add(input, "dynamics_count", cJSON_CreateNumber((double)source->dynamics_count)) &&
          json_add(input, "warning_count", cJSON_CreateNumber((double)source->warning_count)))) {
        cJSON_Delete(input);
        return NULL;
    }
    return input;
}

static cJSON *metadata_json(DocBuild *build, const char *input_path, const ImportedNotes *notes, const AnalysisReport *report) {
    cJSON *metadata = cJSON_CreateObject();
    if (metadata &&
        !(json_add(metadata, "source_file", cJSON_CreateString(path_basename(input_path))) &&
          json_add(metadata, "analyzer_version", cJSON_CreateString(URSATZ_ANALYZER_VERSION)) &&
          json_add(metadata, "ursatz_commit", cJSON_CreateString(URSATZ_ANALYZER_URSATZ_COMMIT)) &&
          json_add(metadata, "note_count", cJSON_CreateNumber((double)notes->count)) &&
          json_add(metadata, "ticks_per_quarter", notes->ticks_per_quarter > 0
                                                 ? cJSON_CreateNumber((double)notes->ticks_per_quarter)
                                                 : cJSON_CreateNull()) &&
          json_add(metadata, "input", input_json(notes)) &&
          json_add(metadata, "key_found", cJSON_CreateBool(report->key_region_count > 0)) &&
          json_add(metadata, "key_signatures", key_signatures_json(build, notes)))) {
        cJSON_Delete(metadata);
        return NULL;
    }
    return metadata;
}

static Rational region_start(DocBuild *build, const AnalysisKeyRegion *region) {
    return doc_rational(build, region->start_numerator, region->start_denominator);
}

static Rational region_end(DocBuild *build, const AnalysisKeyRegion *region) {
    return doc_rational(build, region->end_numerator, region->end_denominator);
}

/* The imported notes that start in [start, end), as indices; *count may be 0, and the result is then still a
   valid pointer. NULL on allocation failure. */
static size_t *notes_starting_in(DocBuild *build, const ImportedNotes *notes, Rational start, Rational end, size_t *count) {
    size_t *ids = malloc(sizeof(size_t) * (notes->count > 0 ? notes->count : 1));
    if (!ids) {
        return NULL;
    }
    *count = 0;
    for (size_t i = 0; i < notes->count; i++) {
        Rational onset = note_onset(build, notes->events[i]);
        if (!time_before(onset, start) && time_before(onset, end)) {
            ids[(*count)++] = i;
        }
    }
    return ids;
}

/* A claim over one key region's span: the notes that start in it, its label, and the library's uncertainty
   for the region (in [0, 1], higher is LESS certain, unlike a chord's confidence). */
static cJSON *region_span_claim(DocBuild *build, const ImportedNotes *notes, const AnalysisKeyRegion *region, const char *label,
                                cJSON *uncertainty) {
    size_t id_count = 0;
    size_t *ids = notes_starting_in(build, notes, region_start(build, region), region_end(build, region), &id_count);
    if (!ids) {
        cJSON_Delete(uncertainty);
        return NULL;
    }
    cJSON *claim = claim_json_with(build, library_framework_name(), notes, ids, id_count, region_start(build, region),
                                   region_end(build, region), label, uncertainty);
    free(ids);
    return claim;
}

static const char *region_source_name(AnalysisRegionSource source) {
    return source == ANALYSIS_REGION_SOURCE_KEY_SIGNATURE ? "key_signature" : "profile";
}

static cJSON *measures_json(int first, int last) {
    cJSON *object = cJSON_CreateObject();
    if (object && !(json_add(object, "first", cJSON_CreateNumber(first)) && json_add(object, "last", cJSON_CreateNumber(last)))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* One key region: the shared claim (its span runs barline to barline) and the key's fields. */
static cJSON *key_region_json(DocBuild *build, const ImportedNotes *notes, const AnalysisKeyRegion *region, size_t index) {
    cJSON *claim = region_span_claim(build, notes, region, region->label, cJSON_CreateNumber(region->uncertainty));
    if (!(claim && json_add(claim, "index", cJSON_CreateNumber((double)index)) &&
          json_add(claim, "tonic", cJSON_CreateString(pitch_class_name(region->tonic_pitch_class, region->prefer_flats))) &&
          json_add(claim, "mode", cJSON_CreateString(region->is_major ? "major" : "minor")) &&
          json_add(claim, "measures", measures_json(region->first_measure, region->last_measure)) &&
          json_add(claim, "source", cJSON_CreateString(region_source_name(region->source))))) {
        cJSON_Delete(claim);
        return NULL;
    }
    return claim;
}

/* The indices of every region in the home key, in time order. */
static cJSON *home_region_indices_json(const AnalysisReport *report) {
    const AnalysisKeyRegion *home = &report->key_regions[report->home_region];
    cJSON *indices = cJSON_CreateArray();
    for (size_t i = 0; indices && i < report->key_region_count; i++) {
        const AnalysisKeyRegion *region = &report->key_regions[i];
        if (region->tonic_pitch_class == home->tonic_pitch_class && region->is_major == home->is_major &&
            !json_append(indices, cJSON_CreateNumber((double)i))) {
            cJSON_Delete(indices);
            return NULL;
        }
    }
    return indices;
}

static cJSON *home_key_json(DocBuild *build, const AnalysisReport *report) {
    const AnalysisKeyRegion *home = &report->key_regions[report->home_region];
    cJSON *object = cJSON_CreateObject();
    if (object &&
        !(json_add(object, "rule", cJSON_CreateString(HOME_KEY_RULE)) &&
          json_add(object, "label", cJSON_CreateString(home->label)) &&
          json_add(object, "region_indices", home_region_indices_json(report)) &&
          json_add(object, "duration",
                   rational_json(doc_rational(build, report->home_duration_numerator, report->home_duration_denominator))))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* The keys section: the key regions in time order, the home key derived from them, and the indices of the
   opening and closing regions. With no key found the regions are empty and the rest null. */
static cJSON *keys_json(DocBuild *build, const ImportedNotes *notes, const AnalysisReport *report) {
    cJSON *keys = cJSON_CreateObject();
    cJSON *regions = cJSON_CreateArray();
    if (!keys || !regions) {
        cJSON_Delete(keys);
        cJSON_Delete(regions);
        return NULL;
    }
    for (size_t i = 0; i < report->key_region_count; i++) {
        if (!json_append(regions, key_region_json(build, notes, &report->key_regions[i], i))) {
            cJSON_Delete(keys);
            cJSON_Delete(regions);
            return NULL;
        }
    }
    bool found = report->key_region_count > 0;
    if (!(json_add(keys, "regions", regions) && json_add(keys, "home_key", found ? home_key_json(build, report) : cJSON_CreateNull()) &&
          json_add(keys, "opening_region", found ? cJSON_CreateNumber(0) : cJSON_CreateNull()) &&
          json_add(keys, "closing_region",
              found ? cJSON_CreateNumber((double)(report->key_region_count - 1)) : cJSON_CreateNull()))) {
        cJSON_Delete(keys);
        return NULL;
    }
    return keys;
}

/* ---- Key changes: tonicizations, modulations and pivot chords ---- */

static const char *extent_name(AnalysisKeyChangeExtent extent) {
    switch (extent) {
        case ANALYSIS_KEY_CHANGE_EXTENDED_TONICIZATION:
            return "extended-tonicization";
        case ANALYSIS_KEY_CHANGE_WEAK_MODULATION:
            return "weak-modulation";
        case ANALYSIS_KEY_CHANGE_MODULATION:
            return "modulation";
        default:
            return "tonicization";
    }
}

static bool change_is_tonicization(const AnalysisKeyChange *change) {
    return change->extent == ANALYSIS_KEY_CHANGE_TONICIZATION ||
           change->extent == ANALYSIS_KEY_CHANGE_EXTENDED_TONICIZATION;
}

/* A change the library leaves as a modulation or weak modulation: a lasting new key. */
static bool change_is_modulation(const AnalysisKeyChange *change) {
    return !change_is_tonicization(change);
}

/* "<from> to <to>" for the labels of two regions. */
static char *join_labels(const char *prefix, const char *middle, const char *label) {
    size_t size = strlen(prefix) + strlen(middle) + strlen(label) + 1;
    char *text = malloc(size);
    if (text) {
        snprintf(text, size, "%s%s%s", prefix, middle, label);
    }
    return text;
}

/* Fields every key change shares: its extent, the two regions and their keys. */
static bool add_change_fields(cJSON *claim, const AnalysisReport *report, const AnalysisKeyChange *change,
                              const char *to_name) {
    const AnalysisKeyRegion *from = &report->key_regions[change->from_region];
    const AnalysisKeyRegion *to = &report->key_regions[change->to_region];
    return json_add(claim, "extent", cJSON_CreateString(extent_name(change->extent))) &&
           json_add(claim, "cadence_confirmed", cJSON_CreateBool(change->cadence_confirmed)) &&
           json_add(claim, "from_region", cJSON_CreateNumber((double)change->from_region)) &&
           json_add(claim, to_name, cJSON_CreateNumber((double)change->to_region)) &&
           json_add(claim, "from_key", cJSON_CreateString(from->label)) &&
           json_add(claim, "to_key", cJSON_CreateString(to->label));
}

/* One claim per tonicization: a brief excursion to another key that returns to the one before it. Its span is
   the excursion (the region the library puts the new key in), its target the key of that region. The change
   back out of the excursion is not listed again. */
static cJSON *tonicizations_json(DocBuild *build, const ImportedNotes *notes, const AnalysisReport *report) {
    cJSON *list = cJSON_CreateArray();
    if (!list) {
        return NULL;
    }
    for (size_t i = 0; i < report->key_change_count; i++) {
        const AnalysisKeyChange *change = &report->key_changes[i];
        if (!change_is_tonicization(change) || change->is_return) {
            continue;
        }
        const AnalysisKeyRegion *target = &report->key_regions[change->to_region];
        char *label = join_labels("tonicization of ", "", target->label);
        cJSON *claim = label ? region_span_claim(build, notes, target, label, cJSON_CreateNull()) : NULL;
        free(label);
        if (!(claim && add_change_fields(claim, report, change, "target_region"))) {
            cJSON_Delete(claim);
            cJSON_Delete(list);
            return NULL;
        }
        if (!json_append(list, claim)) {
            cJSON_Delete(list);
            return NULL;
        }
    }
    return list;
}

/* Whether a change's pivot chord was identified, so it appears in pivot_chords. */
static bool change_has_pivot_reading(const AnalysisKeyChange *change) {
    return change->has_pivot && change->pivot.label != NULL;
}

/* One claim per modulation (and weak modulation): a lasting new key. Its span is the passage in the new key,
   so location.start is where the new key arrives. type is "pivot" when the library found a pivot chord and
   "direct" when it did not; pivot_chord_index points into pivot_chords. is_return marks the change back to the
   key before an excursion. */
static cJSON *modulations_json(DocBuild *build, const ImportedNotes *notes, const AnalysisReport *report) {
    cJSON *list = cJSON_CreateArray();
    if (!list) {
        return NULL;
    }
    size_t pivot_index = 0;
    for (size_t i = 0; i < report->key_change_count; i++) {
        const AnalysisKeyChange *change = &report->key_changes[i];
        if (!change_is_modulation(change)) {
            continue;
        }
        const AnalysisKeyRegion *from = &report->key_regions[change->from_region];
        const AnalysisKeyRegion *to = &report->key_regions[change->to_region];
        char *label = join_labels(from->label, " to ", to->label);
        cJSON *claim = label ? region_span_claim(build, notes, to, label, cJSON_CreateNull()) : NULL;
        free(label);
        bool has_reading = change_has_pivot_reading(change);
        if (!(claim && add_change_fields(claim, report, change, "to_region") &&
              json_add(claim, "type", cJSON_CreateString(change->has_pivot ? "pivot" : "direct")) &&
              json_add(claim, "is_return", cJSON_CreateBool(change->is_return)) &&
              json_add(claim, "pivot_chord_index",
                  has_reading ? cJSON_CreateNumber((double)pivot_index) : cJSON_CreateNull()))) {
            cJSON_Delete(claim);
            cJSON_Delete(list);
            return NULL;
        }
        pivot_index += has_reading ? 1 : 0;
        if (!json_append(list, claim)) {
            cJSON_Delete(list);
            return NULL;
        }
    }
    return list;
}

/* One claim per pivot chord: the chord that is diatonic in both the old and the new key at a modulation,
   identified without a Roman numeral (root, quality, bass, inversion), read in the old key. The claim is the
   chord's segment. */
static cJSON *pivot_chords_json(DocBuild *build, const ImportedNotes *notes, const AnalysisReport *report) {
    cJSON *list = cJSON_CreateArray();
    if (!list) {
        return NULL;
    }
    size_t modulation_index = 0;
    for (size_t i = 0; i < report->key_change_count; i++) {
        const AnalysisKeyChange *change = &report->key_changes[i];
        if (!change_is_modulation(change)) {
            continue;
        }
        size_t this_modulation = modulation_index++;
        if (!change_has_pivot_reading(change) || change->pivot_entry >= report->entry_count) {
            continue;
        }
        const AnalysisReportEntry *entry = &report->entries[change->pivot_entry];
        const AnalysisChordReading *pivot = &change->pivot;
        char *label = join_labels(pivot->root, " ", pivot->quality);
        cJSON *claim =
            label ? claim_json_at(build, library_framework_name(), notes, entry->note_ids, entry->note_id_count,
                                  doc_rational(build, entry->time_numerator, entry->time_denominator),
                                  doc_rational(build, entry->end_numerator, entry->end_denominator), label, pivot->confidence)
                  : NULL;
        free(label);
        if (!(claim && json_add(claim, "modulation_index", cJSON_CreateNumber((double)this_modulation)) &&
              json_add(claim, "chord_index", cJSON_CreateNumber((double)change->pivot_entry)) &&
              json_add(claim, "root", cJSON_CreateString(pivot->root)) &&
              json_add(claim, "quality", cJSON_CreateString(pivot->quality)) &&
              json_add(claim, "bass", cJSON_CreateString(pivot->bass)) &&
              json_add(claim, "inversion",
                  pivot->inversion >= 0 ? cJSON_CreateNumber(pivot->inversion) : cJSON_CreateNull()) &&
              json_add(claim, "from_key", cJSON_CreateString(report->key_regions[change->from_region].label)) &&
              json_add(claim, "to_key", cJSON_CreateString(report->key_regions[change->to_region].label)))) {
            cJSON_Delete(claim);
            cJSON_Delete(list);
            return NULL;
        }
        if (!json_append(list, claim)) {
            cJSON_Delete(list);
            return NULL;
        }
    }
    return list;
}

/* Whether a key is the major key of a notated signature or its relative minor. */
static bool key_fits_signature(const AnalysisKeyRegion *region, int sharps_flats) {
    int major = key_signature_major_tonic(sharps_flats);
    return region->is_major ? region->tonic_pitch_class == major : region->tonic_pitch_class == (major + 9) % 12;
}

/* The region in effect at time: the one whose span holds it, the first for a time before the piece's first
   region; NULL for a time at or after the end of the last. */
static const AnalysisKeyRegion *region_at(DocBuild *build, const AnalysisReport *report, Rational time, size_t *index) {
    for (size_t i = 0; i < report->key_region_count; i++) {
        if (time_before(time, region_end(build, &report->key_regions[i]))) {
            *index = i;
            return &report->key_regions[i];
        }
    }
    return NULL;
}

/* One disagreement between a notated signature and the detected keys. signature_index is -1 for a note about
   the home key, which fits no signature at all. */
static cJSON *disagreement_json(const char *scope, long signature_index, const MidiKeySignatureEvent *event,
                                size_t region_index, const AnalysisKeyRegion *region) {
    char notated[96] = "";
    char message[256];
    if (event) {
        int major = key_signature_major_tonic(event->sharps_flats);
        char *major_label = key_label_create(major, true);
        char *minor_label = key_label_create((major + 9) % 12, false);
        if (major_label && minor_label) {
            snprintf(notated, sizeof(notated), "%s or %s", major_label, minor_label);
        }
        free(major_label);
        free(minor_label);
        snprintf(message, sizeof(message),
                 "The notated key signature allows %s, but the key detected there is %s. Both are reported; "
                 "neither is preferred.",
                 notated, region->label);
    } else {
        snprintf(message, sizeof(message),
                 "The home key %s is not the major key or relative minor of any notated key signature. Both are "
                 "reported; neither is preferred.",
                 region->label);
    }
    cJSON *note = cJSON_CreateObject();
    if (note && !(json_add(note, "kind", cJSON_CreateString("key_signature_disagreement")) &&
                  json_add(note, "scope", cJSON_CreateString(scope)) &&
                  json_add(note, "signature_index",
                      signature_index >= 0 ? cJSON_CreateNumber((double)signature_index) : cJSON_CreateNull()) &&
                  json_add(note, "region_index", cJSON_CreateNumber((double)region_index)) &&
                  json_add(note, "notated", event ? cJSON_CreateString(notated) : cJSON_CreateNull()) &&
                  json_add(note, "detected", cJSON_CreateString(region->label)) &&
                  json_add(note, "message", cJSON_CreateString(message)))) {
        cJSON_Delete(note);
        return NULL;
    }
    return note;
}

/* Notes on the analysis. A notated key signature that the detected keys do not fit is surfaced here, never
   resolved silently: where a signature starts, the region in effect there should be its major key or relative
   minor (the library restricts it to those, so a mismatch means the restriction did not hold); and the home
   key should be the major key or relative minor of at least one signature. Inside a signature any key may
   come and go, so the other regions are not compared. */
static cJSON *notes_json(DocBuild *build, const ImportedNotes *notes, const AnalysisReport *report) {
    cJSON *list = cJSON_CreateArray();
    if (!list) {
        return NULL;
    }
    size_t signature_count = notes->key_signature_events.count;
    if (report->key_region_count == 0 || signature_count == 0) {
        return list;
    }
    bool home_fits_any = false;
    const AnalysisKeyRegion *home = &report->key_regions[report->home_region];
    for (size_t i = 0; i < signature_count; i++) {
        const MidiKeySignatureEvent *event = &notes->key_signature_events.events[i];
        home_fits_any = home_fits_any || key_fits_signature(home, event->sharps_flats);
        if (i > 0 && event->sharps_flats == notes->key_signature_events.events[i - 1].sharps_flats) {
            continue; /* the same signature repeated is not a new one (the library does not treat it as a boundary) */
        }
        size_t region_index = 0;
        const AnalysisKeyRegion *region = region_at(build, report, time_rational(build, event->position), &region_index);
        if (region && !key_fits_signature(region, event->sharps_flats) &&
            !json_append(list, disagreement_json("region_at_signature", (long)i, event, region_index, region))) {
            cJSON_Delete(list);
            return NULL;
        }
    }
    if (!home_fits_any && !json_append(list, disagreement_json("home_key", -1, NULL, report->home_region, home))) {
        cJSON_Delete(list);
        return NULL;
    }
    return list;
}

/* The span's length in beats of the signature its start is read in: a rational, or null when the
   position cannot be read or the signature has no beat grid. Segments do not cross a barline, so one
   signature covers the whole span. */
static cJSON *span_beats_json(DocBuild *build, const MeterMap *map, Rational start, Rational end) {
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

    Rational length = doc_sub(build, end, start);
    cJSON *beats = rational_json(doc_in_beats(build, length, beat));
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
    if (object && !(json_add(object, "label", cJSON_CreateString(reading->label)) &&
                    json_add(object, "root", cJSON_CreateString(reading->root)) &&
                    json_add(object, "quality", cJSON_CreateString(reading->quality)) &&
                    json_add(object, "score", cJSON_CreateNumber(reading->score)) &&
                    json_add(object, "uncertainty", uncertainty_json(reading->confidence)))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

static const char *chord_context_name(AnalysisChordContext context) {
    switch (context) {
        case ANALYSIS_CHORD_CONTEXT_PREVIOUS:
            return "previous";
        case ANALYSIS_CHORD_CONTEXT_NEXT:
            return "next";
        case ANALYSIS_CHORD_CONTEXT_ARPEGGIO:
            return "arpeggio";
        default:
            return "none";
    }
}

/* The chord fields beyond the shared claim: root, quality, bass, inversion, duration, score, runner-up and the
   context the reading came from. */
static bool add_chord_fields(DocBuild *build, cJSON *claim, const ImportedNotes *notes, const AnalysisReportEntry *entry, Rational start,
                             Rational end) {
    const AnalysisChordReading *chord = &entry->chord;
    bool matched = chord->label != NULL;
    cJSON *duration = cJSON_CreateObject();
    if (duration && !(json_add(duration, "whole_notes", rational_json(doc_sub(build, end, start))) &&
                      json_add(duration, "beats", span_beats_json(build, notes->meter.meter_map, start, end)))) {
        cJSON_Delete(duration);
        duration = NULL;
    }
    return json_add(claim, "root", optional_string_json(chord->root)) &&
           json_add(claim, "quality", optional_string_json(chord->quality)) &&
           json_add(claim, "bass", optional_string_json(chord->bass)) &&
           json_add(claim, "inversion", matched && chord->inversion >= 0 ? cJSON_CreateNumber(chord->inversion)
                                                                    : cJSON_CreateNull()) &&
           json_add(claim, "duration", duration) &&
           json_add(claim, "score", matched ? cJSON_CreateNumber(chord->score) : cJSON_CreateNull()) &&
           json_add(claim, "runner_up", runner_up_json(&entry->runner_up)) &&
           json_add(claim, "context", matched ? cJSON_CreateString(chord_context_name(entry->chord_context))
                                         : cJSON_CreateNull());
}

static const char *outer_status_name(AnalysisOuterStatus status) {
    switch (status) {
        case ANALYSIS_OUTER_PRESENT:
            return "present";
        case ANALYSIS_OUTER_BRIDGED:
            return "bridged";
        case ANALYSIS_OUTER_GUARDED:
            return "guarded";
        default:
            return "absent";
    }
}

/* The uncertainty of a degree: the library's uncertainty for the key region the segment lies in, since the
   degree is only as sure as the key it is read against (in [0, 1], higher is LESS certain). Null when the
   voice is not present, or the segment lies in no key region. */
static cJSON *outer_uncertainty_json(const AnalysisReport *report, const AnalysisReportEntry *entry,
                                     const AnalysisOuterNote *note) {
    if (note->status != ANALYSIS_OUTER_PRESENT || !entry->in_region || entry->region >= report->key_region_count) {
        return cJSON_CreateNull();
    }
    return cJSON_CreateNumber(report->key_regions[entry->region].uncertainty);
}

/* One outer voice of a chord: its status, the note and pitch name when a note sounds as the line, its scale
   degree in the local key (null in no key region), and the degree's uncertainty. */
static cJSON *outer_voice_summary_json(const AnalysisReport *report, const AnalysisReportEntry *entry,
                                       const AnalysisOuterNote *note) {
    bool present = note->status == ANALYSIS_OUTER_PRESENT;
    cJSON *object = cJSON_CreateObject();
    if (object &&
        !(json_add(object, "status", cJSON_CreateString(outer_status_name(note->status))) &&
          json_add(object, "note_id", present ? cJSON_CreateNumber((double)note->note_id) : cJSON_CreateNull()) &&
          json_add(object, "pitch", optional_string_json(note->pitch_name)) &&
          json_add(object, "degree", optional_string_json(note->degree)) &&
          json_add(object, "uncertainty", outer_uncertainty_json(report, entry, note)))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* The chord's "outer" field: the soprano and the bass sounding in its segment. (The chord's own "bass" field is
   the pitch class the chord is read over.) */
static cJSON *chord_outer_json(const AnalysisReport *report, const AnalysisReportEntry *entry) {
    cJSON *outer = cJSON_CreateObject();
    if (outer && !(json_add(outer, "soprano", outer_voice_summary_json(report, entry, &entry->soprano)) &&
                   json_add(outer, "bass", outer_voice_summary_json(report, entry, &entry->bass)))) {
        cJSON_Delete(outer);
        return NULL;
    }
    return outer;
}

/* Whether a chord's span lies in, or runs straight into, the span of a region: the tonicization region the
   chord belongs to, or the one it leads to. */
static bool span_touches(Rational chord_start, Rational chord_end, Rational start, Rational end) {
    return (time_before(chord_start, end) && time_before(start, chord_end)) || rational_equals(chord_end, start);
}

/* The chord's applied reading and its cross-reference to the tonicization claims, or null for a chord that is
   not applied. The tonicization claims are numbered as in "tonicizations". Among the claims whose span touches the
   chord's (see span_touches), one whose key has the tonic the applied target names is the agreeing one; if
   only claims with another tonic touch it, the two disagree; if none touches, there is no tonicization.
   Neither side is changed to fit the other. */
static cJSON *applied_json(DocBuild *build, const AnalysisReport *report, const AnalysisReportEntry *entry) {
    const AnalysisRomanNumeral *roman = &entry->roman;
    if (!roman->applied) {
        return cJSON_CreateNull();
    }
    bool in_region = entry->in_region && entry->region < report->key_region_count;
    int target_tonic = -1;
    if (in_region) {
        const AnalysisKeyRegion *local = &report->key_regions[entry->region];
        int semitones = standard_tonality_degree_semitone(
            local->is_major ? STANDARD_TONALITY_MODE_MAJOR : STANDARD_TONALITY_MODE_NATURAL_MINOR, roman->target_degree);
        target_tonic = semitones < 0 ? -1 : (local->tonic_pitch_class + semitones) % 12;
    }
    Rational chord_start = doc_rational(build, entry->time_numerator, entry->time_denominator);
    Rational chord_end = doc_rational(build, entry->end_numerator, entry->end_denominator);

    bool touching = false;
    size_t touching_index = 0;
    size_t touching_region = 0;
    bool agrees = false;
    size_t tonicization_index = 0;
    for (size_t i = 0; i < report->key_change_count && !agrees; i++) {
        const AnalysisKeyChange *change = &report->key_changes[i];
        if (!change_is_tonicization(change) || change->is_return) {
            continue;
        }
        const AnalysisKeyRegion *target = &report->key_regions[change->to_region];
        if (span_touches(chord_start, chord_end, region_start(build, target), region_end(build, target))) {
            if (!touching || target->tonic_pitch_class == target_tonic) {
                touching = true;
                touching_index = tonicization_index;
                touching_region = change->to_region;
                agrees = target->tonic_pitch_class == target_tonic;
            }
        }
        tonicization_index++;
    }

    const char *agreement = agrees ? "agrees" : (touching ? "target_differs" : "no_tonicization");
    cJSON *object = cJSON_CreateObject();
    if (object &&
        !(json_add(object, "numeral", cJSON_CreateString(roman->applied_numeral)) &&
          json_add(object, "target_numeral", cJSON_CreateString(roman->target_numeral)) &&
          json_add(object, "target_degree", cJSON_CreateNumber(roman->target_degree)) &&
          json_add(object, "target_tonic", target_tonic >= 0 ? cJSON_CreateString(pitch_class_name(
                                          target_tonic, report->key_regions[entry->region].prefer_flats))
                                                         : cJSON_CreateNull()) &&
          json_add(object, "kind", cJSON_CreateString(roman->applied_kind)) &&
          json_add(object, "resolves_to", roman->resolution_entry == SIZE_MAX
                                          ? cJSON_CreateNull()
                                          : cJSON_CreateNumber((double)roman->resolution_entry)) &&
          json_add(object, "deceptive", cJSON_CreateBool(roman->deceptive)) &&
          json_add(object, "tonicization_agreement", cJSON_CreateString(agreement)) &&
          json_add(object, "tonicization_index", touching ? cJSON_CreateNumber((double)touching_index) : cJSON_CreateNull()) &&
          json_add(object, "target_region", touching ? cJSON_CreateNumber((double)touching_region) : cJSON_CreateNull()) &&
          json_add(object, "tonicization_key", touching ? cJSON_CreateString(report->key_regions[touching_region].label)
                                                   : cJSON_CreateNull()))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* The chord's Roman numeral: the library's label, the key region it is relative to (the region the segment
   starts in, which is also the key the chord was read in), its scale degree and uncertainty; or null label with
   the reason there is none. */
static cJSON *roman_numeral_json(DocBuild *build, const AnalysisReport *report, const AnalysisReportEntry *entry) {
    const AnalysisRomanNumeral *roman = &entry->roman;
    bool in_region = entry->in_region && entry->region < report->key_region_count;
    cJSON *object = cJSON_CreateObject();
    if (object &&
        !(json_add(object, "label", optional_string_json(roman->numeral)) &&
          json_add(object, "base_numeral", optional_string_json(roman->base_numeral)) &&
          json_add(object, "key_region", in_region ? cJSON_CreateNumber((double)entry->region) : cJSON_CreateNull()) &&
          json_add(object, "key", in_region ? cJSON_CreateString(report->key_regions[entry->region].label)
                                       : cJSON_CreateNull()) &&
          json_add(object, "degree", roman->labelled ? cJSON_CreateNumber(roman->degree) : cJSON_CreateNull()) &&
          json_add(object, "uncertainty", roman->labelled ? cJSON_CreateNumber(roman->uncertainty) : cJSON_CreateNull()) &&
          json_add(object, "unlabelled_reason", optional_string_json(roman->reason)) &&
          json_add(object, "applied", applied_json(build, report, entry)))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* The chord's harmonic function, the library's (standard-harmony), with the target degree of an applied chord (null
   otherwise). Chords the library gives no function have none. */
static bool add_function(cJSON *claim, const AnalysisReportEntry *entry) {
    if (entry->six_four) {
        cJSON *six_four = cJSON_CreateObject();
        if (!(six_four && json_add(six_four, "type", cJSON_CreateString(entry->six_four)) &&
              json_add(claim, "six_four", six_four))) {
            cJSON_Delete(six_four);
            return false;
        }
    }
    if (!entry->function) {
        return true;
    }
    cJSON *object = cJSON_CreateObject();
    if (!(object && json_add(object, "name", cJSON_CreateString(entry->function)) &&
          json_add(object, "applied_to",
              entry->function_applied ? cJSON_CreateNumber(entry->function_applied_to) : cJSON_CreateNull()) &&
          json_add(object, "predominant", optional_string_json(entry->predominant_strength)) &&
          json_add(object, "framework", cJSON_CreateString(framework_name(standard_harmony_framework()))) &&
          json_add(object, "uncertainty", cJSON_CreateNumber(entry->function_uncertainty)))) {
        cJSON_Delete(object);
        return false;
    }
    return json_add(claim, "function", object);
}

/* One chord claim per report entry, in time order. */
static cJSON *chords_json(DocBuild *build, const ImportedNotes *notes, const AnalysisReport *report) {
    cJSON *chords = cJSON_CreateArray();
    if (!chords) {
        return NULL;
    }
    const char *framework = library_framework_name();
    for (size_t i = 0; i < report->entry_count; i++) {
        const AnalysisReportEntry *entry = &report->entries[i];
        Rational start = doc_rational(build, entry->time_numerator, entry->time_denominator);
        Rational end = doc_rational(build, entry->end_numerator, entry->end_denominator);
        cJSON *claim = claim_json_at(build, framework, notes, entry->note_ids, entry->note_id_count, start, end,
                                     entry->chord_label, entry->chord_confidence);
        if (!(claim && json_add(claim, "key_label", cJSON_CreateString(entry->key_label)) &&
              add_chord_fields(build, claim, notes, entry, start, end) &&
              json_add(claim, "outer", chord_outer_json(report, entry)) &&
              json_add(claim, "roman_numeral", roman_numeral_json(build, report, entry)) && add_function(claim, entry) &&
              (!entry->phrase_model || json_add(claim, "phrase_model", cJSON_CreateString(entry->phrase_model))))) {
            cJSON_Delete(claim);
            cJSON_Delete(chords);
            return NULL;
        }
        if (!json_append(chords, claim)) {
            cJSON_Delete(chords);
            return NULL;
        }
    }
    return chords;
}

/* One claim per non-chord tone, in time order. Each is located where the note sounds against the chord:
   that chord's segment. */
static cJSON *non_chord_tones_json(DocBuild *build, const ImportedNotes *notes, const AnalysisReport *report) {
    cJSON *tones = cJSON_CreateArray();
    if (!tones) {
        return NULL;
    }
    const char *framework = library_framework_name();
    for (size_t i = 0; i < report->non_chord_tone_count; i++) {
        const AnalysisNonChordTone *tone = &report->non_chord_tones[i];
        const AnalysisReportEntry *chord = &report->entries[tone->chord_index];
        AnalysisConfidence none = {false, 0, 0};
        cJSON *claim = claim_json_at(build, 
            framework, notes, &tone->note_id, 1, doc_rational(build, chord->time_numerator, chord->time_denominator),
            doc_rational(build, chord->end_numerator, chord->end_denominator), tone->type ? tone->type : "unclassified", none);
        if (!(claim && json_add(claim, "type", optional_string_json(tone->type)) &&
              json_add(claim, "classified", cJSON_CreateBool(tone->type != NULL)) &&
              json_add(claim, "chord_index", cJSON_CreateNumber((double)tone->chord_index)) &&
              json_add(claim, "chord_label", cJSON_CreateString(chord->chord_label)))) {
            cJSON_Delete(claim);
            cJSON_Delete(tones);
            return NULL;
        }
        if (!json_append(tones, claim)) {
            cJSON_Delete(tones);
            return NULL;
        }
    }
    return tones;
}

/* ---- Cadences ---- */

/* The note ids of entries first..last, each once, in order. NULL on allocation failure (*count 0 with none). */
static size_t *entry_range_note_ids(const AnalysisReport *report, size_t first, size_t last, size_t *count) {
    size_t total = 0;
    for (size_t i = first; i <= last; i++) {
        total += report->entries[i].note_id_count;
    }
    size_t *ids = malloc(sizeof(size_t) * (total > 0 ? total : 1));
    if (!ids) {
        return NULL;
    }
    size_t next = 0;
    for (size_t i = first; i <= last; i++) {
        memcpy(ids + next, report->entries[i].note_ids, sizeof(size_t) * report->entries[i].note_id_count);
        next += report->entries[i].note_id_count;
    }
    qsort(ids, total, sizeof(size_t), size_compare);
    size_t unique = 0;
    for (size_t i = 0; i < total; i++) {
        if (i == 0 || ids[i] != ids[i - 1]) {
            ids[unique++] = ids[i];
        }
    }
    *count = unique;
    return ids;
}

static Rational entry_start_time(DocBuild *build, const AnalysisReportEntry *entry) {
    return doc_rational(build, entry->time_numerator, entry->time_denominator);
}

static Rational entry_end_time(DocBuild *build, const AnalysisReportEntry *entry) {
    return doc_rational(build, entry->end_numerator, entry->end_denominator);
}

/* {"span": ...} and {"span": ..., "location": ...} for the parts of a cadence. */
static cJSON *span_only_json(Rational start, Rational end) {
    cJSON *object = cJSON_CreateObject();
    if (object && !json_add(object, "span", span_json(start, end))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

static cJSON *span_and_location_json(DocBuild *build, const ImportedNotes *notes, Rational start, Rational end) {
    cJSON *object = span_only_json(start, end);
    if (object && !json_add(object, "location", location_json(build, notes->meter.meter_map, start, end))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* The competing reading of a cadence: {"reading", "type" (null for a prolongation), "measure", "beat" (where its
   arrival begins; null when the meter cannot place it), "share", "note_ids" (the notes of its arrival)}. */
static cJSON *cadence_alternative_json(DocBuild *build, const ImportedNotes *notes, const AnalysisReport *report,
                                       const AnalysisCadenceAlternative *alternative) {
    size_t id_count = 0;
    size_t *ids = entry_range_note_ids(report, alternative->arrival_first, alternative->arrival_last, &id_count);
    if (!ids) {
        return NULL;
    }
    cJSON *position = position_json(build, notes->meter.meter_map, entry_start_time(build, &report->entries[alternative->arrival_first]));
    cJSON *object = cJSON_CreateObject();
    cJSON *measure = position ? cJSON_GetObjectItemCaseSensitive(position, "measure") : NULL;
    cJSON *beat = position ? cJSON_GetObjectItemCaseSensitive(position, "beat") : NULL;
    bool ok = object &&
              json_add(object, "reading", cJSON_CreateString(alternative->reading)) &&
              json_add(object, "type", optional_string_json(alternative->type)) &&
              json_add(object, "measure", measure ? cJSON_CreateNumber(measure->valuedouble) : cJSON_CreateNull()) &&
              json_add(object, "beat", beat ? cJSON_CreateNumber(beat->valuedouble) : cJSON_CreateNull()) &&
              json_add(object, "share", cJSON_CreateNumber((double)alternative->share)) &&
              json_add(object, "note_ids", note_ids_json(ids, id_count));
    free(ids);
    cJSON_Delete(position);
    if (!ok) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* One claim per cadence, in time order. The claim's span runs from the dominant's start to the arrival's end;
   arrival has the arrival's span and location, dominant its span. soprano_degree is the outer voice's label at the
   arrival (null when absent); region the key region of the arrival (null outside every region); boundary marks a half
   cadence the boundary step added before a section start. share is the thousandths of this reading (1000 with no
   other); alternative, present only for a cadence with a competing reading, is the other one. Everything else is the
   library's: the type, the beat strength, the uncertainty. */
static cJSON *cadences_json(DocBuild *build, const ImportedNotes *notes, const AnalysisReport *report) {
    cJSON *list = cJSON_CreateArray();
    if (!list) {
        return NULL;
    }
    const char *framework = framework_name(standard_harmony_framework());
    for (size_t i = 0; i < report->cadence_count; i++) {
        const AnalysisCadence *cadence = &report->cadences[i];
        const AnalysisReportEntry *dominant = &report->entries[cadence->dominant_first];
        const AnalysisReportEntry *arrival_first = &report->entries[cadence->arrival_first];
        const AnalysisReportEntry *arrival_last = &report->entries[cadence->arrival_last];
        Rational start = entry_start_time(build, dominant);
        Rational end = entry_end_time(build, arrival_last);
        Rational arrival_start = entry_start_time(build, arrival_first);
        Rational dominant_end = entry_end_time(build, &report->entries[cadence->dominant_last]);
        size_t id_count = 0;
        size_t *ids = entry_range_note_ids(report, cadence->dominant_first, cadence->arrival_last, &id_count);
        cJSON *claim = ids ? claim_json_with(build, framework, notes, ids, id_count, start, end, cadence->type,
                                             cJSON_CreateNumber(cadence->uncertainty))
                           : NULL;
        free(ids);
        if (!(claim && json_add(claim, "type", cJSON_CreateString(cadence->type)) &&
              json_add(claim, "arrival", span_and_location_json(build, notes, arrival_start, end)) &&
              json_add(claim, "dominant", span_only_json(start, dominant_end)) &&
              json_add(claim, "soprano_degree", optional_string_json(cadence->soprano_degree)) &&
              json_add(claim, "arrival_beat_strength", cJSON_CreateString(cadence->arrival_beat_strength)) &&
              json_add(claim, "boundary", cJSON_CreateBool(cadence->boundary)) &&
              json_add(claim, "region",
                  cadence->in_region ? cJSON_CreateNumber((double)cadence->region) : cJSON_CreateNull()) &&
              json_add(claim, "share", cJSON_CreateNumber((double)cadence->share)) &&
              (!cadence->has_alternative ||
               json_add(claim, "alternative", cadence_alternative_json(build, notes, report, &cadence->alternative))))) {
            cJSON_Delete(claim);
            cJSON_Delete(list);
            return NULL;
        }
        if (!json_append(list, claim)) {
            cJSON_Delete(list);
            return NULL;
        }
    }
    return list;
}

/* One claim per prolongation, in order of the prolonged chord. The claim's span runs from the prolonged chord's start
   to the end of the closing chord's first segment; first_chord and last_chord are the indices of those chords in
   "chords" and numeral is the prolonged chord's Roman numeral (null when the library gave it none). reasons has one
   library reason name per interior chord. Everything else is the library's: the function, the reasons. */
static cJSON *prolongations_json(DocBuild *build, const ImportedNotes *notes, const AnalysisReport *report) {
    cJSON *list = cJSON_CreateArray();
    if (!list) {
        return NULL;
    }
    const char *framework = framework_name(standard_harmony_framework());
    for (size_t i = 0; i < report->prolongation_count; i++) {
        const AnalysisProlongation *prolongation = &report->prolongations[i];
        const AnalysisReportEntry *first = &report->entries[prolongation->first_entry];
        const AnalysisReportEntry *last = &report->entries[prolongation->last_entry];
        Rational start = entry_start_time(build, first);
        Rational end = entry_end_time(build, last);
        size_t id_count = 0;
        size_t *ids = entry_range_note_ids(report, prolongation->first_entry, prolongation->last_entry, &id_count);
        cJSON *claim = ids ? claim_json_with(build, framework, notes, ids, id_count, start, end, prolongation->function,
                                             cJSON_CreateNumber(prolongation->uncertainty))
                           : NULL;
        free(ids);
        cJSON *reasons = cJSON_CreateArray();
        bool reasons_ok = reasons != NULL;
        for (size_t r = 0; reasons_ok && r < prolongation->interior_count; r++) {
            reasons_ok = json_append(reasons, cJSON_CreateString(prolongation->reasons[r]));
        }
        bool reasons_taken = false; /* json_add() takes the reasons over, and frees them itself when it fails */
        if (!(claim && reasons_ok && json_add(claim, "function", cJSON_CreateString(prolongation->function)) &&
              json_add(claim, "numeral", optional_string_json(first->roman.numeral)) &&
              json_add(claim, "first_chord", cJSON_CreateNumber((double)prolongation->first_entry)) &&
              json_add(claim, "last_chord", cJSON_CreateNumber((double)prolongation->last_entry)) &&
              (reasons_taken = true, json_add(claim, "reasons", reasons)) &&
              json_add(claim, "region",
                  prolongation->in_region ? cJSON_CreateNumber((double)prolongation->region) : cJSON_CreateNull()))) {
            if (!reasons_taken) {
                cJSON_Delete(reasons);
            }
            cJSON_Delete(claim);
            cJSON_Delete(list);
            return NULL;
        }
        if (!json_append(list, claim)) {
            cJSON_Delete(list);
            return NULL;
        }
    }
    return list;
}

/* ---- Phrases ---- */

/* An idea of a phrase: the claim over its soprano notes (label the library's IdeaLabel name) with its place in the
   phrase, its measures and length, and the similarity to the phrase's first idea (null for the first idea). */
static cJSON *idea_json(DocBuild *build, const ImportedNotes *notes, const char *framework, const AnalysisIdea *idea) {
    Rational start = doc_rational(build, idea->start_numerator, idea->start_denominator);
    Rational end = doc_rational(build, idea->end_numerator, idea->end_denominator);
    cJSON *claim = claim_json_with(build, framework, notes, idea->note_ids, idea->note_id_count, start, end, idea->label,
                                   cJSON_CreateNumber(idea->uncertainty));
    if (!(claim && json_add(claim, "index", cJSON_CreateNumber(idea->index)) &&
          json_add(claim, "measures", measures_json(idea->first_measure, idea->last_measure)) &&
          json_add(claim, "length", cJSON_CreateNumber(idea->length_measures)) &&
          json_add(claim, "similarity", idea->compared ? cJSON_CreateNumber(idea->similarity) : cJSON_CreateNull()))) {
        cJSON_Delete(claim);
        return NULL;
    }
    return claim;
}

/* A subphrase: the claim over its soprano notes, labelled "subphrase" (the library gives subphrases no name), with its
   index (0 or 1), the ideas it holds (first_idea within the phrase's ideas), measures and length. */
static cJSON *subphrase_json(DocBuild *build, const ImportedNotes *notes, const char *framework, const AnalysisReport *report,
                             const AnalysisPhrase *phrase, const AnalysisSubphrase *subphrase) {
    Rational start = doc_rational(build, subphrase->start_numerator, subphrase->start_denominator);
    Rational end = doc_rational(build, subphrase->end_numerator, subphrase->end_denominator);
    cJSON *claim = claim_json_with(build, framework, notes, subphrase->note_ids, subphrase->note_id_count, start, end,
                                   "subphrase", cJSON_CreateNumber(subphrase->uncertainty));
    if (!(claim && json_add(claim, "index", cJSON_CreateNumber(subphrase->index)) &&
          json_add(claim, "first_idea", cJSON_CreateNumber((double)(subphrase->first_idea - phrase->first_idea))) &&
          json_add(claim, "idea_count", cJSON_CreateNumber((double)subphrase->idea_count)) &&
          json_add(claim, "measures", measures_json(subphrase->first_measure, subphrase->last_measure)) &&
          json_add(claim, "length", cJSON_CreateNumber(subphrase->length_measures)))) {
        cJSON_Delete(claim);
        return NULL;
    }
    (void)report;
    return claim;
}

/* The labelled chords of a phrase in order: {"chord": index into "chords", "label": "Tb" | "PD" | "D" | "Te"}. Empty
   when the library labelled none. */
static cJSON *phrase_model_json(const AnalysisReport *report, const AnalysisPhrase *phrase) {
    cJSON *list = cJSON_CreateArray();
    for (size_t k = 0; list && k < phrase->entry_count; k++) {
        const AnalysisReportEntry *entry = &report->entries[phrase->first_entry + k];
        if (!entry->phrase_model) {
            continue;
        }
        cJSON *segment = cJSON_CreateObject();
        if (!(segment && json_add(segment, "chord", cJSON_CreateNumber((double)(phrase->first_entry + k))) &&
              json_add(segment, "label", cJSON_CreateString(entry->phrase_model)) && json_append(list, segment))) {
            cJSON_Delete(segment);
            cJSON_Delete(list);
            return NULL;
        }
    }
    return list;
}

static bool add_ideas_and_subphrases(DocBuild *build, cJSON *claim, const ImportedNotes *notes, const char *framework,
                                     const AnalysisReport *report, const AnalysisPhrase *phrase) {
    cJSON *subphrases = cJSON_CreateArray();
    cJSON *ideas = cJSON_CreateArray();
    bool ok = subphrases && ideas;
    for (size_t k = 0; ok && k < phrase->subphrase_count; k++) {
        ok = json_append(subphrases, subphrase_json(build, notes, framework, report, phrase,
                                               &report->subphrases[phrase->first_subphrase + k]));
    }
    for (size_t k = 0; ok && k < phrase->idea_count; k++) {
        ok = json_append(ideas, idea_json(build, notes, framework, &report->ideas[phrase->first_idea + k]));
    }
    if (ok && json_add(claim, "subphrases", subphrases)) {
        subphrases = NULL;
        if (json_add(claim, "ideas", ideas)) {
            return true;
        }
        ideas = NULL;
    }
    cJSON_Delete(subphrases);
    cJSON_Delete(ideas);
    return false;
}

static cJSON *expansion_techniques_json(const AnalysisExpansion *expansion) {
    cJSON *techniques = cJSON_CreateArray();
    for (size_t n = 0; techniques && n < expansion->technique_count; n++) {
        if (!json_append(techniques, cJSON_CreateString(expansion->techniques[n]))) {
            cJSON_Delete(techniques);
            return NULL;
        }
    }
    return techniques;
}

/* An expansion or contraction of a phrase: the claim over its soprano notes (label the kind: "internal", "external" or
   "contraction"), with the change ("expansion" or "contraction"), the theme it concerns (null for an external
   expansion of a phrase in no theme), the expected and actual lengths and the measures changed, its measures, and the
   techniques the library finds evidence for. An external one adds external_kind and the two judgements of its length. */
static cJSON *expansion_json(DocBuild *build, const ImportedNotes *notes, const char *framework, const AnalysisExpansion *expansion) {
    Rational start = doc_rational(build, expansion->start_numerator, expansion->start_denominator);
    Rational end = doc_rational(build, expansion->end_numerator, expansion->end_denominator);
    cJSON *claim = claim_json_with(build, framework, notes, expansion->note_ids, expansion->note_id_count, start, end,
                                   expansion->kind, cJSON_CreateNumber(expansion->uncertainty));
    if (!(claim && json_add(claim, "index", cJSON_CreateNumber(expansion->index)) &&
          json_add(claim, "change", cJSON_CreateString(expansion->change)) &&
          json_add(claim, "theme", expansion->has_theme ? cJSON_CreateNumber((double)expansion->theme) : cJSON_CreateNull()) &&
          json_add(claim, "expected_length", cJSON_CreateNumber(expansion->expected_length)) &&
          json_add(claim, "actual_length", cJSON_CreateNumber(expansion->actual_length)) &&
          json_add(claim, "measures_changed", cJSON_CreateNumber(expansion->measures_changed)) &&
          json_add(claim, "measures", measures_json(expansion->first_measure, expansion->last_measure)) &&
          json_add(claim, "techniques", expansion_techniques_json(expansion)) &&
          (!expansion->external_kind ||
           (json_add(claim, "external_kind", cJSON_CreateString(expansion->external_kind)) &&
            json_add(claim, "suffix_length_typical", cJSON_CreateBool(expansion->suffix_length_typical)) &&
            json_add(claim, "suffix_agrees_with_location", cJSON_CreateBool(expansion->suffix_agrees_with_location)))))) {
        cJSON_Delete(claim);
        return NULL;
    }
    return claim;
}

/* The phrase's expansions and contractions as "expansions", in the library's order; absent when it has none. */
static bool add_expansions(DocBuild *build, cJSON *claim, const ImportedNotes *notes, const char *framework, const AnalysisReport *report,
                           size_t phrase) {
    cJSON *list = NULL;
    for (size_t e = 0; e < report->expansion_count; e++) {
        if (report->expansions[e].phrase != phrase) {
            continue;
        }
        if (!list && !(list = cJSON_CreateArray())) {
            return false;
        }
        if (!json_append(list, expansion_json(build, notes, framework, &report->expansions[e]))) {
            cJSON_Delete(list);
            return false;
        }
    }
    return !list || json_add(claim, "expansions", list);
}

/* One claim per phrase, in time order, from the library's standard-form analysis. The claim's span is the phrase's
   (an extension after its cadence included) and its note ids are the notes sounding in the chords that start in it.
   measures runs from the phrase's first measure to the measure it ends in; length is its length in measures (a pickup
   not counted); extension_measures the measures the extension adds after it; cadence the index into "cadences" of the
   cadence that ends it (null with none); signal what ended it: "cadence", "discontinuity" or "none" (the end of the
   input). role is where the phrase stands in the piece: "beginning" for the first, "ending" for the last, "middle"
   between (the library names no role for a phrase, so this is position alone). subphrases and ideas are claims inside
   it, in time order. expansions (absent with none) are the claims about its length: an expansion or contraction of its
   ending against its beginning, or an extension after its cadence. phrase_model lists the chords the library labelled for the phrase model, in order. */
static cJSON *phrases_json(DocBuild *build, const ImportedNotes *notes, const AnalysisReport *report) {
    cJSON *list = cJSON_CreateArray();
    if (!list) {
        return NULL;
    }
    const char *framework = framework_name(standard_form_framework());
    for (size_t i = 0; i < report->phrase_count; i++) {
        const AnalysisPhrase *phrase = &report->phrases[i];
        Rational start = doc_rational(build, phrase->start_numerator, phrase->start_denominator);
        Rational end = doc_rational(build, phrase->end_numerator, phrase->end_denominator);
        size_t id_count = 0;
        size_t *ids = phrase->entry_count > 0
                          ? entry_range_note_ids(report, phrase->first_entry,
                                                 phrase->first_entry + phrase->entry_count - 1, &id_count)
                          : NULL;
        cJSON *claim = (ids || phrase->entry_count == 0)
                           ? claim_json_with(build, framework, notes, ids, id_count, start, end, "phrase",
                                             cJSON_CreateNumber(phrase->uncertainty))
                           : NULL;
        free(ids);
        if (!(claim && json_add(claim, "index", cJSON_CreateNumber((double)i)) &&
              json_add(claim, "measures", measures_json(phrase->first_measure, phrase->end_measure)) &&
              json_add(claim, "length", cJSON_CreateNumber(phrase->length_measures)) &&
              json_add(claim, "extension_measures", cJSON_CreateNumber(phrase->extension_measures)) &&
              json_add(claim, "cadence", phrase->has_cadence ? cJSON_CreateNumber((double)phrase->cadence) : cJSON_CreateNull()) &&
              json_add(claim, "signal", cJSON_CreateString(phrase->signal)) &&
              json_add(claim, "role", cJSON_CreateString(phrase->role)) &&
              json_add(claim, "phrase_model", phrase_model_json(report, phrase)) &&
              add_ideas_and_subphrases(build, claim, notes, framework, report, phrase) &&
              add_expansions(build, claim, notes, framework, report, i))) {
            cJSON_Delete(claim);
            cJSON_Delete(list);
            return NULL;
        }
        if (!json_append(list, claim)) {
            cJSON_Delete(list);
            return NULL;
        }
    }
    return list;
}

/* ---- Themes ---- */

static cJSON *string_or_null_json(const char *text) {
    return text ? cJSON_CreateString(text) : cJSON_CreateNull();
}

/* One part of a theme: the library's name for it (null when the library gives none), its measures and length, and the
   index into "cadences" of the cadence that ends it (null with none: a presentation or the first half of a
   two-phrase theme can end on none). */
static cJSON *theme_part_json(const AnalysisThemePart *part) {
    cJSON *object = cJSON_CreateObject();
    if (!(object && json_add(object, "name", string_or_null_json(part->name)) &&
          json_add(object, "measures", measures_json(part->first_measure, part->last_measure)) &&
          json_add(object, "length", cJSON_CreateNumber(part->length_measures)) &&
          json_add(object, "cadence", part->has_cadence ? cJSON_CreateNumber((double)part->cadence) : cJSON_CreateNull()))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* The phrases a theme spans: the first one's index and how many. */
static cJSON *theme_phrases_json(const AnalysisTheme *theme) {
    cJSON *object = cJSON_CreateObject();
    if (!(object && json_add(object, "first", cJSON_CreateNumber((double)theme->first_phrase)) &&
          json_add(object, "count", cJSON_CreateNumber((double)theme->phrase_count)))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

static cJSON *theme_parts_json(const AnalysisTheme *theme) {
    cJSON *parts = cJSON_CreateArray();
    if (!(parts && json_append(parts, theme_part_json(&theme->parts[0])) && json_append(parts, theme_part_json(&theme->parts[1])))) {
        cJSON_Delete(parts);
        return NULL;
    }
    return parts;
}

/* The continuation traits the library's claim gives, as their names. */
static cJSON *theme_traits_json(const AnalysisTheme *theme) {
    cJSON *traits = cJSON_CreateArray();
    for (size_t n = 0; traits && n < theme->trait_count; n++) {
        if (!json_append(traits, cJSON_CreateString(theme->traits[n]))) {
            cJSON_Delete(traits);
            return NULL;
        }
    }
    return traits;
}

/* The two themes that make up a compound period, as indices into "themes". */
static cJSON *theme_halves_json(const AnalysisTheme *theme) {
    cJSON *halves = cJSON_CreateArray();
    if (!(halves && json_append(halves, cJSON_CreateNumber((double)theme->halves[0])) &&
          json_append(halves, cJSON_CreateNumber((double)theme->halves[1])))) {
        cJSON_Delete(halves);
        return NULL;
    }
    return halves;
}

/* One claim per theme, in the library's order, from its standard-form analysis. label is the library's form name
   ("sentence", "period", "hybrid", "compound-period", "repeated-phrase"); beginning and ending are the library's names
   for the two parts (null for a repeated phrase and a compound period); phrases are the phrases it spans (first index
   and count); measures run from its first measure to the last of its last phrase proper (an extension is not part of
   it); parts are the two parts in order, subphrases of a one-phrase theme or phrases of a two-phrase one.
   commonality is the library's name for how common the pairing of the two parts is (null when it has none);
   part_of_compound is true for a sentence that is a half of a compound period; halves (a compound period only) are the
   indices into "themes" of its two sentences. traits are the library's names for what its claim finds in the
   continuation ("fragmentation"; the claim gives no other); archetypal, cadences_usual and lengths_usual are the
   claim's own judgements of the pairing. */
static cJSON *themes_json(DocBuild *build, const ImportedNotes *notes, const AnalysisReport *report) {
    cJSON *list = cJSON_CreateArray();
    if (!list) {
        return NULL;
    }
    const char *framework = framework_name(standard_form_framework());
    for (size_t i = 0; i < report->theme_count; i++) {
        const AnalysisTheme *theme = &report->themes[i];
        Rational start = doc_rational(build, theme->start_numerator, theme->start_denominator);
        Rational end = doc_rational(build, theme->end_numerator, theme->end_denominator);
        cJSON *claim = claim_json_with(build, framework, notes, theme->note_ids, theme->note_id_count, start, end,
                                       theme->form, cJSON_CreateNumber(theme->uncertainty));
        if (!(claim && json_add(claim, "index", cJSON_CreateNumber(theme->index)) &&
              json_add(claim, "beginning", string_or_null_json(theme->beginning)) &&
              json_add(claim, "ending", string_or_null_json(theme->ending)) &&
              json_add(claim, "phrases", theme_phrases_json(theme)) &&
              json_add(claim, "measures", measures_json(theme->first_measure, theme->last_measure)) &&
              json_add(claim, "length", cJSON_CreateNumber(theme->length_measures)) &&
              json_add(claim, "parts_are_phrases", cJSON_CreateBool(theme->parts_are_phrases)) &&
              json_add(claim, "commonality", string_or_null_json(theme->commonality)) &&
              json_add(claim, "part_of_compound", cJSON_CreateBool(theme->part_of_compound)) &&
              json_add(claim, "traits", theme_traits_json(theme)) &&
              json_add(claim, "archetypal", cJSON_CreateBool(theme->archetypal)) &&
              json_add(claim, "cadences_usual", cJSON_CreateBool(theme->cadences_usual)) &&
              json_add(claim, "lengths_usual", cJSON_CreateBool(theme->lengths_usual)) &&
              json_add(claim, "parts", theme_parts_json(theme)) &&
              (!theme->has_halves || json_add(claim, "halves", theme_halves_json(theme))))) {
            cJSON_Delete(claim);
            cJSON_Delete(list);
            return NULL;
        }
        if (!json_append(list, claim)) {
            cJSON_Delete(list);
            return NULL;
        }
    }
    return list;
}

/* ---- Form plan ---- */

/* The form tree's nodes are named by level: "form", "section", "phrase", "subphrase" and "idea". A phrase, subphrase
   or idea node points at its full claim in "phrases" (phrase, and index within the phrase for the last two) and
   repeats only what reading the outline needs. */

static cJSON *idea_node_json(const AnalysisIdea *idea) {
    cJSON *node = cJSON_CreateObject();
    if (!(node && json_add(node, "level", cJSON_CreateString("idea")) &&
          json_add(node, "phrase", cJSON_CreateNumber((double)idea->phrase)) &&
          json_add(node, "index", cJSON_CreateNumber(idea->index)) && json_add(node, "label", cJSON_CreateString(idea->label)) &&
          json_add(node, "measures", measures_json(idea->first_measure, idea->last_measure)))) {
        cJSON_Delete(node);
        return NULL;
    }
    return node;
}

/* A subphrase node holds the ideas the subphrase holds. */
static cJSON *subphrase_node_json(const AnalysisReport *report, const AnalysisSubphrase *subphrase) {
    cJSON *node = cJSON_CreateObject();
    cJSON *ideas = cJSON_CreateArray();
    bool ok = node && ideas;
    for (size_t k = 0; ok && k < subphrase->idea_count; k++) {
        ok = json_append(ideas, idea_node_json(&report->ideas[subphrase->first_idea + k]));
    }
    bool taken = false; /* json_add() takes the ideas over, and frees them itself when it fails */
    if (!(ok && json_add(node, "level", cJSON_CreateString("subphrase")) &&
          json_add(node, "phrase", cJSON_CreateNumber((double)subphrase->phrase)) &&
          json_add(node, "index", cJSON_CreateNumber(subphrase->index)) &&
          json_add(node, "measures", measures_json(subphrase->first_measure, subphrase->last_measure)) &&
          (taken = true, json_add(node, "ideas", ideas)))) {
        if (!taken) {
            cJSON_Delete(ideas);
        }
        cJSON_Delete(node);
        return NULL;
    }
    return node;
}

static bool idea_in_subphrase(const AnalysisReport *report, const AnalysisPhrase *phrase, size_t idea) {
    for (size_t k = 0; k < phrase->subphrase_count; k++) {
        const AnalysisSubphrase *subphrase = &report->subphrases[phrase->first_subphrase + k];
        if (idea >= subphrase->first_idea && idea < subphrase->first_idea + subphrase->idea_count) {
            return true;
        }
    }
    return false;
}

/* A phrase node holds its subphrases, each with its ideas, and the ideas that lie in no subphrase (all of them for a
   phrase of fewer than three ideas, which has no subphrases), so each idea appears once in the tree. */
static cJSON *phrase_node_json(const AnalysisReport *report, size_t index) {
    const AnalysisPhrase *phrase = &report->phrases[index];
    cJSON *node = cJSON_CreateObject();
    cJSON *subphrases = cJSON_CreateArray();
    cJSON *ideas = cJSON_CreateArray();
    bool ok = node && subphrases && ideas;
    for (size_t k = 0; ok && k < phrase->subphrase_count; k++) {
        ok = json_append(subphrases, subphrase_node_json(report, &report->subphrases[phrase->first_subphrase + k]));
    }
    for (size_t k = 0; ok && k < phrase->idea_count; k++) {
        if (!idea_in_subphrase(report, phrase, phrase->first_idea + k)) {
            ok = json_append(ideas, idea_node_json(&report->ideas[phrase->first_idea + k]));
        }
    }
    bool taken_subphrases = false;
    bool taken_ideas = false;
    if (!(ok && json_add(node, "level", cJSON_CreateString("phrase")) &&
          json_add(node, "phrase", cJSON_CreateNumber((double)index)) &&
          json_add(node, "measures", measures_json(phrase->first_measure, phrase->end_measure)) &&
          json_add(node, "length", cJSON_CreateNumber(phrase->length_measures)) &&
          json_add(node, "cadence", phrase->has_cadence ? cJSON_CreateNumber((double)phrase->cadence) : cJSON_CreateNull()) &&
          (taken_subphrases = true, json_add(node, "subphrases", subphrases)) &&
          (taken_ideas = true, json_add(node, "ideas", ideas)))) {
        if (!taken_subphrases) {
            cJSON_Delete(subphrases);
        }
        if (!taken_ideas) {
            cJSON_Delete(ideas);
        }
        cJSON_Delete(node);
        return NULL;
    }
    return node;
}

static cJSON *section_phrases_json(const AnalysisReport *report, const AnalysisSection *section) {
    cJSON *phrases = cJSON_CreateArray();
    for (size_t k = 0; phrases && k < section->phrase_count; k++) {
        if (!json_append(phrases, phrase_node_json(report, section->first_phrase + k))) {
            cJSON_Delete(phrases);
            return NULL;
        }
    }
    return phrases;
}

static bool add_section_fields(cJSON *claim, const AnalysisReport *report, const AnalysisSection *section,
                               size_t index) {
    return json_add(claim, "level", cJSON_CreateString("section")) && json_add(claim, "index", cJSON_CreateNumber((double)index)) &&
           json_add(claim, "measures", measures_json(section->first_measure, section->last_measure)) &&
           json_add(claim, "key", section->in_region ? cJSON_CreateString(report->key_regions[section->region].label)
                                                : cJSON_CreateNull()) &&
           json_add(claim, "region", section->in_region ? cJSON_CreateNumber((double)section->region) : cJSON_CreateNull()) &&
           json_add(claim, "closed", cJSON_CreateBool(section->closed)) &&
           json_add(claim, "cadence", section->has_cadence ? cJSON_CreateNumber((double)section->cadence) : cJSON_CreateNull()) &&
           json_add(claim, "returns_from",
               section->has_return ? cJSON_CreateNumber((double)section->returns_from) : cJSON_CreateNull()) &&
           json_add(claim, "return",
               section->has_return ? cJSON_CreateNumber((double)section->return_index) : cJSON_CreateNull()) &&
           json_add(claim, "phrases", section_phrases_json(report, section));
}

/* One claim per section, labelled with its letter: the span runs from its first phrase's start to its last phrase's
   end, the notes are those sounding in its chords, the uncertainty the greatest of its phrases'. key is the key
   region the section opens in, closed whether one of the library's section-closing cadences ended it (false for the
   phrases after the last one), cadence the cadence that ends its last phrase, returns_from the earlier section whose
   material its opening theme returns (null for a section with a new letter) and return the index into the form's
   "returns" of that return. */
static cJSON *section_json(DocBuild *build, const ImportedNotes *notes, const AnalysisReport *report, size_t index) {
    const AnalysisSection *section = &report->sections[index];
    Rational start = doc_rational(build, section->start_numerator, section->start_denominator);
    Rational end = doc_rational(build, section->end_numerator, section->end_denominator);
    size_t id_count = 0;
    size_t *ids = section->entry_count > 0 ? entry_range_note_ids(report, section->first_entry,
                                                                  section->first_entry + section->entry_count - 1,
                                                                  &id_count)
                                           : NULL;
    cJSON *claim = (ids || section->entry_count == 0)
                       ? claim_json_with(build, framework_name(standard_form_framework()), notes, ids, id_count, start, end,
                                         section->label, cJSON_CreateNumber(section->uncertainty))
                       : NULL;
    free(ids);
    if (!(claim && add_section_fields(claim, report, section, index))) {
        cJSON_Delete(claim);
        return NULL;
    }
    return claim;
}

static cJSON *return_similarity_json(const AnalysisThematicReturn *item) {
    cJSON *object = cJSON_CreateObject();
    if (!(object && json_add(object, "value", cJSON_CreateNumber(item->similarity)) &&
          json_add(object, "contour", cJSON_CreateNumber(item->contour_score)) &&
          json_add(object, "rhythm", cJSON_CreateNumber(item->rhythm_score)) &&
          json_add(object, "window", cJSON_CreateNumber((double)item->window_length)) &&
          json_add(object, "earlier_offset", cJSON_CreateNumber((double)item->earlier_offset)) &&
          json_add(object, "later_offset", cJSON_CreateNumber((double)item->later_offset)))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* One claim per thematic return, by later theme, then earlier theme: the later theme recalls the earlier one. The
   claim is over the later theme (its span, notes and location); earlier_theme and later_theme are indices into
   "themes", earlier_section and later_section into the form's "sections", earlier and later the measures of the
   two themes, and similarity the kernel's scores for the best stretch pair (value is the lower of contour and
   rhythm; window the notes compared, the offsets where the stretches begin inside each theme). The uncertainty is
   1 minus the similarity. */
static cJSON *return_json(DocBuild *build, const ImportedNotes *notes, const AnalysisReport *report,
                          const AnalysisThematicReturn *item) {
    const AnalysisTheme *earlier = &report->themes[item->earlier_theme];
    const AnalysisTheme *later = &report->themes[item->later_theme];
    Rational start = doc_rational(build, later->start_numerator, later->start_denominator);
    Rational end = doc_rational(build, later->end_numerator, later->end_denominator);
    cJSON *claim = claim_json_with(build, framework_name(standard_form_framework()), notes, later->note_ids,
                                   later->note_id_count, start, end, "thematic-return",
                                   cJSON_CreateNumber(1.0 - item->similarity));
    if (!(claim && json_add(claim, "earlier_theme", cJSON_CreateNumber((double)item->earlier_theme)) &&
          json_add(claim, "later_theme", cJSON_CreateNumber((double)item->later_theme)) &&
          json_add(claim, "earlier_section", cJSON_CreateNumber((double)item->earlier_section)) &&
          json_add(claim, "later_section", cJSON_CreateNumber((double)item->later_section)) &&
          json_add(claim, "earlier", measures_json(earlier->first_measure, earlier->last_measure)) &&
          json_add(claim, "later", measures_json(later->first_measure, later->last_measure)) &&
          json_add(claim, "similarity", return_similarity_json(item)))) {
        cJSON_Delete(claim);
        return NULL;
    }
    return claim;
}

/* The form: a tree of sections, each holding its phrases, subphrases and ideas, and the thematic returns between
   its themes. null when the library found no phrases to group. The root has no span of its own; its measures run
   from the first section's first measure to the last section's last. */
static cJSON *form_json(DocBuild *build, const ImportedNotes *notes, const AnalysisReport *report) {
    if (report->section_count == 0) {
        return cJSON_CreateNull();
    }
    cJSON *form = cJSON_CreateObject();
    cJSON *sections = cJSON_CreateArray();
    cJSON *returns = cJSON_CreateArray();
    bool ok = form && sections && returns;
    for (size_t s = 0; ok && s < report->section_count; s++) {
        ok = json_append(sections, section_json(build, notes, report, s));
    }
    for (size_t r = 0; ok && r < report->thematic_return_count; r++) {
        ok = json_append(returns, return_json(build, notes, report, &report->thematic_returns[r]));
    }
    bool taken_sections = false;
    bool taken_returns = false;
    if (!(ok && json_add(form, "level", cJSON_CreateString("form")) &&
          json_add(form, "measures",
              measures_json(report->sections[0].first_measure,
                            report->sections[report->section_count - 1].last_measure)) &&
          (taken_sections = true, json_add(form, "sections", sections)) &&
          (taken_returns = true, json_add(form, "returns", returns)))) {
        if (!taken_sections) {
            cJSON_Delete(sections);
        }
        if (!taken_returns) {
            cJSON_Delete(returns);
        }
        cJSON_Delete(form);
        return NULL;
    }
    return form;
}

/* ---- Repeats ---- */

/* One claim per repeat or return of the library, in its order: the claim is over the later span (its measures and its
   soprano notes), kind and label are "repeat" or "return", earlier and later the measures of the two runs, and
   transposition the semitones from the earlier run to the later (0 for a repeat). The uncertainty is 0: the library
   finds repeats by exact match. */
static cJSON *repeat_json(DocBuild *build, const ImportedNotes *notes, const AnalysisRepeat *repeat) {
    const char *kind = repeat->is_return ? "return" : "repeat";
    Rational start = doc_rational(build, repeat->start_numerator, repeat->start_denominator);
    Rational end = doc_rational(build, repeat->end_numerator, repeat->end_denominator);
    cJSON *claim = claim_json_with(build, framework_name(standard_form_framework()), notes, repeat->note_ids,
                                   repeat->note_id_count, start, end, kind, cJSON_CreateNumber(0.0));
    if (!(claim && json_add(claim, "kind", cJSON_CreateString(kind)) &&
          json_add(claim, "earlier", measures_json(repeat->earlier_first, repeat->earlier_last)) &&
          json_add(claim, "later", measures_json(repeat->later_first, repeat->later_last)) &&
          json_add(claim, "transposition", cJSON_CreateNumber((double)repeat->transposition)))) {
        cJSON_Delete(claim);
        return NULL;
    }
    return claim;
}

/* The repeats and returns as "repeats", in the library's order; an empty array when there are none. */
static cJSON *repeats_json(DocBuild *build, const ImportedNotes *notes, const AnalysisReport *report) {
    cJSON *list = cJSON_CreateArray();
    for (size_t i = 0; list && i < report->repeat_count; i++) {
        if (!json_append(list, repeat_json(build, notes, &report->repeats[i]))) {
            cJSON_Delete(list);
            return NULL;
        }
    }
    return list;
}

/* ---- Progressions ---- */

/* The pattern as a JSON array of its base numerals. */
static cJSON *progression_pattern_json(const AnalysisProgression *progression) {
    cJSON *pattern = cJSON_CreateArray();
    for (size_t n = 0; pattern && n < progression->length; n++) {
        if (!json_append(pattern, cJSON_CreateString(progression->pattern[n]))) {
            cJSON_Delete(pattern);
            return NULL;
        }
    }
    return pattern;
}

/* The indices into "chords" of every chord from first to last, held repeats included. */
static cJSON *chord_range_json(size_t first, size_t last) {
    cJSON *indices = cJSON_CreateArray();
    for (size_t i = first; indices && i <= last; i++) {
        if (!json_append(indices, cJSON_CreateNumber((double)i))) {
            cJSON_Delete(indices);
            return NULL;
        }
    }
    return indices;
}

/* One occurrence of a progression, a claim over the chords of the run: its span and location run from the first
   chord's start to the last chord's end, note_ids are the notes sounding in them, the label is the pattern's, and
   the uncertainty is the least certain Roman numeral in the run (higher is less certain). region is the key region
   all the chords lie in; chords are their indices in "chords". */
static cJSON *progression_occurrence_json(DocBuild *build, const ImportedNotes *notes, const AnalysisReport *report, const char *label,
                                          const AnalysisProgressionOccurrence *occurrence) {
    double uncertainty = 0.0;
    for (size_t i = occurrence->first_entry; i <= occurrence->last_entry; i++) {
        if (report->entries[i].roman.labelled && report->entries[i].roman.uncertainty > uncertainty) {
            uncertainty = report->entries[i].roman.uncertainty;
        }
    }
    size_t id_count = 0;
    size_t *ids = entry_range_note_ids(report, occurrence->first_entry, occurrence->last_entry, &id_count);
    cJSON *claim = ids ? claim_json_with(build, library_framework_name(), notes, ids, id_count,
                                         entry_start_time(build, &report->entries[occurrence->first_entry]),
                                         entry_end_time(build, &report->entries[occurrence->last_entry]), label,
                                         cJSON_CreateNumber(uncertainty))
                       : NULL;
    free(ids);
    if (!(claim && json_add(claim, "region", cJSON_CreateNumber((double)report->entries[occurrence->first_entry].region)) &&
          json_add(claim, "chords", chord_range_json(occurrence->first_entry, occurrence->last_entry)))) {
        cJSON_Delete(claim);
        return NULL;
    }
    return claim;
}

/* One entry per recurring progression, in rank order (most occurrences, then longest, then earliest): pattern (the
   base numerals), label ("I > IV > V > I"), length, count and its occurrences in time order. The progressions are
   the analyzer's count over the library's Roman numerals; nothing here is a claim of its own. */
static cJSON *progressions_json(DocBuild *build, const ImportedNotes *notes, const AnalysisReport *report) {
    cJSON *list = cJSON_CreateArray();
    if (!list) {
        return NULL;
    }
    for (size_t i = 0; i < report->progression_count; i++) {
        const AnalysisProgression *progression = &report->progressions[i];
        char *label = progression_label(progression);
        cJSON *entry = label ? cJSON_CreateObject() : NULL;
        cJSON *occurrences = entry ? cJSON_CreateArray() : NULL;
        bool ok = occurrences != NULL;
        for (size_t o = 0; ok && o < progression->occurrence_count; o++) {
            ok = json_append(occurrences, progression_occurrence_json(build, notes, report, label, &progression->occurrences[o]));
        }
        bool taken = false; /* json_add() takes the occurrences over, and frees them itself when it fails */
        ok = ok && json_add(entry, "pattern", progression_pattern_json(progression)) &&
             json_add(entry, "label", cJSON_CreateString(label)) &&
             json_add(entry, "length", cJSON_CreateNumber((double)progression->length)) &&
             json_add(entry, "count", cJSON_CreateNumber((double)progression->occurrence_count)) &&
             (taken = true, json_add(entry, "occurrences", occurrences));
        free(label);
        if (!(ok && json_append(list, entry))) {
            if (!taken) {
                cJSON_Delete(occurrences);
            }
            if (!ok) {
                cJSON_Delete(entry);
            }
            cJSON_Delete(list);
            return NULL;
        }
    }
    return list;
}

/* ---- Harmonic rhythm ---- */

#define RHYTHM_WINDOW_MEASURES 4

/* The mean number of changes per measure over report->harmonic_rhythm[first..first + measures), a rational. */
static Rational rhythm_mean(DocBuild *build, const AnalysisReport *report, size_t first, size_t measures) {
    long long changes = 0;
    for (size_t m = first; m < first + measures; m++) {
        changes += (long long)report->harmonic_rhythm[m].changes;
    }
    return doc_rational(build, changes, measures);
}

static cJSON *rhythm_per_measure_json(const AnalysisReport *report) {
    cJSON *list = cJSON_CreateArray();
    for (size_t m = 0; list && m < report->harmonic_rhythm_count; m++) {
        cJSON *entry = cJSON_CreateObject();
        if (!(entry && json_add(entry, "measure", cJSON_CreateNumber(report->harmonic_rhythm[m].measure)) &&
              json_add(entry, "changes", cJSON_CreateNumber((double)report->harmonic_rhythm[m].changes)) &&
              json_append(list, entry))) {
            cJSON_Delete(entry);
            cJSON_Delete(list);
            return NULL;
        }
    }
    return list;
}

/* The mean per window of RHYTHM_WINDOW_MEASURES consecutive measures from the first, the last window shorter when
   the measures run out. */
static cJSON *rhythm_per_window_json(DocBuild *build, const AnalysisReport *report) {
    cJSON *list = cJSON_CreateArray();
    for (size_t first = 0; list && first < report->harmonic_rhythm_count; first += RHYTHM_WINDOW_MEASURES) {
        size_t measures = report->harmonic_rhythm_count - first;
        measures = measures < RHYTHM_WINDOW_MEASURES ? measures : RHYTHM_WINDOW_MEASURES;
        cJSON *entry = cJSON_CreateObject();
        if (!(entry && json_add(entry, "first_measure", cJSON_CreateNumber(report->harmonic_rhythm[first].measure)) &&
              json_add(entry, "last_measure", cJSON_CreateNumber(report->harmonic_rhythm[first + measures - 1].measure)) &&
              json_add(entry, "mean_per_measure", rational_json(rhythm_mean(build, report, first, measures))) &&
              json_append(list, entry))) {
            cJSON_Delete(entry);
            cJSON_Delete(list);
            return NULL;
        }
    }
    return list;
}

/* The mean of changes over measures measures, or null when the piece has none of them. */
static cJSON *window_mean_json(DocBuild *build, size_t changes, size_t measures) {
    return measures > 0 ? rational_json(doc_rational(build, (long long)changes, measures)) : cJSON_CreateNull();
}

/* One entry per cadence, in the cadences' order: its arrival measure, the mean changes per measure over the two
   measures before the arrival (approach) and over the two before those (before), and whether the approach is faster.
   A window the start of the piece clips to nothing has a null mean, and then accelerates is false. */
static cJSON *cadence_approaches_json(DocBuild *build, const AnalysisReport *report) {
    cJSON *list = cJSON_CreateArray();
    for (size_t c = 0; list && c < report->cadence_approach_count; c++) {
        const AnalysisCadenceApproach *approach = &report->cadence_approaches[c];
        bool accelerates = approach->before_measures > 0 && approach->approach_measures > 0 &&
                           approach->approach_changes * approach->before_measures >
                               approach->before_changes * approach->approach_measures;
        cJSON *entry = cJSON_CreateObject();
        if (!(entry && json_add(entry, "cadence", cJSON_CreateNumber((double)approach->cadence)) &&
              json_add(entry, "arrival_measure", cJSON_CreateNumber(approach->arrival_measure)) &&
              json_add(entry, "before", window_mean_json(build, approach->before_changes, approach->before_measures)) &&
              json_add(entry, "approach", window_mean_json(build, approach->approach_changes, approach->approach_measures)) &&
              json_add(entry, "accelerates", cJSON_CreateBool(accelerates)) && json_append(list, entry))) {
            cJSON_Delete(entry);
            cJSON_Delete(list);
            return NULL;
        }
    }
    return list;
}

/* The harmonic rhythm: the changes of harmony in each measure (a chord segment whose base numeral or key region
   differs from the one before), their mean over the piece, the mean per window of four measures, from the first, and
   how the rhythm moves into each cadence (cadence_approaches). Null when the piece has no chords the meter can place. */
static cJSON *harmonic_rhythm_json(DocBuild *build, const AnalysisReport *report) {
    if (report->harmonic_rhythm_count == 0) {
        return cJSON_CreateNull();
    }
    cJSON *object = cJSON_CreateObject();
    if (!(object && json_add(object, "per_measure", rhythm_per_measure_json(report)) &&
          json_add(object, "mean_per_measure", rational_json(rhythm_mean(build, report, 0, report->harmonic_rhythm_count))) &&
          json_add(object, "per_window", rhythm_per_window_json(build, report)) &&
          json_add(object, "cadence_approaches", cadence_approaches_json(build, report)))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* ---- Outer voices ---- */

/* The note's own timing, apart from the chord segment it sounds in: where it is attacked, as a time and a
   measure and beat, and how long it lasts in whole notes and in beats of the signature it is attacked in.
   Null members when no note sounds as the line. */
static bool add_outer_note_timing(DocBuild *build, cJSON *claim, const ImportedNotes *notes, const AnalysisOuterNote *voice) {
    if (voice->status != ANALYSIS_OUTER_PRESENT) {
        return json_add(claim, "onset", cJSON_CreateNull()) && json_add(claim, "duration", cJSON_CreateNull());
    }
    const NoteEvent *event = notes->events[voice->note_id];
    Rational onset = note_onset(build, event);
    Rational end = note_end(build, event);
    cJSON *onset_object = cJSON_CreateObject();
    cJSON *duration = cJSON_CreateObject();
    bool ok = onset_object && duration && json_add(onset_object, "time", rational_json(onset)) &&
              json_add(onset_object, "location", position_json(build, notes->meter.meter_map, onset));
    if (ok) {
        Rational length = doc_sub(build, end, onset);
        ok = json_add(duration, "whole_notes", rational_json(length)) &&
             json_add(duration, "beats", span_beats_json(build, notes->meter.meter_map, onset, end));
    }
    if (!ok) {
        cJSON_Delete(onset_object);
        cJSON_Delete(duration);
        return false;
    }
    return json_add(claim, "onset", onset_object) && json_add(claim, "duration", duration);
}

/* {"name", "midi"}: the note's pitch name with octave and its MIDI note number; null when no note sounds. */
static cJSON *outer_pitch_json(const AnalysisOuterNote *voice) {
    if (voice->status != ANALYSIS_OUTER_PRESENT) {
        return cJSON_CreateNull();
    }
    cJSON *pitch = cJSON_CreateObject();
    if (pitch && !(json_add(pitch, "name", cJSON_CreateString(voice->pitch_name)) &&
                   json_add(pitch, "midi", cJSON_CreateNumber(voice->pitch)))) {
        cJSON_Delete(pitch);
        return NULL;
    }
    return pitch;
}

/* One entry of an outer line: the claim over the voice segment's window (note_ids is the line's note, or empty
   when none sounds), then the chord whose segment holds the voice segment's start, the status, the note, its
   pitch, its scale degree in that chord's local key, and its timing. The label is the degree; failing that the
   pitch name (the segment lies in no key region), or the status when no note sounds. held is relative to the
   voice segment. */
static cJSON *outer_voice_note_json(DocBuild *build, const ImportedNotes *notes, const AnalysisReport *report,
                                    const AnalysisOuterLineEntry *line, const AnalysisOuterNote *voice) {
    const AnalysisReportEntry *entry = &report->entries[line->chord_index];
    bool present = voice->status == ANALYSIS_OUTER_PRESENT;
    Rational start = doc_rational(build, line->time_numerator, line->time_denominator);
    Rational end = doc_rational(build, line->end_numerator, line->end_denominator);
    const char *label = voice->degree ? voice->degree
                                       : (present ? voice->pitch_name : outer_status_name(voice->status));
    cJSON *claim = claim_json_with(build, library_framework_name(), notes, present ? &voice->note_id : NULL,
                                   present ? 1 : 0, start, end, label, outer_uncertainty_json(report, entry, voice));
    if (!(claim && json_add(claim, "chord_index", cJSON_CreateNumber((double)line->chord_index)) &&
          json_add(claim, "status", cJSON_CreateString(outer_status_name(voice->status))) &&
          json_add(claim, "key_label", cJSON_CreateString(entry->key_label)) &&
          json_add(claim, "note_id", present ? cJSON_CreateNumber((double)voice->note_id) : cJSON_CreateNull()) &&
          json_add(claim, "pitch", outer_pitch_json(voice)) && json_add(claim, "degree", optional_string_json(voice->degree)) &&
          json_add(claim, "held", present ? cJSON_CreateBool(voice->held) : cJSON_CreateNull()) &&
          add_outer_note_timing(build, claim, notes, voice))) {
        cJSON_Delete(claim);
        return NULL;
    }
    return claim;
}

/* One line of the outer voices: an entry for every voice segment (one per melody note), in time order. Several
   entries share a chord on a harmonic span, so a line lines up with the chords by chord_index, many to one. */
static cJSON *outer_line_json(DocBuild *build, const ImportedNotes *notes, const AnalysisReport *report, bool soprano) {
    cJSON *line = cJSON_CreateArray();
    if (!line) {
        return NULL;
    }
    for (size_t i = 0; i < report->outer_line_count; i++) {
        const AnalysisOuterLineEntry *entry = &report->outer_lines[i];
        if (!json_append(line, outer_voice_note_json(build, notes, report, entry, soprano ? &entry->soprano : &entry->bass))) {
            cJSON_Delete(line);
            return NULL;
        }
    }
    return line;
}

/* The outer_voices section: the soprano and the bass, the shared input for voice-leading, melody and schema
   analysis. */
static cJSON *outer_voices_json(DocBuild *build, const ImportedNotes *notes, const AnalysisReport *report) {
    cJSON *outer = cJSON_CreateObject();
    if (outer && !(json_add(outer, "soprano", outer_line_json(build, notes, report, true)) &&
                   json_add(outer, "bass", outer_line_json(build, notes, report, false)))) {
        cJSON_Delete(outer);
        return NULL;
    }
    return outer;
}

/* ---- Chord vocabulary ---- */

/* The groups a labelled chord falls in. Applied is the library's applied reading. The library's Roman numeral
   adapter does not yet name borrowed (modal mixture) chords, so no chord is placed in the mixture group today. */
typedef enum { GROUP_DIATONIC = 0, GROUP_APPLIED, GROUP_MIXTURE, GROUP_COUNT } VocabularyGroup;
static const char *const GROUP_NAMES[GROUP_COUNT] = {"diatonic", "applied", "mixture"};
static const char *const LEVEL_NAMES[2] = {"numeral", "base_numeral"};
#define SCOPE_OVERALL (-2L)

/* One labelled chord, read back from its chord claim. */
typedef struct {
    size_t index;
    long region; /* -1 when the chord lies in no key region */
    int group;
    const char *label;
    const char *base;
    Rational whole;
    bool has_beats;
    Rational beats;
    bool starts_run; /* not a repeat of the chord before it */
    const cJSON *position; /* location.start */
} VocabularyChord;

typedef struct {
    const char *name;
    size_t count;
    size_t runs;
    Rational whole;
    Rational beats;
    bool beats_known;
    size_t first; /* index of its first chord claim */
    const cJSON *position;
} VocabularyTally;

static Rational rational_read(DocBuild *build, const cJSON *object) {
    const cJSON *num = cJSON_GetObjectItemCaseSensitive(object, "num");
    const cJSON *den = cJSON_GetObjectItemCaseSensitive(object, "den");
    long long denominator = cJSON_IsNumber(den) ? (long long)den->valuedouble : 1;
    return doc_rational(build, cJSON_IsNumber(num) ? (long long)num->valuedouble : 0,
                        denominator > 0 ? (unsigned long long)denominator : 1);
}

/* The labelled chords of the chords section, in time order. *count is how many. NULL on allocation failure. */
static VocabularyChord *vocabulary_chords(DocBuild *build, const cJSON *chords, size_t *count) {
    size_t total = (size_t)cJSON_GetArraySize(chords);
    VocabularyChord *records = calloc(total > 0 ? total : 1, sizeof(VocabularyChord));
    if (!records) {
        return NULL;
    }
    *count = 0;
    const VocabularyChord *previous = NULL;
    size_t index = 0;
    const cJSON *chord;
    cJSON_ArrayForEach(chord, chords) {
        const cJSON *roman = cJSON_GetObjectItemCaseSensitive(chord, "roman_numeral");
        const char *label = json_string_member(roman, "label");
        const char *base = json_string_member(roman, "base_numeral");
        if (label && base) {
            const cJSON *region = cJSON_GetObjectItemCaseSensitive(roman, "key_region");
            const cJSON *duration = cJSON_GetObjectItemCaseSensitive(chord, "duration");
            const cJSON *beats = cJSON_GetObjectItemCaseSensitive(duration, "beats");
            VocabularyChord *record = &records[(*count)++];
            record->index = index;
            record->region = cJSON_IsNumber(region) ? (long)region->valuedouble : -1;
            record->group = cJSON_IsObject(cJSON_GetObjectItemCaseSensitive(roman, "applied")) ? GROUP_APPLIED
                                                                                                 : GROUP_DIATONIC;
            record->label = label;
            record->base = base;
            record->whole = rational_read(build, cJSON_GetObjectItemCaseSensitive(duration, "whole_notes"));
            record->has_beats = cJSON_IsObject(beats);
            record->beats = record->has_beats ? rational_read(build, beats) : doc_rational(build, 0, 1);
            record->position = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(chord, "location"),
                                                                "start");
            record->starts_run = !(previous && previous->index + 1 == index && previous->region == record->region &&
                                   strcmp(previous->label, label) == 0);
            previous = record;
        } else {
            previous = NULL;
        }
        index++;
    }
    return records;
}

/* Most chords first, then the longest, then the earliest. */
static int compare_tallies(const void *a, const void *b) {
    const VocabularyTally *left = a;
    const VocabularyTally *right = b;
    if (left->count != right->count) {
        return left->count > right->count ? -1 : 1;
    }
    if (time_before(right->whole, left->whole)) {
        return -1;
    }
    if (time_before(left->whole, right->whole)) {
        return 1;
    }
    return left->first < right->first ? -1 : (left->first > right->first ? 1 : 0);
}

static cJSON *vocabulary_duration_json(const VocabularyTally *tally) {
    cJSON *duration = cJSON_CreateObject();
    if (duration && !(json_add(duration, "whole_notes", rational_json(tally->whole)) &&
                      json_add(duration, "beats", tally->beats_known ? rational_json(tally->beats) : cJSON_CreateNull()))) {
        cJSON_Delete(duration);
        return NULL;
    }
    return duration;
}

static cJSON *vocabulary_first_json(const VocabularyTally *tally) {
    cJSON *first = cJSON_CreateObject();
    if (first && !(json_add(first, "chord_index", cJSON_CreateNumber((double)tally->first)) &&
                   json_add(first, "position", cJSON_Duplicate(tally->position, true)))) {
        cJSON_Delete(first);
        return NULL;
    }
    return first;
}

static cJSON *vocabulary_entry_json(const AnalysisReport *report, long scope, int group, int level,
                                    const VocabularyTally *tally, Rational total) {
    bool in_region = scope >= 0 && (size_t)scope < report->key_region_count;
    double share = total.num > 0 ? ((double)tally->whole.num / (double)tally->whole.den) /
                                       ((double)total.num / (double)total.den)
                                 : 0.0;
    cJSON *object = cJSON_CreateObject();
    if (object &&
        !(json_add(object, "scope", cJSON_CreateString(scope == SCOPE_OVERALL ? "overall" : "region")) &&
          json_add(object, "key_region", in_region ? cJSON_CreateNumber((double)scope) : cJSON_CreateNull()) &&
          json_add(object, "key", in_region ? cJSON_CreateString(report->key_regions[scope].label) : cJSON_CreateNull()) &&
          json_add(object, "group", cJSON_CreateString(GROUP_NAMES[group])) &&
          json_add(object, "level", cJSON_CreateString(LEVEL_NAMES[level])) &&
          json_add(object, "numeral", cJSON_CreateString(tally->name)) &&
          json_add(object, "count", cJSON_CreateNumber((double)tally->count)) &&
          json_add(object, "runs", cJSON_CreateNumber((double)tally->runs)) &&
          json_add(object, "duration", vocabulary_duration_json(tally)) &&
          json_add(object, "share", cJSON_CreateNumber(share)) && json_add(object, "first", vocabulary_first_json(tally)))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* The entries of one scope, level and group, sorted, appended to list. False on allocation failure. */
static bool append_tallies(DocBuild *build, cJSON *list, const AnalysisReport *report, const VocabularyChord *records, size_t count,
                           long scope, int group, int level, Rational total) {
    VocabularyTally *tallies = calloc(count > 0 ? count : 1, sizeof(VocabularyTally));
    if (!tallies) {
        return false;
    }
    size_t distinct = 0;
    for (size_t i = 0; i < count; i++) {
        const VocabularyChord *record = &records[i];
        if (record->group != group || (scope != SCOPE_OVERALL && record->region != scope)) {
            continue;
        }
        const char *name = level == 0 ? record->label : record->base;
        size_t t = 0;
        while (t < distinct && strcmp(tallies[t].name, name) != 0) {
            t++;
        }
        VocabularyTally *tally = &tallies[t];
        if (t == distinct) {
            *tally = (VocabularyTally){
                name, 0, 0, doc_rational(build, 0, 1), doc_rational(build, 0, 1), true, record->index, record->position};
            distinct++;
        }
        tally->count++;
        tally->runs += record->starts_run ? 1 : 0;
        tally->whole = doc_add(build, tally->whole, record->whole);
        tally->beats_known = tally->beats_known && record->has_beats;
        if (tally->beats_known) {
            tally->beats = doc_add(build, tally->beats, record->beats);
        }
    }
    qsort(tallies, distinct, sizeof(VocabularyTally), compare_tallies);
    bool ok = true;
    for (size_t t = 0; ok && t < distinct; t++) {
        cJSON *entry = vocabulary_entry_json(report, scope, group, level, &tallies[t], total);
        ok = entry && json_append(list, entry);
    }
    free(tallies);
    return ok;
}

/* The chord vocabulary: for the whole piece and for each key region, each distinct Roman numeral of the labelled
   chords in three groups (diatonic, applied, mixture), once as the library names it (inversion figure included)
   and once by base numeral (the same numeral in root position). count is the number of chord claims with the
   numeral; runs counts a stretch of adjacent claims with the identical numeral in one key region once. duration is
   their total length, share its fraction of the scope's total labelled chord duration, and first the earliest.
   Entries are sorted by count, then duration, then first occurrence. Chords without a numeral are not counted. */
static cJSON *chord_vocabulary_json(DocBuild *build, const AnalysisReport *report, const cJSON *chords) {
    cJSON *list = cJSON_CreateArray();
    size_t count = 0;
    VocabularyChord *records = list ? vocabulary_chords(build, chords, &count) : NULL;
    if (!records) {
        cJSON_Delete(list);
        return NULL;
    }
    bool ok = true;
    for (long scope = SCOPE_OVERALL; ok && scope < (long)report->key_region_count; scope = scope == SCOPE_OVERALL ? 0 : scope + 1) {
        Rational total = doc_rational(build, 0, 1);
        for (size_t i = 0; i < count; i++) {
            if (scope == SCOPE_OVERALL || records[i].region == scope) {
                total = doc_add(build, total, records[i].whole);
            }
        }
        for (int level = 0; ok && level < 2; level++) {
            for (int group = 0; ok && group < GROUP_COUNT; group++) {
                ok = append_tallies(build, list, report, records, count, scope, group, level, total);
            }
        }
    }
    free(records);
    if (!ok) {
        cJSON_Delete(list);
        return NULL;
    }
    return list;
}

/* ---- Motives ---- */

static bool add_owned(cJSON *object, const char *key, cJSON **item) {
    bool ok = json_add(object, key, *item);
    *item = NULL;
    return ok;
}

/* An exact fraction of a whole note as "n/d". */
static cJSON *fraction_string_json(unsigned long long numerator, unsigned long long denominator) {
    char text[48];
    snprintf(text, sizeof text, "%llu/%llu", numerator, denominator);
    return cJSON_CreateString(text);
}

static cJSON *strings_json(const char *const *labels, size_t count) {
    cJSON *list = cJSON_CreateArray();
    for (size_t i = 0; list && i < count; i++) {
        if (!json_append(list, labels[i] ? cJSON_CreateString(labels[i]) : cJSON_CreateNull())) {
            return NULL;
        }
    }
    return list;
}

static cJSON *numbers_json(const long *values, size_t count) {
    cJSON *list = cJSON_CreateArray();
    for (size_t i = 0; list && i < count; i++) {
        if (!json_append(list, cJSON_CreateNumber((double)values[i]))) {
            return NULL;
        }
    }
    return list;
}

/* One occurrence of a motive family as a claim over its soprano notes. The label is the library's kind of the
   occurrence relative to the family's prototype (the transformation: "exact", "real-transposed" for the same semitone
   intervals from another pitch, "tonal-transposed" for the same scale steps, "inverted", "retrograde", "augmented",
   "diminished", "fragmented" or "varied"). measures run from the measure of its first note to the measure of its last;
   beat is the 1-based beat of the first measure its first note starts on. score is the library's match score and
   mismatches the interval mismatches of a varied occurrence out of intervals compared. A fragmented occurrence
   names the part of the prototype it is (fragment: "prefix" or "suffix") and says whether it lies in a continuation
   (in_continuation). phrase is the index into "phrases" of the phrase the whole occurrence lies in, and idea the
   index (within that phrase's "ideas") of the idea it lies in; each is null when the occurrence crosses a boundary.
   Both are placed by span against the report's phrases and ideas. */
static cJSON *motive_occurrence_json(DocBuild *build, const ImportedNotes *notes, const AnalysisReport *report, const char *framework,
                                     const AnalysisMotiveOccurrence *occurrence) {
    Rational start = doc_rational(build, occurrence->start_numerator, occurrence->start_denominator);
    Rational end = doc_rational(build, occurrence->end_numerator, occurrence->end_denominator);
    cJSON *claim = claim_json_with(build, framework, notes, occurrence->note_ids, occurrence->note_id_count, start, end,
                                   occurrence->kind, cJSON_CreateNumber(occurrence->uncertainty));
    if (!(claim && json_add(claim, "index", cJSON_CreateNumber((double)occurrence->index)) &&
          json_add(claim, "measures", measures_json(occurrence->first_measure, occurrence->last_measure)) &&
          json_add(claim, "beat", cJSON_CreateNumber(occurrence->first_beat)) &&
          json_add(claim, "score", cJSON_CreateNumber(occurrence->score)) &&
          json_add(claim, "mismatches", cJSON_CreateNumber(occurrence->mismatches)) &&
          json_add(claim, "intervals", cJSON_CreateNumber(occurrence->intervals)) &&
          json_add(claim, "fragment", string_or_null_json(occurrence->fragment)) &&
          json_add(claim, "in_continuation", cJSON_CreateBool(occurrence->in_continuation)) &&
          json_add(claim, "phrase", occurrence->has_phrase ? cJSON_CreateNumber((double)occurrence->phrase) : cJSON_CreateNull()) &&
          json_add(claim, "idea",
              occurrence->has_idea ? cJSON_CreateNumber(report->ideas[occurrence->idea].index) : cJSON_CreateNull()) &&
          (!occurrence->degrees ||
           json_add(claim, "degrees", strings_json(occurrence->degrees, occurrence->note_id_count))))) {
        cJSON_Delete(claim);
        return NULL;
    }
    return claim;
}

/* What a family is made of, read from its prototype: its notes (id, MIDI pitch, onset from the first note and duration
   as "n/d" whole notes), the key region's tonic and mode at the first note, the bar and beat there and the first
   note's position in its bar (pickup, "0/1" on a downbeat), the degrees of the notes against the tonic's major scale
   ("1 b2 2 b3 3 4 #4 5 b6 6 b7 7"), the semitone and scale-step intervals between neighbours, the rhythm (the durations),
   the non-chord-tone type of the segment each note starts in ("chord-tone" for a chord tone, null when the segment has
   no chord or the library left the tone unclassified) and the sketch's fingerprint (16 hex digits). */
static cJSON *motive_features_json(const AnalysisMotiveFeatures *features) {
    cJSON *object = cJSON_CreateObject();
    cJSON *notes = cJSON_CreateArray();
    cJSON *rhythm = cJSON_CreateArray();
    cJSON *intervals = cJSON_CreateArray();
    bool ok = object && notes && rhythm && intervals;
    for (size_t k = 0; ok && k < features->note_count; k++) {
        const AnalysisMotiveNote *note = &features->notes[k];
        cJSON *entry = cJSON_CreateObject();
        ok = entry && json_add(entry, "note_id", cJSON_CreateNumber((double)note->note_id)) &&
             json_add(entry, "pitch", cJSON_CreateNumber(note->pitch)) &&
             json_add(entry, "onset", fraction_string_json(note->onset_numerator, note->onset_denominator)) &&
             json_add(entry, "duration", fraction_string_json(note->duration_numerator, note->duration_denominator));
        ok = ok ? json_append(notes, entry) : (cJSON_Delete(entry), false);
        ok = ok && json_append(rhythm, fraction_string_json(note->duration_numerator, note->duration_denominator));
        if (ok && k > 0) {
            ok = json_append(intervals, cJSON_CreateNumber(features->intervals[k - 1]));
        }
    }
    size_t gaps = features->note_count > 0 ? features->note_count - 1 : 0;
    /* json_add() takes an item over, and frees it itself when it fails, so each is cleared once handed over */
    ok = ok && add_owned(object, "notes", &notes) &&
         json_add(object, "tonic_pc", cJSON_CreateNumber(features->tonic_pc)) &&
         json_add(object, "minor", cJSON_CreateBool(features->minor)) &&
         json_add(object, "bar", fraction_string_json(features->bar_numerator, features->bar_denominator)) &&
         json_add(object, "beat", fraction_string_json(features->beat_numerator, features->beat_denominator)) &&
         json_add(object, "pickup", fraction_string_json(features->pickup_numerator, features->pickup_denominator)) &&
         json_add(object, "degrees", strings_json(features->degrees, features->note_count)) &&
         add_owned(object, "intervals", &intervals) && json_add(object, "steps", numbers_json(features->steps, gaps)) &&
         add_owned(object, "rhythm", &rhythm) &&
         json_add(object, "ncts", strings_json((const char *const *)features->ncts, features->note_count)) &&
         json_add(object, "fingerprint", cJSON_CreateString(features->fingerprint));
    cJSON_Delete(notes);
    cJSON_Delete(rhythm);
    cJSON_Delete(intervals);
    if (!ok) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* The development of a family: a step per occurrence in time order. transformation is the library's kind of that
   occurrence against the prototype, null for the prototype itself (which is not always the earliest: the library
   searches continuations for fragments first). from is the occurrence before it in time (null for the first);
   occurrence and from are indices into the family's "occurrences". */
static cJSON *motive_development_json(const AnalysisMotive *motive) {
    cJSON *steps = cJSON_CreateArray();
    if (!steps) {
        return NULL;
    }
    for (size_t k = 0; k < motive->occurrence_count; k++) {
        size_t index = motive->development[k];
        const AnalysisMotiveOccurrence *occurrence = &motive->occurrences[index];
        cJSON *step = cJSON_CreateObject();
        if (!(step && json_add(step, "occurrence", cJSON_CreateNumber((double)index)) &&
              json_add(step, "measure", cJSON_CreateNumber(occurrence->first_measure)) &&
              json_add(step, "beat", cJSON_CreateNumber(occurrence->first_beat)) &&
              json_add(step, "transformation", index == 0 ? cJSON_CreateNull() : cJSON_CreateString(occurrence->kind)) &&
              json_add(step, "from", k == 0 ? cJSON_CreateNull() : cJSON_CreateNumber((double)motive->development[k - 1])))) {
            cJSON_Delete(step);
            cJSON_Delete(steps);
            return NULL;
        }
        if (!json_append(steps, step)) {
            cJSON_Delete(steps);
            return NULL;
        }
    }
    return steps;
}

/* One claim per motive family, in the library's order. Its span, note ids and location are the prototype's (the first
   occurrence); note_count is the prototype's length; generic says the pattern is the scale-step interval sequence
   rather than the semitones; uncertainty is the greatest of its occurrences'. occurrences are the claims of every
   occurrence, the prototype first and the rest in the library's order (not always time order); development is the
   family's occurrences in time order as steps. */
static cJSON *motives_json(DocBuild *build, const ImportedNotes *notes, const AnalysisReport *report) {
    cJSON *list = cJSON_CreateArray();
    if (!list) {
        return NULL;
    }
    const char *framework = framework_name(standard_form_framework());
    for (size_t i = 0; i < report->motive_count; i++) {
        const AnalysisMotive *motive = &report->motives[i];
        const AnalysisMotiveOccurrence *prototype = &motive->occurrences[0];
        double uncertainty = 0.0;
        for (size_t o = 0; o < motive->occurrence_count; o++) {
            if (motive->occurrences[o].uncertainty > uncertainty) {
                uncertainty = motive->occurrences[o].uncertainty;
            }
        }
        Rational start = doc_rational(build, prototype->start_numerator, prototype->start_denominator);
        Rational end = doc_rational(build, prototype->end_numerator, prototype->end_denominator);
        cJSON *claim = claim_json_with(build, framework, notes, prototype->note_ids, prototype->note_id_count, start, end,
                                       "motive", cJSON_CreateNumber(uncertainty));
        cJSON *occurrences = cJSON_CreateArray();
        bool ok = claim && occurrences;
        for (size_t o = 0; ok && o < motive->occurrence_count; o++) {
            ok = json_append(occurrences, motive_occurrence_json(build, notes, report, framework, &motive->occurrences[o]));
        }
        bool taken = false; /* json_add() takes the occurrences over, and frees them itself when it fails */
        if (!(ok && json_add(claim, "index", cJSON_CreateNumber((double)motive->index)) &&
              json_add(claim, "note_count", cJSON_CreateNumber(motive->note_count)) &&
              json_add(claim, "generic", cJSON_CreateBool(motive->generic)) &&
              (taken = true, json_add(claim, "occurrences", occurrences)) &&
              json_add(claim, "development", motive_development_json(motive)) &&
              (!motive->has_features || json_add(claim, "features", motive_features_json(&motive->features))))) {
            if (!taken) {
                cJSON_Delete(occurrences);
            }
            cJSON_Delete(claim);
            cJSON_Delete(list);
            return NULL;
        }
        if (!json_append(list, claim)) {
            cJSON_Delete(list);
            return NULL;
        }
    }
    return list;
}

/* ---- Melody ---- */

/* A pitch and the note it is the pitch of: { "pitch": MIDI note number, "name": "E5", "note_id": ImportedNotes
   index }. */
static cJSON *melody_pitch_json(long pitch, const char *name, size_t note) {
    cJSON *object = cJSON_CreateObject();
    if (object && !(json_add(object, "pitch", cJSON_CreateNumber((double)pitch)) &&
                    json_add(object, "name", cJSON_CreateString(name)) &&
                    json_add(object, "note_id", cJSON_CreateNumber((double)note)))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

static cJSON *melody_range_json(const AnalysisMelody *melody) {
    cJSON *object = cJSON_CreateObject();
    if (object && !(json_add(object, "lowest", melody_pitch_json(melody->lowest_pitch, melody->lowest_name, melody->lowest_note)) &&
                    json_add(object, "highest", melody_pitch_json(melody->highest_pitch, melody->highest_name, melody->highest_note)) &&
                    json_add(object, "semitones", cJSON_CreateNumber((double)melody->range_semitones)))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* The climax as a claim over its first note: the highest pitch, how many notes sound it, and where the first one
   stands (its place among the notes, its beat strength, whether a leap approaches it, whether it ends the phrase). */
static cJSON *melody_climax_json(DocBuild *build, const ImportedNotes *notes, const AnalysisMelody *melody) {
    Rational start;
    Rational end;
    notes_span(build, notes, &melody->climax_note, 1, &start, &end);
    cJSON *claim = claim_json_with(build, melody->framework, notes, &melody->climax_note, 1, start, end, "climax",
                                   cJSON_CreateNumber(melody->uncertainty));
    if (!(claim && json_add(claim, "pitch", cJSON_CreateNumber((double)melody->highest_pitch)) &&
          json_add(claim, "occurrences", cJSON_CreateNumber(melody->climax_occurrences)) &&
          json_add(claim, "unique", cJSON_CreateBool(melody->climax_unique)) &&
          json_add(claim, "relative_position", cJSON_CreateNumber(melody->climax_relative_position)) &&
          json_add(claim, "strength",
              melody->climax_has_strength ? cJSON_CreateString(melody->climax_strength) : cJSON_CreateNull()) &&
          json_add(claim, "by_leap", cJSON_CreateBool(melody->climax_by_leap)) &&
          json_add(claim, "final_note", cJSON_CreateBool(melody->climax_final)))) {
        cJSON_Delete(claim);
        return NULL;
    }
    return claim;
}

/* The contour: how many intervals go up, down and stay, how many are steps and leaps, the last note against the first,
   and the direction of each interval in time order. No shape is named. */
static cJSON *melody_contour_json(const AnalysisMelody *melody) {
    cJSON *object = cJSON_CreateObject();
    cJSON *directions = cJSON_CreateArray();
    bool ok = object && directions;
    for (size_t k = 0; ok && k < melody->direction_count; k++) {
        ok = json_append(directions, cJSON_CreateString(melody->directions[k]));
    }
    bool taken = false; /* json_add() takes the directions over, and frees them itself when it fails */
    if (!(ok && json_add(object, "up", cJSON_CreateNumber(melody->up)) &&
          json_add(object, "down", cJSON_CreateNumber(melody->down)) &&
          json_add(object, "same", cJSON_CreateNumber(melody->same)) &&
          json_add(object, "steps", cJSON_CreateNumber(melody->step_count)) &&
          json_add(object, "leaps", cJSON_CreateNumber(melody->leap_count)) &&
          json_add(object, "net_direction", cJSON_CreateString(melody->net_direction)) &&
          json_add(object, "net_semitones", cJSON_CreateNumber((double)melody->net_semitones)) &&
          (taken = true, json_add(object, "directions", directions)))) {
        if (!taken) {
            cJSON_Delete(directions);
        }
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* One leap as a claim over its two notes: the interval in semitones and in letter names (null when the library has no
   size), whether it was read from the semitones alone, and the library's verdict on its recovery. */
static cJSON *melody_leap_json(DocBuild *build, const ImportedNotes *notes, const char *framework, const AnalysisMelodyLeap *leap) {
    size_t ids[2] = {leap->from_note, leap->to_note};
    Rational start;
    Rational end;
    notes_span(build, notes, ids, 2, &start, &end);
    cJSON *claim = claim_json_with(build, framework, notes, ids, 2, start, end, "leap", cJSON_CreateNumber(leap->uncertainty));
    if (!(claim && json_add(claim, "semitones", cJSON_CreateNumber((double)leap->semitones)) &&
          json_add(claim, "direction", cJSON_CreateString(leap->direction)) &&
          json_add(claim, "generic_size", leap->generic_size > 0 ? cJSON_CreateNumber(leap->generic_size) : cJSON_CreateNull()) &&
          json_add(claim, "approximate", cJSON_CreateBool(leap->approximate)) &&
          json_add(claim, "status", cJSON_CreateString(leap->status)))) {
        cJSON_Delete(claim);
        return NULL;
    }
    return claim;
}

/* The leaps of a phrase: how many the library finds recovered, unrecovered, not required and not judged, and every
   leap as a claim in time order. */
static cJSON *melody_leaps_json(DocBuild *build, const ImportedNotes *notes, const AnalysisMelody *melody) {
    static const char *const STATUSES[4] = {"recovered", "unrecovered", "not-required", "not-judged"};
    static const char *const KEYS[4] = {"recovered", "unrecovered", "not_required", "not_judged"};
    cJSON *object = cJSON_CreateObject();
    cJSON *items = cJSON_CreateArray();
    bool ok = object && items;
    for (size_t k = 0; ok && k < 4; k++) {
        size_t count = 0;
        for (size_t i = 0; i < melody->leap_total; i++) {
            count += strcmp(melody->leaps[i].status, STATUSES[k]) == 0 ? 1 : 0;
        }
        ok = json_add(object, KEYS[k], cJSON_CreateNumber((double)count));
    }
    for (size_t i = 0; ok && i < melody->leap_total; i++) {
        ok = json_append(items, melody_leap_json(build, notes, melody->framework, &melody->leaps[i]));
    }
    bool taken = false; /* json_add() takes the items over, and frees them itself when it fails */
    if (!(ok && (taken = true, json_add(object, "items", items)))) {
        if (!taken) {
            cJSON_Delete(items);
        }
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* The pairs of leaps in a row the library's rule finds fault with, each a claim over its three notes. */
static cJSON *melody_consecutive_leaps_json(DocBuild *build, const ImportedNotes *notes, const AnalysisMelody *melody) {
    cJSON *list = cJSON_CreateArray();
    for (size_t i = 0; list && i < melody->consecutive_leap_count; i++) {
        const AnalysisMelodyConsecutiveLeaps *pair = &melody->consecutive_leaps[i];
        size_t ids[3] = {pair->first_note, pair->middle_note, pair->last_note};
        Rational start;
        Rational end;
        notes_span(build, notes, ids, 3, &start, &end);
        cJSON *claim = claim_json_with(build, melody->framework, notes, ids, 3, start, end, "consecutive-leaps",
                                       cJSON_CreateNumber(pair->uncertainty));
        if (!(claim && json_add(claim, "same_direction", cJSON_CreateBool(pair->same_direction)) &&
              json_add(claim, "three_in_a_row", cJSON_CreateBool(pair->three_in_a_row)) && json_append(list, claim))) {
            cJSON_Delete(claim);
            cJSON_Delete(list);
            return NULL;
        }
    }
    return list;
}

/* The non-chord tones of a phrase's melody: the library's counts (chord tones, non-chord tones, unclassified notes, and
   the non-chord tones by type, the types that occur in the library's order) and each non-chord tone as a claim over its
   note, labelled with the library's type. */
static cJSON *melody_non_chord_tones_json(DocBuild *build, const ImportedNotes *notes, const AnalysisMelody *melody) {
    cJSON *object = cJSON_CreateObject();
    cJSON *by_type = cJSON_CreateArray();
    cJSON *items = cJSON_CreateArray();
    bool ok = object && by_type && items;
    for (int type = 0; ok && type < ANALYSIS_MELODY_NCT_TYPES; type++) {
        if (melody->nct_by_type[type] > 0) {
            cJSON *entry = cJSON_CreateObject();
            ok = entry && json_add(entry, "type", cJSON_CreateString(melody->nct_type_names[type])) &&
                 json_add(entry, "count", cJSON_CreateNumber(melody->nct_by_type[type])) && json_append(by_type, entry);
            if (!ok) {
                cJSON_Delete(entry);
            }
        }
    }
    for (size_t i = 0; ok && i < melody->nct_total; i++) {
        const AnalysisMelodyNct *nct = &melody->ncts[i];
        Rational start;
        Rational end;
        notes_span(build, notes, &nct->note, 1, &start, &end);
        cJSON *claim = claim_json_with(build, melody->framework, notes, &nct->note, 1, start, end, nct->type,
                                       cJSON_CreateNumber(nct->uncertainty));
        ok = claim && json_append(items, claim);
        if (!ok) {
            cJSON_Delete(claim);
        }
    }
    bool by_type_taken = false; /* json_add() takes each array over, and frees it itself when it fails */
    bool items_taken = false;
    if (!(ok && json_add(object, "chord_tones", cJSON_CreateNumber(melody->chord_tone_count)) &&
          json_add(object, "non_chord_tones", cJSON_CreateNumber(melody->non_chord_tone_count)) &&
          json_add(object, "unclassified", cJSON_CreateNumber(melody->unclassified_count)) &&
          (by_type_taken = true, json_add(object, "by_type", by_type)) &&
          (items_taken = true, json_add(object, "items", items)))) {
        if (!by_type_taken) {
            cJSON_Delete(by_type);
        }
        if (!items_taken) {
            cJSON_Delete(items);
        }
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* The library's line tendencies as { "tendency": name, "holds": bool }, in its order; empty when the phrase's notes do not
   spell. */
static cJSON *melody_line_tendencies_json(const AnalysisMelody *melody) {
    cJSON *list = cJSON_CreateArray();
    for (int t = 0; list && t < ANALYSIS_MELODY_TENDENCIES && melody->tendency_names[t]; t++) {
        cJSON *entry = cJSON_CreateObject();
        if (!(entry && json_add(entry, "tendency", cJSON_CreateString(melody->tendency_names[t])) &&
              json_add(entry, "holds", cJSON_CreateBool(melody->tendency_holds[t])) && json_append(list, entry))) {
            cJSON_Delete(entry);
            cJSON_Delete(list);
            return NULL;
        }
    }
    return list;
}

/* The seventh degree: the notes the library labels degree 7 that a note follows, judged by its rule LEADING_TONE: how many
   followed it and how many broke it, with each broken one as a claim over the seventh and the note after it. Null when
   the phrase's notes do not spell. */
static cJSON *melody_seventh_degree_json(DocBuild *build, const ImportedNotes *notes, const AnalysisMelody *melody) {
    if (!melody->seventh_rule) {
        return cJSON_CreateNull();
    }
    cJSON *object = cJSON_CreateObject();
    cJSON *breaks = cJSON_CreateArray();
    bool ok = object && breaks;
    for (int i = 0; ok && i < melody->seventh_broken; i++) {
        const AnalysisMelodyBreak *broken = &melody->seventh_breaks[i];
        size_t ids[2] = {broken->note, broken->next_note};
        Rational start;
        Rational end;
        notes_span(build, notes, ids, 2, &start, &end);
        cJSON *claim = claim_json_with(build, melody->framework, notes, ids, 2, start, end, melody->seventh_rule,
                                       cJSON_CreateNumber(broken->uncertainty));
        ok = claim && json_append(breaks, claim);
        if (!ok) {
            cJSON_Delete(claim);
        }
    }
    bool taken = false; /* json_add() takes the breaks over, and frees them itself when it fails */
    if (!(ok && json_add(object, "rule", cJSON_CreateString(melody->seventh_rule)) &&
          json_add(object, "followed", cJSON_CreateNumber(melody->seventh_followed)) &&
          json_add(object, "broken", cJSON_CreateNumber(melody->seventh_broken)) &&
          (taken = true, json_add(object, "breaks", breaks)))) {
        if (!taken) {
            cJSON_Delete(breaks);
        }
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

static cJSON *melody_tendencies_json(DocBuild *build, const ImportedNotes *notes, const AnalysisMelody *melody) {
    cJSON *object = cJSON_CreateObject();
    if (object && !(json_add(object, "line", melody_line_tendencies_json(melody)) &&
                    json_add(object, "seventh_degree", melody_seventh_degree_json(build, notes, melody)))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* One claim per phrase with melody notes, in phrase order: the soprano notes that start in the phrase. index is the
   phrase's, measures its measures; range, climax, contour, leaps, consecutive leaps, non-chord tones and tendencies are
   the melody adapter's and the library's rules' results over the notes. */
static cJSON *melody_json(DocBuild *build, const ImportedNotes *notes, const AnalysisReport *report) {
    cJSON *list = cJSON_CreateArray();
    if (!list) {
        return NULL;
    }
    for (size_t i = 0; i < report->melody_count; i++) {
        const AnalysisMelody *melody = &report->melodies[i];
        Rational start = doc_rational(build, melody->start_numerator, melody->start_denominator);
        Rational end = doc_rational(build, melody->end_numerator, melody->end_denominator);
        cJSON *claim = claim_json_with(build, melody->framework, notes, melody->note_ids, melody->note_id_count, start, end,
                                       "melody", cJSON_CreateNumber(melody->uncertainty));
        if (!(claim && json_add(claim, "index", cJSON_CreateNumber((double)melody->phrase)) &&
              json_add(claim, "measures", measures_json(melody->first_measure, melody->last_measure)) &&
              json_add(claim, "note_count", cJSON_CreateNumber((double)melody->note_id_count)) &&
              json_add(claim, "range", melody_range_json(melody)) && json_add(claim, "climax", melody_climax_json(build, notes, melody)) &&
              json_add(claim, "contour", melody_contour_json(melody)) &&
              json_add(claim, "leaps", melody_leaps_json(build, notes, melody)) &&
              json_add(claim, "consecutive_leaps", melody_consecutive_leaps_json(build, notes, melody)) &&
              json_add(claim, "non_chord_tones", melody_non_chord_tones_json(build, notes, melody)) &&
              json_add(claim, "tendencies", melody_tendencies_json(build, notes, melody)))) {
            cJSON_Delete(claim);
            cJSON_Delete(list);
            return NULL;
        }
        if (!json_append(list, claim)) {
            cJSON_Delete(list);
            return NULL;
        }
    }
    return list;
}

/* ---- Rhythm and meter ---- */

/* The line a density profile is of, as the document names it: "all" for every note of the phrase, then the outer-voice
   soprano and bass, and "other" for the rest. */
static const char *rhythm_line_name(int line) {
    switch (line) {
        case 0:
            return "soprano";
        case 1:
            return "bass";
        case 2:
            return "other";
        default:
            return "all";
    }
}

static cJSON *density_durations_json(DocBuild *build, const AnalysisDensity *density) {
    cJSON *list = cJSON_CreateArray();
    for (unsigned b = 0; list && b < density->bin_count; b++) {
        cJSON *bin = cJSON_CreateObject();
        if (!(bin &&
              json_add(bin, "duration",
                       rational_json(doc_rational(build, density->bins[b].numerator, density->bins[b].denominator))) &&
              json_add(bin, "count", cJSON_CreateNumber((double)density->bins[b].count)) && json_append(list, bin))) {
            cJSON_Delete(bin);
            cJSON_Delete(list);
            return NULL;
        }
    }
    return list;
}

static cJSON *density_per_beat_json(const AnalysisDensity *density) {
    cJSON *object = cJSON_CreateObject();
    if (object && !(json_add(object, "mean", cJSON_CreateNumber(density->attacks_per_beat_mean)) &&
                    json_add(object, "max", cJSON_CreateNumber(density->attacks_per_beat_max)))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* A density claim: the library's profile of a phrase's notes (all of them, or one line's), over the phrase's span. */
static cJSON *density_json(DocBuild *build, const ImportedNotes *notes, const AnalysisReport *report, const AnalysisDensity *density) {
    const AnalysisPhrase *phrase = &report->phrases[density->phrase];
    Rational start = doc_rational(build, phrase->start_numerator, phrase->start_denominator);
    Rational end = doc_rational(build, phrase->end_numerator, phrase->end_denominator);
    cJSON *claim = claim_json_with(build, density->framework, notes, density->note_ids, density->note_id_count, start, end,
                                   "density", cJSON_CreateNumber(density->uncertainty));
    if (!(claim && json_add(claim, "phrase", cJSON_CreateNumber((double)density->phrase)) &&
          json_add(claim, "measures", measures_json(phrase->first_measure, phrase->last_measure)) &&
          json_add(claim, "line", cJSON_CreateString(rhythm_line_name(density->line))) &&
          json_add(claim, "notes", cJSON_CreateNumber((double)density->notes)) &&
          json_add(claim, "attacks", cJSON_CreateNumber((double)density->attacks)) &&
          json_add(claim, "measure_count", cJSON_CreateNumber(density->measures)) &&
          json_add(claim, "attacks_per_beat", density_per_beat_json(density)) &&
          json_add(claim, "mean_inter_onset", cJSON_CreateNumber(density->mean_inter_onset)) &&
          json_add(claim, "on_beat_share", cJSON_CreateNumber(density->on_beat_share)) &&
          json_add(claim, "off_beat_share", cJSON_CreateNumber(density->off_beat_share)) &&
          json_add(claim, "durations", density_durations_json(build, density)) &&
          json_add(claim, "durations_truncated", cJSON_CreateBool(density->bins_truncated)))) {
        cJSON_Delete(claim);
        return NULL;
    }
    return claim;
}

static cJSON *syncopation_through_json(const AnalysisSyncopation *syncopation) {
    cJSON *object = cJSON_CreateObject();
    if (object && !(json_add(object, "level", cJSON_CreateString(syncopation->through_level)) &&
                    json_add(object, "measure", cJSON_CreateNumber(syncopation->through_measure)) &&
                    json_add(object, "beat", cJSON_CreateNumber(syncopation->through_beat)))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* A syncopation claim: one note, attacked at a weaker level and held through a stronger one that no note of its line
   attacks. */
static cJSON *syncopation_json(DocBuild *build, const ImportedNotes *notes, const AnalysisSyncopation *syncopation) {
    Rational start;
    Rational end;
    notes_span(build, notes, &syncopation->note, 1, &start, &end);
    cJSON *claim = claim_json_with(build, syncopation->framework, notes, &syncopation->note, 1, start, end, "syncopation",
                                   cJSON_CreateNumber(syncopation->uncertainty));
    if (!(claim && json_add(claim, "line", cJSON_CreateString(rhythm_line_name(syncopation->line))) &&
          json_add(claim, "measure", cJSON_CreateNumber(syncopation->measure)) &&
          json_add(claim, "beat", cJSON_CreateNumber(syncopation->beat)) &&
          json_add(claim, "attack_level", cJSON_CreateString(syncopation->attack_level)) &&
          json_add(claim, "strength", cJSON_CreateNumber((double)syncopation->strength)) &&
          json_add(claim, "held_through", syncopation_through_json(syncopation)))) {
        cJSON_Delete(claim);
        return NULL;
    }
    return claim;
}

/* An evidence value of a hemiola: true when it agrees, false when it does not, null when it is unavailable. */
static cJSON *evidence_json(int evidence) {
    return evidence < 0 ? cJSON_CreateNull() : cJSON_CreateBool(evidence != 0);
}

static cJSON *hemiola_evidence_json(const AnalysisHemiola *hemiola) {
    cJSON *object = cJSON_CreateObject();
    if (object && !(json_add(object, "duration", evidence_json(hemiola->duration_evidence)) &&
                    json_add(object, "chord", evidence_json(hemiola->chord_evidence)) &&
                    json_add(object, "accent", evidence_json(hemiola->accent_evidence)) &&
                    json_add(object, "agreeing", cJSON_CreateNumber((double)hemiola->agreeing)) &&
                    json_add(object, "available", cJSON_CreateNumber((double)hemiola->available)))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

static cJSON *hemiola_json(DocBuild *build, const ImportedNotes *notes, const AnalysisHemiola *hemiola) {
    Rational start;
    Rational end;
    notes_span(build, notes, hemiola->note_ids, hemiola->note_id_count, &start, &end);
    cJSON *claim = claim_json_with(build, hemiola->framework, notes, hemiola->note_ids, hemiola->note_id_count, start, end,
                                   "hemiola", cJSON_CreateNumber(hemiola->uncertainty));
    if (!(claim && json_add(claim, "measures", measures_json(hemiola->first_measure, hemiola->last_measure)) &&
          json_add(claim, "evidence", hemiola_evidence_json(hemiola)))) {
        cJSON_Delete(claim);
        return NULL;
    }
    return claim;
}

static cJSON *hypermeasures_json(const AnalysisHypermeter *hypermeter) {
    cJSON *list = cJSON_CreateArray();
    for (size_t h = 0; list && h < hypermeter->hypermeasure_count; h++) {
        const AnalysisHypermeasure *hypermeasure = &hypermeter->hypermeasures[h];
        cJSON *item = cJSON_CreateObject();
        if (!(item && json_add(item, "index", cJSON_CreateNumber(hypermeasure->index)) &&
              json_add(item, "measures", measures_json(hypermeasure->first_measure, hypermeasure->last_measure)) &&
              json_add(item, "kind", cJSON_CreateString(hypermeasure->kind)) &&
              json_add(item, "extra_hyperbeats", cJSON_CreateNumber(hypermeasure->extra_hyperbeats)) &&
              json_add(item, "contains_cadence", cJSON_CreateBool(hypermeasure->contains_cadence)) &&
              json_add(item, "score", cJSON_CreateNumber(hypermeasure->score)) &&
              json_add(item, "uncertainty", cJSON_CreateNumber(hypermeasure->uncertainty)) && json_append(list, item))) {
            cJSON_Delete(item);
            cJSON_Delete(list);
            return NULL;
        }
    }
    return list;
}

/* The first measure of every hypermeasure of the phrase: its hyperdownbeats. */
static cJSON *hyperdownbeats_json(const AnalysisHypermeter *hypermeter) {
    cJSON *list = cJSON_CreateArray();
    for (size_t h = 0; list && h < hypermeter->hypermeasure_count; h++) {
        if (!json_append(list, cJSON_CreateNumber(hypermeter->hypermeasures[h].first_measure))) {
            cJSON_Delete(list);
            return NULL;
        }
    }
    return list;
}

/* A hypermeter claim: one phrase read in hypermeasures of two or four measures. */
static cJSON *hypermeter_json(DocBuild *build, const ImportedNotes *notes, const AnalysisReport *report,
                              const AnalysisHypermeter *hypermeter) {
    const AnalysisPhrase *phrase = &report->phrases[hypermeter->phrase];
    Rational start = doc_rational(build, phrase->start_numerator, phrase->start_denominator);
    Rational end = doc_rational(build, phrase->end_numerator, phrase->end_denominator);
    cJSON *claim = claim_json_with(build, hypermeter->framework, notes, hypermeter->note_ids, hypermeter->note_id_count,
                                   start, end, "hypermeter", cJSON_CreateNumber(1.0 - hypermeter->score));
    if (!(claim && json_add(claim, "phrase", cJSON_CreateNumber((double)hypermeter->phrase)) &&
          json_add(claim, "measures",
              measures_json(hypermeter->first_measure, hypermeter->first_measure + hypermeter->measures - 1)) &&
          json_add(claim, "length", cJSON_CreateNumber((double)hypermeter->length)) &&
          json_add(claim, "score", cJSON_CreateNumber(hypermeter->score)) &&
          json_add(claim, "arrival_measure",
              hypermeter->has_arrival ? cJSON_CreateNumber(hypermeter->arrival_measure) : cJSON_CreateNull()) &&
          json_add(claim, "hyperdownbeats", hyperdownbeats_json(hypermeter)) &&
          json_add(claim, "hypermeasures", hypermeasures_json(hypermeter)))) {
        cJSON_Delete(claim);
        return NULL;
    }
    return claim;
}

static cJSON *tuplet_ratio_json(const AnalysisTuplet *tuplet) {
    cJSON *object = cJSON_CreateObject();
    if (object && !(json_add(object, "actual", cJSON_CreateNumber((double)tuplet->actual)) &&
                    json_add(object, "normal", cJSON_CreateNumber((double)tuplet->normal)))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* A tuplet claim: a run of notes that divide the beat the way the meter does not. The library gives it no uncertainty
   (the borrowed-division rules are rules, not a detector), so the claim's is null. */
static cJSON *tuplet_json(DocBuild *build, const ImportedNotes *notes, const AnalysisTuplet *tuplet) {
    Rational start;
    Rational end;
    notes_span(build, notes, tuplet->note_ids, tuplet->note_id_count, &start, &end);
    cJSON *claim = claim_json_with(build, tuplet->framework, notes, tuplet->note_ids, tuplet->note_id_count, start, end,
                                   "tuplet", cJSON_CreateNull());
    if (!(claim && json_add(claim, "ratio", tuplet_ratio_json(tuplet)) && json_add(claim, "name", cJSON_CreateString(tuplet->name)) &&
          json_add(claim, "meter", cJSON_CreateString(tuplet->meter)) &&
          json_add(claim, "borrowed", cJSON_CreateBool(tuplet->borrowed)) &&
          json_add(claim, "level", tuplet->has_level ? cJSON_CreateString(tuplet->level) : cJSON_CreateNull()) &&
          json_add(claim, "level_typical", tuplet->has_level ? cJSON_CreateBool(tuplet->level_typical) : cJSON_CreateNull()) &&
          json_add(claim, "measure", cJSON_CreateNumber(tuplet->measure)) &&
          json_add(claim, "beat", cJSON_CreateNumber(tuplet->beat)) &&
          json_add(claim, "attacks", cJSON_CreateNumber((double)tuplet->attacks)))) {
        cJSON_Delete(claim);
        return NULL;
    }
    return claim;
}

/* A meter reading of the meter check: the meter, the scale and the pickup as a notation reading states them. */
static cJSON *meter_reading_json(DocBuild *build, const AnalysisMeterReading *reading) {
    cJSON *object = cJSON_CreateObject();
    cJSON *meter = cJSON_CreateObject();
    bool ok = object && meter && json_add(meter, "numerator", cJSON_CreateNumber(reading->beats)) &&
              json_add(meter, "denominator", cJSON_CreateNumber(reading->beat_type));
    if (ok) {
        ok = json_add(object, "meter", meter); /* attached, or freed by add */
        meter = NULL;
    }
    ok = ok &&
         json_add(object, "scale", rational_json(doc_rational(build, reading->scale_numerator, reading->scale_denominator))) &&
         json_add(object, "pickup",
                  rational_json(doc_rational(build, reading->pickup_numerator, reading->pickup_denominator))) &&
         json_add(object, "score", cJSON_CreateNumber(reading->score));
    if (!ok) {
        cJSON_Delete(meter);
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* The meter-fit claim: the meter the analysis used against the best other reading, which is null unless it beats the
   effective one by the stage's margin. Over the whole piece, with no notes of its own. */
static cJSON *meter_fit_json(DocBuild *build, const ImportedNotes *notes, const AnalysisMeterCheck *check) {
    Rational start;
    Rational end;
    notes_span(build, notes, NULL, notes->count, &start, &end);
    cJSON *claim = claim_json_with(build, "standard-meter-v1", notes, NULL, 0, start, end, "meter-fit",
                                   cJSON_CreateNumber(check->uncertainty));
    if (!(claim && json_add(claim, "declared", meter_reading_json(build, &check->effective)) &&
          json_add(claim, "alternative",
              check->has_alternative ? meter_reading_json(build, &check->other) : cJSON_CreateNull()))) {
        cJSON_Delete(claim);
        return NULL;
    }
    return claim;
}

/* The rhythm section: a list of claims, each labelled by the result it is ("meter-fit", "density", "syncopation",
   "hemiola", "hypermeter", "tuplet"): the meter fit first, then the densities and each kind in time order. The library's
   results are claims over the notes of a phrase or a measure, so the list is flat and the label says which kind. A
   piece without phrases has only the meter fit, or nothing when it has no chord segments. */
static cJSON *rhythm_json(DocBuild *build, const ImportedNotes *notes, const AnalysisReport *report) {
    cJSON *list = cJSON_CreateArray();
    if (list && report->meter_check.present && !json_append(list, meter_fit_json(build, notes, &report->meter_check))) {
        cJSON_Delete(list);
        return NULL;
    }
    for (size_t i = 0; list && i < report->density_count; i++) {
        if (!json_append(list, density_json(build, notes, report, &report->densities[i]))) {
            cJSON_Delete(list);
            return NULL;
        }
    }
    for (size_t i = 0; list && i < report->syncopation_count; i++) {
        if (!json_append(list, syncopation_json(build, notes, &report->syncopations[i]))) {
            cJSON_Delete(list);
            return NULL;
        }
    }
    for (size_t i = 0; list && i < report->hemiola_count; i++) {
        if (!json_append(list, hemiola_json(build, notes, &report->hemiolas[i]))) {
            cJSON_Delete(list);
            return NULL;
        }
    }
    for (size_t i = 0; list && i < report->hypermeter_count; i++) {
        if (!json_append(list, hypermeter_json(build, notes, report, &report->hypermeters[i]))) {
            cJSON_Delete(list);
            return NULL;
        }
    }
    for (size_t i = 0; list && i < report->tuplet_count; i++) {
        if (!json_append(list, tuplet_json(build, notes, &report->tuplets[i]))) {
            cJSON_Delete(list);
            return NULL;
        }
    }
    return list;
}

/* ---- Texture ---- */

/* The measured features of a texture passage: the library's means over its measures, the voice count the largest. */
static cJSON *texture_features_json(const AnalysisTexturePassage *passage) {
    cJSON *object = cJSON_CreateObject();
    if (object && !(json_add(object, "voices_max", cJSON_CreateNumber(passage->voices_max)) &&
                    json_add(object, "voices_mean", cJSON_CreateNumber(passage->voices_mean)) &&
                    json_add(object, "synchrony", cJSON_CreateNumber(passage->synchrony)) &&
                    json_add(object, "independence", cJSON_CreateNumber(passage->independence)) &&
                    json_add(object, "prominence", cJSON_CreateNumber(passage->prominence)) &&
                    json_add(object, "top_gap", cJSON_CreateNumber(passage->top_gap)) &&
                    json_add(object, "register_span", cJSON_CreateNumber(passage->register_span)) &&
                    json_add(object, "spacing", cJSON_CreateNumber(passage->spacing)) &&
                    json_add(object, "density", cJSON_CreateNumber(passage->density)) &&
                    json_add(object, "pattern", cJSON_CreateNumber(passage->pattern)))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

static cJSON *texture_confidence_json(const AnalysisTexturePassage *passage) {
    cJSON *object = cJSON_CreateObject();
    if (object && !(json_add(object, "texture", cJSON_CreateNumber(passage->texture_confidence)) &&
                    json_add(object, "voices", cJSON_CreateNumber(passage->voice_confidence)) &&
                    json_add(object, "overall", cJSON_CreateNumber(passage->confidence)))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

static Rational texture_passage_start(DocBuild *build, const AnalysisTexturePassage *passage) {
    return doc_rational(build, passage->start_numerator, passage->start_denominator);
}

static Rational texture_passage_end(DocBuild *build, const AnalysisTexturePassage *passage) {
    return doc_rational(build, passage->end_numerator, passage->end_denominator);
}

/* A texture passage claim: a run of measures with the same texture, as the library classifies it. */
static cJSON *texture_passage_json(DocBuild *build, const ImportedNotes *notes, const AnalysisTexturePassage *passage) {
    cJSON *claim = claim_json_with(build, passage->framework, notes, passage->note_ids, passage->note_id_count,
                                   texture_passage_start(build, passage), texture_passage_end(build, passage), "passage",
                                   cJSON_CreateNumber(passage->uncertainty));
    if (!(claim && json_add(claim, "texture", cJSON_CreateString(passage->texture)) &&
          json_add(claim, "category", cJSON_CreateString(passage->category)) &&
          json_add(claim, "measures", measures_json(passage->first_measure, passage->last_measure)) &&
          json_add(claim, "windows", cJSON_CreateNumber(passage->windows)) &&
          json_add(claim, "unclassified_windows", cJSON_CreateNumber(passage->unclassified_windows)) &&
          json_add(claim, "confidence", texture_confidence_json(passage)) &&
          json_add(claim, "features", texture_features_json(passage)))) {
        cJSON_Delete(claim);
        return NULL;
    }
    return claim;
}

/* A texture change claim: the boundary between two consecutive passages, spanning the passage changed to. */
static cJSON *texture_change_json(DocBuild *build, const ImportedNotes *notes, const AnalysisReport *report,
                                  const AnalysisTextureChange *change) {
    const AnalysisTexturePassage *to = &report->texture_passages[change->to];
    cJSON *claim = claim_json_with(build, change->framework, notes, NULL, 0, texture_passage_start(build, to), texture_passage_end(build, to),
                                   "change", cJSON_CreateNumber(change->uncertainty));
    if (!(claim && json_add(claim, "from", cJSON_CreateNumber((double)change->from)) &&
          json_add(claim, "to", cJSON_CreateNumber((double)change->to)) &&
          json_add(claim, "boundary_measure", cJSON_CreateNumber(change->boundary_measure)) &&
          json_add(claim, "from_texture", cJSON_CreateString(change->from_texture)) &&
          json_add(claim, "to_texture", cJSON_CreateString(change->to_texture)) &&
          json_add(claim, "category_changed", cJSON_CreateBool(change->category_changed)))) {
        cJSON_Delete(claim);
        return NULL;
    }
    return claim;
}

/* The texture section: a list of claims, the passages in time order (labelled "passage") and then the changes between
   them (labelled "change"). Empty when the library found no texture. */
static cJSON *texture_json(DocBuild *build, const ImportedNotes *notes, const AnalysisReport *report) {
    cJSON *list = cJSON_CreateArray();
    for (size_t i = 0; list && i < report->texture_passage_count; i++) {
        if (!json_append(list, texture_passage_json(build, notes, &report->texture_passages[i]))) {
            cJSON_Delete(list);
            return NULL;
        }
    }
    for (size_t i = 0; list && i < report->texture_change_count; i++) {
        if (!json_append(list, texture_change_json(build, notes, report, &report->texture_changes[i]))) {
            cJSON_Delete(list);
            return NULL;
        }
    }
    return list;
}

/* ---- Markings ---- */

/* The metronome mark of a tempo claim: the beat unit in whole notes and the beats per minute, both rational. */
static cJSON *metronome_json(DocBuild *build, const AnalysisTempoMarking *marking) {
    if (!marking->has_metronome) {
        return cJSON_CreateNull();
    }
    cJSON *object = cJSON_CreateObject();
    if (object &&
        !(json_add(
              object, "beat_unit",
              rational_json(doc_rational(build, (long long)marking->beat_unit_numerator, marking->beat_unit_denominator))) &&
          json_add(object, "per_minute",
                   rational_json(doc_rational(build, (long long)marking->bpm_numerator, marking->bpm_denominator))))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

static cJSON *tempo_marking_json(DocBuild *build, const ImportedNotes *notes, const AnalysisTempoMarking *marking) {
    cJSON *claim = claim_json_with(build, marking->framework, notes, NULL, 0,
                                   doc_rational(build, marking->start_numerator, marking->start_denominator),
                                   doc_rational(build, marking->end_numerator, marking->end_denominator), "tempo",
                                   cJSON_CreateNumber(marking->uncertainty));
    if (!(claim && json_add(claim, "text", optional_string_json(marking->text)) &&
          json_add(claim, "metronome", metronome_json(build, marking)) &&
          json_add(claim, "part", cJSON_CreateNumber((double)marking->part)) &&
          json_add(claim, "rank", marking->has_rank ? cJSON_CreateNumber(marking->rank) : cJSON_CreateNull()) &&
          json_add(claim, "group", optional_string_json(marking->group)) &&
          json_add(claim, "change", cJSON_CreateString(marking->change)))) {
        cJSON_Delete(claim);
        return NULL;
    }
    return claim;
}

static cJSON *dynamic_json(DocBuild *build, const ImportedNotes *notes, const AnalysisDynamic *dynamic) {
    cJSON *claim = claim_json_with(build, dynamic->framework, notes, NULL, 0,
                                   doc_rational(build, dynamic->start_numerator, dynamic->start_denominator),
                                   doc_rational(build, dynamic->end_numerator, dynamic->end_denominator), "dynamic",
                                   cJSON_CreateNumber(dynamic->uncertainty));
    if (!(claim && json_add(claim, "marking", cJSON_CreateString(dynamic->marking)) &&
          json_add(claim, "part", cJSON_CreateNumber((double)dynamic->part)) &&
          json_add(claim, "staff", cJSON_CreateNumber(dynamic->staff)) &&
          json_add(claim, "rank", dynamic->has_rank ? cJSON_CreateNumber(dynamic->rank) : cJSON_CreateNull()) &&
          json_add(claim, "change", cJSON_CreateString(dynamic->change)) &&
          json_add(claim, "relation", optional_string_json(dynamic->relation)))) {
        cJSON_Delete(claim);
        return NULL;
    }
    return claim;
}

/* The markings section: what the notated input carries, read with standard-markings' rules. MIDI input has none
   ("none"). The importer reads no articulations (R-056), so they are null and say so. */
static cJSON *markings_json(DocBuild *build, const ImportedNotes *notes, const AnalysisReport *report) {
    bool notated = notes->source.format == INPUT_FORMAT_MUSICXML;
    cJSON *object = cJSON_CreateObject();
    cJSON *tempo = cJSON_CreateArray();
    cJSON *dynamics = cJSON_CreateArray();
    bool ok = object && tempo && dynamics;
    for (size_t i = 0; ok && i < report->tempo_marking_count; i++) {
        ok = json_append(tempo, tempo_marking_json(build, notes, &report->tempo_markings[i]));
    }
    for (size_t i = 0; ok && i < report->dynamic_count; i++) {
        ok = json_append(dynamics, dynamic_json(build, notes, &report->dynamics[i]));
    }
    ok = ok && json_add(object, "source", cJSON_CreateString(notated ? "notated" : "none"));
    if (ok) {
        ok = json_add(object, "tempo", tempo); /* json_add() frees the item itself when it fails */
        tempo = NULL;
    }
    if (ok) {
        ok = json_add(object, "dynamics", dynamics);
        dynamics = NULL;
    }
    ok = ok && json_add(object, "articulations", cJSON_CreateNull()) &&
         json_add(object, "articulations_imported", cJSON_CreateBool(false));
    if (!ok) {
        cJSON_Delete(tempo);
        cJSON_Delete(dynamics);
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* ---- Counterpoint check ---- */

/* One rule group of the species the exercise breaks, a claim over the whole exercise: the library's aggregate check
   names the group and whether a hard or a soft rule broke in it, and does not say where. */
static cJSON *counterpoint_violation_json(DocBuild *build, const ImportedNotes *notes, const AnalysisCounterpointCheck *check,
                                          const AnalysisCounterpointViolation *violation) {
    cJSON *claim = claim_json_with(build, check->framework, notes, check->note_ids, check->note_id_count,
                                   doc_rational(build, check->start_numerator, check->start_denominator),
                                   doc_rational(build, check->end_numerator, check->end_denominator), violation->group,
                                   cJSON_CreateNumber(check->uncertainty));
    if (!(claim && json_add(claim, "group", cJSON_CreateString(violation->group)) &&
          json_add(claim, "severity", cJSON_CreateString(violation->hard ? "hard" : "soft")))) {
        cJSON_Delete(claim);
        return NULL;
    }
    return claim;
}

/* The counterpoint check: null unless --check-counterpoint ran on two voices. valid is null when the voices are not an
   exercise of the species (shape_error says why), and the violations are then empty. */
static cJSON *counterpoint_check_json(DocBuild *build, const ImportedNotes *notes, const AnalysisReport *report) {
    const AnalysisCounterpointCheck *check = &report->counterpoint_check;
    if (!check->present) {
        return cJSON_CreateNull();
    }
    cJSON *object = cJSON_CreateObject();
    cJSON *violations = cJSON_CreateArray();
    bool ok = object && violations;
    for (size_t i = 0; ok && i < check->violation_count; i++) {
        ok = json_append(violations, counterpoint_violation_json(build, notes, check, &check->violations[i]));
    }
    ok = ok && json_add(object, "species", cJSON_CreateNumber(check->species)) &&
         json_add(object, "cantus", cJSON_CreateString(check->cantus_upper ? "upper" : "lower")) &&
         json_add(object, "framework", cJSON_CreateString(check->framework)) &&
         json_add(object, "shape_error", optional_string_json(check->shape_error)) &&
         json_add(object, "valid", check->shape_error ? cJSON_CreateNull() : cJSON_CreateBool(check->valid));
    if (ok) {
        ok = json_add(object, "violations", violations); /* json_add() frees the item itself when it fails */
        violations = NULL;
    }
    if (!ok) {
        cJSON_Delete(violations);
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* ---- Voice leading ---- */

static const char *const MOTION_NAMES[ANALYSIS_MOTION_TYPES] = {"contrary", "similar", "parallel", "oblique", "static"};

static cJSON *motion_counts_json(const size_t *counts) {
    cJSON *object = cJSON_CreateObject();
    for (size_t type = 0; object && type < ANALYSIS_MOTION_TYPES; type++) {
        if (!json_add(object, MOTION_NAMES[type], cJSON_CreateNumber((double)counts[type]))) {
            cJSON_Delete(object);
            return NULL;
        }
    }
    return object;
}

static cJSON *motion_pairs_json(const AnalysisVoiceLeading *voice_leading) {
    cJSON *list = cJSON_CreateArray();
    for (size_t i = 0; list && i < voice_leading->pair_count; i++) {
        const AnalysisVoicePairMotion *pair = &voice_leading->pairs[i];
        cJSON *object = cJSON_CreateObject();
        if (!(object && json_add(object, "upper_voice", cJSON_CreateNumber((double)pair->upper_voice)) &&
              json_add(object, "lower_voice", cJSON_CreateNumber((double)pair->lower_voice)) &&
              json_add(object, "total", cJSON_CreateNumber((double)pair->total)) &&
              json_add(object, "counts", motion_counts_json(pair->counts)) && json_append(list, object))) {
            cJSON_Delete(object);
            cJSON_Delete(list);
            return NULL;
        }
    }
    return list;
}

/* The start of the first entry and the end of the last: the span the voice leading covers. */
static void voice_leading_span(DocBuild *build, const AnalysisReport *report, Rational *start, Rational *end) {
    const AnalysisReportEntry *first = &report->entries[0];
    const AnalysisReportEntry *last = &report->entries[report->entry_count - 1];
    *start = doc_rational(build, first->time_numerator, first->time_denominator);
    *end = doc_rational(build, last->end_numerator, last->end_denominator);
}

/* The motion claim: how the voices move against each other between consecutive chord segments, in total and per pair of
   voices. It covers the whole piece. */
static cJSON *voice_motions_json(DocBuild *build, const ImportedNotes *notes, const AnalysisReport *report) {
    const AnalysisVoiceLeading *voice_leading = &report->voice_leading;
    Rational start;
    Rational end;
    voice_leading_span(build, report, &start, &end);
    cJSON *claim = claim_json_with(build, voice_leading->framework, notes, NULL, 0, start, end, "motions",
                                   cJSON_CreateNumber(voice_leading->motion_uncertainty));
    if (!(claim && json_add(claim, "voices", cJSON_CreateNumber((double)voice_leading->voice_count)) &&
          json_add(claim, "segments", cJSON_CreateNumber((double)voice_leading->row_count)) &&
          json_add(claim, "motion_count", cJSON_CreateNumber((double)voice_leading->motion_count)) &&
          json_add(claim, "totals", motion_counts_json(voice_leading->motion_totals)) &&
          json_add(claim, "pairs", motion_pairs_json(voice_leading)))) {
        cJSON_Delete(claim);
        return NULL;
    }
    return claim;
}

/* The span from the start of one entry to the end of a later one. */
static void voice_leading_entries_span(DocBuild *build, const AnalysisReport *report, size_t from, size_t to, Rational *start,
                                       Rational *end) {
    const AnalysisReportEntry *first = &report->entries[from];
    const AnalysisReportEntry *last = &report->entries[to];
    *start = doc_rational(build, first->time_numerator, first->time_denominator);
    *end = doc_rational(build, last->end_numerator, last->end_denominator);
}

/* A fault of the voices' leading, as the claim for the move between two consecutive chords. */
static cJSON *voice_violation_json(DocBuild *build, const ImportedNotes *notes, const AnalysisReport *report,
                                   const AnalysisVoiceLeading *voice_leading, const AnalysisVoiceViolation *violation) {
    Rational start;
    Rational end;
    voice_leading_entries_span(build, report, violation->entry_from, violation->entry_to, &start, &end);
    cJSON *claim = claim_json_with(build, voice_leading->framework, notes, violation->note_ids, 4, start, end, "violation",
                                   cJSON_CreateNumber(violation->uncertainty));
    if (!(claim && json_add(claim, "rule", cJSON_CreateString(violation->rule)) &&
          json_add(claim, "upper_voice", cJSON_CreateNumber((double)violation->upper_voice)) &&
          json_add(claim, "lower_voice", cJSON_CreateNumber((double)violation->lower_voice)) &&
          json_add(claim, "interval", cJSON_CreateNumber(violation->arrival_interval)) &&
          json_add(claim, "from_chord", cJSON_CreateNumber((double)violation->entry_from)) &&
          json_add(claim, "to_chord", cJSON_CreateNumber((double)violation->entry_to)))) {
        cJSON_Delete(claim);
        return NULL;
    }
    return claim;
}

/* A tendency tone left unresolved when its chord changes. */
static cJSON *voice_resolution_json(DocBuild *build, const ImportedNotes *notes, const AnalysisReport *report,
                                    const AnalysisVoiceLeading *voice_leading,
                                    const AnalysisVoiceResolution *resolution) {
    Rational start;
    Rational end;
    voice_leading_entries_span(build, report, resolution->entry_from, resolution->entry_to, &start, &end);
    cJSON *claim = claim_json_with(build, framework_name(standard_harmony_framework()), notes, resolution->note_ids, 2, start, end, "resolution",
                                   cJSON_CreateNumber(resolution->uncertainty));
    if (!(claim && json_add(claim, "rule", cJSON_CreateString(resolution->rule)) &&
          json_add(claim, "voice", cJSON_CreateNumber((double)resolution->voice)) &&
          json_add(claim, "from_chord", cJSON_CreateNumber((double)resolution->entry_from)) &&
          json_add(claim, "to_chord", cJSON_CreateNumber((double)resolution->entry_to)))) {
        cJSON_Delete(claim);
        return NULL;
    }
    return claim;
}

static const char *const CHORD_MEMBER_NAMES[ANALYSIS_CHORD_MEMBERS] = {"root", "third", "fifth", "seventh"};

static cJSON *chord_gaps_json(const AnalysisVoiceChord *chord) {
    cJSON *list = cJSON_CreateArray();
    for (size_t i = 0; list && i < chord->gap_count; i++) {
        if (!json_append(list, cJSON_CreateNumber((double)chord->gaps[i]))) {
            cJSON_Delete(list);
            return NULL;
        }
    }
    return list;
}

/* The spacing of a chord: whether the library judged it, and if so the upper voice of the first pair too wide. */
static cJSON *chord_spacing_json(const AnalysisVoiceChord *chord) {
    cJSON *object = cJSON_CreateObject();
    if (object &&
        !(json_add(object, "judged", cJSON_CreateBool(chord->spacing_judged)) &&
          json_add(object, "valid", chord->spacing_judged ? cJSON_CreateBool(chord->wide_voice < 0) : cJSON_CreateNull()) &&
          json_add(object, "wide_voice",
              chord->spacing_judged && chord->wide_voice >= 0 ? cJSON_CreateNumber(chord->wide_voice) : cJSON_CreateNull()))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* The doubling of a chord: the member most voices sound when more than one does, or null. */
static cJSON *chord_doubled_json(const AnalysisVoiceChord *chord) {
    if (chord->doubled_member < 0) {
        return cJSON_CreateNull();
    }
    cJSON *object = cJSON_CreateObject();
    if (object && !(json_add(object, "member", cJSON_CreateString(CHORD_MEMBER_NAMES[chord->doubled_member])) &&
                    json_add(object, "voices", cJSON_CreateNumber((double)chord->member_voices[chord->doubled_member])))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* One chord's doubling and spacing, on the chord's own span. */
static cJSON *voice_chord_json(DocBuild *build, const ImportedNotes *notes, const AnalysisReport *report,
                               const AnalysisVoiceLeading *voice_leading, const AnalysisVoiceChord *chord) {
    Rational start;
    Rational end;
    voice_leading_entries_span(build, report, chord->entry, chord->entry, &start, &end);
    cJSON *claim = claim_json_with(build, voicing_framework_name(), notes, chord->note_ids, chord->note_id_count, start, end,
                                   "doubling", cJSON_CreateNumber(chord->uncertainty));
    if (!(claim && json_add(claim, "chord", cJSON_CreateNumber((double)chord->entry)) &&
          json_add(claim, "voices", cJSON_CreateNumber((double)chord->voices_sounding)) &&
          json_add(claim, "doubled", chord_doubled_json(chord)) &&
          json_add(claim, "non_chord_voices", cJSON_CreateNumber((double)chord->non_chord_voices)) &&
          json_add(claim, "gaps", chord_gaps_json(chord)) && json_add(claim, "spacing", chord_spacing_json(chord)))) {
        cJSON_Delete(claim);
        return NULL;
    }
    return claim;
}

/* The voice leading section: a list of claims, each labelled by the result it is. Empty when the library declined the
   voices. */
static cJSON *voice_leading_json(DocBuild *build, const ImportedNotes *notes, const AnalysisReport *report) {
    cJSON *list = cJSON_CreateArray();
    const AnalysisVoiceLeading *voice_leading = &report->voice_leading;
    if (!list || !voice_leading->present) {
        return list;
    }
    bool ok = json_append(list, voice_motions_json(build, notes, report));
    for (size_t i = 0; ok && i < voice_leading->violation_count; i++) {
        ok = json_append(list, voice_violation_json(build, notes, report, voice_leading, &voice_leading->violations[i]));
    }
    for (size_t i = 0; ok && i < voice_leading->resolution_count; i++) {
        ok = json_append(list, voice_resolution_json(build, notes, report, voice_leading, &voice_leading->resolutions[i]));
    }
    for (size_t i = 0; ok && i < voice_leading->chord_count; i++) {
        ok = json_append(list, voice_chord_json(build, notes, report, voice_leading, &voice_leading->chords[i]));
    }
    if (!ok) {
        cJSON_Delete(list);
        return NULL;
    }
    return list;
}

/* ---- Schemas and sequences ---- */

/* The measure a time lies in, as the library numbers them (a pickup is 0); 0 when the position cannot be read. */
static int measure_of_time(const MeterMap *map, Rational time) {
    MusicalTime *musical_time = NULL;
    MetricalPosition *position = NULL;
    int measure = 0;
    if (musical_time_create((long)time.num, (unsigned long)time.den, &musical_time) != MUSICAL_TIME_OK) {
        return 0;
    }
    if (metrical_position_from_time(map, musical_time, &position) == METRICAL_POSITION_OK) {
        measure = metrical_position_measure(position);
        metrical_position_destroy(position);
    }
    musical_time_destroy(musical_time);
    return measure;
}

/* A degree as the library's galant-schemas reads it: the letter distance from the tonic (1 to 7) and the alteration in
   semitones against the major scale (-1 lowered, +1 raised). */
static cJSON *galant_degree_json(int degree, int alteration) {
    cJSON *object = cJSON_CreateObject();
    if (object && !(json_add(object, "degree", cJSON_CreateNumber(degree)) &&
                    json_add(object, "alteration", cJSON_CreateNumber(alteration)))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* The first and last measure of the chords entries[first..last] start in. */
static cJSON *entry_measures_json(DocBuild *build, const ImportedNotes *notes, const AnalysisReport *report, size_t first, size_t last) {
    const MeterMap *map = notes->meter.meter_map;
    return measures_json(measure_of_time(map, entry_start_time(build, &report->entries[first])),
                         measure_of_time(map, entry_start_time(build, &report->entries[last])));
}

/* One stage of a schema: where it sits (span, location and the measure it starts in), the soprano degree the library
   matched (null for a bass-only stage), a two-note stage's second soprano degree (otherwise null), the bass degree and
   the stage's credit. */
static cJSON *schema_stage_json(DocBuild *build, const ImportedNotes *notes, const AnalysisReport *report,
                                const AnalysisSchemaStage *stage) {
    Rational start = entry_start_time(build, &report->entries[stage->first_entry]);
    Rational end = entry_end_time(build, &report->entries[stage->last_entry]);
    cJSON *object = cJSON_CreateObject();
    if (!(object && json_add(object, "span", span_json(start, end)) &&
          json_add(object, "location", location_json(build, notes->meter.meter_map, start, end)) &&
          json_add(object, "measure", cJSON_CreateNumber(measure_of_time(notes->meter.meter_map, start))) &&
          json_add(object, "soprano",
              stage->has_melody ? galant_degree_json(stage->melody_degree, stage->melody_alteration)
                                : cJSON_CreateNull()) &&
          json_add(object, "second_soprano",
              stage->has_second ? galant_degree_json(stage->second_degree, stage->second_alteration)
                                : cJSON_CreateNull()) &&
          json_add(object, "bass", galant_degree_json(stage->bass_degree, stage->bass_alteration)) &&
          json_add(object, "bass_only", cJSON_CreateBool(stage->bass_only)) &&
          json_add(object, "credit", cJSON_CreateNumber(stage->credit)))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

static cJSON *schema_stages_json(DocBuild *build, const ImportedNotes *notes, const AnalysisReport *report, const AnalysisSchema *schema) {
    cJSON *list = cJSON_CreateArray();
    for (size_t i = 0; list && i < schema->stage_count; i++) {
        if (!json_append(list, schema_stage_json(build, notes, report, &schema->stages[i]))) {
            cJSON_Delete(list);
            return NULL;
        }
    }
    return list;
}

/* One claim per schema, in order of the first stage. The claim's span runs from the first stage's chord to the last
   chord the stages use. frame is the key the stages were read in, as a degree of the home key; confidence holds the
   library's parts (pair share, metric fit, mean uncertainty) and the whole. Everything else is the library's: the name,
   function, stages and degrees. */
static cJSON *schema_json(DocBuild *build, const ImportedNotes *notes, const AnalysisReport *report, const AnalysisSchema *schema) {
    size_t id_count = 0;
    size_t *ids = entry_range_note_ids(report, schema->first_entry, schema->last_entry, &id_count);
    cJSON *claim = ids ? claim_json_with(build, schema->framework, notes, ids, id_count,
                                         entry_start_time(build, &report->entries[schema->first_entry]),
                                         entry_end_time(build, &report->entries[schema->last_entry]), "schema",
                                         cJSON_CreateNumber(schema->uncertainty))
                       : NULL;
    free(ids);
    cJSON *confidence = cJSON_CreateObject();
    bool taken = false; /* json_add() takes the confidence over, and frees it itself when it fails */
    if (!(claim && confidence && json_add(confidence, "pair_share", cJSON_CreateNumber(schema->pair_share)) &&
          json_add(confidence, "metric_fit", cJSON_CreateNumber(schema->metric_fit)) &&
          json_add(confidence, "mean_uncertainty", cJSON_CreateNumber(schema->mean_uncertainty)) &&
          json_add(confidence, "overall", cJSON_CreateNumber(schema->confidence)) &&
          json_add(claim, "name", cJSON_CreateString(schema->name)) &&
          json_add(claim, "function", cJSON_CreateString(schema->function)) &&
          json_add(claim, "measures", entry_measures_json(build, notes, report, schema->first_entry, schema->last_entry)) &&
          json_add(claim, "frame", galant_degree_json(schema->frame_degree, schema->frame_alteration)) &&
          json_add(claim, "bass_only_stages", cJSON_CreateNumber((double)schema->bass_only_stages)) &&
          (taken = true, json_add(claim, "confidence", confidence)) &&
          json_add(claim, "stages", schema_stages_json(build, notes, report, schema)))) {
        if (!taken) {
            cJSON_Delete(confidence);
        }
        cJSON_Delete(claim);
        return NULL;
    }
    return claim;
}

/* One stage of a sequence: where it sits and the bass, soprano and chord root degrees the library read there in the home
   key (soprano and root null where the segment has none). */
static cJSON *sequence_stage_json(DocBuild *build, const ImportedNotes *notes, const AnalysisReport *report,
                                  const AnalysisSequenceStage *stage) {
    Rational start = entry_start_time(build, &report->entries[stage->first_entry]);
    Rational end = entry_end_time(build, &report->entries[stage->last_entry]);
    cJSON *object = cJSON_CreateObject();
    if (!(object && json_add(object, "span", span_json(start, end)) &&
          json_add(object, "location", location_json(build, notes->meter.meter_map, start, end)) &&
          json_add(object, "measure", cJSON_CreateNumber(measure_of_time(notes->meter.meter_map, start))) &&
          json_add(object, "bass", galant_degree_json(stage->bass_degree, stage->bass_alteration)) &&
          json_add(object, "soprano",
              stage->has_soprano ? galant_degree_json(stage->soprano_degree, stage->soprano_alteration)
                                 : cJSON_CreateNull()) &&
          json_add(object, "root",
              stage->has_root ? galant_degree_json(stage->root_degree, stage->root_alteration) : cJSON_CreateNull()))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

static cJSON *sequence_stages_json(DocBuild *build, const ImportedNotes *notes, const AnalysisReport *report,
                                   const AnalysisSequence *sequence) {
    cJSON *list = cJSON_CreateArray();
    for (size_t i = 0; list && i < sequence->stage_count; i++) {
        if (!json_append(list, sequence_stage_json(build, notes, report, &sequence->stages[i]))) {
            cJSON_Delete(list);
            return NULL;
        }
    }
    return list;
}

/* One claim per sequence, in order of the first segment. The claim's span runs from the first segment to the end of
   the last; the label is "sequence". kind is the library's name for what repeats; pattern and direction are null where
   the library encodes none (the cycle of fifths has no rule, so rule_backed is false). */
static cJSON *sequence_json(DocBuild *build, const ImportedNotes *notes, const AnalysisReport *report, const AnalysisSequence *sequence) {
    size_t id_count = 0;
    size_t *ids = entry_range_note_ids(report, sequence->first_entry, sequence->last_entry, &id_count);
    cJSON *claim = ids ? claim_json_with(build, sequence->framework, notes, ids, id_count,
                                         entry_start_time(build, &report->entries[sequence->first_entry]),
                                         entry_end_time(build, &report->entries[sequence->last_entry]), "sequence",
                                         cJSON_CreateNumber(sequence->uncertainty))
                       : NULL;
    free(ids);
    cJSON *confidence = cJSON_CreateObject();
    bool taken = false; /* json_add() takes the confidence over, and frees it itself when it fails */
    if (!(claim && confidence && json_add(confidence, "unit_share", cJSON_CreateNumber(sequence->unit_share)) &&
          json_add(confidence, "soprano_bonus", cJSON_CreateNumber(sequence->soprano_bonus)) &&
          json_add(confidence, "mean_uncertainty", cJSON_CreateNumber(sequence->mean_uncertainty)) &&
          json_add(confidence, "overall", cJSON_CreateNumber(sequence->confidence)) &&
          json_add(claim, "kind", cJSON_CreateString(sequence->kind)) &&
          json_add(claim, "pattern", optional_string_json(sequence->pattern)) &&
          json_add(claim, "rule_backed", cJSON_CreateBool(sequence->rule_backed)) &&
          json_add(claim, "direction", optional_string_json(sequence->direction)) &&
          json_add(claim, "unit_length", cJSON_CreateNumber(sequence->unit_length)) &&
          json_add(claim, "units", cJSON_CreateNumber(sequence->units)) &&
          json_add(claim, "transposition_steps", cJSON_CreateNumber(sequence->transposition_steps)) &&
          json_add(claim, "measures", entry_measures_json(build, notes, report, sequence->first_entry, sequence->last_entry)) &&
          (taken = true, json_add(claim, "confidence", confidence)) &&
          json_add(claim, "stages", sequence_stages_json(build, notes, report, sequence)))) {
        if (!taken) {
            cJSON_Delete(confidence);
        }
        cJSON_Delete(claim);
        return NULL;
    }
    return claim;
}

/* The schemas section: one flat list of claims, the schemas (labelled "schema") in order of their first stage and then
   the sequences (labelled "sequence") in order of their first segment. */
static cJSON *schemas_json(DocBuild *build, const ImportedNotes *notes, const AnalysisReport *report) {
    cJSON *list = cJSON_CreateArray();
    for (size_t i = 0; list && i < report->schema_count; i++) {
        if (!json_append(list, schema_json(build, notes, report, &report->schemas[i]))) {
            cJSON_Delete(list);
            return NULL;
        }
    }
    for (size_t i = 0; list && i < report->sequence_count; i++) {
        if (!json_append(list, sequence_json(build, notes, report, &report->sequences[i]))) {
            cJSON_Delete(list);
            return NULL;
        }
    }
    return list;
}

static cJSON *document_json(DocBuild *build, const char *input_path, const ImportedNotes *notes, const AnalysisReport *report) {
    cJSON *document = cJSON_CreateObject();
    if (document &&
        !(json_add(document, "schema_version", cJSON_CreateNumber(URSATZ_ANALYZER_DOCUMENT_SCHEMA_VERSION)) &&
          json_add(document, "metadata", metadata_json(build, input_path, notes, report)) &&
          json_add(document, "meter", meter_json(build, notes)) && json_add(document, "keys", keys_json(build, notes, report)) &&
          json_add(document, "tonicizations", tonicizations_json(build, notes, report)) &&
          json_add(document, "modulations", modulations_json(build, notes, report)) &&
          json_add(document, "pivot_chords", pivot_chords_json(build, notes, report)) &&
          json_add(document, "chords", chords_json(build, notes, report)) &&
          json_add(document, "chord_vocabulary",
              chord_vocabulary_json(build, report, cJSON_GetObjectItemCaseSensitive(document, "chords"))) &&
          json_add(document, "prolongations", prolongations_json(build, notes, report)) &&
          json_add(document, "non_chord_tones", non_chord_tones_json(build, notes, report)) &&
          json_add(document, "outer_voices", outer_voices_json(build, notes, report)) &&
          json_add(document, "cadences", cadences_json(build, notes, report)) &&
          json_add(document, "phrases", phrases_json(build, notes, report)) &&
          json_add(document, "themes", themes_json(build, notes, report)) && json_add(document, "form", form_json(build, notes, report)) &&
          json_add(document, "repeats", repeats_json(build, notes, report)) &&
          json_add(document, "motives", motives_json(build, notes, report)) && json_add(document, "melody", melody_json(build, notes, report)) &&
          json_add(document, "rhythm", rhythm_json(build, notes, report)) && json_add(document, "texture", texture_json(build, notes, report)) &&
          json_add(document, "voice_leading", voice_leading_json(build, notes, report)) && json_add(document, "schemas", schemas_json(build, notes, report)) &&
          json_add(document, "progressions", progressions_json(build, notes, report)) && json_add(document, "harmonic_rhythm", harmonic_rhythm_json(build, report)) &&
          json_add(document, "markings", markings_json(build, notes, report)) && json_add(document, "counterpoint_check", counterpoint_check_json(build, notes, report)) &&
          json_add(document, "notes", notes_json(build, notes, report)))) {
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
    DocBuild build = {false};
    document->root = document_json(&build, input_path, notes, report);
    if (document->root && build.overflowed) {
        cJSON_Delete(document->root);
        document->root = NULL;
    }
    if (!document->root) {
        free(document);
        return NULL;
    }
    return document;
}

AnalysisDocument *analysis_document_build_from_result(const char *input_path, const AnalyzerResult *result) {
    if (!analyzer_result_report_complete(result)) {
        return NULL;
    }
    return analysis_document_build(input_path, analyzer_result_notes(result), analyzer_result_report(result));
}

char *analysis_document_render_result(const char *input_path, const AnalyzerResult *result) {
    AnalysisDocument *document = analysis_document_build_from_result(input_path, result);
    if (!document) {
        return NULL;
    }
    char *json = analysis_document_to_json(document);
    analysis_document_destroy(document);
    return json;
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
