#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "io/midi_importer.h"
#include "io/midi_renderer.h"
#include "time/tempo_map.h"

#include <ursatz_composer/compose.h>
#include <ursatz_composer/constraint.h>
#include <ursatz_composer/export.h>
#include <ursatz_composer/form_template.h>
#include <ursatz_composer/piece.h>
#include <ursatz_composer/piece_plan.h>
#include <ursatz_composer/render.h>

/*
 * The DAW-friendly export (export.h, R-087): a filled rounded binary (24 bars, C major 4/4, seed 3) exported with
 * tempo 96.
 *
 *   - the file re-imported by Ursatz's importer has the same notes and one declared tempo region of 96 quarter notes
 *     per minute;
 *   - the conductor track is named for the form, the other tracks "melody" and "bass", and the markers A, B and A' are
 *     at the section starts, found by walking the track bytes for FF 03 and FF 06;
 *   - without names and markers (the --plain-midi options) no track carries either, and the bytes are those of
 *     Ursatz's renderer called with no options;
 *   - the MusicXML has <sound tempo="96"/>;
 *   - a tempo that cannot be written is refused.
 */

#define DIVISION 480u
#define MAX_META 16

typedef struct {
    unsigned long long tick;
    char text[32];
} Meta;

typedef struct {
    Meta names[MAX_META];
    size_t name_count;
    Meta markers[MAX_META];
    size_t marker_count;
} TrackMeta;

static unsigned long read_vlq(const unsigned char *data, size_t *at) {
    unsigned long value = 0;
    unsigned char byte;
    do {
        byte = data[(*at)++];
        value = (value << 7) | (byte & 0x7Fu);
    } while (byte & 0x80u);
    return value;
}

/* Walks the file (no running status: the renderer never uses it) and collects each track's names and markers. */
static size_t scan_tracks(const unsigned char *data, size_t size, TrackMeta *tracks, size_t max_tracks) {
    assert(size >= 14 && memcmp(data, "MThd", 4) == 0);
    size_t at = 14;
    size_t track = 0;
    while (at + 8 <= size) {
        assert(memcmp(data + at, "MTrk", 4) == 0);
        size_t length = ((size_t)data[at + 4] << 24) | ((size_t)data[at + 5] << 16) | ((size_t)data[at + 6] << 8) |
                        data[at + 7];
        at += 8;
        size_t end = at + length;
        assert(end <= size && track < max_tracks);
        memset(&tracks[track], 0, sizeof(tracks[track]));
        unsigned long long tick = 0;
        while (at < end) {
            tick += read_vlq(data, &at);
            unsigned char status = data[at++];
            if (status == 0xFF) {
                unsigned char type = data[at++];
                unsigned long meta_length = read_vlq(data, &at);
                if (type == 0x03 || type == 0x06) {
                    TrackMeta *meta = &tracks[track];
                    Meta *slot = type == 0x03 ? &meta->names[meta->name_count++] : &meta->markers[meta->marker_count++];
                    assert(meta->name_count <= MAX_META && meta->marker_count <= MAX_META);
                    assert(meta_length < sizeof(slot->text));
                    memcpy(slot->text, data + at, meta_length);
                    slot->text[meta_length] = '\0';
                    slot->tick = tick;
                }
                at += meta_length;
            } else if ((status & 0xF0u) == 0xC0u || (status & 0xF0u) == 0xD0u) {
                at += 1;
            } else {
                assert(status >= 0x80u && status < 0xF0u);
                at += 2;
            }
        }
        assert(at == end);
        track++;
    }
    return track;
}

static PiecePlan *filled_rounded_binary(void) {
    PieceRequest request;
    memset(&request, 0, sizeof(request));
    request.form = FORM_TEMPLATE_ROUNDED_BINARY;
    request.bars = 24;
    request.key.tonic_pitch_class = 0;
    request.key.mode = PLAN_MODE_MAJOR;
    request.meter.beats_per_bar = 4;
    request.meter.beat_unit = 4;
    ComposeConfig config;
    memset(&config, 0, sizeof(config));
    config.seed = 3;
    PiecePlan *plan = NULL;
    ComposeOutcome outcome;
    assert(piece_build_plan(&request, &plan) == PIECE_OK);
    assert(compose_run(plan, &config, &outcome) == PIECE_OK);
    compose_outcome_destroy(&outcome);
    return plan;
}

static bool same_event(const NoteEvent *a, long pitch_a, const NoteEvent *b, long pitch_b) {
    return pitch_a == pitch_b &&
           musical_time_equals(temporal_anchor_position(note_event_temporal_anchor(a)),
                               temporal_anchor_position(note_event_temporal_anchor(b))) &&
           duration_equals(note_event_duration(a), note_event_duration(b));
}

static bool has_name(const TrackMeta *track, const char *name) {
    for (size_t i = 0; i < track->name_count; i++) {
        if (strcmp(track->names[i].text, name) == 0) {
            return true;
        }
    }
    return false;
}

