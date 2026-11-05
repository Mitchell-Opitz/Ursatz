#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <io/midi_importer.h>
#include <io/midi_key_signature.h>
#include <io/musicxml_importer.h>
#include <pitch/pitch_spelling.h>

#include <ursatz_composer/constraint.h>
#include <ursatz_composer/export.h>
#include <ursatz_composer/lanes.h>
#include <ursatz_composer/piece_plan.h>
#include <ursatz_composer/plan_json.h>
#include <ursatz_composer/plan_state.h>
#include <ursatz_composer/render.h>

/*
 * A hand-written 8-bar sentence in D minor, 4/4, with melody and bass. It is
 * rendered, exported to MIDI and MusicXML, and read back with Ursatz's own
 * importers: every pitch, onset and duration, the meter and the key survive.
 * A Constrained span is skipped and reported.
 */

static PlanSpan bars(long start, long length) {
    PlanSpan span;
    assert(rational_make(start, 1, &span.start) == RATIONAL_OK);
    assert(rational_make(length, 1, &span.length) == RATIONAL_OK);
    return span;
}

static Rational time_of(long num, unsigned long den) {
    Rational time;
    assert(rational_make(num, den, &time) == RATIONAL_OK);
    return time;
}

static NodeId add(PiecePlan *plan, NodeId parent, PlanNodeKind kind, PlanSpan span, const char *name) {
    NodeId id = PLAN_NODE_NONE;
    assert(plan_add_node(plan, parent, kind, span, name, &id) == PLAN_OK);
    return id;
}

static PlanNote note(int pitch, Rational offset, Rational duration) {
    return plan_note(pitch, offset, duration);
}

static LaneSpanId add_notes(PiecePlan *plan, NodeId owner, PlanLaneKind lane, int voice, PlanSpan span,
                            const PlanNote *notes, size_t count) {
    PlanLanePayload payload;
    memset(&payload, 0, sizeof(payload));
    payload.notes.notes = notes;
    payload.notes.note_count = count;
    LaneSpanId id = PLAN_LANE_SPAN_NONE;
    assert(plan_add_lane_span(plan, owner, lane, voice, span, &payload, &id) == PLAN_OK);
    return id;
}

static void add_meter_and_key(PiecePlan *plan, NodeId owner, PlanSpan span) {
    PlanLanePayload meter;
    memset(&meter, 0, sizeof(meter));
    meter.meter.beats_per_bar = 4;
    meter.meter.beat_unit = 4;
    LaneSpanId meter_span = PLAN_LANE_SPAN_NONE;
    assert(plan_add_lane_span(plan, owner, PLAN_LANE_METER, 0, span, &meter, &meter_span) == PLAN_OK);
    assert(plan_lane_span_set_state(plan, meter_span, PLAN_STATE_GIVEN) == PLAN_OK);

    PlanLanePayload key;
    memset(&key, 0, sizeof(key));
    key.key = plan_key_of_pitch_class(2, KEY_STRUCTURE_MODE_MINOR);
    LaneSpanId key_span = PLAN_LANE_SPAN_NONE;
    assert(plan_add_lane_span(plan, owner, PLAN_LANE_KEY, 0, span, &key, &key_span) == PLAN_OK);
    assert(plan_lane_span_set_state(plan, key_span, PLAN_STATE_GIVEN) == PLAN_OK);
}

#define EXPECTED_NOTES 13

