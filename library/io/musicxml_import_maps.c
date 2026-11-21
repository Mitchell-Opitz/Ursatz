#include "io/musicxml_importer.h"
#include <common/files.h>
#include <common/hash.h>
#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ezxml.h"
#include "model/pitch/pitch_arithmetic.h"
#include "model/core/provenance_reference.h"
#include "model/core/voice_reference.h"
#include "model/identity/entity_id.h"
#include "io/midi_key_signature.h"
#include "model/pitch/pitch_identity.h"
#include "model/pitch/pitch_numeric_value.h"
#include "model/pitch/pitch_spelling.h"
#include "model/pitch/tuning_system.h"
#include "model/time/duration.h"
#include "model/core/rational.h"
#include "model/time/meter_map.h"
#include "model/time/note_value.h"
#include "model/time/tempo_map.h"
#include "model/time/time_span.h"
#include "model/time/tuplet.h"
#include "model/events/dynamics.h"
#include "model/events/tempo_marking.h"
#include "model/time/metronome_marking.h"
#include "model/time/time_signature.h"
#include "model/time/musical_time.h"
#include "model/time/temporal_anchor.h"
#include "io/musicxml_import_internal.h"

static size_t id_hash(const char *text) {
    return (size_t)common_fnv1a64(text, strlen(text), 1469598103934665603ULL);
}

static bool id_set_has(const IdSet *set, const char *text) {
    for (size_t at = id_hash(text) & (set->capacity - 1); set->slots[at] != 0; at = (at + 1) & (set->capacity - 1)) {
        if (strcmp(entity_id_value(note_event_id(set->events[set->slots[at] - 1])), text) == 0) {
            return true;
        }
    }
    return false;
}

void musicxml_import_id_set_add(IdSet *set, size_t event_index) {
    const char *text = entity_id_value(note_event_id(set->events[event_index]));
    size_t at = id_hash(text) & (set->capacity - 1);
    while (set->slots[at] != 0) {
        at = (at + 1) & (set->capacity - 1);
    }
    set->slots[at] = event_index + 1;
}

/*
 * The id of a note's event: its xml:id when it has one, is at most 63 characters and no earlier event of the import (in the sorted
 * order) has it; otherwise the positional "musicxml-<part id>-<n>" (a repeated xml:id is warned about), and a positional id an earlier
 * event already has gets "-2", "-3" and on.
 */
void musicxml_import_choose_event_id(Ctx *ctx, const IdSet *set, const XNote *note, const char *part_id, char *out, size_t size) {
    if (note->xml_id[0] != '\0') {
        if (!id_set_has(set, note->xml_id)) {
            snprintf(out, size, "%s", note->xml_id);
            return;
        }
        musicxml_import_warn(ctx, "repeated xml:id '%s'", note->xml_id);
    }
    char base[128];
    snprintf(base, sizeof(base), "musicxml-%s-%zu", part_id, note->order);
    snprintf(out, size, "%s", base);
    for (size_t suffix = 2; id_set_has(set, out); suffix++) {
        snprintf(out, size, "%s-%zu", base, suffix);
    }
}

