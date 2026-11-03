#include <ursatz_analyzer/composer_api.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/provenance_reference.h"
#include "core/voice_reference.h"
#include "identity/entity_id.h"
#include "io/midi_key_signature.h"
#include "pitch/pitch_arithmetic.h"
#include "pitch/pitch_identity.h"
#include "pitch/pitch_numeric_value.h"
#include "pitch/pitch_spelling.h"
#include "pitch/tuning_system.h"
#include "standard-tonality/tonal_key.h"
#include "theory/style_model_build.h"
#include "time/duration.h"
#include "time/metrical_position.h"
#include "time/note_value.h"
#include "time/temporal_anchor.h"
#include "time/time_signature.h"

#include <ursatz_analyzer/analysis_document.h>
#include <ursatz_analyzer/analysis_pipeline.h>
#include <ursatz_analyzer/style_profile.h>
#include <ursatz_analyzer/version.h>

#include "pipeline/analysis_result_internal.h"
#include "pipeline/counterpoint_pipeline.h"
#include "style/style_profile_internal.h"
#include "util/misc_util.h"

#define MIDI_NOTE_COUNT 128
#define FRAGMENT_INPUT_PATH "<fragment>"
#define FRAGMENT_PROVENANCE_ID "composer-fragment-v1"

/* How the MIDI importer spells a note: sharps only (io/midi_importer.c), the pitch class's place on the line of fifths
   from F (-1) to A# (10). */
static void sharps_only_spelling(int pitch_class, PitchLetter *letter, int *accidental) {
    int fifths = pitch_class * 7 % PITCH_SEMITONES_PER_OCTAVE;
    pitch_spelling_from_fifths(fifths == 11 ? -1 : fifths, letter, accidental);
}

/* The pitch identity of every MIDI note number, made the way the MIDI importer makes it. An event's pitch cannot be
   decoded (by design), so it is recovered by finding the note number whose identity equals it. */
typedef struct {
    TuningSystem *tuning;
    PitchIdentity *identities[MIDI_NOTE_COUNT];
} PitchTable;

static void pitch_table_destroy(PitchTable *table) {
    for (int n = 0; n < MIDI_NOTE_COUNT; n++) {
        pitch_identity_destroy(table->identities[n]);
    }
    tuning_system_destroy(table->tuning);
}

/* A new numeric value of the MIDI note number n, in the table's tuning system. */
static PitchNumericValue *table_value(const PitchTable *table, int n) {
    PitchNumericValue *value = NULL;
    return pitch_numeric_value_create(table->tuning, n, 1, &value) == PITCH_NUMERIC_VALUE_OK ? value : NULL;
}

static bool pitch_table_init(PitchTable *table) {
    *table = (PitchTable){0};
    EntityID *id = NULL;
    if (entity_id_create("midi-note-number", &id) != ENTITY_ID_OK) {
        return false;
    }
    if (tuning_system_create(id, "MIDI note number", &table->tuning) != TUNING_SYSTEM_OK) {
        entity_id_destroy(id);
        return false;
    }
    for (int n = 0; n < MIDI_NOTE_COUNT; n++) {
        PitchNumericValue *value = table_value(table, n);
        bool ok = value && pitch_numeric_value_to_identity(value, &table->identities[n]) ==
                               PITCH_NUMERIC_VALUE_TO_IDENTITY_OK;
        pitch_numeric_value_destroy(value);
        if (!ok) {
            pitch_table_destroy(table);
            return false;
        }
    }
    return true;
}

static int pitch_table_find(const PitchTable *table, const PitchIdentity *pitch) {
    for (int n = 0; n < MIDI_NOTE_COUNT; n++) {
        if (pitch_identity_equals(table->identities[n], pitch)) {
            return n;
        }
    }
    return -1;
}

/* A copy of the event, with the pitch rebuilt from its MIDI note number (the numeric value the importer would have
   computed goes to *out_value). The copy takes the event's spelling, voice and provenance when it has them. */
