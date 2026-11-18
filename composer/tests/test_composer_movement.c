#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>
#include <io/midi_renderer.h>
#include <io/musicxml_export.h>
#include <io/musicxml_importer.h>
#include <realization/movement.h>
#include <realization/work.h>
#include <ursatz_composer/composer_api.h>
#include <ursatz_composer/export.h>
#include <ursatz_composer/lanes.h>
#include <ursatz_composer/plan_json.h>
#include <ursatz_composer/render.h>

#include "render/movement_build.h"
#include "util/util.h"
#include "support/test_composer.h"
#include "support/test_knowledge.h"
#include "support/test_support.h"
#include "ursatz_test.h"

#ifndef COMPOSER_FIXTURE_DIR
#error "COMPOSER_FIXTURE_DIR is not defined"
#endif
#ifndef COMPOSER_SNAPSHOT_DIR
#error "COMPOSER_SNAPSHOT_DIR is not defined"
#endif

/*
 * The piano part of every stored plan (R-599, R-601): a plan renders into a work of one movement with one part `piano` that holds every
 * line, one assignment per line, melody on staff 1 and bass on staff 2 and an inner line by the mean of its notes, the performer ids
 * `piano-n<plan note id>`, and the line notes of motive-carrying melody notes naming their source. The MIDI has one part track with a
 * channel per line, and for every stored plan the MIDI notes per channel and the MusicXML notes are those of a render as voice tracks.
 */

typedef struct {
    unsigned channel;
    uint32_t on, off;
    unsigned key;
} Played;

static int compare_played(const void *left, const void *right) {
    const Played *a = left, *b = right;
    if (a->channel != b->channel) {
        return a->channel < b->channel ? -1 : 1;
    }
    if (a->on != b->on) {
        return a->on < b->on ? -1 : 1;
    }
    if (a->key != b->key) {
        return a->key < b->key ? -1 : 1;
    }
    return a->off < b->off ? -1 : (a->off > b->off);
}

/* A Standard MIDI File's header and track chunks (the bytes stay with the caller). */
typedef struct {
    const unsigned char *data;
    size_t size;
} SmfTrack;

typedef struct {
    unsigned division;
} SmfHeader;

static size_t smf_tracks(const unsigned char *data, size_t size, SmfHeader *header, SmfTrack *out, size_t capacity) {
    assert(size >= 14 && memcmp(data, "MThd", 4) == 0);
    header->division = ((unsigned)data[12] << 8) | data[13];
    size_t at = 14, count = 0;
    while (at + 8 <= size) {
        size_t length = ((size_t)data[at + 4] << 24) | ((size_t)data[at + 5] << 16) | ((size_t)data[at + 6] << 8) | data[at + 7];
        assert(memcmp(data + at, "MTrk", 4) == 0 && at + 8 + length <= size && count < capacity);
        out[count].data = data + at + 8;
        out[count].size = length;
        count++;
        at += 8 + length;
    }
    return count;
}

static uint32_t vlq(const SmfTrack *track, size_t *at) {
    uint32_t value = 0;
    for (;;) {
        unsigned char byte = track->data[(*at)++];
        value = (value << 7) | (byte & 0x7F);
        if (!(byte & 0x80)) {
            return value;
        }
    }
}

/* Every note of the tracks from `first` on as (channel, on, off, key), sorted; *channels is a bit set of the channels that sound. */
static Played *played_of(const unsigned char *data, size_t size, size_t first, size_t *count, unsigned *channels, size_t *tracks) {
    SmfHeader header;
    SmfTrack track_list[32];
    size_t track_count = smf_tracks(data, size, &header, track_list, 32);
    *tracks = track_count;
    size_t capacity = 64, used = 0;
    Played *notes = malloc(capacity * sizeof(Played));
    assert(notes);
    *channels = 0;
    for (size_t t = first; t < track_count; t++) {
        const SmfTrack *track = &track_list[t];
        size_t at = 0;
        uint32_t tick = 0;
        size_t open_from = used;
        while (at < track->size) {
            tick += vlq(track, &at);
            unsigned char status = track->data[at++];
            if (status == 0xFF) {
                at++;
                at += vlq(track, &at);
                continue;
            }
            unsigned kind = status & 0xF0, key = 0;
            if (kind == 0xC0 || kind == 0xD0) {
                at++;
                continue;
            }
            key = track->data[at++];
            unsigned char value = track->data[at++];
            if (kind == 0x90 && value > 0) {
                if (used == capacity) {
                    capacity *= 2;
                    notes = realloc(notes, capacity * sizeof(Played));
                    assert(notes);
                }
                notes[used].channel = status & 0x0F;
                notes[used].on = tick;
                notes[used].off = 0;
                notes[used].key = key;
                *channels |= 1u << (status & 0x0F);
                used++;
            } else if (kind == 0x80 || kind == 0x90) {
                for (size_t i = open_from; i < used; i++) {
                    if (notes[i].off == 0 && notes[i].key == key && notes[i].channel == (status & 0x0Fu)) {
                        notes[i].off = tick;
                        break;
                    }
                }
            }
        }
    }
    qsort(notes, used, sizeof(Played), compare_played);
    *count = used;
    return notes;
}