bool musicxml_import_build_event(Ctx *ctx, const XNote *note, const char *id_text, const char *part_id, const TuningSystem *tuning,
                        NoteEvent **out_event, PitchNumericValue **out_value) {
    EntityID *id = NULL;
    Duration duration;
    PitchNumericValue *numeric = NULL;
    PitchIdentity *pitch = NULL;
    PitchSpelling *spelling = NULL;
    EntityID *provenance_id = NULL;
    ProvenanceReference *provenance = NULL;
    EntityID *voice_id = NULL;
    VoiceReference *voice = NULL;
    NoteEvent *event = NULL;

    char voice_text[128];
    snprintf(voice_text, sizeof(voice_text), "musicxml-voice-%s-%s", part_id, note->voice);
    bool fits = note->duration.num <= (unsigned long long)LONG_MAX && note->duration.den <= (unsigned long long)ULONG_MAX;
    bool ok = fits && entity_id_create(id_text, &id) == ENTITY_ID_OK &&
              duration_make((long long)note->duration.num, (long long)note->duration.den, &duration) == DURATION_OK &&
              pitch_numeric_value_create(tuning, note->midi, 1, &numeric) == PITCH_NUMERIC_VALUE_OK &&
              pitch_numeric_value_to_identity(numeric, &pitch) == PITCH_NUMERIC_VALUE_TO_IDENTITY_OK &&
              pitch_spelling_create(note->letter, note->accidental, note->octave, &spelling) == PITCH_SPELLING_OK &&
              entity_id_create(MUSICXML_IMPORTER_PROVENANCE_ID, &provenance_id) == ENTITY_ID_OK &&
              provenance_reference_create_present(provenance_id, &provenance) == PROVENANCE_REFERENCE_OK;
    if (ok) {
        provenance_id = NULL; /* owned by the provenance reference */
        if (note->voice[0] != '\0') {
            ok = entity_id_create(voice_text, &voice_id) == ENTITY_ID_OK &&
                 voice_reference_create(voice_id, &voice) == VOICE_REFERENCE_OK;
            if (voice != NULL) {
                voice_id = NULL; /* owned by the voice reference */
            }
        }
        ok = ok && note_event_create(id, musical_time_of(note->onset), duration, pitch, spelling, voice, provenance, &event) == NOTE_EVENT_OK;
        if (ok) {
            /* the event owns id, pitch, spelling, voice and provenance now */
            id = NULL; pitch = NULL; spelling = NULL; voice = NULL; provenance = NULL;
            if (note->tuplet_actual != 0 && note_event_set_tuplet(event, note->tuplet_actual, note->tuplet_normal) != NOTE_EVENT_OK) {
                note_event_destroy(event);
                event = NULL;
                ok = false;
            }
        }
    }
    if (!ok) {
        if (!fits) {
            musicxml_import_fail(ctx, MUSICXML_IMPORT_ERR_OVERFLOW, "a note's position or duration does not fit");
        } else {
            musicxml_import_fail(ctx, MUSICXML_IMPORT_ERR_ALLOC, "out of memory");
        }
        entity_id_destroy(id);
        pitch_identity_destroy(pitch);
        pitch_spelling_destroy(spelling);
        pitch_numeric_value_destroy(numeric);
        entity_id_destroy(provenance_id);
        provenance_reference_destroy(provenance);
        entity_id_destroy(voice_id);
        voice_reference_destroy(voice);
        return false;
    }
    *out_event = event;
    *out_value = numeric;
    return true;
}

void musicxml_import_clear_import(MusicXmlImport *import) {
    memset(import, 0, sizeof(*import));
}

void musicxml_import_destroy(MusicXmlImport *import) {
    if (import == NULL) {
        return;
    }
    for (size_t i = 0; i < import->count; i++) {
        note_event_destroy(import->events != NULL ? import->events[i] : NULL);
        pitch_numeric_value_destroy(import->pitch_values != NULL ? import->pitch_values[i] : NULL);
    }
    free(import->events);
    free(import->pitch_values);
    free(import->part_index);
    free(import->staff);
    for (size_t i = 0; i < import->part_count; i++) {
        free(import->parts[i].id);
        free(import->parts[i].name);
    }
    free(import->parts);
    meter_map_destroy(import->meter_map);
    for (size_t i = 0; import->part_meter_maps != NULL && i < import->part_count; i++) {
        meter_map_destroy(import->part_meter_maps[i]);
    }
    free(import->part_meter_maps);
    tempo_map_destroy(import->tempo_map);
    for (size_t i = 0; i < import->tempo_marking_count; i++) {
        tempo_marking_destroy(import->tempo_markings[i]);
    }
    free(import->tempo_markings);
    free(import->tempo_marking_part);
    for (size_t i = 0; i < import->dynamics_count; i++) {
        dynamics_destroy(import->dynamics[i]);
    }
    free(import->dynamics);
    free(import->dynamics_part);
    free(import->dynamics_staff);
    for (size_t i = 0; i < import->key_signature_count; i++) {
        key_signature_destroy(import->key_signatures[i].key_signature);
    }
    free(import->key_signatures);
    musicxml_import_clear_import(import);
}

/* --- the meter map and the key signature events --- */

static int compare_time_events(const void *a, const void *b) {
    const TimeEvent *x = (const TimeEvent *)a;
    const TimeEvent *y = (const TimeEvent *)b;
    int by_position = rational_cmp(x->position, y->position);
    if (by_position != 0) {
        return by_position;
    }
    return x->part < y->part ? -1 : (x->part > y->part ? 1 : 0);
}

static int compare_key_events(const void *a, const void *b) {
    const KeyEvent *x = (const KeyEvent *)a;
    const KeyEvent *y = (const KeyEvent *)b;
    int by_position = rational_cmp(x->position, y->position);
    if (by_position != 0) {
        return by_position;
    }
    return x->part < y->part ? -1 : (x->part > y->part ? 1 : 0);
}