static AnalyzerFragmentResult copy_event(const NoteEvent *event, const PitchTable *table, NoteEvent **out,
                                         PitchNumericValue **out_value) {
    int midi = pitch_table_find(table, note_event_pitch(event));
    if (midi < 0) {
        return ANALYZER_FRAGMENT_ERR_PITCH;
    }
    const MusicalTime *at = temporal_anchor_position(note_event_temporal_anchor(event));
    const Duration *length = note_event_duration(event);
    const PitchSpelling *event_spelling = note_event_optional_pitch_spelling(event);
    const VoiceReference *event_voice = note_event_optional_voice(event);
    const ProvenanceReference *event_provenance = note_event_provenance(event);

    EntityID *id = entity_id_copy(note_event_id(event));
    MusicalTime *position = NULL;
    TemporalAnchor *anchor = NULL;
    Duration *duration = NULL;
    PitchIdentity *pitch = NULL;
    PitchSpelling *spelling = NULL;
    VoiceReference *voice = NULL;
    ProvenanceReference *provenance = NULL;
    PitchNumericValue *value = table_value(table, midi);
    bool ok = id && value;

    ok = ok && musical_time_create(musical_time_numerator(at), musical_time_denominator(at), &position) ==
                   MUSICAL_TIME_OK;
    if (ok && temporal_anchor_create(position, &anchor) == TEMPORAL_ANCHOR_OK) {
        position = NULL; /* the anchor owns it */
    } else {
        ok = false;
    }
    ok = ok && duration_create(duration_numerator(length), duration_denominator(length), &duration) == DURATION_OK;
    ok = ok && pitch_numeric_value_to_identity(value, &pitch) == PITCH_NUMERIC_VALUE_TO_IDENTITY_OK;
    if (ok) {
        if (event_spelling) {
            ok = pitch_spelling_create(pitch_spelling_letter(event_spelling), pitch_spelling_accidental(event_spelling),
                                       pitch_spelling_octave(event_spelling), &spelling) == PITCH_SPELLING_OK;
        } else {
            PitchLetter letter;
            int accidental;
            sharps_only_spelling(midi % PITCH_SEMITONES_PER_OCTAVE, &letter, &accidental);
            ok = pitch_spelling_create(letter, accidental, midi / PITCH_SEMITONES_PER_OCTAVE - 1, &spelling) ==
                 PITCH_SPELLING_OK;
        }
    }
    if (ok && event_voice) {
        EntityID *voice_id = entity_id_copy(voice_reference_target_id(event_voice));
        ok = voice_id && voice_reference_create(voice_id, &voice) == VOICE_REFERENCE_OK;
        if (voice_id && !voice) {
            entity_id_destroy(voice_id);
        }
    }
    if (ok) {
        if (provenance_reference_state(event_provenance) == PROVENANCE_REFERENCE_PRESENT) {
            EntityID *target = entity_id_copy(provenance_reference_target_id(event_provenance));
            ok = target && provenance_reference_create_present(target, &provenance) == PROVENANCE_REFERENCE_OK;
            if (target && !provenance) {
                entity_id_destroy(target);
            }
        } else {
            ok = provenance_reference_create_absent(&provenance) == PROVENANCE_REFERENCE_OK;
        }
    }

    NoteEvent *copy = NULL;
    if (ok && note_event_create(id, anchor, duration, pitch, spelling, voice, provenance, &copy) == NOTE_EVENT_OK) {
        *out = copy;
        *out_value = value;
        return ANALYZER_FRAGMENT_OK;
    }
    /* note_event_create takes ownership only on success */
    entity_id_destroy(id);
    musical_time_destroy(position);
    temporal_anchor_destroy(anchor);
    duration_destroy(duration);
    pitch_identity_destroy(pitch);
    pitch_spelling_destroy(spelling);
    voice_reference_destroy(voice);
    provenance_reference_destroy(provenance);
    pitch_numeric_value_destroy(value);
    return ANALYZER_FRAGMENT_ERR_ALLOC;
}

/* A copy of the meter map, or a defaulted 4/4 map for NULL or a defaulted one. NULL on allocation failure. */
static MeterMap *copy_meter_map(const MeterMap *map) {
    EntityID *id = NULL;
    MeterMap *copy = NULL;
    if (!map || meter_map_is_defaulted(map)) {
        if (entity_id_create("fragment-meter", &id) != ENTITY_ID_OK) {
            return NULL;
        }
        if (meter_map_create_defaulted(id, &copy) != METER_MAP_OK) {
            entity_id_destroy(id);
            return NULL;
        }
        return copy;
    }
    size_t count = meter_map_region_count(map);
    MeterMapChangePoint *points = calloc(count > 0 ? count : 1, sizeof *points);
    TimeSignature **signatures = calloc(count > 0 ? count : 1, sizeof *signatures);
    if (!points || !signatures) {
        free(points);
        free(signatures);
        return NULL;
    }
    bool ok = true;
    for (size_t i = 0; ok && i < count; i++) {
        const MeterMapRegion *region = meter_map_region(map, i);
        const TimeSignature *source = meter_map_region_time_signature(region);
        NoteValue *unit = NULL;
        ok = note_value_create(time_signature_bottom_unit_exponent(source), &unit) == NOTE_VALUE_OK;
        ok = ok && time_signature_create(time_signature_top_number(source), unit, &signatures[i]) == TIME_SIGNATURE_OK;
        note_value_destroy(unit);
        points[i].time_signature = signatures[i];
        points[i].start = meter_map_region_start(region);
    }
    ok = ok && entity_id_create("fragment-meter", &id) == ENTITY_ID_OK;
    if (ok && meter_map_create(id, points, count, &copy) != METER_MAP_OK) {
        entity_id_destroy(id);
        copy = NULL;
    }
    for (size_t i = 0; i < count; i++) {
        time_signature_destroy(signatures[i]);
    }
    free(points);
    free(signatures);
    return copy;
}