static PiecePlan *build_sentence_plan(void) {
    PiecePlan *plan = plan_create();
    NodeId form = add(plan, PLAN_NODE_NONE, PLAN_NODE_FORM, bars(0, 8), "form");
    NodeId section = add(plan, form, PLAN_NODE_SECTION, bars(0, 8), "A");
    NodeId phrase = add(plan, section, PLAN_NODE_PHRASE, bars(0, 8), "sentence");
    NodeId presentation = add(plan, phrase, PLAN_NODE_SUBPHRASE, bars(0, 4), "presentation");
    NodeId basic_idea = add(plan, presentation, PLAN_NODE_IDEA, bars(0, 2), "basic idea");
    NodeId repetition = add(plan, presentation, PLAN_NODE_IDEA, bars(2, 2), "repetition");
    NodeId continuation = add(plan, phrase, PLAN_NODE_SUBPHRASE, bars(4, 4), "continuation");

    add_meter_and_key(plan, form, bars(0, 8));

    /* Melody of the basic idea: seven notes, ending on a triplet eighth. */
    Rational quarter = time_of(1, 4);
    PlanNote idea_notes[] = {
        note(69, time_of(0, 1), quarter),   note(74, time_of(1, 4), quarter),   note(77, time_of(1, 2), quarter),
        note(74, time_of(3, 4), quarter),   note(72, time_of(1, 1), time_of(1, 2)),
        note(70, time_of(3, 2), time_of(1, 4)), note(67, time_of(7, 4), time_of(1, 12)),
    };
    LaneSpanId idea_melody = add_notes(plan, basic_idea, PLAN_LANE_MELODY, 0, bars(0, 2), idea_notes, 7);
    assert(plan_lane_span_set_state(plan, idea_melody, PLAN_STATE_GIVEN) == PLAN_OK);

    PlanNote repetition_notes[] = {note(69, time_of(0, 1), time_of(1, 2)), note(74, time_of(1, 2), time_of(1, 2))};
    LaneSpanId repetition_melody = add_notes(plan, repetition, PLAN_LANE_MELODY, 0, bars(2, 2), repetition_notes, 2);
    assert(plan_lane_span_set_state(plan, repetition_melody, PLAN_STATE_REALIZED) == PLAN_OK);

    PlanNote bass_one[] = {note(50, time_of(0, 1), time_of(1, 1)), note(45, time_of(1, 1), time_of(1, 1))};
    PlanNote bass_two[] = {note(43, time_of(0, 1), time_of(1, 1)), note(45, time_of(1, 1), time_of(1, 1))};
    LaneSpanId bass_a = add_notes(plan, basic_idea, PLAN_LANE_BASS, 0, bars(0, 2), bass_one, 2);
    LaneSpanId bass_b = add_notes(plan, repetition, PLAN_LANE_BASS, 0, bars(2, 2), bass_two, 2);
    assert(plan_lane_span_set_state(plan, bass_a, PLAN_STATE_REALIZED) == PLAN_OK);
    assert(plan_lane_span_set_state(plan, bass_b, PLAN_STATE_REALIZED) == PLAN_OK);

    /* The continuation's melody is not written yet. */
    PlanNote unwritten[] = {note(72, time_of(0, 1), time_of(1, 1))};
    add_notes(plan, continuation, PLAN_LANE_MELODY, 0, bars(4, 4), unwritten, 1);

    PlanLanePayload chord;
    memset(&chord, 0, sizeof(chord));
    strcpy(chord.harmony.roman_numeral, "i");
    LaneSpanId chord_span = PLAN_LANE_SPAN_NONE;
    assert(plan_add_lane_span(plan, basic_idea, PLAN_LANE_HARMONY, 0, bars(0, 2), &chord, &chord_span) == PLAN_OK);
    assert(plan_lane_span_set_state(plan, chord_span, PLAN_STATE_REALIZED) == PLAN_OK);
    return plan;
}

/* ---------------------------------------------------------------- rendering */

