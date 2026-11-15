#include <ursatz_composer/export.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <core/rational.h>
#include <export/musicxml_export.h>
#include <identity/entity_id.h>
#include <io/midi_renderer.h>
#include <time/musical_time.h>
#include <time/time_rational.h>
#include <time/tempo_map.h>

#define PREFERRED_DIVISION 480u
#define MIDI_NAME_TEXT_MAX 48

static void set_message(PlanExportError *error, const char *format, ...) {
    if (!error) {
        return;
    }
    va_list args;
    va_start(args, format);
    vsnprintf(error->message, sizeof(error->message), format, args);
    va_end(args);
}

PlanExportOptions plan_export_options_default(void) {
    PlanExportOptions options;
    memset(&options, 0, sizeof(options));
    options.track_names = true;
    options.section_markers = true;
    return options;
}

static PlanExportOptions resolve_options(const PlanExportOptions *options) {
    return options ? *options : plan_export_options_default();
}

/* The options' tempo as a one-region TempoMap, NULL for none. */
static PlanExportResult build_tempo_map(const PlanExportOptions *options, TempoMap **out, PlanExportError *error) {
    *out = NULL;
    if (options->tempo_numerator == 0 && options->tempo_denominator == 0) {
        return PLAN_EXPORT_OK;
    }
    if (options->tempo_numerator == 0 || options->tempo_denominator == 0) {
        set_message(error, "the tempo must be a ratio of two numbers of 1 or more");
        return PLAN_EXPORT_ERR_NOT_REPRESENTABLE;
    }
    unsigned long long numerator = options->tempo_numerator;
    unsigned long long micros = (60000000ULL * options->tempo_denominator + numerator / 2) / numerator;
    if (micros < 1 || micros > 0xFFFFFFULL) {
        set_message(error, "a tempo of %lu/%lu quarter notes per minute cannot be written", options->tempo_numerator,
                    options->tempo_denominator);
        return PLAN_EXPORT_ERR_NOT_REPRESENTABLE;
    }
    EntityID *id = NULL;
    MusicalTime *start = NULL;
    TempoMapChangePoint point;
    TempoMapResult made = TEMPO_MAP_ERR_ALLOC;
    if (entity_id_create("composer-tempo-map", &id) == ENTITY_ID_OK &&
        musical_time_create(0, 1, &start) == MUSICAL_TIME_OK) {
        point.start = start;
        point.microseconds_per_quarter = (unsigned long)micros;
        made = tempo_map_create(id, &point, 1, out);
    }
    musical_time_destroy(start);
    if (made != TEMPO_MAP_OK) {
        entity_id_destroy(id);
        set_message(error, "out of memory");
        return PLAN_EXPORT_ERR_ALLOC;
    }
    return PLAN_EXPORT_OK;
}

/* ---------------------------------------------------------------- MIDI */

/* Folds the denominator of one time into the lcm of ticks-per-quarter it needs. False on overflow. */
static bool need_division_for(unsigned long denominator, unsigned long long *lcm) {
    unsigned long long factor = denominator / rational_gcd_unsigned(denominator, 4);
    unsigned long long divisor = rational_gcd_unsigned(*lcm, factor);
    unsigned long long grown;
    if (!rational_checked_mul_unsigned(*lcm / divisor, factor, &grown) || grown > MIDI_RENDER_MAX_DIVISION) {
        return false;
    }
    *lcm = grown;
    return true;
}

/* 480 when every time is a whole number of ticks at 480, else the smallest division that works; 0 if none fits. */
static unsigned choose_division(const RenderedPlan *rendered, const PlanExportOptions *options) {
    bool with_markers = options->section_markers;
    unsigned long long needed = 1;
    if (options->loop_markers &&
        (!need_division_for(options->loop_start.den, &needed) || !need_division_for(options->loop_end.den, &needed))) {
        return 0;
    }
    for (size_t i = 0; i < rendered->count; i++) {
        const NoteEvent *event = rendered->events[i];
        const MusicalTime *position = temporal_anchor_position(note_event_temporal_anchor(event));
        if (!need_division_for(musical_time_denominator(position), &needed) ||
            !need_division_for(duration_denominator(note_event_duration(event)), &needed)) {
            return 0;
        }
    }
    for (size_t i = 0; i < rendered->key_count; i++) {
        if (!need_division_for(musical_time_denominator(rendered->keys[i].position), &needed)) {
            return 0;
        }
    }
    for (size_t i = 0; with_markers && i < rendered->section_count; i++) {
        if (!need_division_for(musical_time_denominator(rendered->sections[i].position), &needed)) {
            return 0;
        }
    }
    if (rendered->meter_map) {
        for (size_t i = 0; i < meter_map_region_count(rendered->meter_map); i++) {
            const MusicalTime *start = meter_map_region_start(meter_map_region(rendered->meter_map, i));
            if (!need_division_for(musical_time_denominator(start), &needed)) {
                return 0;
            }
        }
    }
    return PREFERRED_DIVISION % needed == 0 ? PREFERRED_DIVISION : (unsigned)needed;
}