/* The key signature event at time 0 from a tonic and a mode, shaped as the MIDI scan gives them. */
static bool add_key_signature(ImportedNotes *notes, int tonic, bool minor) {
    PitchLetter letter;
    int accidental;
    int fifths;
    if (!standard_tonality_key_tonic_of_pitch_class(tonic, minor, &letter, &accidental) ||
        !standard_tonality_key_fifths(letter, accidental, minor, &fifths)) {
        return false;
    }
    MidiKeySignatureEvent *event = calloc(1, sizeof *event);
    if (!event) {
        return false;
    }
    if (midi_key_signature_build(fifths, &event->key_signature) != MIDI_KEY_SIGNATURE_BUILD_OK ||
        musical_time_create(0, 1, &event->position) != MUSICAL_TIME_OK) {
        key_signature_destroy(event->key_signature);
        free(event);
        return false;
    }
    event->mode_hint = minor ? MIDI_MODE_HINT_MINOR : MIDI_MODE_HINT_MAJOR;
    event->tick = 0;
    event->track_index = 0;
    event->sharps_flats = (int8_t)fifths;
    event->mode_byte = minor ? 1 : 0;
    notes->key_signature_events.events = event;
    notes->key_signature_events.count = 1;
    notes->key_signature = (MidiKeySignature){true, (int8_t)fifths, minor};
    return true;
}

static AnalyzerFragmentResult fragment_to_notes(const AnalyzerFragment *fragment, ImportedNotes *notes) {
    *notes = (ImportedNotes){0};
    notes->source = (ImportedSource){.format = INPUT_FORMAT_MIDI};
    size_t slots = fragment->count > 0 ? fragment->count : 1;
    notes->events = calloc(slots, sizeof *notes->events);
    notes->pitch_values = calloc(slots, sizeof *notes->pitch_values);
    notes->streams = calloc(slots, sizeof *notes->streams);
    PitchTable table;
    if (!notes->events || !notes->pitch_values || !notes->streams || !pitch_table_init(&table)) {
        imported_notes_destroy(notes);
        return ANALYZER_FRAGMENT_ERR_ALLOC;
    }
    AnalyzerFragmentResult result = ANALYZER_FRAGMENT_OK;
    for (size_t i = 0; i < fragment->count; i++) {
        result = copy_event(fragment->events[i], &table, &notes->events[i], &notes->pitch_values[i]);
        if (result != ANALYZER_FRAGMENT_OK) {
            break;
        }
        notes->count = i + 1;
        notes->streams[i] = fragment->streams ? fragment->streams[i] : 0;
    }
    pitch_table_destroy(&table);
    if (result == ANALYZER_FRAGMENT_OK) {
        notes->meter.meter_map = copy_meter_map(fragment->meter);
        notes->meter.end_time = notes_last_end((const NoteEvent *const *)notes->events, notes->count);
        if (!notes->meter.meter_map || !notes->meter.end_time) {
            result = ANALYZER_FRAGMENT_ERR_ALLOC;
        } else if (fragment->has_key && !add_key_signature(notes, fragment->key_tonic_pitch_class,
                                                           fragment->key_minor)) {
            result = ANALYZER_FRAGMENT_ERR_ALLOC;
        }
    }
    if (result != ANALYZER_FRAGMENT_OK) {
        imported_notes_destroy(notes);
    }
    return result;
}