static void test_render(void) {
    PiecePlan *plan = build_sentence_plan();
    RenderedPlan rendered;
    assert(plan_render(plan, &rendered) == PLAN_RENDER_OK);

    assert(rendered.count == EXPECTED_NOTES);
    assert(rendered.part_count == 2);
    assert(rendered.parts[0].lane == PLAN_LANE_MELODY);
    assert(rendered.parts[1].lane == PLAN_LANE_BASS);
    assert(rendered.meter_map != NULL);
    assert(rendered.key_count == 1);
    assert(rendered.keys[0].sharps_flats == -1);
    assert(rendered.keys[0].mode == KEY_STRUCTURE_MODE_MINOR);

    /* In time order: the first two events sound together, melody before bass. */
    const MusicalTime *first = temporal_anchor_position(note_event_temporal_anchor(rendered.events[0]));
    const MusicalTime *second = temporal_anchor_position(note_event_temporal_anchor(rendered.events[1]));
    assert(musical_time_numerator(first) == 0 && musical_time_numerator(second) == 0);
    assert(rendered.part_index[0] == 0 && rendered.part_index[1] == 1);
    assert(pitch_numeric_value_position_numerator(rendered.pitch_values[0]) == 69);
    assert(pitch_numeric_value_position_numerator(rendered.pitch_values[1]) == 50);
    for (size_t i = 1; i < rendered.count; i++) {
        const MusicalTime *before = temporal_anchor_position(note_event_temporal_anchor(rendered.events[i - 1]));
        const MusicalTime *after = temporal_anchor_position(note_event_temporal_anchor(rendered.events[i]));
        assert((long long)musical_time_numerator(before) * (long long)musical_time_denominator(after) <=
               (long long)musical_time_numerator(after) * (long long)musical_time_denominator(before));
    }

    /* B-flat 4 is spelled with a flat in D minor. */
    bool found_flat = false;
    for (size_t i = 0; i < rendered.count; i++) {
        const PitchSpelling *spelling = note_event_optional_pitch_spelling(rendered.events[i]);
        assert(spelling != NULL);
        if (pitch_numeric_value_position_numerator(rendered.pitch_values[i]) == 70) {
            assert(pitch_spelling_letter(spelling) == PITCH_LETTER_B);
            assert(pitch_spelling_accidental(spelling) == -1);
            assert(pitch_spelling_octave(spelling) == 4);
            found_flat = true;
        }
    }
    assert(found_flat);

    /* The unwritten melody and the harmony are skipped, and reported. */
    assert(rendered.skipped_count == 2);
    size_t not_realized = 0;
    size_t not_rendered = 0;
    for (size_t i = 0; i < rendered.skipped_count; i++) {
        not_realized += rendered.skipped[i].reason == RENDER_SKIP_NOT_REALIZED;
        not_rendered += rendered.skipped[i].reason == RENDER_SKIP_LANE_NOT_RENDERED;
    }
    assert(not_realized == 1 && not_rendered == 1);

    /* The same plan renders to the same bytes. */
    RenderedPlan again;
    assert(plan_render(plan, &again) == PLAN_RENDER_OK);
    unsigned char *midi_a = NULL;
    unsigned char *midi_b = NULL;
    size_t size_a = 0;
    size_t size_b = 0;
    char *xml_a = NULL;
    char *xml_b = NULL;
    assert(rendered_plan_midi_bytes(&rendered, NULL, &midi_a, &size_a, NULL) == PLAN_EXPORT_OK);
    assert(rendered_plan_midi_bytes(&again, NULL, &midi_b, &size_b, NULL) == PLAN_EXPORT_OK);
    assert(size_a == size_b && memcmp(midi_a, midi_b, size_a) == 0);
    assert(rendered_plan_musicxml_text(&rendered, NULL, &xml_a, NULL) == PLAN_EXPORT_OK);
    assert(rendered_plan_musicxml_text(&again, NULL, &xml_b, NULL) == PLAN_EXPORT_OK);
    assert(strcmp(xml_a, xml_b) == 0);
    free(midi_a);
    free(midi_b);
    free(xml_a);
    free(xml_b);

    rendered_plan_destroy(&again);
    rendered_plan_destroy(&rendered);
    plan_destroy(plan);
}

/* --------------------------------------------------------------- re-import */

static bool same_event(const NoteEvent *a, long pitch_a, const NoteEvent *b, long pitch_b) {
    return pitch_a == pitch_b &&
           musical_time_equals(temporal_anchor_position(note_event_temporal_anchor(a)),
                               temporal_anchor_position(note_event_temporal_anchor(b))) &&
           duration_equals(note_event_duration(a), note_event_duration(b));
}

/* Every rendered note has exactly one match among the imported ones. */
static void assert_same_notes(const RenderedPlan *rendered, NoteEvent *const *events,
                              PitchNumericValue *const *pitches, size_t count, const size_t *part_index,
                              const size_t *rendered_parts) {
    assert(count == rendered->count);
    bool *used = calloc(count, sizeof(bool));
    for (size_t i = 0; i < rendered->count; i++) {
        bool matched = false;
        for (size_t j = 0; j < count && !matched; j++) {
            if (!used[j] &&
                same_event(rendered->events[i], pitch_numeric_value_position_numerator(rendered->pitch_values[i]),
                           events[j], pitch_numeric_value_position_numerator(pitches[j])) &&
                (!part_index || part_index[j] == rendered_parts[i])) {
                used[j] = true;
                matched = true;
            }
        }
        assert(matched);
    }
    free(used);
}