PlanExportResult rendered_plan_midi_bytes(const RenderedPlan *rendered, const PlanExportOptions *options_in,
                                          unsigned char **out_data, size_t *out_size, PlanExportError *error) {
    PlanExportOptions options = resolve_options(options_in);
    if (!rendered || !out_data || !out_size) {
        set_message(error, "rendered plan or output is NULL");
        return PLAN_EXPORT_ERR_NULL;
    }
    if (rendered->count == 0) {
        set_message(error, "the plan has no Given or Realized notes to write");
        return PLAN_EXPORT_ERR_NO_CONTENT;
    }
    unsigned division = choose_division(rendered, &options);
    if (division == 0) {
        set_message(error, "the note times need more than %d ticks per quarter", MIDI_RENDER_MAX_DIVISION);
        return PLAN_EXPORT_ERR_NOT_REPRESENTABLE;
    }

    MidiKeySignatureEvent *keys = rendered->key_count ? calloc(rendered->key_count, sizeof(*keys)) : NULL;
    if (rendered->key_count && !keys) {
        set_message(error, "out of memory");
        return PLAN_EXPORT_ERR_ALLOC;
    }
    for (size_t i = 0; i < rendered->key_count; i++) {
        keys[i].position = rendered->keys[i].position;
        keys[i].sharps_flats = (int8_t)rendered->keys[i].sharps_flats;
        keys[i].mode_byte = rendered->keys[i].mode == KEY_STRUCTURE_MODE_MINOR ? 1 : 0;
        keys[i].mode_hint = rendered->keys[i].mode == KEY_STRUCTURE_MODE_MINOR ? MIDI_MODE_HINT_MINOR : MIDI_MODE_HINT_MAJOR;
    }

    /* Track names: the conductor track for the form, each part's track for its lane. */
    char (*voice_ids)[MIDI_NAME_TEXT_MAX] = calloc(rendered->part_count, sizeof(*voice_ids));
    char (*part_names)[MIDI_NAME_TEXT_MAX] = calloc(rendered->part_count, sizeof(*part_names));
    MidiTrackName *names = calloc(rendered->part_count, sizeof(*names));
    if (!voice_ids || !part_names || !names) {
        free(voice_ids);
        free(part_names);
        free(names);
        free(keys);
        set_message(error, "out of memory");
        return PLAN_EXPORT_ERR_ALLOC;
    }
    for (size_t i = 0; i < rendered->part_count; i++) {
        rendered_part_voice_id(&rendered->parts[i], voice_ids[i], MIDI_NAME_TEXT_MAX);
        rendered_part_name(&rendered->parts[i], part_names[i], MIDI_NAME_TEXT_MAX);
        names[i].voice_id = voice_ids[i];
        names[i].name = part_names[i];
    }
    size_t section_markers = options.section_markers ? rendered->section_count : 0;
    size_t marker_count = section_markers + (options.loop_markers ? 2 : 0);
    MidiMarker *markers = marker_count ? calloc(marker_count, sizeof(*markers)) : NULL;
    MusicalTime *loop_positions[2] = {NULL, NULL};
    TempoMap *tempo_map = NULL;
    PlanExportResult tempo_result = build_tempo_map(&options, &tempo_map, error);
    bool loop_ok = !options.loop_markers ||
                   (musical_time_from_rational(options.loop_start, &loop_positions[0]) == TIME_RATIONAL_OK &&
                    musical_time_from_rational(options.loop_end, &loop_positions[1]) == TIME_RATIONAL_OK);
    if ((marker_count && !markers) || !loop_ok || tempo_result != PLAN_EXPORT_OK) {
        musical_time_destroy(loop_positions[0]);
        musical_time_destroy(loop_positions[1]);
        free(markers);
        free(voice_ids);
        free(part_names);
        free(names);
        free(keys);
        tempo_map_destroy(tempo_map);
        if (tempo_result == PLAN_EXPORT_OK) {
            set_message(error, loop_ok ? "out of memory" : "the loop markers are not times a file can hold");
            return loop_ok ? PLAN_EXPORT_ERR_ALLOC : PLAN_EXPORT_ERR_NOT_REPRESENTABLE;
        }
        return tempo_result;
    }
    MidiRenderOptions midi_options;
    memset(&midi_options, 0, sizeof(midi_options));
    if (markers) {
        for (size_t i = 0; i < section_markers; i++) {
            markers[i].position = rendered->sections[i].position;
            markers[i].text = rendered->sections[i].label;
        }
        if (options.loop_markers) {
            markers[section_markers].position = loop_positions[0];
            markers[section_markers].text = "loopStart";
            markers[section_markers + 1].position = loop_positions[1];
            markers[section_markers + 1].text = "loopEnd";
        }
        midi_options.markers = markers;
        midi_options.marker_count = marker_count;
    }
    if (options.track_names) {
        midi_options.conductor_name = rendered->form_name ? rendered->form_name : "piece";
        midi_options.track_names = names;
        midi_options.track_name_count = rendered->part_count;
    }

    MidiRenderResult result = midi_renderer_render_with_options(
        rendered->events, rendered->pitch_values, rendered->count, rendered->meter_map, tempo_map, keys,
        rendered->key_count, &midi_options, division, out_data, out_size);
    free(keys);
    free(voice_ids);
    free(part_names);
    free(names);
    free(markers);
    musical_time_destroy(loop_positions[0]);
    musical_time_destroy(loop_positions[1]);
    tempo_map_destroy(tempo_map);
    switch (result) {
    case MIDI_RENDER_OK:
        return PLAN_EXPORT_OK;
    case MIDI_RENDER_ERR_TOO_MANY_VOICES:
        set_message(error, "more than %d voice tracks; MIDI has no channel for the rest", MIDI_RENDER_MAX_VOICE_TRACKS);
        return PLAN_EXPORT_ERR_TOO_MANY_VOICES;
    case MIDI_RENDER_ERR_ALLOC:
        set_message(error, "out of memory");
        return PLAN_EXPORT_ERR_ALLOC;
    default:
        set_message(error,
                    "MIDI cannot represent the plan: a pitch outside 0 to 127, or two notes of one pitch overlapping "
                    "in one voice");
        return PLAN_EXPORT_ERR_NOT_REPRESENTABLE;
    }
}

