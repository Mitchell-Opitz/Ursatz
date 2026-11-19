#include "render/movement_build.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <model/core/rational.h>
#include <model/identity/entity_id.h>
#include <model/pitch/pitch_numeric_value.h>
#include <model/pitch/region.h>
#include <model/pitch/pitch_spelling.h>
#include <model/time/musical_time.h>
#include <model/time/tempo_map.h>
#include <model/time/temporal_anchor.h>
#include <realization/line.h>
#include <realization/movement.h>
#include <realization/piano_reduction.h>

#include "util/util.h"

#define SOLO_PIANO_ENSEMBLE "omt.instrument.solo-piano"

void render_set_message(PlanExportError *error, const char *format, ...) {
    if (!error) {
        return;
    }
    va_list args;
    va_start(args, format);
    vsnprintf(error->message, sizeof(error->message), format, args);
    va_end(args);
}

/* The options' tempo as a one-region TempoMap, NULL for none. */
static PlanExportResult build_tempo_map(const PlanExportOptions *options, TempoMap **out, PlanExportError *error) {
    *out = NULL;
    if (options->tempo_numerator == 0 && options->tempo_denominator == 0) {
        return PLAN_EXPORT_OK;
    }
    if (options->tempo_numerator == 0 || options->tempo_denominator == 0) {
        render_set_message(error, "the tempo must be a ratio of two numbers of 1 or more");
        return PLAN_EXPORT_ERR_NOT_REPRESENTABLE;
    }
    unsigned long long numerator = options->tempo_numerator;
    unsigned long long micros = (60000000ULL * options->tempo_denominator + numerator / 2) / numerator;
    if (micros < 1 || micros > 0xFFFFFFULL) {
        render_set_message(error, "a tempo of %lu/%lu quarter notes per minute cannot be written", options->tempo_numerator,
                    options->tempo_denominator);
        return PLAN_EXPORT_ERR_NOT_REPRESENTABLE;
    }
    EntityID *id = NULL;
    TempoMapChangePoint point;
    TempoMapResult made = TEMPO_MAP_ERR_ALLOC;
    if (entity_id_create("composer-tempo-map", &id) == ENTITY_ID_OK) {
        point.start = musical_time_of(rational_ratio(0, 1));
        point.microseconds_per_quarter = (unsigned long)micros;
        point.kind = TEMPO_MAP_CHANGE_ABSOLUTE;
        made = tempo_map_create(id, &point, 1, out);
    }
    if (made != TEMPO_MAP_OK) {
        entity_id_destroy(id);
        render_set_message(error, "out of memory");
        return PLAN_EXPORT_ERR_ALLOC;
    }
    return PLAN_EXPORT_OK;
}

static PlanExportResult realization_failed(RealizationResult result, const char *what, PlanExportError *error) {
    if (result == REALIZATION_ERR_ALLOC) {
        render_set_message(error, "out of memory");
        return PLAN_EXPORT_ERR_ALLOC;
    }
    render_set_message(error, "the plan's %s cannot be held as a movement (realization result %d)", what, (int)result);
    return PLAN_EXPORT_ERR_NOT_REPRESENTABLE;
}

static const char *const OPERATION_NAMES[] = {"", "copy", "motive", "return"};

/* The id of the line note made from rendered event `event`: `n<plan note id>`, or `e<event + 1>` for a note with no plan note. */
static void line_note_id(const RenderedPlan *rendered, size_t event, char *out, size_t size) {
    if (rendered->note_ids[event] != PLAN_NOTE_NONE) {
        snprintf(out, size, "n%zu", (size_t)rendered->note_ids[event]);
    } else {
        snprintf(out, size, "e%zu", event + 1);
    }
}

bool rendered_plan_note_id(const RenderedPlan *rendered, size_t event, char out[REALIZATION_ID_MAX]) {
    if (!rendered || event >= rendered->count) {
        return false;
    }
    char line_note[32]; /* "n" or "e" and a number */
    line_note_id(rendered, event, line_note, sizeof(line_note));
    snprintf(out, REALIZATION_ID_MAX, "piano-%s", line_note);
    return true;
}

static void part_line_id(const RenderedPart *part, char *out, size_t size) {
    if (part->lane == PLAN_LANE_INNER_VOICE) {
        snprintf(out, size, "inner-%d", part->voice);
    } else {
        snprintf(out, size, "%s", rendered_part_lane_name(part));
    }
}