AnalyzerFragmentResult analyzer_analyze_fragment(const AnalyzerFragment *fragment, AnalyzerFragmentAnalysis *out) {
    if (!out) {
        return ANALYZER_FRAGMENT_ERR_ARGUMENT;
    }
    *out = (AnalyzerFragmentAnalysis){0};
    if (!fragment || (fragment->count > 0 && !fragment->events) ||
        (fragment->has_key && (fragment->key_tonic_pitch_class < 0 || fragment->key_tonic_pitch_class > 11))) {
        return ANALYZER_FRAGMENT_ERR_ARGUMENT;
    }
    for (size_t i = 0; i < fragment->count; i++) {
        if (!fragment->events[i]) {
            return ANALYZER_FRAGMENT_ERR_ARGUMENT;
        }
    }
    AnalyzerFragmentResult result = fragment_to_notes(fragment, &out->notes);
    if (result != ANALYZER_FRAGMENT_OK) {
        *out = (AnalyzerFragmentAnalysis){0};
        return result;
    }
    AnalyzerStatus status = analyzer_analyze(&out->notes, NULL, &out->result);
    if (status != ANALYZER_OK) {
        imported_notes_destroy(&out->notes);
        *out = (AnalyzerFragmentAnalysis){0};
        return ANALYZER_FRAGMENT_ERR_ALLOC;
    }
    out->document_json = analysis_document_render_result(FRAGMENT_INPUT_PATH, out->result);
    if (!out->document_json) {
        analyzer_result_destroy(out->result);
        imported_notes_destroy(&out->notes);
        *out = (AnalyzerFragmentAnalysis){0};
        return ANALYZER_FRAGMENT_ERR_ALLOC;
    }
    out->valid = true;
    return ANALYZER_FRAGMENT_OK;
}

/* ---- the hearing entry ---- */

/* What a hearing builds for the pass: an ImportedNotes whose events are the caller's. */
typedef struct {
    ImportedNotes notes;
    bool owns_meter;
} HearingNotes;

static void hearing_notes_destroy(void *owner) {
    HearingNotes *hearing = owner;
    if (!hearing) {
        return;
    }
    ImportedNotes *notes = &hearing->notes;
    for (size_t i = 0; i < notes->count; i++) {
        pitch_numeric_value_destroy(notes->pitch_values[i]); /* the events stay the caller's */
    }
    if (!hearing->owns_meter) {
        notes->meter.meter_map = NULL;
    }
    notes->count = 0;
    free(notes->events);
    notes->events = NULL;
    imported_notes_destroy(notes);
    free(hearing);
}

static bool hearing_valid(const AnalyzerHearing *hearing) {
    if (!hearing || (hearing->count > 0 && (!hearing->events || !hearing->midi)) ||
        (hearing->has_key && (hearing->key_tonic_pitch_class < 0 || hearing->key_tonic_pitch_class > 11))) {
        return false;
    }
    for (size_t i = 0; i < hearing->count; i++) {
        if (!hearing->events[i] || hearing->midi[i] < 0 || hearing->midi[i] >= MIDI_NOTE_COUNT) {
            return false;
        }
    }
    return true;
}

/* The notes of a hearing: the arrays are the hearing's, the events the caller's. NULL on allocation failure. */
static HearingNotes *hearing_notes_build(const AnalyzerHearing *hearing) {
    HearingNotes *built = calloc(1, sizeof *built);
    if (!built) {
        return NULL;
    }
    ImportedNotes *notes = &built->notes;
    notes->source = (ImportedSource){.format = INPUT_FORMAT_MIDI};
    size_t slots = hearing->count > 0 ? hearing->count : 1;
    notes->events = calloc(slots, sizeof *notes->events);
    notes->pitch_values = calloc(slots, sizeof *notes->pitch_values);
    notes->streams = calloc(slots, sizeof *notes->streams);
    EntityID *tuning_id = NULL;
    TuningSystem *tuning = NULL;
    bool ok = notes->events && notes->pitch_values && notes->streams &&
              entity_id_create("midi-note-number", &tuning_id) == ENTITY_ID_OK;
    if (ok && tuning_system_create(tuning_id, "MIDI note number", &tuning) != TUNING_SYSTEM_OK) {
        entity_id_destroy(tuning_id);
        ok = false;
    }
    for (size_t i = 0; ok && i < hearing->count; i++) {
        ok = pitch_numeric_value_create(tuning, hearing->midi[i], 1, &notes->pitch_values[i]) ==
             PITCH_NUMERIC_VALUE_OK;
        notes->events[i] = (NoteEvent *)hearing->events[i];
        notes->streams[i] = hearing->streams ? hearing->streams[i] : 0;
        notes->count = i + 1;
    }
    tuning_system_destroy(tuning);
    if (ok) {
        built->owns_meter = !hearing->meter || meter_map_is_defaulted(hearing->meter);
        notes->meter.meter_map = built->owns_meter ? copy_meter_map(NULL) : (MeterMap *)hearing->meter;
        notes->meter.end_time = notes_last_end(hearing->events, hearing->count);
        ok = notes->meter.meter_map && notes->meter.end_time;
    }
    if (ok && hearing->has_key) {
        ok = add_key_signature(notes, hearing->key_tonic_pitch_class, hearing->key_minor);
    }
    if (!ok) {
        hearing_notes_destroy(built);
        return NULL;
    }
    return built;
}