/* ------------------------------------------------------------- MusicXML */

#define PART_TEXT_MAX 32

PlanExportResult rendered_plan_musicxml_text(const RenderedPlan *rendered, const PlanExportOptions *options_in,
                                             char **out_xml, PlanExportError *error) {
    PlanExportOptions options = resolve_options(options_in);
    if (!rendered || !out_xml) {
        set_message(error, "rendered plan or output is NULL");
        return PLAN_EXPORT_ERR_NULL;
    }
    if (rendered->count == 0) {
        set_message(error, "the plan has no Given or Realized notes to write");
        return PLAN_EXPORT_ERR_NO_CONTENT;
    }

    char (*names)[PART_TEXT_MAX] = calloc(rendered->part_count, sizeof(*names));
    MusicXmlExportPart *parts = calloc(rendered->part_count, sizeof(*parts));
    MusicXmlExportKeySignature *keys = rendered->key_count ? calloc(rendered->key_count, sizeof(*keys)) : NULL;
    if (!names || !parts || (rendered->key_count && !keys)) {
        free(names);
        free(parts);
        free(keys);
        set_message(error, "out of memory");
        return PLAN_EXPORT_ERR_ALLOC;
    }
    for (size_t i = 0; i < rendered->part_count; i++) {
        const RenderedPart *part = &rendered->parts[i];
        rendered_part_name(part, names[i], PART_TEXT_MAX);
        parts[i].id = names[i];
        parts[i].name = names[i];
        parts[i].staff_count = 1;
    }
    for (size_t i = 0; i < rendered->key_count; i++) {
        keys[i].sharps_flats = rendered->keys[i].sharps_flats;
        keys[i].mode = rendered->keys[i].mode == KEY_STRUCTURE_MODE_MINOR ? MUSICXML_EXPORT_MODE_MINOR
                                                                  : MUSICXML_EXPORT_MODE_MAJOR;
        keys[i].position = rendered->keys[i].position;
        keys[i].part_index = 0;
    }

    MusicXmlExportScore score;
    memset(&score, 0, sizeof(score));
    score.events = rendered->events;
    score.pitch_values = rendered->pitch_values;
    score.count = rendered->count;
    score.part_index = rendered->part_index;
    score.parts = parts;
    score.part_count = rendered->part_count;
    score.meter_map = rendered->meter_map;
    score.key_signatures = keys;
    score.key_signature_count = rendered->key_count;

    TempoMap *tempo_map = NULL;
    PlanExportResult tempo_result = build_tempo_map(&options, &tempo_map, error);
    if (tempo_result != PLAN_EXPORT_OK) {
        free(names);
        free(parts);
        free(keys);
        return tempo_result;
    }
    score.tempo_map = tempo_map;

    MusicXmlExportResult result = musicxml_export_write_score(&score, out_xml);
    tempo_map_destroy(tempo_map);
    free(names);
    free(parts);
    free(keys);
    switch (result) {
    case MUSICXML_EXPORT_OK:
        return PLAN_EXPORT_OK;
    case MUSICXML_EXPORT_ERR_OVERLAPPING_NOTES:
        set_message(error, "two notes overlap within one voice");
        return PLAN_EXPORT_ERR_OVERLAP;
    case MUSICXML_EXPORT_ERR_ALLOC:
        set_message(error, "out of memory");
        return PLAN_EXPORT_ERR_ALLOC;
    default:
        set_message(error, "MusicXML export refused the plan (code %d)", (int)result);
        return PLAN_EXPORT_ERR_NOT_REPRESENTABLE;
    }
}

