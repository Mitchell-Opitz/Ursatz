#include <ursatz_analyzer/notation_reading.h>

#include <ctype.h>
#include <limits.h>
#include <stdlib.h>

#include <ursatz_analyzer/imported_notes.h>

#include "core/rational.h"
#include "identity/entity_id.h"
#include "time/duration.h"
#include "time/meter_map.h"
#include "time/musical_time.h"
#include "time/note_value.h"
#include "time/temporal_anchor.h"
#include "time/time_signature.h"
#include "util/misc_util.h"

/* One unsigned decimal number from *text, advancing past it; false when there is none or it overflows. */
static bool parse_number(const char **text, unsigned long *out) {
    const char *p = *text;
    if (!isdigit((unsigned char)*p)) {
        return false;
    }
    unsigned long value = 0;
    for (; isdigit((unsigned char)*p); p++) {
        unsigned digit = (unsigned)(*p - '0');
        if (value > (ULONG_MAX - digit) / 10) {
            return false;
        }
        value = value * 10 + digit;
    }
    *text = p;
    *out = value;
    return true;
}

/* "n" or "n/d", the whole text; the denominator is 1 for "n". */
static bool parse_fraction(const char *text, unsigned long *numerator, unsigned long *denominator) {
    if (!text || !parse_number(&text, numerator)) {
        return false;
    }
    *denominator = 1;
    if (*text == '/') {
        text++;
        if (!parse_number(&text, denominator)) {
            return false;
        }
    }
    return *text == '\0' && *denominator != 0;
}

static bool is_power_of_two(unsigned long value) {
    return value != 0 && (value & (value - 1)) == 0;
}

bool notation_reading_parse_meter(const char *text, NotationReading *r) {
    unsigned long beats;
    unsigned long beat_type;
    if (!text || !parse_number(&text, &beats) || *text++ != '/' || !parse_number(&text, &beat_type) || *text != '\0') {
        return false;
    }
    if (beats < 1 || beats > 64 || beat_type > 16 || !is_power_of_two(beat_type)) {
        return false;
    }
    r->has_meter = true;
    r->beats = (int)beats;
    r->beat_type = (int)beat_type;
    return true;
}

bool notation_reading_parse_scale(const char *text, NotationReading *r) {
    unsigned long numerator;
    unsigned long denominator;
    if (!parse_fraction(text, &numerator, &denominator)) {
        return false;
    }
    bool allowed = (denominator == 1 && (numerator == 1 || numerator == 2 || numerator == 4)) ||
                   (numerator == 1 && (denominator == 2 || denominator == 4));
    if (!allowed) {
        return false;
    }
    r->scale_numerator = (long)numerator;
    r->scale_denominator = denominator;
    return true;
}

bool notation_reading_parse_pickup(const char *text, NotationReading *r) {
    unsigned long numerator;
    unsigned long denominator;
    if (!parse_fraction(text, &numerator, &denominator) || numerator == 0 || numerator > LONG_MAX) {
        return false;
    }
    r->has_pickup = true;
    r->pickup_numerator = (long)numerator;
    r->pickup_denominator = denominator;
    return true;
}

NotationReading notation_reading_none(void) {
    NotationReading none = {.scale_numerator = 1, .scale_denominator = 1, .pickup_denominator = 1};
    return none;
}

/* ---- Applying a reading ---- */

/* num/den times the reading's scale, in lowest terms; false when it does not fit. */
static bool scale_fraction(long num, unsigned long den, const NotationReading *reading, long *out_num,
                           unsigned long *out_den) {
    Rational value;
    Rational scale;
    Rational scaled;
    if (den > (unsigned long)LLONG_MAX || reading->scale_denominator > (unsigned long)LLONG_MAX ||
        rational_make(num, (long long)den, &value) != RATIONAL_OK ||
        rational_make(reading->scale_numerator, (long long)reading->scale_denominator, &scale) != RATIONAL_OK ||
        rational_mul(value, scale, &scaled) != RATIONAL_OK || scaled.num < LONG_MIN || scaled.num > LONG_MAX ||
        (unsigned long long)scaled.den > ULONG_MAX) {
        return false;
    }
    *out_num = (long)scaled.num;
    *out_den = (unsigned long)scaled.den;
    return true;
}