static void test_midi(const RenderedPlan *rendered, const PiecePlan *plan) {
    PlanExportOptions options = plan_export_options_default();
    assert(options.track_names && options.section_markers);
    assert(options.tempo_numerator == 0 && options.tempo_denominator == 0);
    options.tempo_numerator = 96;
    options.tempo_denominator = 1;

    unsigned char *data = NULL;
    size_t size = 0;
    assert(rendered_plan_midi_bytes(rendered, &options, &data, &size, NULL) == PLAN_EXPORT_OK);

    /* The same notes and one declared tempo of 96. */
    NoteEvent **events = NULL;
    PitchNumericValue **pitches = NULL;
    size_t count = 0;
    MidiTempoImport tempo;
    assert(midi_importer_import_with_tempo(data, size, &events, &pitches, &count, &tempo) == MIDI_IMPORT_OK);
    assert(count == rendered->count);
    bool *used = calloc(count, sizeof(bool));
    for (size_t i = 0; i < rendered->count; i++) {
        bool matched = false;
        for (size_t j = 0; j < count && !matched; j++) {
            if (!used[j] &&
                same_event(rendered->events[i], pitch_numeric_value_position_numerator(rendered->pitch_values[i]),
                           events[j], pitch_numeric_value_position_numerator(pitches[j]))) {
                used[j] = matched = true;
            }
        }
        assert(matched);
    }
    free(used);
    assert(!tempo_map_is_defaulted(tempo.tempo_map));
    assert(tempo_map_region_count(tempo.tempo_map) == 1);
    assert(tempo_map_region_microseconds_per_quarter(tempo_map_region(tempo.tempo_map, 0)) == 625000UL);
    for (size_t i = 0; i < count; i++) {
        note_event_destroy(events[i]);
        pitch_numeric_value_destroy(pitches[i]);
    }
    free(events);
    free(pitches);
    midi_tempo_import_destroy(&tempo);

    /* Names and markers. */
    TrackMeta tracks[16];
    size_t track_count = scan_tracks(data, size, tracks, 16);
    assert(track_count == 1 + rendered->part_count);
    assert(tracks[0].name_count == 1 && tracks[0].names[0].tick == 0);
    assert(strcmp(tracks[0].names[0].text, rendered->form_name ? rendered->form_name : "piece") == 0);
    bool melody = false;
    bool bass = false;
    for (size_t i = 1; i < track_count; i++) {
        assert(tracks[i].name_count == 1 && tracks[i].names[0].tick == 0);
        assert(tracks[i].marker_count == 0);
        melody = melody || has_name(&tracks[i], "melody");
        bass = bass || has_name(&tracks[i], "bass");
    }
    assert(melody && bass);

    /* Markers A, B and A' at the starts of the plan's sections, which the plan's own nodes give. */
    static const char *const LETTERS[3] = {"A", "B", "A'"};
    NodeId root = plan_root(plan);
    size_t found = 0;
    assert(tracks[0].marker_count == 3);
    for (size_t i = 0; i < plan_node_child_count(plan, root); i++) {
        NodeId node = plan_node_child_at(plan, root, i);
        bool material = false;
        for (size_t k = 0; k < plan_node_constraint_count(plan, node); k++) {
            material = material || plan_node_constraint_at(plan, node, k)->kind == PLAN_CONSTRAINT_MATERIAL;
        }
        if (plan_node_kind(plan, node) != PLAN_NODE_SECTION || material) {
            continue;
        }
        PlanTime start = plan_node_span(plan, node).start;
        assert(found < 3);
        assert(strcmp(tracks[0].markers[found].text, LETTERS[found]) == 0);
        assert(tracks[0].markers[found].tick * start.den == (unsigned long long)start.num * 4 * DIVISION);
        found++;
    }
    assert(found == 3);
    assert(tracks[0].markers[0].tick == 0 && tracks[0].markers[1].tick > 0);
    free(data);
}

static void test_plain(const RenderedPlan *rendered) {
    PlanExportOptions plain = plan_export_options_default();
    plain.track_names = false;
    plain.section_markers = false;
    unsigned char *data = NULL;
    size_t size = 0;
    assert(rendered_plan_midi_bytes(rendered, &plain, &data, &size, NULL) == PLAN_EXPORT_OK);

    TrackMeta tracks[16];
    size_t track_count = scan_tracks(data, size, tracks, 16);
    for (size_t i = 0; i < track_count; i++) {
        assert(tracks[i].name_count == 0 && tracks[i].marker_count == 0);
    }

    /* Ursatz's renderer with no options, the old export. */
    MidiKeySignatureEvent *keys = calloc(rendered->key_count ? rendered->key_count : 1, sizeof(*keys));
    for (size_t i = 0; i < rendered->key_count; i++) {
        keys[i].position = rendered->keys[i].position;
        keys[i].sharps_flats = (int8_t)rendered->keys[i].sharps_flats;
        keys[i].mode_byte = rendered->keys[i].mode == PLAN_MODE_MINOR ? 1 : 0;
        keys[i].mode_hint = rendered->keys[i].mode == PLAN_MODE_MINOR ? MIDI_MODE_HINT_MINOR : MIDI_MODE_HINT_MAJOR;
    }
    unsigned char *old = NULL;
    size_t old_size = 0;
    assert(midi_renderer_render(rendered->events, rendered->pitch_values, rendered->count, rendered->meter_map, NULL,
                                keys, rendered->key_count, DIVISION, &old, &old_size) == MIDI_RENDER_OK);
    assert(old_size == size && memcmp(old, data, size) == 0);
    free(keys);
    free(old);
    free(data);
}