static bool same_signature(const TimeEvent *a, const TimeEvent *b) {
    return a->beats == b->beats && a->exponent == b->exponent && a->group_count == b->group_count &&
           memcmp(a->groups, b->groups, a->group_count * sizeof(unsigned)) == 0;
}

/* The meter map of the time events (sorted by position, then part): one signature per position, the lowest part's wins a
   conflict, and a pickup's region ended where the pickup ends. `report` says whether to musicxml_import_warn about what it drops (the
   movement's map does; a part's own map has only its own events). *out is NULL on ERR_ALLOC. */
static MeterMapResult map_of_events(Ctx *ctx, const TimeEvent *events, size_t count, bool report, const char *name,
                                    MeterMap **out) {
    EntityID *map_id = NULL;
    if (entity_id_create(name, &map_id) != ENTITY_ID_OK) {
        return METER_MAP_ERR_ALLOC;
    }
    /* the signatures kept: one per position, plus the pickup's end */
    TimeEvent *kept = (TimeEvent *)calloc(count + 1, sizeof(TimeEvent));
    if (kept == NULL) {
        entity_id_destroy(map_id);
        return METER_MAP_ERR_ALLOC;
    }
    size_t kept_count = 0;
    for (size_t i = 0; i < count; i++) {
        if (kept_count > 0 && rational_cmp(kept[kept_count - 1].position, events[i].position) == 0) {
            if (report && !same_signature(&kept[kept_count - 1], &events[i])) {
                musicxml_import_warn(ctx, "parts disagree about the time signature at one position; part %zu's is used",
                     kept[kept_count - 1].part);
            }
            continue;
        }
        kept[kept_count++] = events[i];
    }
    if (ctx->has_pickup) {
        if (kept[0].position.num != 0) {
            if (report) {
                musicxml_import_warn(ctx, "the implicit first measure is not read as a pickup: no time signature starts the piece");
            }
        } else if (kept_count == 1 || rational_cmp(kept[1].position, ctx->pickup_length) > 0) {
            /* nothing ends the pickup's region where the pickup ends: end it there, with the same signature, which
               is what the MeterMap reads as a short first region (a pickup) */
            for (size_t m = kept_count; m > 1; m--) {
                kept[m] = kept[m - 1];
            }
            kept[1] = kept[0];
            kept[1].position = ctx->pickup_length;
            kept_count++;
        }
    }

    MeterMapChangePoint *points = (MeterMapChangePoint *)calloc(kept_count, sizeof(MeterMapChangePoint));
    TimeSignature **signatures = (TimeSignature **)calloc(kept_count, sizeof(TimeSignature *));
    bool ok = points != NULL && signatures != NULL;
    for (size_t i = 0; i < kept_count && ok; i++) {
        NoteValue *unit = NULL;
        ok = note_value_create(kept[i].exponent, &unit) == NOTE_VALUE_OK &&
             (kept[i].group_count > 0
                  ? time_signature_create_grouped(kept[i].groups, kept[i].group_count, unit, &signatures[i])
                  : time_signature_create(kept[i].beats, unit, &signatures[i])) == TIME_SIGNATURE_OK;
        note_value_destroy(unit);
        points[i].time_signature = signatures[i];
        points[i].start = musical_time_of(kept[i].position);
    }
    ok = ok && meter_map_create(map_id, points, kept_count, out) == METER_MAP_OK;
    for (size_t i = 0; signatures != NULL && i < kept_count; i++) {
        time_signature_destroy(signatures[i]);
    }
    free(signatures);
    free(points);
    free(kept);
    if (!ok) {
        entity_id_destroy(map_id);
        return METER_MAP_ERR_ALLOC;
    }
    return METER_MAP_OK;
}

/* Whether two meter maps lay the same regions: the same starts, signatures and pickup. */
static bool maps_agree(const MeterMap *a, const MeterMap *b) {
    if (meter_map_region_count(a) != meter_map_region_count(b)) {
        return false;
    }
    for (size_t i = 0; i < meter_map_region_count(a); i++) {
        const MeterMapRegion *x = meter_map_region(a, i);
        const MeterMapRegion *y = meter_map_region(b, i);
        if (musical_time_cmp(meter_map_region_start(x), meter_map_region_start(y)) != 0 ||
            !time_signature_equals(meter_map_region_time_signature(x), meter_map_region_time_signature(y)) ||
            meter_map_region_is_pickup(x) != meter_map_region_is_pickup(y)) {
            return false;
        }
    }
    return true;
}

