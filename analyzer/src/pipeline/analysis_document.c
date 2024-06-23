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
#include "pipeline/pitch_class_util.h"
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

/* How the home key is chosen: the library offers no determination of its own, so it is the key with the
   greatest total duration over all its regions, a tie going to the key that appears first. */
static const char HOME_KEY_RULE[] = "longest_total_duration";

/* Source ids for the meter: a time signature the MIDI file declares or the score notates, and
   the 4/4 the library assumes when it declares none. */
static const char TIME_SIGNATURE_SOURCE[] = "midi-time-signature";
static const char NOTATED_TIME_SIGNATURE_SOURCE[] = "musicxml-time-signature";
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

/* The shared claim fields, in schema order, with the uncertainty as built by the caller (taken over, freed on
   failure). ids NULL means every imported note. */
static cJSON *claim_json_with(const char *framework, const ImportedNotes *notes, const size_t *ids,
                              size_t id_count, Rational start, Rational end, const char *label,
                              cJSON *uncertainty) {
    cJSON *claim = cJSON_CreateObject();
    bool consumed = false; /* add() takes the uncertainty over, and frees it itself when it fails */
    if (claim && !(add(claim, "framework", cJSON_CreateString(framework)) &&
                   add(claim, "span", span_json(start, end)) &&
                   add(claim, "location", location_json(notes->meter.meter_map, start, end)) &&
                   add(claim, "note_ids", note_ids_json(ids, id_count)) &&
                   add(claim, "label", cJSON_CreateString(label)) &&
                   (consumed = true, add(claim, "uncertainty", uncertainty)))) {
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

static cJSON *claim_json_at(const char *framework, const ImportedNotes *notes, const size_t *ids,
                            size_t id_count, Rational start, Rational end, const char *label,
                            AnalysisConfidence confidence) {
    return claim_json_with(framework, notes, ids, id_count, start, end, label, uncertainty_json(confidence));
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

    bool notated = !inferred && notes->source.meter_notated;
    const char *claim_source =
        inferred ? DEFAULT_METER_SOURCE : (notated ? NOTATED_TIME_SIGNATURE_SOURCE : TIME_SIGNATURE_SOURCE);
    cJSON *meter = claim_json(claim_source, notes, NULL, notes->count, label, (AnalysisConfidence){false, 0, 0});
    if (!(meter && add(meter, "source", cJSON_CreateString(inferred ? "inferred" : (notated ? "notated" : "file"))) &&
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
static cJSON *key_signatures_json(const ImportedNotes *notes) {
    cJSON *signatures = cJSON_CreateArray();
    if (!signatures) {
        return NULL;
    }
    for (size_t i = 0; i < notes->key_signature_events.count; i++) {
        const MidiKeySignatureEvent *event = &notes->key_signature_events.events[i];
        int major = key_signature_major_tonic(event->sharps_flats);
        char *major_label = key_label_create(major, true);
        char *minor_label = key_label_create((major + 9) % 12, false);
        Rational start = time_rational(event->position);
        cJSON *object = cJSON_CreateObject();
        bool ok = major_label && minor_label && object &&
                  add(object, "start", rational_json(start)) &&
                  add(object, "location", position_json(notes->meter.meter_map, start)) &&
                  add(object, "sharps_flats", cJSON_CreateNumber(event->sharps_flats)) &&
                  add(object, "mode_hint", cJSON_CreateString(mode_hint_name(event->mode_hint))) &&
                  add(object, "major_key", cJSON_CreateString(major_label)) &&
                  add(object, "minor_key", cJSON_CreateString(minor_label));
        free(major_label);
        free(minor_label);
        if (!ok || !append(signatures, object)) {
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
        if (!(object && add(object, "id", cJSON_CreateString(part->id)) &&
              add(object, "name", cJSON_CreateString(part->name)) &&
              add(object, "staff_count", cJSON_CreateNumber(part->staff_count)) &&
              add(object, "note_count", cJSON_CreateNumber((double)note_count)) && append(parts, object))) {
            cJSON_Delete(object);
            cJSON_Delete(parts);
            return NULL;
        }
    }
    return parts;
}

static cJSON *input_tempo_json(const ImportedSource *source) {
    cJSON *tempo = cJSON_CreateObject();
    if (tempo && !(add(tempo, "declared", cJSON_CreateBool(source->tempo_declared)) &&
                   add(tempo, "quarters_per_minute", source->tempo_declared
                                                         ? cJSON_CreateNumber(source->initial_quarters_per_minute)
                                                         : cJSON_CreateNull()) &&
                   add(tempo, "marking_count", cJSON_CreateNumber((double)source->tempo_marking_count)))) {
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
        !(add(input, "format", cJSON_CreateString(score ? "musicxml" : "midi")) &&
          add(input, "source_type", cJSON_CreateString(score ? "score" : "performance")) &&
          add(input, "meter", cJSON_CreateString(knowledge_name(meter_stated, source->meter_notated))) &&
          add(input, "key",
              cJSON_CreateString(knowledge_name(notes->key_signature_events.count > 0, source->key_notated))) &&
          add(input, "spelling", cJSON_CreateString(source->spelling_notated ? "notated" : "rule")) &&
          add(input, "parts", input_parts_json(notes)) && add(input, "tempo", input_tempo_json(source)) &&
          add(input, "dynamics_count", cJSON_CreateNumber((double)source->dynamics_count)) &&
          add(input, "warning_count", cJSON_CreateNumber((double)source->warning_count)))) {
        cJSON_Delete(input);
        return NULL;
    }
    return input;
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
          add(metadata, "input", input_json(notes)) &&
          add(metadata, "key_found", cJSON_CreateBool(report->key_region_count > 0)) &&
          add(metadata, "key_signatures", key_signatures_json(notes)))) {
        cJSON_Delete(metadata);
        return NULL;
    }
    return metadata;
}

static Rational region_start(const AnalysisKeyRegion *region) {
    return rational_make(region->start_numerator, region->start_denominator);
}

static Rational region_end(const AnalysisKeyRegion *region) {
    return rational_make(region->end_numerator, region->end_denominator);
}

static Rational rational_difference(Rational end, Rational start) {
    return rational_make(end.num * (long long)start.den - start.num * (long long)end.den, start.den * end.den);
}

/* The imported notes that start in [start, end), as indices; *count may be 0, and the result is then still a
   valid pointer. NULL on allocation failure. */
static size_t *notes_starting_in(const ImportedNotes *notes, Rational start, Rational end, size_t *count) {
    size_t *ids = malloc(sizeof(size_t) * (notes->count > 0 ? notes->count : 1));
    if (!ids) {
        return NULL;
    }
    *count = 0;
    for (size_t i = 0; i < notes->count; i++) {
        Rational onset = note_onset(notes->events[i]);
        if (!rational_less(onset, start) && rational_less(onset, end)) {
            ids[(*count)++] = i;
        }
    }
    return ids;
}

/* A claim over one key region's span: the notes that start in it, its label, and the library's uncertainty
   for the region (in [0, 1], higher is LESS certain, unlike a chord's confidence). */
static cJSON *region_span_claim(const ImportedNotes *notes, const AnalysisKeyRegion *region, const char *label,
                                cJSON *uncertainty) {
    size_t id_count = 0;
    size_t *ids = notes_starting_in(notes, region_start(region), region_end(region), &id_count);
    if (!ids) {
        cJSON_Delete(uncertainty);
        return NULL;
    }
    cJSON *claim = claim_json_with(library_framework_name(), notes, ids, id_count, region_start(region),
                                   region_end(region), label, uncertainty);
    free(ids);
    return claim;
}

static const char *region_source_name(AnalysisRegionSource source) {
    return source == ANALYSIS_REGION_SOURCE_KEY_SIGNATURE ? "key_signature" : "profile";
}

static cJSON *measures_json(int first, int last) {
    cJSON *object = cJSON_CreateObject();
    if (object && !(add(object, "first", cJSON_CreateNumber(first)) && add(object, "last", cJSON_CreateNumber(last)))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* One key region: the shared claim (its span runs barline to barline) and the key's fields. */
static cJSON *key_region_json(const ImportedNotes *notes, const AnalysisKeyRegion *region, size_t index) {
    cJSON *claim = region_span_claim(notes, region, region->label, cJSON_CreateNumber(region->uncertainty));
    if (!(claim && add(claim, "index", cJSON_CreateNumber((double)index)) &&
          add(claim, "tonic", cJSON_CreateString(pitch_class_name(region->tonic_pitch_class, region->prefer_flats))) &&
          add(claim, "mode", cJSON_CreateString(region->is_major ? "major" : "minor")) &&
          add(claim, "measures", measures_json(region->first_measure, region->last_measure)) &&
          add(claim, "source", cJSON_CreateString(region_source_name(region->source))))) {
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
            !append(indices, cJSON_CreateNumber((double)i))) {
            cJSON_Delete(indices);
            return NULL;
        }
    }
    return indices;
}

static cJSON *home_key_json(const AnalysisReport *report) {
    const AnalysisKeyRegion *home = &report->key_regions[report->home_region];
    cJSON *object = cJSON_CreateObject();
    if (object && !(add(object, "rule", cJSON_CreateString(HOME_KEY_RULE)) &&
                    add(object, "label", cJSON_CreateString(home->label)) &&
                    add(object, "region_indices", home_region_indices_json(report)) &&
                    add(object, "duration",
                        rational_json(rational_make(report->home_duration_numerator,
                                                    report->home_duration_denominator))))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* The keys section: the key regions in time order, the home key derived from them, and the indices of the
   opening and closing regions. With no key found the regions are empty and the rest null. */
static cJSON *keys_json(const ImportedNotes *notes, const AnalysisReport *report) {
    cJSON *keys = cJSON_CreateObject();
    cJSON *regions = cJSON_CreateArray();
    if (!keys || !regions) {
        cJSON_Delete(keys);
        cJSON_Delete(regions);
        return NULL;
    }
    for (size_t i = 0; i < report->key_region_count; i++) {
        if (!append(regions, key_region_json(notes, &report->key_regions[i], i))) {
            cJSON_Delete(keys);
            cJSON_Delete(regions);
            return NULL;
        }
    }
    bool found = report->key_region_count > 0;
    if (!(add(keys, "regions", regions) && add(keys, "home_key", found ? home_key_json(report) : cJSON_CreateNull()) &&
          add(keys, "opening_region", found ? cJSON_CreateNumber(0) : cJSON_CreateNull()) &&
          add(keys, "closing_region",
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
    return add(claim, "extent", cJSON_CreateString(extent_name(change->extent))) &&
           add(claim, "from_region", cJSON_CreateNumber((double)change->from_region)) &&
           add(claim, to_name, cJSON_CreateNumber((double)change->to_region)) &&
           add(claim, "from_key", cJSON_CreateString(from->label)) &&
           add(claim, "to_key", cJSON_CreateString(to->label));
}

/* One claim per tonicization: a brief excursion to another key that returns to the one before it. Its span is
   the excursion (the region the library puts the new key in), its target the key of that region. The change
   back out of the excursion is not listed again. */
static cJSON *tonicizations_json(const ImportedNotes *notes, const AnalysisReport *report) {
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
        cJSON *claim = label ? region_span_claim(notes, target, label, cJSON_CreateNull()) : NULL;
        free(label);
        if (!(claim && add_change_fields(claim, report, change, "target_region"))) {
            cJSON_Delete(claim);
            cJSON_Delete(list);
            return NULL;
        }
        if (!append(list, claim)) {
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
static cJSON *modulations_json(const ImportedNotes *notes, const AnalysisReport *report) {
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
        cJSON *claim = label ? region_span_claim(notes, to, label, cJSON_CreateNull()) : NULL;
        free(label);
        bool has_reading = change_has_pivot_reading(change);
        if (!(claim && add_change_fields(claim, report, change, "to_region") &&
              add(claim, "type", cJSON_CreateString(change->has_pivot ? "pivot" : "direct")) &&
              add(claim, "is_return", cJSON_CreateBool(change->is_return)) &&
              add(claim, "pivot_chord_index",
                  has_reading ? cJSON_CreateNumber((double)pivot_index) : cJSON_CreateNull()))) {
            cJSON_Delete(claim);
            cJSON_Delete(list);
            return NULL;
        }
        pivot_index += has_reading ? 1 : 0;
        if (!append(list, claim)) {
            cJSON_Delete(list);
            return NULL;
        }
    }
    return list;
}

/* One claim per pivot chord: the chord that is diatonic in both the old and the new key at a modulation,
   identified without a Roman numeral (root, quality, bass, inversion), read in the old key. The claim is the
   chord's segment. */
static cJSON *pivot_chords_json(const ImportedNotes *notes, const AnalysisReport *report) {
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
        cJSON *claim = label ? claim_json_at(library_framework_name(), notes, entry->note_ids, entry->note_id_count,
                                             rational_make(entry->time_numerator, entry->time_denominator),
                                             rational_make(entry->end_numerator, entry->end_denominator), label,
                                             pivot->confidence)
                             : NULL;
        free(label);
        if (!(claim && add(claim, "modulation_index", cJSON_CreateNumber((double)this_modulation)) &&
              add(claim, "chord_index", cJSON_CreateNumber((double)change->pivot_entry)) &&
              add(claim, "root", cJSON_CreateString(pivot->root)) &&
              add(claim, "quality", cJSON_CreateString(pivot->quality)) &&
              add(claim, "bass", cJSON_CreateString(pivot->bass)) &&
              add(claim, "inversion",
                  pivot->inversion >= 0 ? cJSON_CreateNumber(pivot->inversion) : cJSON_CreateNull()) &&
              add(claim, "from_key", cJSON_CreateString(report->key_regions[change->from_region].label)) &&
              add(claim, "to_key", cJSON_CreateString(report->key_regions[change->to_region].label)))) {
            cJSON_Delete(claim);
            cJSON_Delete(list);
            return NULL;
        }
        if (!append(list, claim)) {
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
static const AnalysisKeyRegion *region_at(const AnalysisReport *report, Rational time, size_t *index) {
    for (size_t i = 0; i < report->key_region_count; i++) {
        if (rational_less(time, region_end(&report->key_regions[i]))) {
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
    if (note && !(add(note, "kind", cJSON_CreateString("key_signature_disagreement")) &&
                  add(note, "scope", cJSON_CreateString(scope)) &&
                  add(note, "signature_index",
                      signature_index >= 0 ? cJSON_CreateNumber((double)signature_index) : cJSON_CreateNull()) &&
                  add(note, "region_index", cJSON_CreateNumber((double)region_index)) &&
                  add(note, "notated", event ? cJSON_CreateString(notated) : cJSON_CreateNull()) &&
                  add(note, "detected", cJSON_CreateString(region->label)) &&
                  add(note, "message", cJSON_CreateString(message)))) {
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
static cJSON *notes_json(const ImportedNotes *notes, const AnalysisReport *report) {
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
        const AnalysisKeyRegion *region = region_at(report, time_rational(event->position), &region_index);
        if (region && !key_fits_signature(region, event->sharps_flats) &&
            !append(list, disagreement_json("region_at_signature", (long)i, event, region_index, region))) {
            cJSON_Delete(list);
            return NULL;
        }
    }
    if (!home_fits_any && !append(list, disagreement_json("home_key", -1, NULL, report->home_region, home))) {
        cJSON_Delete(list);
        return NULL;
    }
    return list;
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
           add(claim, "runner_up", runner_up_json(&entry->runner_up)) &&
           add(claim, "context", matched ? cJSON_CreateString(chord_context_name(entry->chord_context))
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
        !(add(object, "status", cJSON_CreateString(outer_status_name(note->status))) &&
          add(object, "note_id", present ? cJSON_CreateNumber((double)note->note_id) : cJSON_CreateNull()) &&
          add(object, "pitch", optional_string_json(note->pitch_name)) &&
          add(object, "degree", optional_string_json(note->degree)) &&
          add(object, "uncertainty", outer_uncertainty_json(report, entry, note)))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* The chord's "outer" field: the soprano and the bass sounding in its segment. (The chord's own "bass" field is
   the pitch class the chord is read over.) */
static cJSON *chord_outer_json(const AnalysisReport *report, const AnalysisReportEntry *entry) {
    cJSON *outer = cJSON_CreateObject();
    if (outer && !(add(outer, "soprano", outer_voice_summary_json(report, entry, &entry->soprano)) &&
                   add(outer, "bass", outer_voice_summary_json(report, entry, &entry->bass)))) {
        cJSON_Delete(outer);
        return NULL;
    }
    return outer;
}

/* Semitones above the tonic of each degree 1..7 of the major scale and the natural minor scale: the key a
   degree names. This only locates the key the library's applied-chord target degree points at, to compare it
   with a tonicization's key; what is an applied chord is the library's. */
static int degree_semitones(bool major, int degree) {
    static const int MAJOR[7] = {0, 2, 4, 5, 7, 9, 11};
    static const int MINOR[7] = {0, 2, 3, 5, 7, 8, 10};
    return (major ? MAJOR : MINOR)[degree - 1];
}

static bool rational_equal(Rational a, Rational b) {
    return !rational_less(a, b) && !rational_less(b, a);
}

/* Whether a chord's span lies in, or runs straight into, the span of a region: the tonicization region the
   chord belongs to, or the one it leads to. */
static bool span_touches(Rational chord_start, Rational chord_end, Rational start, Rational end) {
    return (rational_less(chord_start, end) && rational_less(start, chord_end)) || rational_equal(chord_end, start);
}

/* The chord's applied reading and its cross-reference to the tonicization claims, or null for a chord that is
   not applied. The tonicization claims are numbered as in "tonicizations". Among the claims whose span touches the
   chord's (see span_touches), one whose key has the tonic the applied target names is the agreeing one; if
   only claims with another tonic touch it, the two disagree; if none touches, there is no tonicization.
   Neither side is changed to fit the other. */
static cJSON *applied_json(const AnalysisReport *report, const AnalysisReportEntry *entry) {
    const AnalysisRomanNumeral *roman = &entry->roman;
    if (!roman->applied) {
        return cJSON_CreateNull();
    }
    bool in_region = entry->in_region && entry->region < report->key_region_count;
    int target_tonic = -1;
    if (in_region) {
        const AnalysisKeyRegion *local = &report->key_regions[entry->region];
        target_tonic = (local->tonic_pitch_class + degree_semitones(local->is_major, roman->target_degree)) % 12;
    }
    Rational chord_start = rational_make(entry->time_numerator, entry->time_denominator);
    Rational chord_end = rational_make(entry->end_numerator, entry->end_denominator);

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
        if (span_touches(chord_start, chord_end, region_start(target), region_end(target))) {
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
        !(add(object, "numeral", cJSON_CreateString(roman->applied_numeral)) &&
          add(object, "target_numeral", cJSON_CreateString(roman->target_numeral)) &&
          add(object, "target_degree", cJSON_CreateNumber(roman->target_degree)) &&
          add(object, "target_tonic", target_tonic >= 0 ? cJSON_CreateString(pitch_class_name(
                                          target_tonic, report->key_regions[entry->region].prefer_flats))
                                                         : cJSON_CreateNull()) &&
          add(object, "kind", cJSON_CreateString(roman->applied_kind)) &&
          add(object, "resolves_to", cJSON_CreateNumber((double)roman->resolution_entry)) &&
          add(object, "deceptive", cJSON_CreateBool(roman->deceptive)) &&
          add(object, "tonicization_agreement", cJSON_CreateString(agreement)) &&
          add(object, "tonicization_index", touching ? cJSON_CreateNumber((double)touching_index) : cJSON_CreateNull()) &&
          add(object, "target_region", touching ? cJSON_CreateNumber((double)touching_region) : cJSON_CreateNull()) &&
          add(object, "tonicization_key", touching ? cJSON_CreateString(report->key_regions[touching_region].label)
                                                   : cJSON_CreateNull()))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* The chord's Roman numeral: the library's label, the key region it is relative to (the region the segment
   starts in, which is also the key the chord was read in), its scale degree and uncertainty; or null label with
   the reason there is none. */
static cJSON *roman_numeral_json(const AnalysisReport *report, const AnalysisReportEntry *entry) {
    const AnalysisRomanNumeral *roman = &entry->roman;
    bool in_region = entry->in_region && entry->region < report->key_region_count;
    cJSON *object = cJSON_CreateObject();
    if (object &&
        !(add(object, "label", optional_string_json(roman->numeral)) &&
          add(object, "base_numeral", optional_string_json(roman->base_numeral)) &&
          add(object, "key_region", in_region ? cJSON_CreateNumber((double)entry->region) : cJSON_CreateNull()) &&
          add(object, "key", in_region ? cJSON_CreateString(report->key_regions[entry->region].label)
                                       : cJSON_CreateNull()) &&
          add(object, "degree", roman->labelled ? cJSON_CreateNumber(roman->degree) : cJSON_CreateNull()) &&
          add(object, "uncertainty", roman->labelled ? cJSON_CreateNumber(roman->uncertainty) : cJSON_CreateNull()) &&
          add(object, "unlabelled_reason", optional_string_json(roman->reason)) &&
          add(object, "applied", applied_json(report, entry)))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
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
              add_chord_fields(claim, notes, entry, start, end) &&
              add(claim, "outer", chord_outer_json(report, entry)) &&
              add(claim, "roman_numeral", roman_numeral_json(report, entry)))) {
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

/* One claim per non-chord tone, in time order. Each is located where the note sounds against the chord:
   that chord's segment. */
static cJSON *non_chord_tones_json(const ImportedNotes *notes, const AnalysisReport *report) {
    cJSON *tones = cJSON_CreateArray();
    if (!tones) {
        return NULL;
    }
    const char *framework = library_framework_name();
    for (size_t i = 0; i < report->non_chord_tone_count; i++) {
        const AnalysisNonChordTone *tone = &report->non_chord_tones[i];
        const AnalysisReportEntry *chord = &report->entries[tone->chord_index];
        AnalysisConfidence none = {false, 0, 0};
        cJSON *claim = claim_json_at(framework, notes, &tone->note_id, 1,
                                     rational_make(chord->time_numerator, chord->time_denominator),
                                     rational_make(chord->end_numerator, chord->end_denominator),
                                     tone->type ? tone->type : "unclassified", none);
        if (!(claim && add(claim, "type", optional_string_json(tone->type)) &&
              add(claim, "classified", cJSON_CreateBool(tone->type != NULL)) &&
              add(claim, "chord_index", cJSON_CreateNumber((double)tone->chord_index)) &&
              add(claim, "chord_label", cJSON_CreateString(chord->chord_label)))) {
            cJSON_Delete(claim);
            cJSON_Delete(tones);
            return NULL;
        }
        if (!append(tones, claim)) {
            cJSON_Delete(tones);
            return NULL;
        }
    }
    return tones;
}

/* ---- Outer voices ---- */

/* The note's own timing, apart from the chord segment it sounds in: where it is attacked, as a time and a
   measure and beat, and how long it lasts in whole notes and in beats of the signature it is attacked in.
   Null members when no note sounds as the line. */
static bool add_outer_note_timing(cJSON *claim, const ImportedNotes *notes, const AnalysisOuterNote *voice) {
    if (voice->status != ANALYSIS_OUTER_PRESENT) {
        return add(claim, "onset", cJSON_CreateNull()) && add(claim, "duration", cJSON_CreateNull());
    }
    const NoteEvent *event = notes->events[voice->note_id];
    Rational onset = note_onset(event);
    Rational end = note_end(event);
    cJSON *onset_object = cJSON_CreateObject();
    cJSON *duration = cJSON_CreateObject();
    bool ok = onset_object && duration && add(onset_object, "time", rational_json(onset)) &&
              add(onset_object, "location", position_json(notes->meter.meter_map, onset));
    if (ok) {
        Rational length = rational_difference(end, onset);
        ok = add(duration, "whole_notes", rational_json(length)) &&
             add(duration, "beats", span_beats_json(notes->meter.meter_map, onset, end));
    }
    if (!ok) {
        cJSON_Delete(onset_object);
        cJSON_Delete(duration);
        return false;
    }
    return add(claim, "onset", onset_object) && add(claim, "duration", duration);
}

/* {"name", "midi"}: the note's pitch name with octave and its MIDI note number; null when no note sounds. */
static cJSON *outer_pitch_json(const AnalysisOuterNote *voice) {
    if (voice->status != ANALYSIS_OUTER_PRESENT) {
        return cJSON_CreateNull();
    }
    cJSON *pitch = cJSON_CreateObject();
    if (pitch && !(add(pitch, "name", cJSON_CreateString(voice->pitch_name)) &&
                   add(pitch, "midi", cJSON_CreateNumber(voice->pitch)))) {
        cJSON_Delete(pitch);
        return NULL;
    }
    return pitch;
}

/* One entry of an outer line: the claim over the chord segment's span (note_ids is the line's note, or empty
   when none sounds), then the chord it belongs to, the status, the note, its pitch, its scale degree in that
   chord's local key, and its timing. The label is the degree; failing that the pitch name (the segment lies in
   no key region), or the status when no note sounds. */
static cJSON *outer_voice_note_json(const ImportedNotes *notes, const AnalysisReport *report, size_t chord_index,
                                    const AnalysisOuterNote *voice) {
    const AnalysisReportEntry *entry = &report->entries[chord_index];
    bool present = voice->status == ANALYSIS_OUTER_PRESENT;
    Rational start = rational_make(entry->time_numerator, entry->time_denominator);
    Rational end = rational_make(entry->end_numerator, entry->end_denominator);
    const char *label = voice->degree ? voice->degree
                                       : (present ? voice->pitch_name : outer_status_name(voice->status));
    cJSON *claim = claim_json_with(library_framework_name(), notes, present ? &voice->note_id : NULL,
                                   present ? 1 : 0, start, end, label, outer_uncertainty_json(report, entry, voice));
    if (!(claim && add(claim, "chord_index", cJSON_CreateNumber((double)chord_index)) &&
          add(claim, "status", cJSON_CreateString(outer_status_name(voice->status))) &&
          add(claim, "key_label", cJSON_CreateString(entry->key_label)) &&
          add(claim, "note_id", present ? cJSON_CreateNumber((double)voice->note_id) : cJSON_CreateNull()) &&
          add(claim, "pitch", outer_pitch_json(voice)) && add(claim, "degree", optional_string_json(voice->degree)) &&
          add(claim, "held", present ? cJSON_CreateBool(voice->held) : cJSON_CreateNull()) &&
          add_outer_note_timing(claim, notes, voice))) {
        cJSON_Delete(claim);
        return NULL;
    }
    return claim;
}

/* One line of the outer voices: an entry for every chord segment, in time order, so a line lines up with the
   chords by chord_index. */
static cJSON *outer_line_json(const ImportedNotes *notes, const AnalysisReport *report, bool soprano) {
    cJSON *line = cJSON_CreateArray();
    if (!line) {
        return NULL;
    }
    for (size_t i = 0; i < report->entry_count; i++) {
        const AnalysisReportEntry *entry = &report->entries[i];
        if (!append(line, outer_voice_note_json(notes, report, i, soprano ? &entry->soprano : &entry->bass))) {
            cJSON_Delete(line);
            return NULL;
        }
    }
    return line;
}

/* The outer_voices section: the soprano and the bass, the shared input for voice-leading, melody and schema
   analysis. */
static cJSON *outer_voices_json(const ImportedNotes *notes, const AnalysisReport *report) {
    cJSON *outer = cJSON_CreateObject();
    if (outer && !(add(outer, "soprano", outer_line_json(notes, report, true)) &&
                   add(outer, "bass", outer_line_json(notes, report, false)))) {
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

static const char *json_string(const cJSON *object, const char *key) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, key);
    return cJSON_IsString(item) ? item->valuestring : NULL;
}

static Rational rational_add(Rational a, Rational b) {
    return rational_make(a.num * (long long)b.den + b.num * (long long)a.den, a.den * b.den);
}

static Rational rational_read(const cJSON *object) {
    const cJSON *num = cJSON_GetObjectItemCaseSensitive(object, "num");
    const cJSON *den = cJSON_GetObjectItemCaseSensitive(object, "den");
    long long denominator = cJSON_IsNumber(den) ? (long long)den->valuedouble : 1;
    return rational_make(cJSON_IsNumber(num) ? (long long)num->valuedouble : 0,
                         denominator > 0 ? (unsigned long long)denominator : 1);
}

/* The labelled chords of the chords section, in time order. *count is how many. NULL on allocation failure. */
static VocabularyChord *vocabulary_chords(const cJSON *chords, size_t *count) {
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
        const char *label = json_string(roman, "label");
        const char *base = json_string(roman, "base_numeral");
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
            record->whole = rational_read(cJSON_GetObjectItemCaseSensitive(duration, "whole_notes"));
            record->has_beats = cJSON_IsObject(beats);
            record->beats = record->has_beats ? rational_read(beats) : rational_make(0, 1);
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
    if (rational_less(right->whole, left->whole)) {
        return -1;
    }
    if (rational_less(left->whole, right->whole)) {
        return 1;
    }
    return left->first < right->first ? -1 : (left->first > right->first ? 1 : 0);
}

static cJSON *vocabulary_duration_json(const VocabularyTally *tally) {
    cJSON *duration = cJSON_CreateObject();
    if (duration && !(add(duration, "whole_notes", rational_json(tally->whole)) &&
                      add(duration, "beats", tally->beats_known ? rational_json(tally->beats) : cJSON_CreateNull()))) {
        cJSON_Delete(duration);
        return NULL;
    }
    return duration;
}

static cJSON *vocabulary_first_json(const VocabularyTally *tally) {
    cJSON *first = cJSON_CreateObject();
    if (first && !(add(first, "chord_index", cJSON_CreateNumber((double)tally->first)) &&
                   add(first, "position", cJSON_Duplicate(tally->position, true)))) {
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
        !(add(object, "scope", cJSON_CreateString(scope == SCOPE_OVERALL ? "overall" : "region")) &&
          add(object, "key_region", in_region ? cJSON_CreateNumber((double)scope) : cJSON_CreateNull()) &&
          add(object, "key", in_region ? cJSON_CreateString(report->key_regions[scope].label) : cJSON_CreateNull()) &&
          add(object, "group", cJSON_CreateString(GROUP_NAMES[group])) &&
          add(object, "level", cJSON_CreateString(LEVEL_NAMES[level])) &&
          add(object, "numeral", cJSON_CreateString(tally->name)) &&
          add(object, "count", cJSON_CreateNumber((double)tally->count)) &&
          add(object, "runs", cJSON_CreateNumber((double)tally->runs)) &&
          add(object, "duration", vocabulary_duration_json(tally)) &&
          add(object, "share", cJSON_CreateNumber(share)) && add(object, "first", vocabulary_first_json(tally)))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* The entries of one scope, level and group, sorted, appended to list. False on allocation failure. */
static bool append_tallies(cJSON *list, const AnalysisReport *report, const VocabularyChord *records, size_t count,
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
            *tally = (VocabularyTally){name, 0, 0, rational_make(0, 1), rational_make(0, 1), true, record->index,
                                       record->position};
            distinct++;
        }
        tally->count++;
        tally->runs += record->starts_run ? 1 : 0;
        tally->whole = rational_add(tally->whole, record->whole);
        tally->beats_known = tally->beats_known && record->has_beats;
        if (tally->beats_known) {
            tally->beats = rational_add(tally->beats, record->beats);
        }
    }
    qsort(tallies, distinct, sizeof(VocabularyTally), compare_tallies);
    bool ok = true;
    for (size_t t = 0; ok && t < distinct; t++) {
        cJSON *entry = vocabulary_entry_json(report, scope, group, level, &tallies[t], total);
        ok = entry && append(list, entry);
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
static cJSON *chord_vocabulary_json(const AnalysisReport *report, const cJSON *chords) {
    cJSON *list = cJSON_CreateArray();
    size_t count = 0;
    VocabularyChord *records = list ? vocabulary_chords(chords, &count) : NULL;
    if (!records) {
        cJSON_Delete(list);
        return NULL;
    }
    bool ok = true;
    for (long scope = SCOPE_OVERALL; ok && scope < (long)report->key_region_count; scope = scope == SCOPE_OVERALL ? 0 : scope + 1) {
        Rational total = rational_make(0, 1);
        for (size_t i = 0; i < count; i++) {
            if (scope == SCOPE_OVERALL || records[i].region == scope) {
                total = rational_add(total, records[i].whole);
            }
        }
        for (int level = 0; ok && level < 2; level++) {
            for (int group = 0; ok && group < GROUP_COUNT; group++) {
                ok = append_tallies(list, report, records, count, scope, group, level, total);
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

static cJSON *document_json(const char *input_path, const ImportedNotes *notes, const AnalysisReport *report) {
    cJSON *document = cJSON_CreateObject();
    if (document &&
        !(add(document, "schema_version", cJSON_CreateNumber(URSATZ_ANALYZER_DOCUMENT_SCHEMA_VERSION)) &&
          add(document, "metadata", metadata_json(input_path, notes, report)) &&
          add(document, "meter", meter_json(notes)) && add(document, "keys", keys_json(notes, report)) &&
          add(document, "tonicizations", tonicizations_json(notes, report)) &&
          add(document, "modulations", modulations_json(notes, report)) &&
          add(document, "pivot_chords", pivot_chords_json(notes, report)) &&
          add(document, "chords", chords_json(notes, report)) &&
          add(document, "chord_vocabulary",
              chord_vocabulary_json(report, cJSON_GetObjectItemCaseSensitive(document, "chords"))) &&
          add(document, "non_chord_tones", non_chord_tones_json(notes, report)) &&
          add(document, "outer_voices", outer_voices_json(notes, report)) &&
          add(document, "cadences", cJSON_CreateArray()) &&
          add(document, "phrases", cJSON_CreateArray()) && add(document, "form", cJSON_CreateNull()) &&
          add(document, "motives", cJSON_CreateArray()) && add(document, "melody", cJSON_CreateArray()) &&
          add(document, "rhythm", cJSON_CreateArray()) && add(document, "texture", cJSON_CreateArray()) &&
          add(document, "voice_leading", cJSON_CreateArray()) && add(document, "schemas", cJSON_CreateArray()) &&
          add(document, "notes", notes_json(notes, report)))) {
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
