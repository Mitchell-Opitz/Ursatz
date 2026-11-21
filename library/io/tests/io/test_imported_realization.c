#include "io/imported_realization.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "model/events/note_event.h"
#include "model/identity/entity_id.h"
#include "model/pitch/pitch_identity.h"
#include "model/time/meter_map.h"
#include "realization/movement.h"
#include "realization/realization_json.h"
#include "test_knowledge.h"

/*
 * The realization of an imported score (R-628): Bach 114 (MIDI) gives a part for each part of the import with a note for each event,
 * the ids the import gave and the import's meter map; two_staves.musicxml gives both staves and spelling.musicxml the notated
 * spellings; a score whose event ids are not realization ids gets n<i+1>; the General MIDI program picks the instrument; note_ids names
 * the note made from each event; the work round-trips through the codec.
 */

#define BACH URSATZ_FIXTURES_DIR "/framework-v1-corpus/Bach 114.mid"
#define TWO_STAVES URSATZ_FIXTURES_DIR "/musicxml/two_staves.musicxml"
#define SPELLING URSATZ_FIXTURES_DIR "/musicxml/spelling.musicxml"

static ImportedScore import(const char *path) {
    ImportedScore score;
    ImportedScoreError error = {{0}};
    assert(imported_score_import_file(path, IMPORTED_FORMAT_AUTO, &score, &error) == IMPORTED_SCORE_OK);
    return score;
}

static const Movement *only_movement(const Work *work) {
    assert(work_movement_count(work) == 1 && strcmp(work_id(work), "w1") == 0 && work_title(work) == NULL && work_composer(work) == NULL);
    const Movement *movement = work_movement_at(work, 0);
    assert(strcmp(movement_id(movement), "m1") == 0);
    return movement;
}

static const PerformerNote *note_named(const PerformerPart *part, const char *id) {
    for (size_t i = 0; i < performer_part_note_count(part); i++) {
        if (strcmp(performer_part_note_at(part, i)->id, id) == 0) {
            return performer_part_note_at(part, i);
        }
    }
    return NULL;
}

/* Every event is the note note_ids names, in the part it belongs to, with its onset, duration and MIDI number. */
static void check_notes(const ImportedScore *score, const Movement *movement, char (*ids)[REALIZATION_ID_MAX]) {
    size_t total = 0;
    for (size_t p = 0; p < movement_part_count(movement); p++) {
        total += performer_part_note_count(movement_part_at(movement, p));
    }
    assert(total == score->count);
    for (size_t i = 0; i < score->count; i++) {
        size_t part = score->source.part_count > 0 ? score->source.part_index[i] : 0;
        const PerformerNote *note = note_named(movement_part_at(movement, part), ids[i]);
        assert(note != NULL);
        assert(musical_time_cmp(note->onset, *temporal_anchor_position(note_event_temporal_anchor(score->events[i]))) == 0);
        assert(duration_same(note->duration, *note_event_duration(score->events[i])));
        assert(note->midi == (int)pitch_numeric_value_position_numerator(score->pitch_values[i]));
        assert(!note_derivation_present(&note->derivation) && !note->edited && !note->detached && !note->review);
    }
}

static void check_round_trip(const Work *work) {
    RealizationJsonError error;
    char *text = realization_work_to_json(work, &error);
    assert(text != NULL);
    Work *read = NULL;
    assert(realization_work_from_json(test_knowledge(), text, &read, &error) == REALIZATION_OK);
    char *again = realization_work_to_json(read, &error);
    assert(again != NULL && strcmp(text, again) == 0);
    free(again);
    work_destroy(read);
    free(text);
}