static void test_midi_round_trip(void) {
    PiecePlan *plan = build_sentence_plan();
    RenderedPlan rendered;
    assert(plan_render(plan, &rendered) == PLAN_RENDER_OK);

    unsigned char *data = NULL;
    size_t size = 0;
    assert(rendered_plan_midi_bytes(&rendered, NULL, &data, &size, NULL) == PLAN_EXPORT_OK);

    MidiImport import;
    assert(midi_importer_import_complete(data, size, &import) == MIDI_IMPORT_OK);
    NoteEvent **events = import.events;
    PitchNumericValue **pitches = import.pitch_values;
    size_t count = import.count;
    assert_same_notes(&rendered, events, pitches, count, NULL, NULL);

    /* 4/4, declared. */
    assert(!meter_map_is_defaulted(import.meter.meter_map));
    const TimeSignature *signature = meter_map_region_time_signature(meter_map_region(import.meter.meter_map, 0));
    assert(time_signature_top_number(signature) == 4);
    assert(time_signature_bottom_number(signature) == 4);

    /* D minor: one flat, minor. */
    MidiKeySignatureEvents keys = {NULL, 0};
    assert(midi_key_signature_scan_all(data, size, &keys) == MIDI_KEY_SIGNATURE_SCAN_ALL_OK);
    assert(keys.count == 1);
    assert(keys.events[0].sharps_flats == -1);
    assert(keys.events[0].mode_hint == MIDI_MODE_HINT_MINOR);
    assert(musical_time_numerator(keys.events[0].position) == 0);

    midi_key_signature_events_destroy(&keys);
    midi_import_destroy(&import);
    free(data);
    rendered_plan_destroy(&rendered);
    plan_destroy(plan);
}

static void test_musicxml_round_trip(void) {
    PiecePlan *plan = build_sentence_plan();
    RenderedPlan rendered;
    assert(plan_render(plan, &rendered) == PLAN_RENDER_OK);

    char *xml = NULL;
    assert(rendered_plan_musicxml_text(&rendered, NULL, &xml, NULL) == PLAN_EXPORT_OK);

    MusicXmlImport imported;
    memset(&imported, 0, sizeof(imported));
    assert(musicxml_import_buffer(xml, strlen(xml), &imported) == MUSICXML_IMPORT_OK);
    assert_same_notes(&rendered, imported.events, imported.pitch_values, imported.count, imported.part_index,
                      rendered.part_index);

    /* One part per lane voice. */
    assert(imported.part_count == 2);
    assert(strcmp(imported.parts[0].name, "melody") == 0);
    assert(strcmp(imported.parts[1].name, "bass") == 0);

    /* 4/4, declared. */
    assert(!meter_map_is_defaulted(imported.meter_map));
    const TimeSignature *signature = meter_map_region_time_signature(meter_map_region(imported.meter_map, 0));
    assert(time_signature_top_number(signature) == 4);
    assert(time_signature_bottom_number(signature) == 4);

    /* D minor. */
    assert(imported.key_signature_count == 1);
    assert(imported.key_signatures[0].sharps_flats == -1);
    assert(imported.key_signatures[0].mode_hint == MUSICXML_MODE_HINT_MINOR);

    /* The written pitch survives: B-flat 4. */
    bool found_flat = false;
    for (size_t i = 0; i < imported.count; i++) {
        if (pitch_numeric_value_position_numerator(imported.pitch_values[i]) == 70) {
            const PitchSpelling *spelling = note_event_optional_pitch_spelling(imported.events[i]);
            assert(pitch_spelling_letter(spelling) == PITCH_LETTER_B);
            assert(pitch_spelling_accidental(spelling) == -1);
            assert(pitch_spelling_octave(spelling) == 4);
            found_flat = true;
        }
    }
    assert(found_flat);

    musicxml_import_destroy(&imported);
    free(xml);
    rendered_plan_destroy(&rendered);
    plan_destroy(plan);
}