static void test_musicxml(const RenderedPlan *rendered) {
    PlanExportOptions options = plan_export_options_default();
    options.tempo_numerator = 96;
    options.tempo_denominator = 1;
    char *xml = NULL;
    assert(rendered_plan_musicxml_text(rendered, &options, &xml, NULL) == PLAN_EXPORT_OK);
    assert(strstr(xml, "<sound tempo=\"96\"/>"));
    free(xml);

    xml = NULL;
    assert(rendered_plan_musicxml_text(rendered, NULL, &xml, NULL) == PLAN_EXPORT_OK);
    assert(!strstr(xml, "<sound tempo="));
    free(xml);
}

static void test_refused(const RenderedPlan *rendered) {
    PlanExportOptions options = plan_export_options_default();
    PlanExportError error;
    unsigned char *data = NULL;
    size_t size = 0;
    char *xml = NULL;

    options.tempo_numerator = 96; /* a denominator of 0 with a numerator */
    assert(rendered_plan_midi_bytes(rendered, &options, &data, &size, &error) == PLAN_EXPORT_ERR_NOT_REPRESENTABLE);
    assert(data == NULL);
    assert(rendered_plan_musicxml_text(rendered, &options, &xml, &error) == PLAN_EXPORT_ERR_NOT_REPRESENTABLE);

    options.tempo_numerator = 1; /* 1/100000 quarter notes per minute: beyond 0xFFFFFF microseconds per quarter */
    options.tempo_denominator = 100000;
    assert(rendered_plan_midi_bytes(rendered, &options, &data, &size, &error) == PLAN_EXPORT_ERR_NOT_REPRESENTABLE);
    assert(data == NULL);
}

/* The loop markers (R-172): loopStart and loopEnd in the conductor track at the times asked, after the section markers. */
static void test_loop_markers(const RenderedPlan *rendered) {
    PlanExportOptions options = plan_export_options_default();
    unsigned char *bytes = NULL;
    size_t size = 0;
    TrackMeta tracks[8];
    assert(!options.loop_markers);
    options.loop_markers = true;
    options.loop_start = (PlanTime){1, 1};
    options.loop_end = (PlanTime){11, 2};
    assert(rendered_plan_midi_bytes(rendered, &options, &bytes, &size, NULL) == PLAN_EXPORT_OK);
    size_t count = scan_tracks(bytes, size, tracks, 8);
    assert(count >= 2);
    bool start = false;
    bool end = false;
    for (size_t i = 0; i < tracks[0].marker_count; i++) {
        start = start || (strcmp(tracks[0].markers[i].text, "loopStart") == 0 && tracks[0].markers[i].tick == 4 * DIVISION);
        end = end || (strcmp(tracks[0].markers[i].text, "loopEnd") == 0 && tracks[0].markers[i].tick == 22 * DIVISION);
    }
    assert(start && end);
    /* The section markers are still there, and no other track carries a loop marker. */
    assert(tracks[0].marker_count == 3 + 2);
    for (size_t t = 1; t < count; t++) {
        assert(tracks[t].marker_count == 0);
    }
    free(bytes);
    /* Without them the file has none; with the plain options they stay, they are not a name or a section. */
    options.loop_markers = false;
    assert(rendered_plan_midi_bytes(rendered, &options, &bytes, &size, NULL) == PLAN_EXPORT_OK);
    scan_tracks(bytes, size, tracks, 8);
    assert(tracks[0].marker_count == 3);
    free(bytes);
    options.loop_markers = true;
    options.section_markers = false;
    options.track_names = false;
    assert(rendered_plan_midi_bytes(rendered, &options, &bytes, &size, NULL) == PLAN_EXPORT_OK);
    scan_tracks(bytes, size, tracks, 8);
    assert(tracks[0].marker_count == 2);
    free(bytes);
    /* A time that is not a whole tick at any division is refused. */
    options.loop_start = (PlanTime){1, 1000003};
    assert(rendered_plan_midi_bytes(rendered, &options, &bytes, &size, NULL) == PLAN_EXPORT_ERR_NOT_REPRESENTABLE);
}

int main(void) {
    PiecePlan *plan = filled_rounded_binary();
    RenderedPlan rendered;
    assert(plan_render(plan, &rendered) == PLAN_RENDER_OK);
    assert(rendered.section_count == 3);

    test_midi(&rendered, plan);
    test_plain(&rendered);
    test_musicxml(&rendered);
    test_refused(&rendered);
    test_loop_markers(&rendered);

    rendered_plan_destroy(&rendered);
    plan_destroy(plan);
    printf("export daw: ok\n");
    return 0;
}