static void test_bach(void) {
    ImportedScore score = import(BACH);
    Work *work = NULL;
    char(*ids)[REALIZATION_ID_MAX] = NULL;
    assert(imported_score_realization(&score, NULL, &work, &ids) == IMPORTED_SCORE_OK && work != NULL && ids != NULL);
    const Movement *movement = only_movement(work);
    assert(movement_title(movement) == NULL && movement_line_count(movement) == 0 && movement_assignment_count(movement) == 0);
    Region home;
    assert(!movement_home(movement, &home));
    assert(movement_part_count(movement) == score.source.part_count && movement_part_count(movement) > 0);
    for (size_t i = 0; i < score.source.part_count; i++) {
        const PerformerPart *part = movement_part_at(movement, i);
        assert(performer_part_players(part) == 1 && performer_part_staff_count(part) == score.source.parts[i].staff_count);
        assert(strcmp(performer_part_name(part), score.source.parts[i].name[0] ? score.source.parts[i].name : performer_part_name(part)) == 0);
    }
    check_notes(&score, movement, ids);
    /* The import's ids are kept: a MIDI note is midi-note-<n>. */
    assert(strncmp(ids[0], "midi-note-", 10) == 0);
    for (size_t i = 0; i < score.count; i++) {
        assert(strcmp(ids[i], entity_id_value(note_event_id(score.events[i]))) == 0);
    }
    /* The meter map and the tempo map are the import's. */
    const MeterMap *meter = movement_meter_map(movement);
    assert(meter != NULL && meter_map_region_count(meter) == meter_map_region_count(score.meter.meter_map));
    for (size_t r = 0; r < meter_map_region_count(meter); r++) {
        const MeterMapRegion *mine = meter_map_region(meter, r);
        const MeterMapRegion *theirs = meter_map_region(score.meter.meter_map, r);
        assert(musical_time_cmp(meter_map_region_start(mine), meter_map_region_start(theirs)) == 0);
        assert(time_signature_equals(meter_map_region_time_signature(mine), meter_map_region_time_signature(theirs)));
        assert(meter_map_region_is_defaulted(mine) == meter_map_region_is_defaulted(theirs));
    }
    assert(movement_tempo_map(movement) != NULL && tempo_map_region_count(movement_tempo_map(movement)) == tempo_map_region_count(score.tempo_map));
    assert(movement_key_signature_count(movement) == score.key_signature_events.count);
    check_round_trip(work);
    work_destroy(work);
    free(ids);
    /* A title goes to the movement. */
    assert(imported_score_realization(&score, "Invention 14", &work, NULL) == IMPORTED_SCORE_OK);
    assert(strcmp(movement_title(only_movement(work)), "Invention 14") == 0);
    work_destroy(work);
    imported_score_destroy(&score);
}

static void test_staves_and_spellings(void) {
    ImportedScore score = import(TWO_STAVES);
    Work *work = NULL;
    char(*ids)[REALIZATION_ID_MAX] = NULL;
    assert(imported_score_realization(&score, NULL, &work, &ids) == IMPORTED_SCORE_OK);
    const Movement *movement = only_movement(work);
    assert(movement_part_count(movement) == score.source.part_count);
    bool staff_one = false;
    bool staff_two = false;
    for (size_t p = 0; p < movement_part_count(movement); p++) {
        for (size_t n = 0; n < performer_part_note_count(movement_part_at(movement, p)); n++) {
            const PerformerNote *note = performer_part_note_at(movement_part_at(movement, p), n);
            staff_one = staff_one || note->staff == 1;
            staff_two = staff_two || note->staff == 2;
            assert(note->staff >= 1 && note->staff <= performer_part_staff_count(movement_part_at(movement, p)));
        }
    }
    assert(staff_one && staff_two && performer_part_staff_count(movement_part_at(movement, 0)) == 2);
    check_notes(&score, movement, ids);
    check_round_trip(work);
    work_destroy(work);
    free(ids);
    imported_score_destroy(&score);

    ImportedScore spelt = import(SPELLING);
    assert(spelt.source.spelling_notated);
    assert(imported_score_realization(&spelt, NULL, &work, &ids) == IMPORTED_SCORE_OK);
    movement = only_movement(work);
    size_t spelled = 0;
    for (size_t i = 0; i < spelt.count; i++) {
        size_t part = spelt.source.part_count > 0 ? spelt.source.part_index[i] : 0;
        const PerformerNote *note = note_named(movement_part_at(movement, part), ids[i]);
        const PitchSpelling *written = note_event_optional_pitch_spelling(spelt.events[i]);
        assert(note != NULL && note->spelled == (written != NULL));
        if (written) {
            assert(note->letter == pitch_spelling_letter(written) && note->accidental == pitch_spelling_accidental(written));
            spelled++;
        }
    }
    assert(spelled > 0);
    /* A MIDI import has no notated spelling: none is kept. */
    ImportedScore midi = import(BACH);
    Work *plain = NULL;
    assert(imported_score_realization(&midi, NULL, &plain, NULL) == IMPORTED_SCORE_OK);
    const PerformerNote *first = performer_part_note_at(movement_part_at(only_movement(plain), 0), 0);
    assert(first != NULL && !first->spelled);
    work_destroy(plain);
    imported_score_destroy(&midi);
    work_destroy(work);
    free(ids);
    imported_score_destroy(&spelt);
}

