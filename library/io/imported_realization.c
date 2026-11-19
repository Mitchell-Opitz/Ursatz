#include "io/imported_realization.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "model/events/note_event.h"
#include "model/identity/entity_id.h"
#include "model/pitch/pitch_numeric_value.h"
#include "model/pitch/pitch_spelling.h"
#include "model/time/meter_map.h"
#include "model/time/tempo_map.h"
#include "realization/movement.h"
#include "realization/performer_part.h"

#define PART_NAME_MAX 63

typedef struct {
    char id[REALIZATION_ID_MAX];
    char instrument[REALIZATION_ID_MAX];
    char name[PART_NAME_MAX + 1];
    int number;
    int staves;
    int program;
    PerformerPart *part;
    const char **voices; /* the distinct voice ids met so far, NULL for "no reference"; borrowed from the events */
    size_t voice_count;
    size_t voice_capacity;
} PartPlan;

static void destroy_plans(PartPlan *plans, size_t count) {
    for (size_t i = 0; plans && i < count; i++) {
        performer_part_destroy(plans[i].part);
        free(plans[i].voices);
    }
    free(plans);
}

static bool id_taken(const PartPlan *plans, size_t count, const char *id) {
    for (size_t i = 0; i < count; i++) {
        if (strcmp(plans[i].id, id) == 0) {
            return true;
        }
    }
    return false;
}

/* The instrument of a General MIDI program: the piano for 0 to 7. */
static void instrument_of(int program, char out[REALIZATION_ID_MAX]) {
    if (program < 0) {
        snprintf(out, REALIZATION_ID_MAX, "%s", "general-midi.instrument.percussion");
    } else if (program <= 7) {
        snprintf(out, REALIZATION_ID_MAX, "%s", "omt.instrument.piano");
    } else {
        snprintf(out, REALIZATION_ID_MAX, "general-midi.instrument.program-%d", program);
    }
}

/* The plan of every part of the import, in its order. */
static ImportedScoreResult plan_parts(const ImportedSource *source, PartPlan **out, size_t *count) {
    size_t total = source->part_count > 0 ? source->part_count : 1;
    PartPlan *plans = calloc(total, sizeof(PartPlan));
    if (!plans) {
        return IMPORTED_SCORE_ERR_ALLOC;
    }
    for (size_t i = 0; i < total; i++) {
        PartPlan *plan = &plans[i];
        const ImportedPart *part = source->part_count > 0 ? &source->parts[i] : NULL;
        if (part && part->id && realization_id_valid(part->id) && !id_taken(plans, i, part->id)) {
            snprintf(plan->id, sizeof(plan->id), "%s", part->id);
        } else {
            for (size_t n = i + 1;; n++) {
                snprintf(plan->id, sizeof(plan->id), "p%zu", n);
                if (!id_taken(plans, i, plan->id)) {
                    break;
                }
            }
        }
        int program = part ? part->midi_program : 0;
        instrument_of(program, plan->instrument);
        plan->program = program < 0 ? 0 : (program > 127 ? 127 : program);
        if (!part) {
            snprintf(plan->name, sizeof(plan->name), "%s", "Piano");
        } else if (part->name && part->name[0] != '\0') {
            snprintf(plan->name, sizeof(plan->name), "%.*s", PART_NAME_MAX, part->name);
        } else {
            snprintf(plan->name, sizeof(plan->name), "Part %zu", i + 1);
        }
        int staves = part ? part->staff_count : 1;
        plan->staves = staves < 1 ? 1 : (staves > 4 ? 4 : staves);
        plan->number = 1;
        for (size_t earlier = 0; earlier < i; earlier++) {
            plan->number += strcmp(plans[earlier].instrument, plan->instrument) == 0;
        }
    }
    *out = plans;
    *count = total;
    return IMPORTED_SCORE_OK;
}

static ImportedScoreResult map_error(RealizationResult result) {
    return result == REALIZATION_ERR_ALLOC ? IMPORTED_SCORE_ERR_ALLOC : IMPORTED_SCORE_ERR_FORMAT;
}