AnalyzerHearingOptions analyzer_hearing_options_default(void) {
    AnalyzerHearingOptions options = {analyzer_options_default(), false};
    return options;
}

AnalyzerStatus analyzer_hear(const AnalyzerHearing *hearing, const AnalyzerHearingOptions *options,
                             AnalyzerResult **out) {
    if (!out) {
        return ANALYZER_ERR_ARGUMENT;
    }
    *out = NULL;
    AnalyzerHearingOptions used = options ? *options : analyzer_hearing_options_default();
    if (!hearing_valid(hearing) || (used.render_document && used.analysis.stages != ANALYZER_STAGE_ALL)) {
        return ANALYZER_ERR_ARGUMENT;
    }
    HearingNotes *built = hearing_notes_build(hearing);
    if (!built) {
        return ANALYZER_ERR_ALLOC;
    }
    AnalyzerResult *result = NULL;
    AnalyzerStatus status = analyzer_analyze(&built->notes, &used.analysis, &result);
    if (status != ANALYZER_OK) {
        hearing_notes_destroy(built);
        return status;
    }
    analyzer_result_set_owner(result, built, hearing_notes_destroy);
    if (used.render_document) {
        char *json = analysis_document_render_result(FRAGMENT_INPUT_PATH, result);
        if (!json) {
            analyzer_result_destroy(result);
            return ANALYZER_ERR_ALLOC;
        }
        analyzer_result_set_document(result, json);
    }
    *out = result;
    return ANALYZER_OK;
}

void analyzer_fragment_analysis_destroy(AnalyzerFragmentAnalysis *analysis) {
    if (!analysis || !analysis->valid) {
        return;
    }
    free(analysis->document_json);
    analyzer_result_destroy(analysis->result); /* before the notes it borrows */
    imported_notes_destroy(&analysis->notes);
    *analysis = (AnalyzerFragmentAnalysis){0};
}

const AnalysisReport *analyzer_fragment_report(const AnalyzerFragmentAnalysis *analysis) {
    return analysis && analysis->valid ? analyzer_result_report(analysis->result) : NULL;
}

const char *analyzer_fragment_document_json(const AnalyzerFragmentAnalysis *analysis) {
    return analysis && analysis->valid ? analysis->document_json : NULL;
}

AnalyzerFragmentResult analyzer_fragment_check_counterpoint(AnalyzerFragmentAnalysis *analysis, int species,
                                                            bool cantus_upper) {
    if (!analysis || !analysis->valid || species < 1 || species > 5) {
        return ANALYZER_FRAGMENT_ERR_ARGUMENT;
    }
    CounterpointCheckResult checked =
        counterpoint_check_collect(&analysis->notes, species, cantus_upper,
                                   analyzer_result_report_mutable(analysis->result));
    if (checked == COUNTERPOINT_CHECK_ERR_ALLOC) {
        return ANALYZER_FRAGMENT_ERR_ALLOC;
    }
    return checked == COUNTERPOINT_CHECK_OK ? ANALYZER_FRAGMENT_OK : ANALYZER_FRAGMENT_ERR_ARGUMENT;
}

void analyzer_violations_free(AnalyzerViolation *violations, size_t count) {
    for (size_t i = 0; violations && i < count; i++) {
        free(violations[i].rule);
        free(violations[i].note_ids);
    }
    free(violations);
}