static size_t count_of(const char *haystack, const char *needle) {
    size_t n = 0;
    for (const char *p = strstr(haystack, needle); p; p = strstr(p + strlen(needle), needle)) {
        n++;
    }
    return n;
}

static unsigned count_bits(unsigned set) {
    unsigned n = 0;
    for (; set; set >>= 1) {
        n += set & 1u;
    }
    return n;
}

/* The pre-B7 layout: one track per voice, built with the library renderer and no parts, at the division the new file used. */
static unsigned char *voice_track_midi(const RenderedPlan *rendered, unsigned division, size_t *size) {
    MidiKeySignatureEvent *keys = rendered->key_count ? calloc(rendered->key_count, sizeof(*keys)) : NULL;
    for (size_t i = 0; i < rendered->key_count; i++) {
        keys[i].position = rendered->keys[i].position;
        keys[i].sharps_flats = (int8_t)rendered->keys[i].sharps_flats;
        keys[i].mode_byte = composer_material_is_minor(rendered->keys[i].material) ? 1 : 0;
        keys[i].mode_hint = composer_material_is_minor(rendered->keys[i].material) ? MIDI_MODE_HINT_MINOR : MIDI_MODE_HINT_MAJOR;
    }
    unsigned char *data = NULL;
    assert(midi_renderer_render_with_options(rendered->events, rendered->pitch_values, rendered->count, rendered->meter_map, NULL, keys,
                                             rendered->key_count, NULL, division, &data, size) == MIDI_RENDER_OK);
    free(keys);
    return data;
}