/* Each part's own meter map where it differs from the movement's: one entry per part, NULL for a part that follows the
   movement's map or declares no time signature; the array stays NULL when every part follows it. times is sorted. */
static bool build_part_meter_maps(Ctx *ctx) {
    MusicXmlImport *out = ctx->out;
    MeterMap **maps = (MeterMap **)calloc(out->part_count > 0 ? out->part_count : 1, sizeof(MeterMap *));
    TimeEvent *own = (TimeEvent *)calloc(ctx->time_count, sizeof(TimeEvent));
    bool ok = maps != NULL && own != NULL;
    bool any = false;
    for (size_t part = 0; part < out->part_count && ok; part++) {
        size_t own_count = 0;
        for (size_t i = 0; i < ctx->time_count; i++) {
            if (ctx->times[i].part == part) {
                own[own_count++] = ctx->times[i];
            }
        }
        if (own_count == 0) {
            continue;
        }
        char name[64];
        snprintf(name, sizeof(name), "musicxml-meter-map-part-%zu", part);
        MeterMap *map = NULL;
        ok = map_of_events(ctx, own, own_count, false, name, &map) == METER_MAP_OK;
        if (ok && maps_agree(map, out->meter_map)) {
            meter_map_destroy(map);
        } else if (ok) {
            maps[part] = map;
            any = true;
        }
    }
    free(own);
    if (!ok || !any) {
        for (size_t part = 0; maps != NULL && part < out->part_count; part++) {
            meter_map_destroy(maps[part]);
        }
        free(maps);
        return ok || musicxml_import_fail(ctx, MUSICXML_IMPORT_ERR_ALLOC, "the time signatures could not form a meter map") == MUSICXML_IMPORT_OK;
    }
    out->part_meter_maps = maps;
    return true;
}

bool musicxml_import_build_meter_map(Ctx *ctx) {
    MusicXmlImport *out = ctx->out;
    if (ctx->time_count == 0) {
        EntityID *map_id = NULL;
        if (entity_id_create("musicxml-meter-map", &map_id) != ENTITY_ID_OK) {
            return musicxml_import_fail(ctx, MUSICXML_IMPORT_ERR_ALLOC, "out of memory") == MUSICXML_IMPORT_OK;
        }
        if (meter_map_create_defaulted(map_id, &out->meter_map) != METER_MAP_OK) {
            entity_id_destroy(map_id);
            return musicxml_import_fail(ctx, MUSICXML_IMPORT_ERR_ALLOC, "out of memory") == MUSICXML_IMPORT_OK;
        }
        return true;
    }

    qsort(ctx->times, ctx->time_count, sizeof(TimeEvent), compare_time_events);
    if (map_of_events(ctx, ctx->times, ctx->time_count, true, "musicxml-meter-map", &out->meter_map) != METER_MAP_OK) {
        return musicxml_import_fail(ctx, MUSICXML_IMPORT_ERR_ALLOC, "the time signatures could not form a meter map") == MUSICXML_IMPORT_OK;
    }
    return build_part_meter_maps(ctx);
}

bool musicxml_import_build_key_signatures(Ctx *ctx) {
    MusicXmlImport *out = ctx->out;
    if (ctx->key_count == 0) {
        return true;
    }
    qsort(ctx->keys, ctx->key_count, sizeof(KeyEvent), compare_key_events);
    out->key_signatures = (MusicXmlKeySignatureEvent *)calloc(ctx->key_count, sizeof(MusicXmlKeySignatureEvent));
    if (out->key_signatures == NULL) {
        return musicxml_import_fail(ctx, MUSICXML_IMPORT_ERR_ALLOC, "out of memory") == MUSICXML_IMPORT_OK;
    }
    for (size_t i = 0; i < ctx->key_count; i++) {
        const KeyEvent *event = &ctx->keys[i];
        bool duplicate = false;
        for (size_t k = 0; k < out->key_signature_count && !duplicate; k++) {
            const MusicXmlKeySignatureEvent *seen = &out->key_signatures[k];
            duplicate = musical_time_same(seen->position, musical_time_of(event->position)) &&
                        seen->sharps_flats == event->fifths && seen->mode_hint == event->hint;
        }
        if (duplicate) {
            continue;
        }
        MusicXmlKeySignatureEvent *slot = &out->key_signatures[out->key_signature_count];
        slot->position = musical_time_of(event->position);
        slot->sharps_flats = (int8_t)event->fifths;
        slot->mode_hint = event->hint;
        slot->part_index = event->part;
        if (midi_key_signature_build(event->fifths, &slot->key_signature) != MIDI_KEY_SIGNATURE_BUILD_OK) {
            return musicxml_import_fail(ctx, MUSICXML_IMPORT_ERR_ALLOC, "out of memory") == MUSICXML_IMPORT_OK;
        }
        out->key_signature_count++;
    }
    return true;
}