static LineRole part_role(const RenderedPart *part) {
    return part->lane == PLAN_LANE_MELODY ? LINE_ROLE_MELODY : part->lane == PLAN_LANE_BASS ? LINE_ROLE_BASS : LINE_ROLE_ACCOMPANIMENT;
}

/* The line of one rendered part: its events in order. */
static PlanExportResult build_line(const RenderedPlan *rendered, size_t part, Line **out, PlanExportError *error) {
    char id[REALIZATION_ID_MAX];
    char name[64];
    part_line_id(&rendered->parts[part], id, sizeof(id));
    rendered_part_name(&rendered->parts[part], name, sizeof(name));
    Line *line = NULL;
    RealizationResult result = line_create(id, part_role(&rendered->parts[part]), name, &line);
    if (result != REALIZATION_OK) {
        return realization_failed(result, "lines", error);
    }
    for (size_t i = 0; i < rendered->count && result == REALIZATION_OK; i++) {
        if (rendered->part_index[i] != part) {
            continue;
        }
        const NoteEvent *event = rendered->events[i];
        const MusicalTime *onset = temporal_anchor_position(note_event_temporal_anchor(event));
        LineNote note;
        memset(&note, 0, sizeof(note));
        line_note_id(rendered, i, note.id, sizeof(note.id));
        note.onset = *onset;
        note.duration = *note_event_duration(event);
        note.midi = (int)pitch_numeric_value_position_numerator(rendered->pitch_values[i]);
        const PitchSpelling *spelling = note_event_optional_pitch_spelling(event);
        if (spelling) {
            note.spelled = true;
            note.letter = pitch_spelling_letter(spelling);
            note.accidental = pitch_spelling_accidental(spelling);
        }
        if (rendered->note_sources[i] != PLAN_NOTE_NONE && rendered->note_operations[i] != PLAN_NOTE_FROM_NONE) {
            char source[REALIZATION_ID_MAX];
            snprintf(source, sizeof(source), "n%zu", (size_t)rendered->note_sources[i]);
            result = note_derivation_add_source(&note.derivation, source);
            if (result == REALIZATION_OK) {
                result = note_derivation_set_operation(&note.derivation, OPERATION_NAMES[rendered->note_operations[i]]);
            }
        }
        if (result == REALIZATION_OK) {
            result = line_add_note(line, &note);
        }
    }
    if (result != REALIZATION_OK) {
        line_destroy(line);
        return realization_failed(result, "notes", error);
    }
    *out = line;
    return PLAN_EXPORT_OK;
}

static PlanExportResult add_markers(Movement *movement, const RenderedPlan *rendered, const PlanExportOptions *options,
                                    PlanExportError *error) {
    RealizationResult result = REALIZATION_OK;
    for (size_t i = 0; options->section_markers && i < rendered->section_count && result == REALIZATION_OK; i++) {
        MovementMarker marker;
        memset(&marker, 0, sizeof(marker));
        marker.position = rendered->sections[i].position;
        snprintf(marker.text, sizeof(marker.text), "%s", rendered->sections[i].label ? rendered->sections[i].label : "");
        if (marker.text[0] != '\0') {
            result = movement_add_marker(movement, &marker);
        }
    }
    if (options->loop_markers) {
        MovementMarker start, end;
        memset(&start, 0, sizeof(start));
        memset(&end, 0, sizeof(end));
        start.position = musical_time_of(options->loop_start);
        end.position = musical_time_of(options->loop_end);
        strcpy(start.text, "loopStart");
        strcpy(end.text, "loopEnd");
        if (result == REALIZATION_OK) {
            result = movement_add_marker(movement, &start);
        }
        if (result == REALIZATION_OK) {
            result = movement_add_marker(movement, &end);
        }
    }
    return result == REALIZATION_OK ? PLAN_EXPORT_OK : realization_failed(result, "markers", error);
}