static ImportedScoreResult copy_tempo_map(const TempoMap *source, const char *id_text, TempoMap **out) {
    *out = NULL;
    EntityID *id = NULL;
    if (!source) {
        return IMPORTED_SCORE_OK;
    }
    if (entity_id_create(id_text, &id) != ENTITY_ID_OK) {
        return IMPORTED_SCORE_ERR_ALLOC;
    }
    if (tempo_map_is_defaulted(source)) {
        TempoMapResult made = tempo_map_create_defaulted(id, out);
        if (made != TEMPO_MAP_OK) {
            entity_id_destroy(id);
        }
        return made == TEMPO_MAP_OK ? IMPORTED_SCORE_OK : IMPORTED_SCORE_ERR_ALLOC;
    }
    size_t count = tempo_map_region_count(source);
    TempoMapChangePoint *points = calloc(count, sizeof(TempoMapChangePoint));
    size_t used = 0;
    for (size_t i = 0; points && i < count; i++) {
        const TempoMapRegion *region = tempo_map_region(source, i);
        if (tempo_map_region_is_defaulted(region)) {
            continue;
        }
        points[used].start = tempo_map_region_start(region);
        points[used].microseconds_per_quarter = tempo_map_region_microseconds_per_quarter(region);
        points[used].kind = TEMPO_MAP_CHANGE_ABSOLUTE;
        used++;
    }
    TempoMapResult made = points && used > 0 ? tempo_map_create(id, points, used, out) : TEMPO_MAP_ERR_ALLOC;
    free(points);
    if (made != TEMPO_MAP_OK) {
        entity_id_destroy(id);
        return made == TEMPO_MAP_ERR_ALLOC ? IMPORTED_SCORE_ERR_ALLOC : IMPORTED_SCORE_ERR_FORMAT;
    }
    return IMPORTED_SCORE_OK;
}

/* The voice number of a note in its part: 1 plus the place of its voice reference among the part's distinct ones. */
static int voice_of(PartPlan *plan, const char *voice, bool *failed) {
    for (size_t i = 0; i < plan->voice_count; i++) {
        bool same = (voice == NULL && plan->voices[i] == NULL) || (voice && plan->voices[i] && strcmp(voice, plan->voices[i]) == 0);
        if (same) {
            return (int)i + 1;
        }
    }
    if (plan->voice_count == plan->voice_capacity) {
        size_t capacity = plan->voice_capacity ? plan->voice_capacity * 2 : 4;
        const char **grown = realloc(plan->voices, capacity * sizeof(const char *));
        if (!grown) {
            *failed = true;
            return 1;
        }
        plan->voices = grown;
        plan->voice_capacity = capacity;
    }
    plan->voices[plan->voice_count++] = voice;
    return (int)plan->voice_count;
}