/* A NoteEvent like note, its start and duration scaled; NULL on allocation failure or overflow. */
static NoteEvent *scaled_event(const NoteEvent *note, const NotationReading *reading, bool *overflow) {
    const MusicalTime *start = temporal_anchor_position(note_event_temporal_anchor(note));
    const Duration *duration = note_event_duration(note);
    long start_num;
    unsigned long start_den;
    long length_num;
    unsigned long length_den;
    if (!scale_fraction(musical_time_numerator(start), musical_time_denominator(start), reading, &start_num,
                        &start_den) ||
        !scale_fraction(duration_numerator(duration), duration_denominator(duration), reading, &length_num,
                        &length_den)) {
        *overflow = true;
        return NULL;
    }

    EntityID *id = entity_id_copy(note_event_id(note));
    MusicalTime *time = NULL;
    TemporalAnchor *anchor = NULL;
    Duration *length = NULL;
    PitchIdentity *pitch = NULL;
    PitchSpelling *spelling = NULL;
    VoiceReference *voice = NULL;
    ProvenanceReference *provenance = NULL;
    NoteEvent *event = NULL;

    bool ok = id && musical_time_create(start_num, start_den, &time) == MUSICAL_TIME_OK &&
              temporal_anchor_create(time, &anchor) == TEMPORAL_ANCHOR_OK;
    if (ok) {
        time = NULL; /* the anchor owns it */
    }
    ok = ok && duration_create(length_num, length_den, &length) == DURATION_OK;

    if (ok) {
        size_t token_size = 0;
        const void *token = pitch_identity_token(note_event_pitch(note), &token_size);
        ok = pitch_identity_create(token, token_size, &pitch) == PITCH_IDENTITY_OK;
    }
    const PitchSpelling *old_spelling = note_event_optional_pitch_spelling(note);
    if (ok && old_spelling) {
        ok = pitch_spelling_create(pitch_spelling_letter(old_spelling), pitch_spelling_accidental(old_spelling),
                                   pitch_spelling_octave(old_spelling), &spelling) == PITCH_SPELLING_OK;
    }
    const VoiceReference *old_voice = note_event_optional_voice(note);
    if (ok && old_voice) {
        EntityID *voice_id = entity_id_copy(voice_reference_target_id(old_voice));
        ok = voice_id && voice_reference_create(voice_id, &voice) == VOICE_REFERENCE_OK;
        if (!ok) {
            entity_id_destroy(voice_id);
        }
    }
    const ProvenanceReference *old_provenance = note_event_provenance(note);
    if (ok && provenance_reference_state(old_provenance) == PROVENANCE_REFERENCE_PRESENT) {
        EntityID *source_id = entity_id_copy(provenance_reference_target_id(old_provenance));
        ok = source_id && provenance_reference_create_present(source_id, &provenance) == PROVENANCE_REFERENCE_OK;
        if (!ok) {
            entity_id_destroy(source_id);
        }
    } else if (ok) {
        ok = provenance_reference_create_absent(&provenance) == PROVENANCE_REFERENCE_OK;
    }

    if (ok && note_event_create(id, anchor, length, pitch, spelling, voice, provenance, &event) == NOTE_EVENT_OK) {
        return event;
    }
    entity_id_destroy(id);
    musical_time_destroy(time);
    temporal_anchor_destroy(anchor);
    duration_destroy(length);
    pitch_identity_destroy(pitch);
    pitch_spelling_destroy(spelling);
    voice_reference_destroy(voice);
    provenance_reference_destroy(provenance);
    return NULL;
}