PlanExportResult rendered_plan_work(const RenderedPlan *rendered, const PlanExportOptions *options_in, const KnowledgeSnapshot *knowledge,
                                    Work **out, PlanExportError *error) {
    PlanExportOptions options = options_in ? *options_in : plan_export_options_default();
    if (!rendered || !out || !knowledge) {
        render_set_message(error, "rendered plan, knowledge snapshot or output is NULL");
        return PLAN_EXPORT_ERR_NULL;
    }
    if (rendered->count == 0) {
        render_set_message(error, "the plan has no Given or Realized notes to write");
        return PLAN_EXPORT_ERR_NO_CONTENT;
    }
    Work *work = NULL;
    Movement *movement = NULL;
    PlanExportResult status = PLAN_EXPORT_OK;
    RealizationResult result = work_create("w1", NULL, NULL, &work);
    if (result == REALIZATION_OK) {
        result = work_set_ensemble(work, SOLO_PIANO_ENSEMBLE);
    }
    if (result == REALIZATION_OK) {
        result = movement_create("m1", rendered->form_name ? rendered->form_name : "piece", &movement);
    }
    /* The movement's home region is the plan's first key (R-610): a tonal region of its tonic and material. */
    if (result == REALIZATION_OK && rendered->key_count > 0 && rendered->keys[0].material && rendered->keys[0].tonic >= 0 &&
        rendered->keys[0].tonic <= 11) {
        result = movement_set_home(movement, region_tonal(rendered->keys[0].tonic, rendered->keys[0].material));
    }
    if (result != REALIZATION_OK) {
        work_destroy(work);
        return realization_failed(result, "work", error);
    }
    if (rendered->meter_map) {
        MeterMap *copy = NULL;
        if (meter_map_copy(rendered->meter_map, &copy) != METER_MAP_OK) {
            status = PLAN_EXPORT_ERR_ALLOC;
            render_set_message(error, "out of memory");
        } else {
            movement_set_meter_map(movement, copy);
        }
    }
    TempoMap *tempo = NULL;
    if (status == PLAN_EXPORT_OK) {
        status = build_tempo_map(&options, &tempo, error);
        if (status == PLAN_EXPORT_OK && tempo) {
            movement_set_tempo_map(movement, tempo);
        }
    }
    for (size_t i = 0; status == PLAN_EXPORT_OK && i < rendered->key_count; i++) {
        MovementKeySignature key;
        key.position = rendered->keys[i].position;
        key.sharps_flats = rendered->keys[i].sharps_flats;
        key.mode = composer_material_is_minor(rendered->keys[i].material) ? MOVEMENT_KEY_MINOR : MOVEMENT_KEY_MAJOR;
        result = movement_add_key_signature(movement, &key);
        if (result != REALIZATION_OK) {
            status = realization_failed(result, "key signatures", error);
        }
    }
    if (status == PLAN_EXPORT_OK) {
        status = add_markers(movement, rendered, &options, error);
    }
    for (size_t p = 0; status == PLAN_EXPORT_OK && p < rendered->part_count; p++) {
        Line *line = NULL;
        status = build_line(rendered, p, &line, error);
        if (status == PLAN_EXPORT_OK && (result = movement_add_line(movement, line)) != REALIZATION_OK) {
            line_destroy(line);
            status = realization_failed(result, "lines", error);
        }
    }
    /* The movement is scored for the ensemble it names, else the work's: the solo piano, whose first slot is the piano. */
    const char *ensemble_id = work_movement_ensemble(work, movement);
    const EnsembleTemplate *ensemble = ensemble_id ? knowledge_snapshot_ensemble(knowledge, ensemble_id) : NULL;
    const Instrument *piano = ensemble && ensemble->slot_count > 0 ? knowledge_snapshot_instrument(knowledge, ensemble->slots[0].instrument) : NULL;
    if (status == PLAN_EXPORT_OK && !piano) {
        render_set_message(error, "the knowledge snapshot has no " SOLO_PIANO_ENSEMBLE " ensemble with a piano");
        status = PLAN_EXPORT_ERR_NOT_REPRESENTABLE;
    }
    if (status == PLAN_EXPORT_OK && (result = movement_reduce_to_piano(movement, piano, "")) != REALIZATION_OK) {
        status = realization_failed(result, "lines for the piano", error);
    }
    if (status == PLAN_EXPORT_OK && (result = work_add_movement(work, movement)) != REALIZATION_OK) {
        status = realization_failed(result, "movement", error);
    }
    if (status != PLAN_EXPORT_OK) {
        if (!work_movement_count(work)) {
            movement_destroy(movement);
        }
        work_destroy(work);
        return status;
    }
    *out = work;
    return PLAN_EXPORT_OK;
}