/* The measure and beat of a time in whole notes on the fragment's meter; 0 and 0.0 when it cannot be placed. */
static void place(const MeterMap *map, long numerator, unsigned long denominator, int *measure, double *beat) {
    *measure = 0;
    *beat = 0.0;
    MusicalTime *time = NULL;
    MetricalPosition *position = NULL;
    const TimeSignature *signature = NULL;
    Duration *unit = NULL;
    if (musical_time_create(numerator, denominator, &time) == MUSICAL_TIME_OK) {
        if (metrical_position_from_time(map, time, &position) == METRICAL_POSITION_OK) {
            int m = metrical_position_measure(position);
            const Duration *offset = metrical_position_offset(position);
            if (metrical_position_reading_signature(map, m, &signature) == METRICAL_POSITION_OK &&
                time_signature_unit_duration(signature, &unit) == DURATION_OK && duration_numerator(unit) > 0) {
                *measure = m;
                *beat = 1.0 + ((double)duration_numerator(offset) / (double)duration_denominator(offset)) /
                                  ((double)duration_numerator(unit) / (double)duration_denominator(unit));
            }
        }
    }
    duration_destroy(unit);
    metrical_position_destroy(position);
    musical_time_destroy(time);
}

/* Appends one violation; the rule and the note ids are copied. false on allocation failure. */
static bool add_violation(const MeterMap *map, AnalyzerViolation **list, size_t *count, size_t *capacity, const char *rule, bool hard, long numerator, unsigned long denominator,
                          const size_t *note_ids, size_t note_id_count) {
    if (*count == *capacity) {
        size_t grown = *capacity ? *capacity * 2 : 8;
        AnalyzerViolation *bigger = realloc(*list, grown * sizeof **list);
        if (!bigger) {
            return false;
        }
        *list = bigger;
        *capacity = grown;
    }
    AnalyzerViolation *violation = &(*list)[*count];
    *violation = (AnalyzerViolation){0};
    violation->rule = strdup(rule);
    violation->note_ids = malloc((note_id_count > 0 ? note_id_count : 1) * sizeof *violation->note_ids);
    if (!violation->rule || !violation->note_ids) {
        free(violation->rule);
        free(violation->note_ids);
        return false;
    }
    memcpy(violation->note_ids, note_ids, note_id_count * sizeof *note_ids);
    violation->note_id_count = note_id_count;
    violation->hard = hard;
    place(map, numerator, denominator, &violation->measure, &violation->beat);
    (*count)++;
    return true;
}

static const char *voice_rule_id(const AnalysisVoiceViolation *fault) {
    if (strcmp(fault->rule, "parallel-fifths") == 0 || strcmp(fault->rule, "parallel-octaves") == 0) {
        return fault->rule;
    }
    return fault->arrival_interval == 5 ? "direct-fifths" : "direct-octaves";
}

AnalyzerStatus analyzer_result_violations(const AnalyzerResult *result, AnalyzerViolation **out, size_t *count) {
    if (!result || !out || !count) {
        return ANALYZER_ERR_ARGUMENT;
    }
    *out = NULL;
    *count = 0;
    const AnalysisReport *report = analyzer_result_report(result);
    const ImportedNotes *notes = analyzer_result_notes(result);
    if (!report || !notes) {
        return ANALYZER_ERR_ARGUMENT;
    }
    const MeterMap *map = notes->meter.meter_map;
    const AnalysisVoiceLeading *leading = &report->voice_leading;
    AnalyzerViolation *list = NULL;
    size_t used = 0, capacity = 0;
    bool ok = true;
    for (size_t i = 0; ok && leading->present && i < leading->violation_count; i++) {
        const AnalysisVoiceViolation *fault = &leading->violations[i];
        const AnalysisReportEntry *arrival = &report->entries[fault->entry_to];
        bool parallel = fault->rule[0] == 'p';
        ok = add_violation(map, &list, &used, &capacity, voice_rule_id(fault), parallel, arrival->time_numerator,
                           arrival->time_denominator, fault->note_ids, 4);
    }
    for (int pass = 0; ok && pass < 2; pass++) {
        const char *wanted = pass == 0 ? "seventh-resolves-down" : "leading-tone-resolves";
        for (size_t i = 0; ok && leading->present && i < leading->resolution_count; i++) {
            const AnalysisVoiceResolution *fault = &leading->resolutions[i];
            if (strcmp(fault->rule, wanted) != 0) {
                continue;
            }
            const AnalysisReportEntry *arrival = &report->entries[fault->entry_to];
            ok = add_violation(map, &list, &used, &capacity,
                               pass == 0 ? "unresolved-seventh" : "unresolved-leading-tone", false,
                               arrival->time_numerator, arrival->time_denominator, fault->note_ids, 2);
        }
    }
    const AnalysisCounterpointCheck *check = &report->counterpoint_check;
    for (size_t i = 0; ok && check->present && i < check->violation_count; i++) {
        char rule[96];
        snprintf(rule, sizeof rule, "counterpoint:%s", check->violations[i].group);
        ok = add_violation(map, &list, &used, &capacity, rule, check->violations[i].hard, check->start_numerator,
                           check->start_denominator, check->note_ids, check->note_id_count);
    }
    if (!ok) {
        analyzer_violations_free(list, used);
        return ANALYZER_ERR_ALLOC;
    }
    *out = list;
    *count = used;
    return ANALYZER_OK;
}