static int compare_tempo_events(const void *a, const void *b) {
    const TempoEvent *x = (const TempoEvent *)a;
    const TempoEvent *y = (const TempoEvent *)b;
    int by_position = rational_cmp(x->position, y->position);
    if (by_position != 0) {
        return by_position;
    }
    return x->part < y->part ? -1 : (x->part > y->part ? 1 : 0);
}

static int compare_marking_events(const void *a, const void *b) {
    const MarkingEvent *x = (const MarkingEvent *)a;
    const MarkingEvent *y = (const MarkingEvent *)b;
    int by_position = rational_cmp(x->position, y->position);
    if (by_position != 0) {
        return by_position;
    }
    if (x->part != y->part) {
        return x->part < y->part ? -1 : 1;
    }
    return x->order < y->order ? -1 : (x->order > y->order ? 1 : 0);
}

static int compare_dynamic_events(const void *a, const void *b) {
    const DynamicEvent *x = (const DynamicEvent *)a;
    const DynamicEvent *y = (const DynamicEvent *)b;
    int by_position = rational_cmp(x->position, y->position);
    if (by_position != 0) {
        return by_position;
    }
    if (x->part != y->part) {
        return x->part < y->part ? -1 : 1;
    }
    return x->order < y->order ? -1 : (x->order > y->order ? 1 : 0);
}

bool musicxml_import_build_tempo_map(Ctx *ctx) {
    MusicXmlImport *out = ctx->out;
    EntityID *map_id = NULL;
    if (entity_id_create("musicxml-tempo-map", &map_id) != ENTITY_ID_OK) {
        return musicxml_import_fail(ctx, MUSICXML_IMPORT_ERR_ALLOC, "out of memory") == MUSICXML_IMPORT_OK;
    }
    if (ctx->tempo_count == 0) {
        if (tempo_map_create_defaulted(map_id, &out->tempo_map) != TEMPO_MAP_OK) {
            entity_id_destroy(map_id);
            return musicxml_import_fail(ctx, MUSICXML_IMPORT_ERR_ALLOC, "out of memory") == MUSICXML_IMPORT_OK;
        }
        return true;
    }
    qsort(ctx->tempos, ctx->tempo_count, sizeof(TempoEvent), compare_tempo_events);
    TempoEvent *kept = (TempoEvent *)calloc(ctx->tempo_count, sizeof(TempoEvent));
    TempoMapChangePoint *points = (TempoMapChangePoint *)calloc(ctx->tempo_count, sizeof(TempoMapChangePoint));
    size_t kept_count = 0;
    bool ok = kept != NULL && points != NULL;
    for (size_t i = 0; i < ctx->tempo_count && ok; i++) {
        if (kept_count > 0 && rational_cmp(kept[kept_count - 1].position, ctx->tempos[i].position) == 0) {
            if (kept[kept_count - 1].microseconds_per_quarter != ctx->tempos[i].microseconds_per_quarter) {
                musicxml_import_warn(ctx, "parts disagree about the tempo at one position; part %zu's is used", kept[kept_count - 1].part);
            }
            continue;
        }
        kept[kept_count++] = ctx->tempos[i];
    }
    for (size_t i = 0; i < kept_count && ok; i++) {
        points[i].start = musical_time_of(kept[i].position);
        points[i].microseconds_per_quarter = kept[i].microseconds_per_quarter;
    }
    TempoMapResult made = ok ? tempo_map_create(map_id, points, kept_count, &out->tempo_map) : TEMPO_MAP_ERR_ALLOC;
    free(points);
    free(kept);
    if (made != TEMPO_MAP_OK) {
        entity_id_destroy(map_id);
        return musicxml_import_fail(ctx, made == TEMPO_MAP_ERR_OVERFLOW ? MUSICXML_IMPORT_ERR_OVERFLOW : MUSICXML_IMPORT_ERR_ALLOC,
                    "the tempos could not form a tempo map") == MUSICXML_IMPORT_OK;
    }
    return true;
}