static NoteEvent *make_event(const char *id, long long onset, const char *voice) {
    EntityID *entity = NULL;
    PitchIdentity *pitch = NULL;
    ProvenanceReference *provenance = NULL;
    VoiceReference *reference = NULL;
    MusicalTime start;
    Duration length;
    NoteEvent *event = NULL;
    assert(entity_id_create(id, &entity) == ENTITY_ID_OK && pitch_identity_create("pitch-x", 7, &pitch) == PITCH_IDENTITY_OK);
    assert(provenance_reference_create_absent(&provenance) == PROVENANCE_REFERENCE_OK);
    if (voice) {
        EntityID *target = NULL;
        assert(entity_id_create(voice, &target) == ENTITY_ID_OK && voice_reference_create(target, &reference) == VOICE_REFERENCE_OK);
    }
    assert(musical_time_make(onset, 4, &start) == MUSICAL_TIME_OK && duration_make(1, 4, &length) == DURATION_OK);
    assert(note_event_create(entity, start, length, pitch, NULL, reference, provenance, &event) == NOTE_EVENT_OK);
    return event;
}

/* Ids that are not realization ids, a repeated id, and the voices of a part: a score the test builds. */
static void test_ids_and_voices(void) {
    enum { COUNT = 5 };
    const char *event_ids[COUNT] = {"1bad", "good", "good", "n3", "also.good"};
    const char *voices[COUNT] = {NULL, "v-b", NULL, "v-a", "v-b"};
    NoteEvent *events[COUNT];
    const NoteEvent *borrowed[COUNT];
    int midi[COUNT] = {60, 62, 64, 65, 67};
    for (size_t i = 0; i < COUNT; i++) {
        events[i] = make_event(event_ids[i], (long long)i, voices[i]);
        borrowed[i] = events[i];
    }
    ImportedScore score;
    ImportedScoreView view = {.events = borrowed, .count = COUNT, .midi = midi};
    assert(imported_score_view(&view, &score) == IMPORTED_SCORE_OK);
    Work *work = NULL;
    char(*ids)[REALIZATION_ID_MAX] = NULL;
    assert(imported_score_realization(&score, NULL, &work, &ids) == IMPORTED_SCORE_OK);
    const Movement *movement = only_movement(work);
    /* No parts: one part p1 named Piano. */
    assert(movement_part_count(movement) == 1);
    const PerformerPart *part = movement_part_at(movement, 0);
    assert(strcmp(performer_part_id(part), "p1") == 0 && strcmp(performer_part_name(part), "Piano") == 0);
    assert(strcmp(performer_part_instrument(part), "omt.instrument.piano") == 0 && performer_part_midi_program(part) == 0);
    assert(performer_part_number(part) == 1 && performer_part_players(part) == 1);
    /* "1bad" is no id: n1; "good" is kept, its repeat is n3 (the event's own index, plus one); the event named "n3" finds that taken: n4. */
    assert(strcmp(ids[0], "n1") == 0 && strcmp(ids[1], "good") == 0 && strcmp(ids[2], "n3") == 0);
    assert(strcmp(ids[3], "n4") == 0 && strcmp(ids[4], "also.good") == 0);
    check_notes(&score, movement, ids);
    /* Voices: 1 plus the place among the part's distinct voice references, "none" being one of them. */
    int expected[COUNT] = {1, 2, 1, 3, 2};
    for (size_t i = 0; i < COUNT; i++) {
        assert(note_named(part, ids[i])->voice == expected[i] && note_named(part, ids[i])->staff == 1);
    }
    check_round_trip(work);
    work_destroy(work);
    free(ids);
    imported_score_destroy(&score);
    for (size_t i = 0; i < COUNT; i++) {
        note_event_destroy(events[i]);
    }
}