static MusicalTime *scaled_time(const MusicalTime *time, const NotationReading *reading, bool *overflow) {
    long num;
    unsigned long den;
    MusicalTime *out = NULL;
    if (!scale_fraction(musical_time_numerator(time), musical_time_denominator(time), reading, &num, &den)) {
        *overflow = true;
        return NULL;
    }
    return musical_time_create(num, den, &out) == MUSICAL_TIME_OK ? out : NULL;
}

static int exponent_of(int power_of_two) {
    int exponent = 0;
    while (power_of_two > 1) {
        power_of_two /= 2;
        exponent++;
    }
    return exponent;
}

/* The time signature of the reading's meter. */
static TimeSignature *reading_signature(const NotationReading *reading) {
    NoteValue *unit = NULL;
    TimeSignature *signature = NULL;
    if (note_value_create(exponent_of(reading->beat_type), &unit) != NOTE_VALUE_OK) {
        return NULL;
    }
    if (time_signature_create((unsigned)reading->beats, unit, &signature) != TIME_SIGNATURE_OK) {
        signature = NULL;
    }
    note_value_destroy(unit);
    return signature;
}

/* A pickup shorter than one measure of signature. */
static bool pickup_fits(const NotationReading *reading, const TimeSignature *signature) {
    Duration *measure = NULL;
    if (time_signature_measure_duration(signature, &measure) != DURATION_OK) {
        return false;
    }
    /* pickup < measure, as cross products */
    bool fits = (unsigned long long)reading->pickup_numerator * duration_denominator(measure) <
                (unsigned long long)duration_numerator(measure) * reading->pickup_denominator;
    duration_destroy(measure);
    return fits;
}

typedef struct {
    NoteEvent **events;
    MusicalTime **key_positions;
    MusicalTime *end_time;
    MeterMap *meter_map;
    long *positions; /* tempo marking positions, then the dynamics' starts and ends: numerator, denominator pairs */
    unsigned long *denominators;
} Prepared;

static void prepared_destroy(Prepared *prepared, const ImportedNotes *notes) {
    for (size_t i = 0; prepared->events && i < notes->count; i++) {
        note_event_destroy(prepared->events[i]);
    }
    for (size_t i = 0; prepared->key_positions && i < notes->key_signature_events.count; i++) {
        musical_time_destroy(prepared->key_positions[i]);
    }
    free(prepared->events);
    free(prepared->key_positions);
    musical_time_destroy(prepared->end_time);
    meter_map_destroy(prepared->meter_map);
    free(prepared->positions);
    free(prepared->denominators);
}