/* ------------------------------------------------------------- skipped, errors */

static PiecePlan *plan_with_one_melody(const PlanNote *notes, size_t count, PlanState state) {
    PiecePlan *plan = plan_create();
    NodeId form = add(plan, PLAN_NODE_NONE, PLAN_NODE_FORM, bars(0, 4), "form");
    LaneSpanId span = add_notes(plan, form, PLAN_LANE_MELODY, 0, bars(0, 4), notes, count);
    assert(plan_lane_span_set_state(plan, span, state) == PLAN_OK);
    return plan;
}

static bool file_exists(const char *path) {
    FILE *file = fopen(path, "rb");
    if (file) {
        fclose(file);
    }
    return file != NULL;
}

static void test_constrained_is_skipped(void) {
    PlanNote notes[] = {note(60, time_of(0, 1), time_of(1, 1))};
    PiecePlan *plan = plan_with_one_melody(notes, 1, PLAN_STATE_CONSTRAINED);
    RenderedPlan rendered;
    PlanExportError error;
    assert(plan_render(plan, &rendered) == PLAN_RENDER_OK);
    assert(rendered.count == 0);
    assert(rendered.skipped_count == 1);
    assert(rendered.skipped[0].lane == PLAN_LANE_MELODY);
    assert(rendered.skipped[0].reason == RENDER_SKIP_NOT_REALIZED);
    assert(rendered_plan_export_midi(&rendered, NULL, "render_test_none.mid", &error) == PLAN_EXPORT_ERR_NO_CONTENT);
    assert(error.message[0] != '\0');
    assert(plan_export_musicxml(plan, NULL, "render_test_none.xml", &error) == PLAN_EXPORT_ERR_NO_CONTENT);
    assert(!file_exists("render_test_none.mid") && !file_exists("render_test_none.xml"));
    rendered_plan_destroy(&rendered);
    plan_destroy(plan);
}

static void test_export_errors(void) {
    PlanExportError error;

    /* Two notes of one pitch overlapping in one voice. */
    PlanNote overlapping[] = {note(60, time_of(0, 1), time_of(1, 2)), note(60, time_of(1, 4), time_of(1, 2))};
    PiecePlan *plan = plan_with_one_melody(overlapping, 2, PLAN_STATE_REALIZED);
    assert(plan_export_midi(plan, NULL, "render_test_overlap.mid", &error) == PLAN_EXPORT_ERR_NOT_REPRESENTABLE);
    assert(error.message[0] != '\0');
    /* MusicXML writes the overlap as a second voice of the part; MIDI refuses it and writes nothing. */
    assert(plan_export_musicxml(plan, NULL, "render_test_overlap.xml", &error) == PLAN_EXPORT_OK);
    assert(!file_exists("render_test_overlap.mid") && file_exists("render_test_overlap.xml"));
    plan_destroy(plan);

    /* A file that cannot be written. */
    PlanNote one[] = {note(60, time_of(0, 1), time_of(1, 1))};
    plan = plan_with_one_melody(one, 1, PLAN_STATE_REALIZED);
    assert(plan_export_midi(plan, NULL, "no/such/directory/out.mid", &error) == PLAN_EXPORT_ERR_IO);
    assert(error.message[0] != '\0');
    assert(plan_export_midi(NULL, NULL, "x.mid", &error) == PLAN_EXPORT_ERR_NULL);
    assert(plan_export_midi(plan, NULL, NULL, &error) == PLAN_EXPORT_ERR_NULL);
    plan_destroy(plan);

    /* More voices than MIDI has tracks. */
    plan = plan_create();
    NodeId form = add(plan, PLAN_NODE_NONE, PLAN_NODE_FORM, bars(0, 1), "form");
    for (int voice = 1; voice <= 16; voice++) {
        LaneSpanId span = add_notes(plan, form, PLAN_LANE_INNER_VOICE, voice, bars(0, 1), one, 1);
        assert(plan_lane_span_set_state(plan, span, PLAN_STATE_REALIZED) == PLAN_OK);
    }
    assert(plan_export_midi(plan, NULL, "render_test_voices.mid", &error) == PLAN_EXPORT_ERR_TOO_MANY_VOICES);
    assert(!file_exists("render_test_voices.mid"));
    plan_destroy(plan);
}