ImportedScoreResult imported_score_realization(const ImportedScore *score, const char *title, Work **out,
                                               char (**note_ids)[REALIZATION_ID_MAX]) {
    if (out) {
        *out = NULL;
    }
    if (note_ids) {
        *note_ids = NULL;
    }
    if (!score || !out) {
        return IMPORTED_SCORE_ERR_ARGUMENT;
    }
    const ImportedSource *source = &score->source;
    PartPlan *plans = NULL;
    size_t part_count = 0;
    ImportedScoreResult result = plan_parts(source, &plans, &part_count);
    char(*ids)[REALIZATION_ID_MAX] = result == IMPORTED_SCORE_OK && score->count > 0 ? calloc(score->count, REALIZATION_ID_MAX) : NULL;
    Work *work = NULL;
    Movement *movement = NULL;
    if (result == IMPORTED_SCORE_OK && score->count > 0 && !ids) {
        result = IMPORTED_SCORE_ERR_ALLOC;
    }
    if (result == IMPORTED_SCORE_OK) {
        result = map_error(work_create("w1", NULL, NULL, &work));
    }
    if (result == IMPORTED_SCORE_OK) {
        result = map_error(movement_create("m1", title, &movement));
    }
    /* The maps, copied: the movement owns them. */
    if (result == IMPORTED_SCORE_OK && score->meter.meter_map) {
        MeterMap *meter = NULL;
        MeterMapResult copied = meter_map_copy(score->meter.meter_map, &meter);
        result = copied == METER_MAP_OK ? IMPORTED_SCORE_OK : IMPORTED_SCORE_ERR_ALLOC;
        if (result == IMPORTED_SCORE_OK && movement_set_meter_map(movement, meter) != REALIZATION_OK) {
            meter_map_destroy(meter);
            result = IMPORTED_SCORE_ERR_ALLOC;
        }
    }
    if (result == IMPORTED_SCORE_OK && score->tempo_map) {
        TempoMap *tempo = NULL;
        result = copy_tempo_map(score->tempo_map, "m1-tempo", &tempo);
        if (result == IMPORTED_SCORE_OK && movement_set_tempo_map(movement, tempo) != REALIZATION_OK) {
            tempo_map_destroy(tempo);
            result = IMPORTED_SCORE_ERR_ALLOC;
        }
    }
    for (size_t i = 0; result == IMPORTED_SCORE_OK && i < score->key_signature_events.count; i++) {
        const MidiKeySignatureEvent *event = &score->key_signature_events.events[i];
        MovementKeySignature key;
        key.position = event->position;
        key.sharps_flats = event->sharps_flats;
        key.mode = event->mode_hint == MIDI_MODE_HINT_MAJOR ? MOVEMENT_KEY_MAJOR
                   : event->mode_hint == MIDI_MODE_HINT_MINOR ? MOVEMENT_KEY_MINOR
                                                              : MOVEMENT_KEY_UNKNOWN;
        result = map_error(movement_add_key_signature(movement, &key));
    }
    for (size_t i = 0; result == IMPORTED_SCORE_OK && i < part_count; i++) {
        PartPlan *plan = &plans[i];
        result = map_error(performer_part_create(plan->id, plan->instrument, plan->name, plan->number, 1, plan->staves, plan->program, &plan->part));
    }
    for (size_t i = 0; result == IMPORTED_SCORE_OK && i < score->count; i++) {
        size_t part_index = source->part_count > 0 && source->part_index ? source->part_index[i] : 0;
        if (part_index >= part_count) {
            result = IMPORTED_SCORE_ERR_FORMAT;
            break;
        }
        PartPlan *plan = &plans[part_index];
        const NoteEvent *event = score->events[i];
        PerformerNote note;
        memset(&note, 0, sizeof(note));
        note.onset = *temporal_anchor_position(note_event_temporal_anchor(event));
        note.duration = *note_event_duration(event);
        note.midi = (int)pitch_numeric_value_position_numerator(score->pitch_values[i]);
        const PitchSpelling *spelling = source->spelling_notated ? note_event_optional_pitch_spelling(event) : NULL;
        if (spelling) {
            note.spelled = true;
            note.letter = pitch_spelling_letter(spelling);
            note.accidental = pitch_spelling_accidental(spelling);
        }
        note.staff = source->part_count > 0 && source->staff ? source->staff[i] : 1;
        if (note.staff < 1) {
            note.staff = 1;
        }
        if (note.staff > plan->staves) {
            note.staff = plan->staves;
        }
        const VoiceReference *reference = note_event_optional_voice(event);
        bool failed = false;
        note.voice = voice_of(plan, reference ? entity_id_value(voice_reference_target_id(reference)) : NULL, &failed);
        if (failed) {
            result = IMPORTED_SCORE_ERR_ALLOC;
            break;
        }
        const char *event_id = entity_id_value(note_event_id(event));
        RealizationResult added = REALIZATION_ERR_BAD_ID;
        if (event_id && realization_id_valid(event_id)) {
            snprintf(note.id, sizeof(note.id), "%s", event_id);
            added = performer_part_add_note(plan->part, &note);
        }
        for (unsigned attempt = 1; added == REALIZATION_ERR_BAD_ID || added == REALIZATION_ERR_DUPLICATE_ID; attempt++) {
            if (attempt == 1) {
                snprintf(note.id, sizeof(note.id), "n%zu", i + 1);
            } else {
                snprintf(note.id, sizeof(note.id), "n%zu-%u", i + 1, attempt);
            }
            added = performer_part_add_note(plan->part, &note);
        }
        if (added != REALIZATION_OK) {
            result = map_error(added);
            break;
        }
        snprintf(ids[i], REALIZATION_ID_MAX, "%s", note.id);
    }
    for (size_t i = 0; result == IMPORTED_SCORE_OK && i < part_count; i++) {
        result = map_error(movement_add_part(movement, plans[i].part));
        if (result == IMPORTED_SCORE_OK) {
            plans[i].part = NULL; /* the movement owns it now */
        }
    }
    if (result == IMPORTED_SCORE_OK) {
        result = map_error(work_add_movement(work, movement));
        if (result == IMPORTED_SCORE_OK) {
            movement = NULL;
        }
    }
    destroy_plans(plans, part_count);
    if (result != IMPORTED_SCORE_OK) {
        movement_destroy(movement);
        work_destroy(work);
        free(ids);
        return result;
    }
    *out = work;
    if (note_ids) {
        *note_ids = ids;
    } else {
        free(ids);
    }
    return IMPORTED_SCORE_OK;
}
