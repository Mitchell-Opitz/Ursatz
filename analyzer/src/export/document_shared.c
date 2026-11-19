/* Shared by every area of the analysis document: the constants, the rational time, the shape of a claim, the meter, the
   metadata and the input, and the helpers more than one area uses. */

#include "export/analysis_document_internal.h"

/* Source ids for the meter: a time signature the MIDI file declares or the score notates, and
   the 4/4 the library assumes when it declares none. */
static const char TIME_SIGNATURE_SOURCE[] = "midi-time-signature";

static const char NOTATED_TIME_SIGNATURE_SOURCE[] = "musicxml-time-signature";

static const char DEFAULT_METER_SOURCE[] = "ursatz-default-meter";

/* The meter a notation reading sets, replacing the input's. */
static const char READING_METER_SOURCE[] = "notation-reading-meter";

Rational doc_rational(DocBuild *build, long long num, unsigned long long den) {
    Rational value = {0, 1};
    if (den > (unsigned long long)LLONG_MAX || rational_make(num, (long long)den, &value) != RATIONAL_OK) {
        build->overflowed = true;
        return (Rational){0, 1};
    }
    return value;
}

Rational doc_add(DocBuild *build, Rational a, Rational b) {
    Rational sum = {0, 1};
    if (rational_add(a, b, &sum) != RATIONAL_OK) {
        build->overflowed = true;
    }
    return sum;
}

Rational doc_sub(DocBuild *build, Rational a, Rational b) {
    Rational difference = {0, 1};
    if (rational_sub(a, b, &difference) != RATIONAL_OK) {
        build->overflowed = true;
    }
    return difference;
}

/* length measured in beats of the given duration. */
Rational doc_in_beats(DocBuild *build, Rational length, const Duration *beat) {
    Rational beats = {0, 1};
    if (rational_div(length, doc_rational(build, beat->value.num, (unsigned long long)beat->value.den), &beats) !=
        RATIONAL_OK) {
        build->overflowed = true;
    }
    return beats;
}

bool doc_time_before(Rational a, Rational b) { return rational_cmp(a, b) < 0; }

Rational doc_note_onset(const NoteEvent *event) {
    return temporal_anchor_position(note_event_temporal_anchor(event))->value;
}

Rational doc_note_end(DocBuild *build, const NoteEvent *event) {
    return doc_add(build, doc_note_onset(event), note_event_duration(event)->value);
}

/* Earliest onset to latest note end over the given notes (ids NULL means
   every imported note). */
void doc_notes_span(DocBuild *build, const ImportedScore *notes, const size_t *ids, size_t id_count, Rational *start, Rational *end) {
    *start = (Rational){0, 1};
    *end = (Rational){0, 1};
    for (size_t i = 0; i < id_count; i++) {
        const NoteEvent *event = notes->events[ids ? ids[i] : i];
        Rational onset = doc_note_onset(event);
        Rational finish = doc_note_end(build, event);
        if (i == 0 || doc_time_before(onset, *start)) {
            *start = onset;
        }
        if (i == 0 || doc_time_before(*end, finish)) {
            *end = finish;
        }
    }
}