/* ----------------------------------------------------------------- files */

static PlanExportResult write_file(const char *path, const void *data, size_t size, PlanExportError *error) {
    if (!path) {
        set_message(error, "path is NULL");
        return PLAN_EXPORT_ERR_NULL;
    }
    size_t path_length = strlen(path);
    char *temporary = malloc(path_length + sizeof(".tmp"));
    if (!temporary) {
        set_message(error, "out of memory");
        return PLAN_EXPORT_ERR_ALLOC;
    }
    memcpy(temporary, path, path_length);
    memcpy(temporary + path_length, ".tmp", sizeof(".tmp"));

    FILE *file = fopen(temporary, "wb");
    bool wrote = file && fwrite(data, 1, size, file) == size;
    bool closed = file && fclose(file) == 0;
    if (!wrote || !closed || rename(temporary, path) != 0) {
        remove(temporary);
        free(temporary);
        set_message(error, "could not write '%s'", path);
        return PLAN_EXPORT_ERR_IO;
    }
    free(temporary);
    return PLAN_EXPORT_OK;
}

PlanExportResult rendered_plan_export_midi(const RenderedPlan *rendered, const PlanExportOptions *options,
                                           const char *path, PlanExportError *error) {
    unsigned char *data = NULL;
    size_t size = 0;
    PlanExportResult result = rendered_plan_midi_bytes(rendered, options, &data, &size, error);
    if (result != PLAN_EXPORT_OK) {
        return result;
    }
    result = write_file(path, data, size, error);
    free(data);
    return result;
}

PlanExportResult rendered_plan_export_musicxml(const RenderedPlan *rendered, const PlanExportOptions *options,
                                               const char *path, PlanExportError *error) {
    char *xml = NULL;
    PlanExportResult result = rendered_plan_musicxml_text(rendered, options, &xml, error);
    if (result != PLAN_EXPORT_OK) {
        return result;
    }
    result = write_file(path, xml, strlen(xml), error);
    free(xml);
    return result;
}

PlanExportResult plan_export_render(const PiecePlan *plan, RenderedPlan *rendered, PlanExportError *error) {
    switch (plan_render(plan, rendered)) {
    case PLAN_RENDER_OK:
        return PLAN_EXPORT_OK;
    case PLAN_RENDER_ERR_NULL:
        set_message(error, "plan is NULL");
        return PLAN_EXPORT_ERR_NULL;
    case PLAN_RENDER_ERR_NOT_REPRESENTABLE:
        set_message(error, "the plan has a meter or time that cannot be written");
        return PLAN_EXPORT_ERR_NOT_REPRESENTABLE;
    case PLAN_RENDER_ERR_ALLOC:
        break;
    }
    set_message(error, "out of memory");
    return PLAN_EXPORT_ERR_ALLOC;
}

PlanExportResult plan_export_midi(const PiecePlan *plan, const PlanExportOptions *options, const char *path,
                                  PlanExportError *error) {
    RenderedPlan rendered;
    PlanExportResult result = plan_export_render(plan, &rendered, error);
    if (result != PLAN_EXPORT_OK) {
        return result;
    }
    result = rendered_plan_export_midi(&rendered, options, path, error);
    rendered_plan_destroy(&rendered);
    return result;
}

PlanExportResult plan_export_musicxml(const PiecePlan *plan, const PlanExportOptions *options, const char *path,
                                      PlanExportError *error) {
    RenderedPlan rendered;
    PlanExportResult result = plan_export_render(plan, &rendered, error);
    if (result != PLAN_EXPORT_OK) {
        return result;
    }
    result = rendered_plan_export_musicxml(&rendered, options, path, error);
    rendered_plan_destroy(&rendered);
    return result;
}