/* The meter map the reading asks for; NULL on failure (overflow set when a time did not fit). */
static MeterMap *prepared_meter_map(const ImportedNotes *notes, const NotationReading *reading, bool *invalid,
                                    bool *overflow) {
    const MeterMap *old = notes->meter.meter_map;
    TimeSignature *own = NULL;
    const TimeSignature *chosen = NULL;
    size_t first = 0;
    if (reading->has_meter) {
        own = reading_signature(reading);
        chosen = own;
    } else if (reading->has_pickup || meter_map_has_pickup(old)) {
        first = meter_map_has_pickup(old) ? 1 : 0;
        chosen = meter_map_region_time_signature(meter_map_region(old, first));
    }

    MeterMap *map = NULL;
    MusicalTime *times[3] = {NULL, NULL, NULL};
    MeterMapChangePoint points[8];
    MusicalTime **scaled = NULL;
    size_t count = 0;
    bool ok = true;

    if (reading->has_pickup) {
        if (!chosen || !pickup_fits(reading, chosen)) {
            *invalid = true;
            ok = false;
        } else {
            ok = musical_time_create(0, 1, &times[0]) == MUSICAL_TIME_OK &&
                 musical_time_create(reading->pickup_numerator, reading->pickup_denominator, &times[1]) ==
                     MUSICAL_TIME_OK;
            points[0] = (MeterMapChangePoint){chosen, times[0]};
            points[1] = (MeterMapChangePoint){chosen, times[1]};
            count = 2;
        }
    } else if (reading->has_meter) {
        ok = own && musical_time_create(0, 1, &times[0]) == MUSICAL_TIME_OK;
        points[0] = (MeterMapChangePoint){chosen, times[0]};
        count = 1;
    }

    if (ok && count == 0) {
        /* No new meter: the old map with its change points scaled. */
        size_t region_count = meter_map_region_count(old);
        size_t from = 0;
        if (meter_map_is_defaulted(old)) {
            from = region_count; /* rebuilt below as the default */
        } else if (region_count > 1 && meter_map_region_is_defaulted(meter_map_region(old, 0))) {
            from = 1; /* a defaulted lead-in is restored by the map itself */
        }
        MeterMapChangePoint *list = malloc((region_count + 1) * sizeof *list);
        scaled = calloc(region_count + 1, sizeof *scaled);
        ok = list && scaled;
        for (size_t i = from; ok && i < region_count; i++) {
            const MeterMapRegion *region = meter_map_region(old, i);
            scaled[count] = scaled_time(meter_map_region_start(region), reading, overflow);
            ok = scaled[count] != NULL;
            list[count] = (MeterMapChangePoint){meter_map_region_time_signature(region), scaled[count]};
            count++;
        }
        EntityID *id = ok ? entity_id_copy(meter_map_id(old)) : NULL;
        if (ok && count == 0) {
            ok = id && meter_map_create_defaulted(id, &map) == METER_MAP_OK;
        } else if (ok) {
            ok = id && meter_map_create(id, list, count, &map) == METER_MAP_OK;
        }
        if (!map) {
            entity_id_destroy(id);
        }
        for (size_t i = 0; scaled && i < region_count + 1; i++) {
            musical_time_destroy(scaled[i]);
        }
        free(scaled);
        free(list);
        musical_time_destroy(times[0]);
        return ok ? map : NULL;
    }

    if (ok) {
        EntityID *id = entity_id_copy(meter_map_id(old));
        ok = id && meter_map_create(id, points, count, &map) == METER_MAP_OK;
        if (!map) {
            entity_id_destroy(id);
        }
    }
    for (int i = 0; i < 3; i++) {
        musical_time_destroy(times[i]);
    }
    time_signature_destroy(own);
    return ok ? map : NULL;
}