/* Adds item under key, taking ownership; false (item freed) on failure. */
cJSON *doc_rational_json(Rational value) {
    cJSON *object = cJSON_CreateObject();
    if (object && !(json_add(object, "num", cJSON_CreateNumber((double)value.num)) &&
                    json_add(object, "den", cJSON_CreateNumber((double)value.den)))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

cJSON *doc_span_json(Rational start, Rational end) {
    cJSON *object = cJSON_CreateObject();
    if (object && !(json_add(object, "start", doc_rational_json(start)) && json_add(object, "end", doc_rational_json(end)))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

cJSON *doc_note_ids_json(const size_t *ids, size_t id_count) {
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

cJSON *doc_uncertainty_json(AnalysisConfidence confidence) {
    if (!confidence.present || confidence.value.den <= 0) {
        return cJSON_CreateNull();
    }
    return cJSON_CreateNumber(rational_to_double(confidence.value));
}

/* Measure and beat of one time on the piece's measure grid:
   {"measure", "beat", "subdivision"}. Measures are numbered as the
   library numbers them (the first full measure is 1, a pickup is 0).
   "beat" is 1-based; "subdivision" is the exact fraction of that beat
   past its start, as a rational (0/1 on the beat). Null when the
   position cannot be read, e.g. a signature with no beat grid. */
cJSON *doc_position_json(DocBuild *build, const MeterMap *map, Rational time) {
    MusicalTime musical_time = musical_time_of(time);
    MetricalPosition *position = NULL;
    MetricalPositionResult result = metrical_position_from_time(map, &musical_time, &position);
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
                             doc_rational_json(doc_rational(build, beat.subdivision_numerator, beat.subdivision_denominator))))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* Where a span starts and ends on the measure grid. The end is the
   position of the span's (exclusive) end time, so a span that ends on a
   barline ends at beat 1 of the next measure. */
cJSON *doc_location_json(DocBuild *build, const MeterMap *map, Rational start, Rational end) {
    cJSON *object = cJSON_CreateObject();
    if (object &&
        !(json_add(object, "start", doc_position_json(build, map, start)) && json_add(object, "end", doc_position_json(build, map, end)))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* The shared claim fields, in schema order, with the uncertainty as built by the caller (taken over, freed on
   failure). ids NULL means every imported note. */
cJSON *doc_claim_json_with(DocBuild *build, const char *framework, const ImportedScore *notes, const size_t *ids, size_t id_count,
                              Rational start, Rational end, const char *label, cJSON *uncertainty) {
    cJSON *claim = cJSON_CreateObject();
    bool consumed = false; /* json_add() takes the uncertainty over, and frees it itself when it fails */
    if (claim && !(json_add(claim, "framework", cJSON_CreateString(framework)) &&
                   json_add(claim, "span", doc_span_json(start, end)) &&
                   json_add(claim, "location", doc_location_json(build, notes->meter.meter_map, start, end)) &&
                   json_add(claim, "note_ids", doc_note_ids_json(ids, id_count)) &&
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

cJSON *doc_claim_json_at(DocBuild *build, const char *framework, const ImportedScore *notes, const size_t *ids, size_t id_count,
                            Rational start, Rational end, const char *label, AnalysisConfidence confidence) {
    return doc_claim_json_with(build, framework, notes, ids, id_count, start, end, label, doc_uncertainty_json(confidence));
}

static cJSON *claim_json(DocBuild *build, const char *framework, const ImportedScore *notes, const size_t *ids, size_t id_count,
                         const char *label, AnalysisConfidence confidence) {
    Rational start;
    Rational end;
    doc_notes_span(build, notes, ids, id_count, &start, &end);
    return doc_claim_json_at(build, framework, notes, ids, id_count, start, end, label, confidence);
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
    MusicalTime end;
    const TimeSignature *reading = NULL;
    if (!meter_map_region_end(region, &end) ||
        metrical_position_reading_signature(map, 0, &reading) != METRICAL_POSITION_OK) {
        return NULL;
    }
    Rational length = end.value;

    cJSON *beats = NULL;
    Duration beat;
    if (standard_meter_beat_start(reading, 1, &beat)) {
        beats = doc_rational_json(doc_in_beats(build, length, &beat));
    } else {
        beats = cJSON_CreateNull();
    }

    cJSON *pickup = cJSON_CreateObject();
    if (pickup && !(json_add(pickup, "length", doc_rational_json(length)) && json_add(pickup, "beats", beats))) {
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
        MusicalTime doc_region_start = meter_map_region_start(region);
        cJSON *change = cJSON_CreateObject();
        if (!(change &&
              json_add(change, "measure", cJSON_CreateNumber((double)meter_map_region_first_measure_number(region))) &&
              json_add(change, "start", doc_rational_json(doc_region_start.value)) &&
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
static cJSON *reading_json(DocBuild *build, const ImportedScore *notes) {
    if (!notes->source.reading_applied) {
        return cJSON_CreateNull();
    }
    const ImportedReading *reading = &notes->source.reading;
    cJSON *object = cJSON_CreateObject();
    if (object && !(json_add(object, "scale",
                             doc_rational_json(doc_rational(build, (long)reading->scale.num, (unsigned long)reading->scale.den))) &&
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
cJSON *doc_meter_json(DocBuild *build, const ImportedScore *notes) {
    const MeterMap *map = notes->meter.meter_map;
    bool inferred = meter_map_is_defaulted(map);
    bool has_pickup = meter_map_has_pickup(map);
    size_t primary_index = has_pickup ? 1 : 0;
    const TimeSignature *time_signature = meter_map_region_time_signature(meter_map_region(map, primary_index));

    size_t measure_count = 0;
    if (meter_map_measure_count(map, &notes->meter.end_time, &measure_count) != METER_MAP_OK) {
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
    cJSON *meter = claim_json(build, claim_source, notes, NULL, notes->count, label, (AnalysisConfidence){false, {0, 1}});
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

const char *doc_library_framework_name(void) {
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
static cJSON *key_signatures_json(DocBuild *build, const ImportedScore *notes) {
    cJSON *signatures = cJSON_CreateArray();
    if (!signatures) {
        return NULL;
    }
    for (size_t i = 0; i < notes->key_signature_events.count; i++) {
        const MidiKeySignatureEvent *event = &notes->key_signature_events.events[i];
        int major = chain_key_signature_major_tonic(event->sharps_flats);
        char *major_label = chain_key_label(major, build->materials.major);
        char *minor_label = chain_key_label((major + 9) % 12, build->materials.natural_minor);
        Rational start = event->position.value;
        cJSON *object = cJSON_CreateObject();
        bool ok = major_label && minor_label && object &&
                  json_add(object, "start", doc_rational_json(start)) &&
                  json_add(object, "location", doc_position_json(build, notes->meter.meter_map, start)) &&
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

static cJSON *input_parts_json(const ImportedScore *notes) {
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
static cJSON *input_json(const ImportedScore *notes) {
    const ImportedSource *source = &notes->source;
    bool score = source->format == IMPORTED_FORMAT_MUSICXML;
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

cJSON *doc_metadata_json(DocBuild *build, const char *input_path, const ImportedScore *notes, const ChainResult *report) {
    cJSON *metadata = cJSON_CreateObject();
    if (metadata &&
        !(json_add(metadata, "source_file", cJSON_CreateString(path_basename(input_path))) &&
          json_add(metadata, "analyzer_version", cJSON_CreateString(URSATZ_ANALYZER_VERSION)) &&
          json_add(metadata, "producer_key", cJSON_CreateString(ursatz_analyzer_producer_key())) &&
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

Rational doc_region_start(DocBuild *build, const AnalysisKeyRegion *region) {
    return doc_rational(build, region->start.num, region->start.den);
}

Rational doc_region_end(DocBuild *build, const AnalysisKeyRegion *region) {
    return doc_rational(build, region->end.num, region->end.den);
}

cJSON *doc_measures_json(int first, int last) {
    cJSON *object = cJSON_CreateObject();
    if (object && !(json_add(object, "first", cJSON_CreateNumber(first)) && json_add(object, "last", cJSON_CreateNumber(last)))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

bool doc_change_is_tonicization(const AnalysisKeyChange *change) {
    return change->extent == ANALYSIS_KEY_CHANGE_TONICIZATION ||
           change->extent == ANALYSIS_KEY_CHANGE_EXTENDED_TONICIZATION;
}

/* The span's length in beats of the signature its start is read in: a rational, or null when the
   position cannot be read or the signature has no beat grid. Segments do not cross a barline, so one
   signature covers the whole span. */
cJSON *doc_span_beats_json(DocBuild *build, const MeterMap *map, Rational start, Rational end) {
    MusicalTime musical_time = musical_time_of(start);
    MetricalPosition *position = NULL;
    MetricalPositionResult result = metrical_position_from_time(map, &musical_time, &position);
    if (result != METRICAL_POSITION_OK) {
        return cJSON_CreateNull();
    }
    const TimeSignature *reading = NULL;
    Duration beat;
    bool readable = metrical_position_reading_signature(map, metrical_position_measure(position), &reading) ==
                        METRICAL_POSITION_OK &&
                    standard_meter_beat_start(reading, 1, &beat);
    metrical_position_destroy(position);
    if (!readable) {
        return cJSON_CreateNull();
    }

    Rational length = doc_sub(build, end, start);
    return doc_rational_json(doc_in_beats(build, length, &beat));
}

cJSON *doc_optional_string_json(const char *value) {
    return value ? cJSON_CreateString(value) : cJSON_CreateNull();
}

const char *doc_outer_status_name(AnalysisOuterStatus status) {
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
cJSON *doc_outer_uncertainty_json(const ChainResult *report, const ChainEntry *entry,
                                     const AnalysisOuterNote *note) {
    if (note->status != ANALYSIS_OUTER_PRESENT || !entry->in_region || entry->region >= report->key_region_count) {
        return cJSON_CreateNull();
    }
    return cJSON_CreateNumber(report->key_regions[entry->region].uncertainty);
}

/* The note ids of entries first..last, each once, in order. NULL on allocation failure (*count 0 with none). */
size_t *doc_entry_range_note_ids(const ChainResult *report, size_t first, size_t last, size_t *count) {
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

Rational doc_entry_start_time(DocBuild *build, const ChainEntry *entry) {
    return doc_rational(build, entry->time.num, entry->time.den);
}

Rational doc_entry_end_time(DocBuild *build, const ChainEntry *entry) {
    return doc_rational(build, entry->end.num, entry->end.den);
}

cJSON *doc_string_or_null_json(const char *text) {
    return text ? cJSON_CreateString(text) : cJSON_CreateNull();
}