static void test_division_is_exact(void) {
    /* A seventh of a whole note is not a whole tick at 480, so the division is 7. */
    PlanNote sevenths[] = {note(60, time_of(1, 7), time_of(1, 7))};
    PiecePlan *plan = plan_with_one_melody(sevenths, 1, PLAN_STATE_REALIZED);
    RenderedPlan rendered;
    unsigned char *data = NULL;
    size_t size = 0;
    assert(plan_render(plan, &rendered) == PLAN_RENDER_OK);
    assert(rendered_plan_midi_bytes(&rendered, NULL, &data, &size, NULL) == PLAN_EXPORT_OK);
    assert(size > 14 && data[12] == 0 && data[13] == 7);

    NoteEvent **events = NULL;
    PitchNumericValue **pitches = NULL;
    size_t count = 0;
    assert(midi_importer_import(data, size, &events, &pitches, &count) == MIDI_IMPORT_OK);
    assert_same_notes(&rendered, events, pitches, count, NULL, NULL);
    for (size_t i = 0; i < count; i++) {
        note_event_destroy(events[i]);
        pitch_numeric_value_destroy(pitches[i]);
    }
    free(events);
    free(pitches);
    free(data);
    rendered_plan_destroy(&rendered);
    plan_destroy(plan);
}

/* --------------------------------------------------------------- files, CLI */

static long file_size(const char *path) {
    FILE *file = fopen(path, "rb");
    long size = -1;
    if (file) {
        fseek(file, 0, SEEK_END);
        size = ftell(file);
        fclose(file);
    }
    return size;
}

static void test_files_and_cli(void) {
    PiecePlan *plan = build_sentence_plan();
    PlanExportError error;
    PlanJsonError json_error;

    assert(plan_export_midi(plan, NULL, "render_test_plan.mid", &error) == PLAN_EXPORT_OK);
    assert(plan_export_musicxml(plan, NULL, "render_test_plan.xml", &error) == PLAN_EXPORT_OK);
    assert(file_size("render_test_plan.mid") > 0 && file_size("render_test_plan.xml") > 0);
    assert(!file_exists("render_test_plan.mid.tmp") && !file_exists("render_test_plan.xml.tmp"));

    MusicXmlImport imported;
    memset(&imported, 0, sizeof(imported));
    assert(musicxml_import_file("render_test_plan.xml", &imported) == MUSICXML_IMPORT_OK);
    assert(imported.count == EXPECTED_NOTES);
    musicxml_import_destroy(&imported);

    /* The CLI exports the same plan from its JSON. */
    assert(plan_write_json_file(plan, "render_test_cli_plan.json", &json_error) == PLAN_OK);
    char command[1024];
    snprintf(command, sizeof(command),
             "\"%s\" export render_test_cli_plan.json --midi render_test_cli.mid --musicxml render_test_cli.xml "
             ">render_test_cli.out 2>render_test_cli.err",
             COMPOSER_CLI_PATH);
    assert(system(command) == 0);
    assert(file_size("render_test_cli.mid") == file_size("render_test_plan.mid"));
    assert(file_size("render_test_cli.xml") == file_size("render_test_plan.xml"));

    snprintf(command, sizeof(command), "\"%s\" export no_such_plan.json --midi x.mid >/dev/null 2>&1",
             COMPOSER_CLI_PATH);
    assert(system(command) != 0);
    snprintf(command, sizeof(command), "\"%s\" export render_test_cli_plan.json >/dev/null 2>&1", COMPOSER_CLI_PATH);
    assert(system(command) != 0);

    plan_destroy(plan);
}

/*
 * A form of a theme (bars 0 to 4) and, after it, a section marked as material (bars 4 to 8) that holds a motive and
 * a Realized melody. Only the theme sounds: the material is skipped for the reason RENDER_SKIP_MATERIAL, whatever
 * its lane and state, and the piece ends where the theme does, in the exports as well.
 */