bool musicxml_import_build_tempo_markings(Ctx *ctx) {
    MusicXmlImport *out = ctx->out;
    if (ctx->marking_count == 0) {
        return true;
    }
    qsort(ctx->markings, ctx->marking_count, sizeof(MarkingEvent), compare_marking_events);
    out->tempo_markings = (TempoMarking **)calloc(ctx->marking_count, sizeof(TempoMarking *));
    out->tempo_marking_part = (size_t *)calloc(ctx->marking_count, sizeof(size_t));
    if (out->tempo_markings == NULL || out->tempo_marking_part == NULL) {
        return musicxml_import_fail(ctx, MUSICXML_IMPORT_ERR_ALLOC, "out of memory") == MUSICXML_IMPORT_OK;
    }
    for (size_t i = 0; i < ctx->marking_count; i++) {
        const MarkingEvent *event = &ctx->markings[i];
        TemporalAnchor *anchor = NULL;
        Duration unit;
        MetronomeMarking *metronome = NULL;
        bool ok = temporal_anchor_create(musical_time_of(event->position), &anchor) == TEMPORAL_ANCHOR_OK;
        if (ok && event->has_metronome) {
            ok = event->beat_unit.num <= (unsigned long long)LLONG_MAX && event->beat_unit.den <= (unsigned long long)LLONG_MAX &&
                 duration_make((long long)event->beat_unit.num, (long long)event->beat_unit.den, &unit) == DURATION_OK &&
                 metronome_marking_create(&unit, event->bpm_numerator, event->bpm_denominator, &metronome) ==
                     METRONOME_MARKING_OK;
        }
        TempoMarking *marking = NULL;
        ok = ok && tempo_marking_create(anchor, event->has_text ? event->text : NULL, metronome, &marking) ==
                       TEMPO_MARKING_OK;
        if (!ok) {
            temporal_anchor_destroy(anchor);
            metronome_marking_destroy(metronome);
            return musicxml_import_fail(ctx, MUSICXML_IMPORT_ERR_ALLOC, "a tempo marking could not be built") == MUSICXML_IMPORT_OK;
        }
        out->tempo_markings[i] = marking;
        out->tempo_marking_part[i] = event->part;
        out->tempo_marking_count = i + 1;
    }
    return true;
}

bool musicxml_import_build_dynamics(Ctx *ctx) {
    MusicXmlImport *out = ctx->out;
    if (ctx->dynamic_count == 0) {
        return true;
    }
    qsort(ctx->dynamics, ctx->dynamic_count, sizeof(DynamicEvent), compare_dynamic_events);
    out->dynamics = (Dynamics **)calloc(ctx->dynamic_count, sizeof(Dynamics *));
    out->dynamics_part = (size_t *)calloc(ctx->dynamic_count, sizeof(size_t));
    out->dynamics_staff = (int *)calloc(ctx->dynamic_count, sizeof(int));
    if (out->dynamics == NULL || out->dynamics_part == NULL || out->dynamics_staff == NULL) {
        return musicxml_import_fail(ctx, MUSICXML_IMPORT_ERR_ALLOC, "out of memory") == MUSICXML_IMPORT_OK;
    }
    for (size_t i = 0; i < ctx->dynamic_count; i++) {
        const DynamicEvent *event = &ctx->dynamics[i];
        /* in force until the next later marking in the same part, or the part's end */
        Rational end = ctx->part_end[event->part];
        for (size_t j = i + 1; j < ctx->dynamic_count; j++) {
            if (ctx->dynamics[j].part == event->part && rational_cmp(ctx->dynamics[j].position, event->position) > 0) {
                end = ctx->dynamics[j].position;
                break;
            }
        }
        if (rational_cmp(end, event->position) <= 0) {
            musicxml_import_warn(ctx, "dynamic marking %s has no extent (it is at or after the end of the part); skipped", event->name);
            continue;
        }
        TimeSpan span;
        Dynamics *dynamics = NULL;
        bool ok = time_span_make(musical_time_of(event->position), musical_time_of(end), &span) == TIME_SPAN_OK &&
                  dynamics_create_for_time_span(event->name, span, &dynamics) == DYNAMICS_OK;
        if (!ok) {
            return musicxml_import_fail(ctx, MUSICXML_IMPORT_ERR_ALLOC, "a dynamic marking could not be built") == MUSICXML_IMPORT_OK;
        }
        out->dynamics[out->dynamics_count] = dynamics;
        out->dynamics_part[out->dynamics_count] = event->part;
        out->dynamics_staff[out->dynamics_count] = event->staff;
        out->dynamics_count++;
    }
    return true;
}