static char *voice_part_musicxml(const RenderedPlan *rendered) {
    MusicXmlExportPart *parts = calloc(rendered->part_count, sizeof(*parts));
    char (*names)[32] = calloc(rendered->part_count, sizeof(*names));
    MusicXmlExportKeySignature *keys = rendered->key_count ? calloc(rendered->key_count, sizeof(*keys)) : NULL;
    for (size_t i = 0; i < rendered->part_count; i++) {
        rendered_part_name(&rendered->parts[i], names[i], 32);
        parts[i].id = names[i];
        parts[i].name = names[i];
        parts[i].staff_count = 1;
    }
    for (size_t i = 0; i < rendered->key_count; i++) {
        keys[i].sharps_flats = rendered->keys[i].sharps_flats;
        keys[i].mode = composer_material_is_minor(rendered->keys[i].material) ? MUSICXML_EXPORT_MODE_MINOR : MUSICXML_EXPORT_MODE_MAJOR;
        keys[i].position = rendered->keys[i].position;
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
    char *xml = NULL;
    assert(musicxml_export_write_score(&score, &xml) == MUSICXML_EXPORT_OK);
    free(parts);
    free(names);
    free(keys);
    return xml;
}

typedef struct {
    long onset_n, onset_d, duration_n, duration_d;
    long midi;
} Fact;

static int compare_facts(const void *left, const void *right) {
    const Fact *a = left, *b = right;
    long long x = (long long)a->onset_n * b->onset_d, y = (long long)b->onset_n * a->onset_d;
    if (x != y) {
        return x < y ? -1 : 1;
    }
    if (a->midi != b->midi) {
        return a->midi < b->midi ? -1 : 1;
    }
    x = (long long)a->duration_n * b->duration_d;
    y = (long long)b->duration_n * a->duration_d;
    return x < y ? -1 : (x > y);
}

/* The notes of a MusicXML document as (onset, duration, pitch), by the importer (ties merged), sorted. */
static Fact *musicxml_facts(const char *xml, size_t *count) {
    MusicXmlImport import;
    assert(musicxml_import_buffer(xml, strlen(xml), &import) == MUSICXML_IMPORT_OK);
    Fact *facts = calloc(import.count ? import.count : 1, sizeof(Fact));
    assert(facts);
    for (size_t i = 0; i < import.count; i++) {
        const MusicalTime *onset = temporal_anchor_position(note_event_temporal_anchor(import.events[i]));
        const Duration *duration = note_event_duration(import.events[i]);
        facts[i].onset_n = (long)onset->value.num;
        facts[i].onset_d = (long)onset->value.den;
        facts[i].duration_n = (long)duration->value.num;
        facts[i].duration_d = (long)duration->value.den;
        facts[i].midi = pitch_numeric_value_position_numerator(import.pitch_values[i]);
    }
    *count = import.count;
    qsort(facts, import.count, sizeof(Fact), compare_facts);
    musicxml_import_destroy(&import);
    return facts;
}

static int expected_staff(const Line *line) {
    if (line_role(line) == LINE_ROLE_MELODY) {
        return 1;
    }
    if (line_role(line) == LINE_ROLE_BASS) {
        return 2;
    }
    long long sum = 0;
    for (size_t i = 0; i < line_note_count(line); i++) {
        sum += line_note_at(line, i)->midi;
    }
    return line_note_count(line) == 0 || sum >= 60 * (long long)line_note_count(line) ? 1 : 2;
}

/* One stored plan, as text: renders into the piano part and agrees with the voice-track layout. */
static void check_plan_text(const char *text, const char *name, bool *saw_motive_link) {
    PiecePlan *plan = NULL;
    PlanJsonError error;
    if (plan_from_json(test_knowledge(), text, &plan, &error) != PLAN_OK) {
        fprintf(stderr, "%s: %s (%s)\n", name, error.message, error.path);
    }
    assert(plan != NULL);
    RenderedPlan rendered;
    PlanExportError detail;
    assert(plan_export_render(plan, &rendered, &detail) == PLAN_EXPORT_OK);
    if (rendered.count == 0) { /* nothing realized to write */
        rendered_plan_destroy(&rendered);
        plan_destroy(plan);
        return;
    }

    Work *work = NULL;
    assert(rendered_plan_work(&rendered, NULL, test_knowledge(), &work, &detail) == PLAN_EXPORT_OK);
    assert(work_movement_count(work) == 1 && work_ensemble(work) != NULL && strcmp(work_ensemble(work), "omt.instrument.solo-piano") == 0);
    Movement *movement = work_movement_at(work, 0);
    assert(movement_part_count(movement) == 1 && movement_line_count(movement) == rendered.part_count &&
           movement_assignment_count(movement) == rendered.part_count);
    PerformerPart *piano = movement_part_named(movement, "piano");
    assert(piano != NULL && strcmp(performer_part_name(piano), "Piano") == 0 && performer_part_note_count(piano) == rendered.count);
    bool any_second_staff = false;
    for (size_t l = 0; l < movement_line_count(movement); l++) {
        const Line *line = movement_line_at(movement, l);
        const ScoringAssignment *a = movement_assignment_at(movement, l);
        assert(strcmp(a->line, line_id(line)) == 0 && strcmp(a->part, "piano") == 0 && a->whole && a->voice == (int)l + 1);
        assert(a->staff == expected_staff(line));
        any_second_staff = any_second_staff || a->staff == 2;
    }
    assert(performer_part_staff_count(piano) == (any_second_staff ? 2 : 1));
    /* every performer note is `piano-n<plan note id>`, and the plan's links are on the line notes */
    for (size_t i = 0; i < rendered.count; i++) {
        char id[64];
        snprintf(id, sizeof(id), "piano-n%zu", (size_t)rendered.note_ids[i]);
        const PerformerNote *found = NULL;
        for (size_t k = 0; k < performer_part_note_count(piano) && !found; k++) {
            found = strcmp(performer_part_note_at(piano, k)->id, id) == 0 ? performer_part_note_at(piano, k) : NULL;
        }
        assert(found != NULL);
        if (rendered.note_operations[i] == PLAN_NOTE_FROM_MOTIVE) {
            char source[64];
            snprintf(source, sizeof(source), "n%zu", (size_t)rendered.note_sources[i]);
            char own_id[64];
            snprintf(own_id, sizeof(own_id), "n%zu", (size_t)rendered.note_ids[i]);
            assert(found->derivation.source_count == 1 && strcmp(found->derivation.sources[0], own_id) == 0 && /* the performer note is made from its line note */
                   strcmp(found->derivation.operation, "score") == 0);
            const Line *line = movement_line_at(movement, rendered.part_index[i]);
            char note_id[64];
            snprintf(note_id, sizeof(note_id), "n%zu", (size_t)rendered.note_ids[i]);
            const LineNote *line_note = line_note_named(line, note_id);
            assert(line_note && line_note->derivation.source_count == 1 && strcmp(line_note->derivation.sources[0], source) == 0 &&
                   strcmp(line_note->derivation.operation, "motive") == 0);
            *saw_motive_link = true;
        }
    }

    /* MIDI: one part track, the notes per channel those of the voice tracks */
    unsigned char *midi = NULL;
    size_t midi_size = 0;
    assert(rendered_plan_midi_bytes(test_knowledge(), &rendered, NULL, &midi, &midi_size, &detail) == PLAN_EXPORT_OK);
    SmfHeader header;
    SmfTrack tracks[32];
    size_t track_count = smf_tracks(midi, midi_size, &header, tracks, 32);
    assert(track_count == 2);
    size_t size_plain = 0;
    unsigned char *plain = voice_track_midi(&rendered, header.division, &size_plain);
    size_t n_new = 0, n_old = 0, tracks_new = 0, tracks_old = 0;
    unsigned channels_new = 0, channels_old = 0;
    Played *new_notes = played_of(midi, midi_size, 1, &n_new, &channels_new, &tracks_new);
    Played *old_notes = played_of(plain, size_plain, 1, &n_old, &channels_old, &tracks_old);
    assert(tracks_old == 1 + rendered.part_count && n_new == n_old && n_new == rendered.count && channels_new == channels_old);
    assert(count_bits(channels_new) == rendered.part_count);
    assert(memcmp(new_notes, old_notes, n_new * sizeof(Played)) == 0);
    free(new_notes);
    free(old_notes);
    free(plain);
    free(midi);

    /* MusicXML: the same notes as the voice-part layout */
    char *xml = NULL;
    assert(rendered_plan_musicxml_text(test_knowledge(), &rendered, NULL, &xml, &detail) == PLAN_EXPORT_OK);
    char *old_xml = voice_part_musicxml(&rendered);
    size_t facts_new = 0, facts_old = 0;
    Fact *a = musicxml_facts(xml, &facts_new);
    Fact *b = musicxml_facts(old_xml, &facts_old);
    assert(facts_new == facts_old && memcmp(a, b, facts_new * sizeof(Fact)) == 0);
    assert(strstr(xml, "xml:id=\"piano-n") != NULL && strstr(xml, "<part-name>Piano</part-name>") != NULL);
    free(a);
    free(b);
    free(xml);
    free(old_xml);

    work_destroy(work);
    rendered_plan_destroy(&rendered);
    plan_destroy(plan);
}

static void test_stored_plans(void) {
    static const char *const FIXTURES[] = {"sentence_c_major.json", "sentence_c_to_g.json", "period_a_minor.json"};
    static const char *const SNAPSHOTS[] = {"grow-period", "grow-sentence", "piece-loop-and-ending", "piece-progression-fill", "piece-regenerate",
                                            "piece-theme-fill", "theme-chords", "theme-fast-minor", "theme-one-motive", "theme-period-contrast"};
    bool saw_motive_link = false;
    for (size_t i = 0; i < sizeof(FIXTURES) / sizeof(FIXTURES[0]); i++) {
        char path[512];
        snprintf(path, sizeof(path), COMPOSER_FIXTURE_DIR "/%s", FIXTURES[i]);
        char *text = test_must_read_file(path, NULL);
        check_plan_text(text, FIXTURES[i], &saw_motive_link);
        free(text);
    }
    for (size_t i = 0; i < sizeof(SNAPSHOTS) / sizeof(SNAPSHOTS[0]); i++) {
        char path[512];
        snprintf(path, sizeof(path), COMPOSER_SNAPSHOT_DIR "/%s.plan.json", SNAPSHOTS[i]);
        char *text = test_must_read_file(path, NULL);
        check_plan_text(text, SNAPSHOTS[i], &saw_motive_link);
        if (strcmp(SNAPSHOTS[i], "theme-one-motive") == 0) {
            /* a format 3 text of the same plan: ids from 1 in file order, the same piano part */
            char *three = test_plan_text_as_format_three(text);
            assert(three != NULL);
            bool ignored = false;
            check_plan_text(three, "theme-one-motive as format 3", &ignored);
            free(three);
        }
        free(text);
    }
    assert(saw_motive_link);
}

static void test_five_lines_in_one_part_track(void) {
    const char *request = "{\"request\": {\"schema\": \"ursatz-piece-request\", \"schema_version\": 2, \"form\": \"layered\", \"key\": \"C\", "
                          "\"mode\": \"major\", \"meter\": \"4/4\", \"motive\": null}, \"seed\": 1, \"midi\": true, \"musicxml\": true}";
    ComposerResponse response;
    ComposerStatus status = composer_api_piece(test_context(), request, NULL, &response);
    if (status != COMPOSER_OK) {
        fprintf(stderr, "%s (%s)\n", response.error.message, response.error.path);
    }
    assert(status == COMPOSER_OK && response.midi != NULL);
    size_t count, tracks;
    unsigned channels;
    Played *notes = played_of(response.midi, response.midi_size, 1, &count, &channels, &tracks);
    assert(tracks == 2 && count_bits(channels) == 5 && count > 0);
    free(notes);
    assert(response.musicxml != NULL && strstr(response.musicxml, "<staves>2</staves>") != NULL && count_of(response.musicxml, "<part id=") == 1);
    composer_response_destroy(&response);
}

int main(void) {
    test_stored_plans();
    test_five_lines_in_one_part_track();
    printf("test_composer_movement: all tests passed\n");
    return 0;
}