AnalyzerFragmentResult analyzer_fragment_violations(const AnalyzerFragmentAnalysis *analysis, AnalyzerViolation **out,
                                                    size_t *count) {
    if (!analysis || !analysis->valid || !out || !count) {
        return ANALYZER_FRAGMENT_ERR_ARGUMENT;
    }
    AnalyzerStatus status = analyzer_result_violations(analysis->result, out, count);
    if (status == ANALYZER_OK) {
        return ANALYZER_FRAGMENT_OK;
    }
    return status == ANALYZER_ERR_ALLOC ? ANALYZER_FRAGMENT_ERR_ALLOC : ANALYZER_FRAGMENT_ERR_ARGUMENT;
}

/* A style model of the statistics. The caller destroys it. */
static StyleModel *model_of(const char *name, const StyleStatistic *statistics, size_t count) {
    EntityID *id = NULL;
    StyleModel *model = NULL;
    if (entity_id_create(name, &id) != ENTITY_ID_OK) {
        return NULL;
    }
    if (style_model_from_statistics(id, name, "<fragment>", URSATZ_ANALYZER_VERSION, statistics, count, &model) !=
        STYLE_MODEL_BUILD_OK) {
        entity_id_destroy(id);
        return NULL;
    }
    return model;
}

static bool has_category(const StyleStatistic *statistics, size_t count, const char *category) {
    for (size_t i = 0; i < count; i++) {
        if (strcmp(statistics[i].category, category) == 0) {
            return true;
        }
    }
    return false;
}

static bool is_dropped(const char *category, const char *const *dropped, size_t dropped_count) {
    for (size_t i = 0; i < dropped_count; i++) {
        if (strcmp(category, dropped[i]) == 0) {
            return true;
        }
    }
    return false;
}

/* Moves the statistics whose category is not dropped to the front of the array. Returns how many stay. */
static size_t keep_statistics(StyleStatistic *statistics, size_t count, const char *const *dropped,
                              size_t dropped_count) {
    size_t used = 0;
    for (size_t i = 0; i < count; i++) {
        if (!is_dropped(statistics[i].category, dropped, dropped_count)) {
            statistics[used++] = statistics[i];
        }
    }
    return used;
}

/* ---- the style reference ---- */

struct AnalyzerStyleReference {
    StyleStatistic *statistics; /* the model's, without the dropped categories */
    size_t count;
    StyleModel *base; /* a model of the statistics */
    char **without;   /* the dropped categories */
    size_t without_count;
};

void analyzer_style_reference_destroy(AnalyzerStyleReference *reference) {
    if (!reference) {
        return;
    }
    style_model_destroy(reference->base);
    style_statistics_free(reference->statistics, reference->count);
    for (size_t i = 0; i < reference->without_count; i++) {
        free(reference->without[i]);
    }
    free(reference->without);
    free(reference);
}

AnalyzerStatus analyzer_style_reference_create(const StyleModel *model, const char *const *without,
                                               size_t without_count, AnalyzerStyleReference **out) {
    if (!out) {
        return ANALYZER_ERR_ARGUMENT;
    }
    *out = NULL;
    if (!model || (without_count > 0 && !without)) {
        return ANALYZER_ERR_ARGUMENT;
    }
    for (size_t i = 0; i < without_count; i++) {
        StyleDimension dimension;
        if (!without[i] || !style_category_dimension(without[i], &dimension)) {
            return ANALYZER_ERR_ARGUMENT;
        }
    }
    AnalyzerStyleReference *reference = calloc(1, sizeof *reference);
    if (!reference) {
        return ANALYZER_ERR_ALLOC;
    }
    reference->without = calloc(without_count > 0 ? without_count : 1, sizeof *reference->without);
    bool ok = reference->without != NULL;
    for (size_t i = 0; ok && i < without_count; i++) {
        reference->without[i] = strdup(without[i]);
        ok = reference->without[i] != NULL;
        reference->without_count = i + 1;
    }
    ok = ok && style_model_statistics(model, &reference->statistics, &reference->count) == STYLE_MODEL_FILE_OK;
    if (ok) {
        reference->count = keep_statistics(reference->statistics, reference->count,
                                           (const char *const *)reference->without, reference->without_count);
        reference->base = model_of("fragment-style-base", reference->statistics, reference->count);
        ok = reference->base != NULL;
    }
    if (!ok) {
        analyzer_style_reference_destroy(reference);
        return ANALYZER_ERR_ALLOC;
    }
    *out = reference;
    return ANALYZER_OK;
}

