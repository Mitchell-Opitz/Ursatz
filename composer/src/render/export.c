#include <ursatz_composer/export.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "export/musicxml_export.h"
#include "io/midi_renderer.h"

#define PREFERRED_DIVISION 480u

static void set_message(PlanExportError *error, const char *format, ...) {
    if (!error) {
        return;
    }
    va_list args;
    va_start(args, format);
    vsnprintf(error->message, sizeof(error->message), format, args);
    va_end(args);
}

static unsigned long long gcd_ull(unsigned long long a, unsigned long long b) {
    while (b != 0) {
        unsigned long long remainder = a % b;
        a = b;
        b = remainder;
    }
    return a;
}

/* ---------------------------------------------------------------- MIDI */

/* Folds the denominator of one time into the lcm of ticks-per-quarter it needs. False on overflow. */
static bool need_division_for(unsigned long denominator, unsigned long long *lcm) {
    unsigned long long factor = denominator / gcd_ull(denominator, 4);
    unsigned long long divisor = gcd_ull(*lcm, factor);
    unsigned long long grown = *lcm / divisor * factor;
    if (grown > MIDI_RENDER_MAX_DIVISION) {
        return false;
    }
    *lcm = grown;
    return true;
}

/* 480 when every time is a whole number of ticks at 480, else the smallest division that works; 0 if none fits. */
static unsigned choose_division(const RenderedPlan *rendered) {
    unsigned long long needed = 1;
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

PlanExportResult rendered_plan_midi_bytes(const RenderedPlan *rendered, unsigned char **out_data, size_t *out_size,
                                          PlanExportError *error) {
    if (!rendered || !out_data || !out_size) {
        set_message(error, "rendered plan or output is NULL");
        return PLAN_EXPORT_ERR_NULL;
    }
    if (rendered->count == 0) {
        set_message(error, "the plan has no Given or Realized notes to write");
        return PLAN_EXPORT_ERR_NO_CONTENT;
    }
    unsigned division = choose_division(rendered);
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
        keys[i].mode_byte = rendered->keys[i].mode == PLAN_MODE_MINOR ? 1 : 0;
        keys[i].mode_hint = rendered->keys[i].mode == PLAN_MODE_MINOR ? MIDI_MODE_HINT_MINOR : MIDI_MODE_HINT_MAJOR;
    }

    MidiRenderResult result = midi_renderer_render(rendered->events, rendered->pitch_values, rendered->count,
                                                   rendered->meter_map, NULL, keys, rendered->key_count, division,
                                                   out_data, out_size);
    free(keys);
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

PlanExportResult rendered_plan_musicxml_text(const RenderedPlan *rendered, char **out_xml, PlanExportError *error) {
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
        if (part->lane == PLAN_LANE_INNER_VOICE) {
            snprintf(names[i], PART_TEXT_MAX, "%s_%d", rendered_part_lane_name(part), part->voice);
        } else {
            snprintf(names[i], PART_TEXT_MAX, "%s", rendered_part_lane_name(part));
        }
        parts[i].id = names[i];
        parts[i].name = names[i];
        parts[i].staff_count = 1;
    }
    for (size_t i = 0; i < rendered->key_count; i++) {
        keys[i].sharps_flats = rendered->keys[i].sharps_flats;
        keys[i].mode = rendered->keys[i].mode == PLAN_MODE_MINOR ? MUSICXML_EXPORT_MODE_MINOR
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

    MusicXmlExportResult result = musicxml_export_write_score(&score, out_xml);
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

PlanExportResult rendered_plan_export_midi(const RenderedPlan *rendered, const char *path, PlanExportError *error) {
    unsigned char *data = NULL;
    size_t size = 0;
    PlanExportResult result = rendered_plan_midi_bytes(rendered, &data, &size, error);
    if (result != PLAN_EXPORT_OK) {
        return result;
    }
    result = write_file(path, data, size, error);
    free(data);
    return result;
}

PlanExportResult rendered_plan_export_musicxml(const RenderedPlan *rendered, const char *path,
                                               PlanExportError *error) {
    char *xml = NULL;
    PlanExportResult result = rendered_plan_musicxml_text(rendered, &xml, error);
    if (result != PLAN_EXPORT_OK) {
        return result;
    }
    result = write_file(path, xml, strlen(xml), error);
    free(xml);
    return result;
}

static PlanExportResult render_for_export(const PiecePlan *plan, RenderedPlan *rendered, PlanExportError *error) {
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

PlanExportResult plan_export_midi(const PiecePlan *plan, const char *path, PlanExportError *error) {
    RenderedPlan rendered;
    PlanExportResult result = render_for_export(plan, &rendered, error);
    if (result != PLAN_EXPORT_OK) {
        return result;
    }
    result = rendered_plan_export_midi(&rendered, path, error);
    rendered_plan_destroy(&rendered);
    return result;
}

PlanExportResult plan_export_musicxml(const PiecePlan *plan, const char *path, PlanExportError *error) {
    RenderedPlan rendered;
    PlanExportResult result = render_for_export(plan, &rendered, error);
    if (result != PLAN_EXPORT_OK) {
        return result;
    }
    result = rendered_plan_export_musicxml(&rendered, path, error);
    rendered_plan_destroy(&rendered);
    return result;
}