/* The General MIDI program picks the instrument and the number among the parts of that instrument; ids and names are cleaned. */
static void test_parts_of_a_hand_built_import(void) {
    enum { COUNT = 4 };
    NoteEvent *events[COUNT];
    const NoteEvent *borrowed[COUNT];
    int midi[COUNT] = {60, 62, 64, 65};
    for (size_t i = 0; i < COUNT; i++) {
        events[i] = make_event("e", (long long)i, NULL);
        borrowed[i] = events[i];
    }
    /* Every event is called "e": the repeats are n2, n3 ... in the part. */
    ImportedScore score;
    ImportedScoreView view = {.events = borrowed, .count = COUNT, .midi = midi};
    assert(imported_score_view(&view, &score) == IMPORTED_SCORE_OK);
    char id_a[] = "A1";
    char id_b[] = "A1";   /* the same id as the first: the second part is p2 */
    char id_c[] = "bad id";
    char name_a[] = "Piano";
    char name_b[] = "";
    char name_c[] = "A part with a name that is far too long to fit in sixty-three characters, so it is cut";
    ImportedPart parts[3] = {{id_a, name_a, 2, 0}, {id_b, name_b, 1, 40}, {id_c, name_c, 9, 5}};
    size_t part_index[COUNT] = {0, 1, 2, 1};
    int staff[COUNT] = {2, 1, 3, 5};
    score.source.parts = parts;
    score.source.part_count = 3;
    score.source.part_index = part_index;
    score.source.staff = staff;
    Work *work = NULL;
    char(*ids)[REALIZATION_ID_MAX] = NULL;
    assert(imported_score_realization(&score, NULL, &work, &ids) == IMPORTED_SCORE_OK);
    const Movement *movement = only_movement(work);
    assert(movement_part_count(movement) == 3);
    const PerformerPart *first = movement_part_at(movement, 0);
    const PerformerPart *second = movement_part_at(movement, 1);
    const PerformerPart *third = movement_part_at(movement, 2);
    assert(strcmp(performer_part_id(first), "A1") == 0 && strcmp(performer_part_id(second), "p2") == 0 && strcmp(performer_part_id(third), "p3") == 0);
    assert(strcmp(performer_part_instrument(first), "omt.instrument.piano") == 0 && performer_part_midi_program(first) == 0);
    assert(strcmp(performer_part_instrument(second), "general-midi.instrument.program-40") == 0 && performer_part_midi_program(second) == 40);
    /* The program 5 is a piano too, the second one: number 2. */
    assert(strcmp(performer_part_instrument(third), "omt.instrument.piano") == 0 && performer_part_number(first) == 1 && performer_part_number(third) == 2);
    assert(strcmp(performer_part_name(second), "Part 2") == 0 && strlen(performer_part_name(third)) == 63);
    assert(performer_part_staff_count(first) == 2 && performer_part_staff_count(second) == 1 && performer_part_staff_count(third) == 4);
    /* The staff is the score's, kept inside the part's staves. */
    assert(note_named(first, ids[0])->staff == 2 && note_named(second, ids[1])->staff == 1);
    assert(note_named(third, ids[2])->staff == 3 && note_named(second, ids[3])->staff == 1);
    /* Equal ids in a part: the repeat is n<i+1>. */
    assert(strcmp(ids[0], "e") == 0 && strcmp(ids[1], "e") == 0 && strcmp(ids[3], "n4") == 0);
    check_notes(&score, movement, ids);
    work_destroy(work);
    free(ids);
    score.source.parts = NULL;
    score.source.part_count = 0;
    score.source.part_index = NULL;
    score.source.staff = NULL;
    imported_score_destroy(&score);
    for (size_t i = 0; i < COUNT; i++) {
        note_event_destroy(events[i]);
    }
}

static void test_arguments(void) {
    Work *work = (Work *)1;
    assert(imported_score_realization(NULL, NULL, &work, NULL) == IMPORTED_SCORE_ERR_ARGUMENT && work == NULL);
    ImportedScore score = import(BACH);
    assert(imported_score_realization(&score, NULL, NULL, NULL) == IMPORTED_SCORE_ERR_ARGUMENT);
    imported_score_destroy(&score);
}

int main(void) {
    test_bach();
    test_staves_and_spellings();
    test_ids_and_voices();
    test_parts_of_a_hand_built_import();
    test_arguments();
    printf("test_imported_realization: ok\n");
    return 0;
}