AnalyzerStatus analyzer_result_style_distance(const AnalyzerResult *result, const AnalyzerStyleReference *reference,
                                              StyleComparison *out) {
    if (!result || !reference || !out) {
        return ANALYZER_ERR_ARGUMENT;
    }
    memset(out, 0, sizeof *out);
    AnalyzerStatus status = ANALYZER_ERR_ALLOC;
    StyleProfile *profile = style_profile_create();
    StyleStatistic *fragment_stats = NULL, *kept = NULL;
    size_t fragment_count = 0, kept_count = 0;
    StyleModel *fragment_model = NULL, *kept_model = NULL;
    StyleComparison full = {0}, means = {0};
    if (!profile) {
        goto done;
    }
    StyleProfileResult added = style_profile_add_result(profile, result);
    if (added != STYLE_PROFILE_OK) {
        status = added == STYLE_PROFILE_ERR_ALLOC ? ANALYZER_ERR_ALLOC : ANALYZER_ERR_ARGUMENT;
        goto done;
    }
    if (style_profile_statistics(profile, &fragment_stats, &fragment_count) != STYLE_PROFILE_OK) {
        goto done;
    }
    fragment_count = keep_statistics(fragment_stats, fragment_count, (const char *const *)reference->without,
                                     reference->without_count);
    kept = calloc(reference->count > 0 ? reference->count : 1, sizeof *kept);
    fragment_model = model_of("fragment-style", fragment_stats, fragment_count);
    if (!kept || !fragment_model) {
        goto done;
    }
    /* The means come from the reference cut down to the categories the result has. */
    for (size_t i = 0; i < reference->count; i++) {
        if (has_category(fragment_stats, fragment_count, reference->statistics[i].category)) {
            kept[kept_count++] = reference->statistics[i];
        }
    }
    kept_model = model_of("fragment-style-reference", kept, kept_count);
    if (!kept_model) {
        goto done;
    }
    if (style_profile_compare(fragment_model, reference->base, &full) != STYLE_COMPARE_OK ||
        style_profile_compare(fragment_model, kept_model, &means) != STYLE_COMPARE_OK) {
        status = ANALYZER_ERR_LIBRARY;
        goto done;
    }
    /* The full comparison's categories (the missing ones too) with the means of the cut one. */
    StyleCategoryDistance *category_list = means.categories;
    size_t category_count = means.category_count;
    *out = means;
    out->categories = full.categories;
    out->category_count = full.category_count;
    full.categories = category_list;
    full.category_count = category_count;
    memset(&means, 0, sizeof means);
    status = ANALYZER_OK;
done:
    style_comparison_destroy(&full);
    style_comparison_destroy(&means);
    style_model_destroy(kept_model);
    style_model_destroy(fragment_model);
    free(kept);
    free(fragment_stats);
    style_profile_destroy(profile);
    return status;
}

/* ---- the fragment entries, over the hearing entries ---- */

AnalyzerFragmentResult analyzer_fragment_style_distance_without(const AnalyzerFragmentAnalysis *analysis,
                                                                const StyleModel *model,
                                                                const char *const *categories, size_t count,
                                                                StyleComparison *out) {
    if (!analysis || !analysis->valid || !model || !out || (count > 0 && !categories)) {
        return ANALYZER_FRAGMENT_ERR_ARGUMENT;
    }
    AnalyzerStyleReference *reference = NULL;
    AnalyzerStatus status = analyzer_style_reference_create(model, categories, count, &reference);
    if (status == ANALYZER_OK) {
        status = analyzer_result_style_distance(analysis->result, reference, out);
    }
    analyzer_style_reference_destroy(reference);
    if (status == ANALYZER_OK) {
        return ANALYZER_FRAGMENT_OK;
    }
    return status == ANALYZER_ERR_ALLOC ? ANALYZER_FRAGMENT_ERR_ALLOC : ANALYZER_FRAGMENT_ERR_ARGUMENT;
}

AnalyzerFragmentResult analyzer_fragment_style_distance(const AnalyzerFragmentAnalysis *analysis,
                                                        const StyleModel *model, StyleComparison *out) {
    return analyzer_fragment_style_distance_without(analysis, model, NULL, 0, out);
}