NotationReadingResult notation_reading_apply(ImportedNotes *notes, const NotationReading *reading) {
    if (!notes || !reading) {
        return NOTATION_READING_ERR_NULL_ARGUMENT;
    }
    bool scale_ok = (reading->scale_denominator == 1 &&
                     (reading->scale_numerator == 1 || reading->scale_numerator == 2 || reading->scale_numerator == 4)) ||
                    (reading->scale_numerator == 1 && (reading->scale_denominator == 2 || reading->scale_denominator == 4));
    bool meter_ok = !reading->has_meter || (reading->beats >= 1 && reading->beats <= 64 && reading->beat_type >= 1 &&
                                            reading->beat_type <= 16 && (reading->beat_type & (reading->beat_type - 1)) == 0);
    bool pickup_ok = !reading->has_pickup || (reading->pickup_numerator > 0 && reading->pickup_denominator > 0);
    if (!scale_ok || !meter_ok || !pickup_ok || !notes->meter.meter_map) {
        return NOTATION_READING_ERR_INVALID;
    }

    Prepared prepared = {0};
    bool overflow = false;
    bool invalid = false;
    bool ok = true;

    prepared.events = calloc(notes->count ? notes->count : 1, sizeof *prepared.events);
    ok = prepared.events != NULL;
    for (size_t i = 0; ok && i < notes->count; i++) {
        prepared.events[i] = scaled_event(notes->events[i], reading, &overflow);
        ok = prepared.events[i] != NULL;
    }

    size_t key_count = notes->key_signature_events.count;
    if (ok) {
        prepared.key_positions = calloc(key_count ? key_count : 1, sizeof *prepared.key_positions);
        ok = prepared.key_positions != NULL;
    }
    for (size_t i = 0; ok && i < key_count; i++) {
        prepared.key_positions[i] = scaled_time(notes->key_signature_events.events[i].position, reading, &overflow);
        ok = prepared.key_positions[i] != NULL;
    }

    if (ok) {
        prepared.end_time = scaled_time(notes->meter.end_time, reading, &overflow);
        ok = prepared.end_time != NULL;
    }

    size_t tempo_count = notes->source.tempo_marking_count;
    size_t dynamics_count = notes->source.dynamics_count;
    size_t value_count = tempo_count + 2 * dynamics_count;
    if (ok) {
        prepared.positions = calloc(value_count ? value_count : 1, sizeof *prepared.positions);
        prepared.denominators = calloc(value_count ? value_count : 1, sizeof *prepared.denominators);
        ok = prepared.positions && prepared.denominators;
    }
    for (size_t i = 0; ok && i < tempo_count; i++) {
        const ImportedTempoMarking *marking = &notes->source.tempo_markings[i];
        ok = scale_fraction(marking->position_numerator, marking->position_denominator, reading, &prepared.positions[i],
                            &prepared.denominators[i]);
        overflow = overflow || !ok;
    }
    for (size_t i = 0; ok && i < dynamics_count; i++) {
        const ImportedDynamic *dynamic = &notes->source.dynamics[i];
        size_t at = tempo_count + 2 * i;
        ok = scale_fraction(dynamic->start_numerator, dynamic->start_denominator, reading, &prepared.positions[at],
                            &prepared.denominators[at]) &&
             scale_fraction(dynamic->end_numerator, dynamic->end_denominator, reading, &prepared.positions[at + 1],
                            &prepared.denominators[at + 1]);
        overflow = overflow || !ok;
    }

    if (ok) {
        prepared.meter_map = prepared_meter_map(notes, reading, &invalid, &overflow);
        ok = prepared.meter_map != NULL;
    }

    if (!ok) {
        prepared_destroy(&prepared, notes);
        return invalid ? NOTATION_READING_ERR_INVALID : (overflow ? NOTATION_READING_ERR_OVERFLOW : NOTATION_READING_ERR_ALLOC);
    }

    /* Nothing below can fail: swap the prepared pieces in. */
    for (size_t i = 0; i < notes->count; i++) {
        note_event_destroy(notes->events[i]);
        notes->events[i] = prepared.events[i];
    }
    free(prepared.events);
    for (size_t i = 0; i < key_count; i++) {
        musical_time_destroy(notes->key_signature_events.events[i].position);
        notes->key_signature_events.events[i].position = prepared.key_positions[i];
    }
    free(prepared.key_positions);
    musical_time_destroy(notes->meter.end_time);
    notes->meter.end_time = prepared.end_time;
    meter_map_destroy(notes->meter.meter_map);
    notes->meter.meter_map = prepared.meter_map;
    for (size_t i = 0; i < tempo_count; i++) {
        notes->source.tempo_markings[i].position_numerator = prepared.positions[i];
        notes->source.tempo_markings[i].position_denominator = prepared.denominators[i];
    }
    for (size_t i = 0; i < dynamics_count; i++) {
        size_t at = tempo_count + 2 * i;
        ImportedDynamic *dynamic = &notes->source.dynamics[i];
        dynamic->start_numerator = prepared.positions[at];
        dynamic->start_denominator = prepared.denominators[at];
        dynamic->end_numerator = prepared.positions[at + 1];
        dynamic->end_denominator = prepared.denominators[at + 1];
    }
    free(prepared.positions);
    free(prepared.denominators);
    notes->source.reading_applied = true;
    notes->source.reading = *reading;
    return NOTATION_READING_OK;
}