static void test_material_is_left_out(void) {
    PiecePlan *plan = plan_create();
    NodeId form = add(plan, PLAN_NODE_NONE, PLAN_NODE_FORM, bars(0, 8), "form");
    NodeId theme = add(plan, form, PLAN_NODE_SECTION, bars(0, 4), "theme");
    NodeId theme_phrase = add(plan, theme, PLAN_NODE_PHRASE, bars(0, 4), "phrase");
    NodeId theme_idea = add(plan, theme_phrase, PLAN_NODE_IDEA, bars(0, 4), "idea");
    NodeId material = add(plan, form, PLAN_NODE_SECTION, bars(4, 4), "material");
    NodeId material_phrase = add(plan, material, PLAN_NODE_PHRASE, bars(4, 4), "phrase");
    NodeId material_idea = add(plan, material_phrase, PLAN_NODE_IDEA, bars(4, 4), "idea");
    NodeId motive = add(plan, material_idea, PLAN_NODE_MOTIVE, bars(4, 1), "m0");
    PlanConstraint mark = {PLAN_CONSTRAINT_MATERIAL, {0}};
    assert(plan_node_add_constraint(plan, material, &mark) == PLAN_OK);
    add_meter_and_key(plan, form, bars(0, 8));

    PlanNote theme_notes[] = {note(69, time_of(0, 1), time_of(4, 1))};
    PlanNote source_notes[] = {note(72, time_of(0, 1), time_of(1, 1))};
    PlanNote later_notes[] = {note(74, time_of(0, 1), time_of(1, 1))};
    LaneSpanId sounding = add_notes(plan, theme_idea, PLAN_LANE_MELODY, 0, bars(0, 4), theme_notes, 1);
    LaneSpanId source = add_notes(plan, motive, PLAN_LANE_MELODY, 0, bars(4, 1), source_notes, 1);
    LaneSpanId developed = add_notes(plan, material_idea, PLAN_LANE_BASS, 0, bars(5, 1), later_notes, 1);
    assert(plan_lane_span_set_state(plan, sounding, PLAN_STATE_REALIZED) == PLAN_OK);
    assert(plan_lane_span_set_state(plan, source, PLAN_STATE_GIVEN) == PLAN_OK);
    assert(plan_lane_span_set_state(plan, developed, PLAN_STATE_REALIZED) == PLAN_OK);

    RenderedPlan rendered;
    assert(plan_render(plan, &rendered) == PLAN_RENDER_OK);
    assert(rendered.count == 1 && rendered.part_count == 1);
    assert(rendered.skipped_count == 2);
    for (size_t i = 0; i < rendered.skipped_count; i++) {
        assert(rendered.skipped[i].reason == RENDER_SKIP_MATERIAL);
    }
    assert(rendered.skipped[0].span == source && rendered.skipped[1].span == developed);
    assert(rational_cmp(rendered.piece_end, time_of(4, 1)) == 0);

    /* The score has the four bars of the theme and no more. */
    char *xml = NULL;
    PlanExportError error;
    assert(rendered_plan_musicxml_text(&rendered, NULL, &xml, &error) == PLAN_EXPORT_OK);
    size_t measures = 0;
    for (const char *at = strstr(xml, "<measure "); at; at = strstr(at + 1, "<measure ")) {
        measures++;
    }
    assert(measures == 4);
    free(xml);
    rendered_plan_destroy(&rendered);

    /* Without the mark the same lanes sound. */
    assert(plan_node_remove_constraint(plan, material, 0) == PLAN_OK);
    assert(plan_render(plan, &rendered) == PLAN_RENDER_OK);
    assert(rendered.count == 3);
    assert(rational_cmp(rendered.piece_end, time_of(8, 1)) == 0);
    rendered_plan_destroy(&rendered);
    plan_destroy(plan);
}

/*
 * A layer sounds only the lanes its LAYERS mask names: the bass layer leaves out the melody and the inner voice, the
 * full layer sounds them all, and a section with no mask sounds everything.
 */
static void test_layers_are_masked(void) {
    PiecePlan *plan = plan_create();
    NodeId form = add(plan, PLAN_NODE_NONE, PLAN_NODE_FORM, bars(0, 12), "layered");
    NodeId sections[3];
    LaneSpanId spans[3][3];
    static const PlanLaneKind LANES[3] = {PLAN_LANE_MELODY, PLAN_LANE_BASS, PLAN_LANE_INNER_VOICE};
    static const int PITCHES[3] = {72, 48, 60};
    PlanNote notes[3][1];
    add_meter_and_key(plan, form, bars(0, 12));
    for (int s = 0; s < 3; s++) {
        sections[s] = add(plan, form, PLAN_NODE_SECTION, bars(4 * s, 4), NULL);
        for (int l = 0; l < 3; l++) {
            notes[l][0] = note(PITCHES[l], time_of(0, 1), time_of(4, 1));
            spans[s][l] = add_notes(plan, sections[s], LANES[l], LANES[l] == PLAN_LANE_INNER_VOICE ? 1 : 0,
                                    bars(4 * s, 4), notes[l], 1);
            assert(plan_lane_span_set_state(plan, spans[s][l], PLAN_STATE_REALIZED) == PLAN_OK);
        }
    }
    PlanConstraint layer = {PLAN_CONSTRAINT_LAYERS, {0}};
    layer.params.layers.lanes = PLAN_LAYER_BIT(PLAN_LANE_BASS);
    assert(plan_node_add_constraint(plan, sections[0], &layer) == PLAN_OK);
    layer.params.layers.lanes = PLAN_LAYERS_ALL;
    assert(plan_node_add_constraint(plan, sections[1], &layer) == PLAN_OK);

    RenderedPlan rendered;
    assert(plan_render(plan, &rendered) == PLAN_RENDER_OK);
    /* Bass alone in the first layer, three parts in the second and the unmasked third. */
    assert(rendered.count == 7);
    assert(rendered.part_count == 3);
    assert(rendered.skipped_count == 2);
    assert(rendered.skipped[0].span == spans[0][0] && rendered.skipped[0].reason == RENDER_SKIP_LAYER &&
           rendered.skipped[0].lane == PLAN_LANE_MELODY);
    assert(rendered.skipped[1].span == spans[0][2] && rendered.skipped[1].reason == RENDER_SKIP_LAYER &&
           rendered.skipped[1].lane == PLAN_LANE_INNER_VOICE);
    assert(rational_cmp(rendered.piece_end, time_of(12, 1)) == 0);
    rendered_plan_destroy(&rendered);
    plan_destroy(plan);
}

/* A note the plan spells is written as spelled; the same pitch unspelled takes the key's spelling (D minor: a flat). */
static void test_spelled_notes(void) {
    PiecePlan *plan = plan_create();
    NodeId form = add(plan, PLAN_NODE_NONE, PLAN_NODE_FORM, bars(0, 2), "form");
    add_meter_and_key(plan, form, bars(0, 2));
    PlanNote notes[2] = {note(61, time_of(0, 1), time_of(1, 1)), note(61, time_of(1, 1), time_of(1, 1))};
    notes[0].spelled = true;
    notes[0].letter = PITCH_LETTER_C;
    notes[0].accidental = 1;
    LaneSpanId melody = add_notes(plan, form, PLAN_LANE_MELODY, 0, bars(0, 2), notes, 2);
    assert(plan_lane_span_set_state(plan, melody, PLAN_STATE_GIVEN) == PLAN_OK);
    RenderedPlan rendered;
    assert(plan_render(plan, &rendered) == PLAN_RENDER_OK && rendered.count == 2);
    const PitchSpelling *written = note_event_optional_pitch_spelling(rendered.events[0]);
    const PitchSpelling *unwritten = note_event_optional_pitch_spelling(rendered.events[1]);
    assert(pitch_spelling_letter(written) == PITCH_LETTER_C && pitch_spelling_accidental(written) == 1);
    assert(pitch_spelling_octave(written) == 4);
    assert(pitch_spelling_letter(unwritten) == PITCH_LETTER_D && pitch_spelling_accidental(unwritten) == -1);
    rendered_plan_destroy(&rendered);
    plan_destroy(plan);
}

int main(void) {
    test_spelled_notes();
    test_render();
    test_material_is_left_out();
    test_layers_are_masked();
    test_midi_round_trip();
    test_musicxml_round_trip();
    test_constrained_is_skipped();
    test_export_errors();
    test_division_is_exact();
    test_files_and_cli();
    return 0;
}
