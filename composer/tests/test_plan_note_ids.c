#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>
#include <ursatz_composer/composer_api.h>
#include <ursatz_composer/lanes.h>
#include <ursatz_composer/piece_plan.h>
#include <ursatz_composer/plan_json.h>
#include <ursatz_composer/plan_state.h>

#include "support/test_composer.h"
#include "support/test_knowledge.h"
#include "support/test_support.h"
#include "ursatz_test.h"
#include <ursatz_composer/piece_plan_keys.h>

#ifndef COMPOSER_FIXTURE_DIR
#error "COMPOSER_FIXTURE_DIR is not defined"
#endif

/*
 * Plan note ids (R-601): ids from 1 in insertion order; a rewrite keeps a span's ids; a note copied within the plan gets a fresh id with
 * the old as its source (COPY); ids are never reused; a JSON round trip keeps ids, sources, operations and next_note_id; a format 3 text
 * reads with ids 1 to n in file order; a repeated id is refused; a return names the notes it was made from (RETURN); a theme given to a
 * piece gets fresh ids and no source; a theme grown from a motive links its melody notes to the motive's notes (MOTIVE).
 */

static Rational ratio(long n, long d) {
    return rational_ratio(n, d);
}

static PlanSpan span_of(long start, long length) {
    PlanSpan span = {ratio(start, 1), ratio(length, 1)};
    return span;
}

typedef struct {
    PiecePlan *plan;
    NodeId form;
    NodeId section;
    NodeId bass_node;
    LaneSpanId melody;
    LaneSpanId bass;
} Fixture;

/* A 4-bar form with a melody span of three notes and a bass span of two, over bars 0 to 2 of it. */
static Fixture make_fixture(void) {
    Fixture f;
    memset(&f, 0, sizeof(f));
    f.plan = plan_create();
    assert(f.plan != NULL);
    assert(plan_next_note_id(f.plan) == 1);
    assert(plan_add_node(f.plan, PLAN_NODE_NONE, PLAN_NODE_FORM, span_of(0, 4), "form", &f.form) == PLAN_OK);
    assert(plan_add_node(f.plan, f.form, PLAN_NODE_SECTION, span_of(0, 2), "A", &f.section) == PLAN_OK);
    assert(plan_add_node(f.plan, f.form, PLAN_NODE_SECTION, span_of(2, 2), "B", &f.bass_node) == PLAN_OK);
    PlanNote melody[3] = {plan_note(60, ratio(0, 1), ratio(1, 2)), plan_note(62, ratio(1, 2), ratio(1, 2)),
                          plan_note(64, ratio(1, 1), ratio(1, 1))};
    PlanLanePayload payload;
    memset(&payload, 0, sizeof(payload));
    payload.notes.notes = melody;
    payload.notes.note_count = 3;
    assert(plan_add_lane_span(f.plan, f.section, PLAN_LANE_MELODY, 0, span_of(0, 2), &payload, &f.melody) == PLAN_OK);
    PlanNote bass[2] = {plan_note(48, ratio(0, 1), ratio(1, 1)), plan_note(43, ratio(1, 1), ratio(1, 1))};
    payload.notes.notes = bass;
    payload.notes.note_count = 2;
    assert(plan_add_lane_span(f.plan, f.bass_node, PLAN_LANE_BASS, 0, span_of(2, 2), &payload, &f.bass) == PLAN_OK);
    return f;
}

static const PlanNote *note_at(const PiecePlan *plan, LaneSpanId span, size_t index) {
    const PlanNotes *notes = &plan_lane_span_payload(plan, span)->notes;
    assert(index < notes->note_count);
    return &notes->notes[index];
}

static void test_ids_in_insertion_order(void) {
    Fixture f = make_fixture();
    for (size_t i = 0; i < 3; i++) {
        assert(note_at(f.plan, f.melody, i)->id == i + 1);
    }
    assert(note_at(f.plan, f.bass, 0)->id == 4 && note_at(f.plan, f.bass, 1)->id == 5);
    assert(plan_next_note_id(f.plan) == 6);
    LaneSpanId in_span = PLAN_LANE_SPAN_NONE;
    const PlanNote *found = plan_note_by_id(f.plan, 5, &in_span);
    assert(found == note_at(f.plan, f.bass, 1) && in_span == f.bass && found->midi == 43);
    assert(plan_note_by_id(f.plan, 3, NULL) == note_at(f.plan, f.melody, 2));
    assert(plan_note_by_id(f.plan, 0, &in_span) == NULL && plan_note_by_id(f.plan, 6, NULL) == NULL && plan_note_by_id(NULL, 1, NULL) == NULL);
    for (size_t i = 0; i < 5; i++) {
        const PlanNote *n = i < 3 ? note_at(f.plan, f.melody, i) : note_at(f.plan, f.bass, i - 3);
        assert(n->source == PLAN_NOTE_NONE && n->operation == PLAN_NOTE_FROM_NONE);
    }
    plan_destroy(f.plan);
}

static void test_a_rewrite_keeps_ids(void) {
    Fixture f = make_fixture();
    const PlanNotes *held = &plan_lane_span_payload(f.plan, f.melody)->notes;
    PlanNote changed[4];
    memcpy(changed, held->notes, 3 * sizeof(PlanNote));
    changed[1].midi = 65; /* a note passed through with a new pitch: the same note */
    changed[3] = plan_note(67, ratio(3, 2), ratio(1, 2)); /* a new note: the next id */
    PlanLanePayload payload;
    memset(&payload, 0, sizeof(payload));
    payload.notes.notes = changed;
    payload.notes.note_count = 4;
    assert(plan_lane_span_write(f.plan, f.melody, &payload) == PLAN_OK);
    assert(note_at(f.plan, f.melody, 0)->id == 1 && note_at(f.plan, f.melody, 1)->id == 2 && note_at(f.plan, f.melody, 1)->midi == 65);
    assert(note_at(f.plan, f.melody, 2)->id == 3 && note_at(f.plan, f.melody, 3)->id == 6);
    assert(note_at(f.plan, f.melody, 1)->source == PLAN_NOTE_NONE && plan_next_note_id(f.plan) == 7);
    /* notes written from nothing, again and again, never reuse an id */
    PlanNote fresh[2] = {plan_note(60, ratio(0, 1), ratio(1, 1)), plan_note(62, ratio(1, 1), ratio(1, 1))};
    payload.notes.notes = fresh;
    payload.notes.note_count = 2;
    assert(plan_lane_span_write(f.plan, f.melody, &payload) == PLAN_OK);
    assert(note_at(f.plan, f.melody, 0)->id == 7 && note_at(f.plan, f.melody, 1)->id == 8 && plan_next_note_id(f.plan) == 9);
    assert(plan_note_by_id(f.plan, 1, NULL) == NULL); /* an id of an overwritten note finds nothing */
    plan_destroy(f.plan);
}

static void test_a_copy_within_the_plan(void) {
    Fixture f = make_fixture();
    /* the melody's notes written into another lane of the same bars: the plan sees ids it already holds */
    const PlanNotes *held = &plan_lane_span_payload(f.plan, f.melody)->notes;
    PlanLanePayload payload;
    memset(&payload, 0, sizeof(payload));
    payload.notes.notes = held->notes;
    payload.notes.note_count = held->note_count;
    LaneSpanId inner;
    assert(plan_add_lane_span(f.plan, f.section, PLAN_LANE_INNER_VOICE, 1, span_of(0, 2), &payload, &inner) == PLAN_OK);
    for (size_t i = 0; i < 3; i++) {
        const PlanNote *copy = note_at(f.plan, inner, i);
        assert(copy->id == 6 + i && copy->source == i + 1 && copy->operation == PLAN_NOTE_FROM_COPY);
        assert(copy->midi == note_at(f.plan, f.melody, i)->midi);
        assert(note_at(f.plan, f.melody, i)->id == i + 1); /* the original is untouched */
    }
    assert(plan_next_note_id(f.plan) == 9);
    /* a copy of a copy keeps the source it already has: the note it was first made from */
    PlanLanePayload again = payload;
    again.notes.notes = plan_lane_span_payload(f.plan, inner)->notes.notes;
    LaneSpanId second;
    assert(plan_add_lane_span(f.plan, f.bass_node, PLAN_LANE_INNER_VOICE, 1, span_of(2, 2), &again, &second) == PLAN_OK);
    assert(note_at(f.plan, second, 0)->id == 9 && note_at(f.plan, second, 0)->source == 1 && note_at(f.plan, second, 0)->operation == PLAN_NOTE_FROM_COPY);
    /* a note that already names its source keeps it when it is given a fresh id */
    PlanNote made = plan_note(70, ratio(0, 1), ratio(1, 1));
    made.id = 1; /* an id the plan holds */
    made.source = 4;
    made.operation = PLAN_NOTE_FROM_MOTIVE;
    PlanLanePayload one;
    memset(&one, 0, sizeof(one));
    one.notes.notes = &made;
    one.notes.note_count = 1;
    LaneSpanId third;
    assert(plan_add_lane_span(f.plan, f.bass_node, PLAN_LANE_INNER_VOICE, 2, span_of(2, 1), &one, &third) == PLAN_OK);
    assert(note_at(f.plan, third, 0)->id == 12 && note_at(f.plan, third, 0)->source == 4 && note_at(f.plan, third, 0)->operation == PLAN_NOTE_FROM_MOTIVE);
    /* two notes of one batch with one id: the second is a copy of the first */
    PlanNote twins[2] = {plan_note(60, ratio(0, 1), ratio(1, 2)), plan_note(62, ratio(1, 2), ratio(1, 2))};
    twins[0].id = twins[1].id = 40;
    PlanLanePayload pair;
    memset(&pair, 0, sizeof(pair));
    pair.notes.notes = twins;
    pair.notes.note_count = 2;
    LaneSpanId fourth;
    assert(plan_add_lane_span(f.plan, f.bass_node, PLAN_LANE_INNER_VOICE, 3, span_of(2, 1), &pair, &fourth) == PLAN_OK);
    assert(note_at(f.plan, fourth, 0)->id == 40 && note_at(f.plan, fourth, 1)->id == 41 && note_at(f.plan, fourth, 1)->source == 40);
    assert(plan_next_note_id(f.plan) == 42); /* an id the plan did not hold is kept, and the next id rises above it */
    plan_destroy(f.plan);
}

static void test_ids_are_not_reused_after_removal(void) {
    Fixture f = make_fixture();
    assert(plan_remove_subtree(f.plan, f.form) == PLAN_OK); /* every section under the form, and the notes they own */
    for (PlanNoteId id = 1; id <= 5; id++) {
        assert(plan_note_by_id(f.plan, id, NULL) == NULL);
    }
    assert(plan_next_note_id(f.plan) == 6);
    NodeId later;
    assert(plan_add_node(f.plan, f.form, PLAN_NODE_SECTION, span_of(2, 2), "C", &later) == PLAN_OK);
    PlanNote one = plan_note(50, ratio(0, 1), ratio(1, 1));
    PlanLanePayload payload;
    memset(&payload, 0, sizeof(payload));
    payload.notes.notes = &one;
    payload.notes.note_count = 1;
    LaneSpanId span;
    assert(plan_add_lane_span(f.plan, later, PLAN_LANE_BASS, 0, span_of(2, 2), &payload, &span) == PLAN_OK);
    assert(note_at(f.plan, span, 0)->id == 6 && plan_next_note_id(f.plan) == 7);
    /* and a file of the plan keeps the note's id and the next id: ids are not renumbered when it is written */
    PlanJsonError error;
    char *text = plan_to_json(f.plan, test_plan_producer(), &error);
    assert(text != NULL && strstr(text, "\"next_note_id\": 7") != NULL);
    PiecePlan *read = NULL;
    assert(plan_from_json(test_knowledge(), text, composer_plan_read_hooks(), &read, &error) == PLAN_OK && plan_next_note_id(read) == 7 && plan_note_by_id(read, 6, NULL) != NULL);
    plan_destroy(read);
    free(text);
    plan_destroy(f.plan);
}

/* Every note of the plan's note lanes, in span order, into out (at most max); the count. */
static size_t all_notes(const PiecePlan *plan, PlanNote *out, size_t max) {
    size_t count = 0;
    for (LaneSpanId id = 1; id <= plan_lane_span_count(plan); id++) {
        PlanLaneKind lane = plan_lane_span_lane(plan, id);
        if (!plan_has_lane_span(plan, id) || (lane != PLAN_LANE_BASS && lane != PLAN_LANE_MELODY && lane != PLAN_LANE_INNER_VOICE)) {
            continue;
        }
        const PlanNotes *notes = &plan_lane_span_payload(plan, id)->notes;
        for (size_t i = 0; i < notes->note_count; i++) {
            assert(count < max);
            out[count++] = notes->notes[i];
        }
    }
    return count;
}

static void test_json_round_trip(void) {
    Fixture f = make_fixture();
    const PlanNotes *held = &plan_lane_span_payload(f.plan, f.melody)->notes;
    PlanLanePayload payload;
    memset(&payload, 0, sizeof(payload));
    payload.notes.notes = held->notes;
    payload.notes.note_count = held->note_count;
    LaneSpanId inner;
    assert(plan_add_lane_span(f.plan, f.section, PLAN_LANE_INNER_VOICE, 1, span_of(0, 2), &payload, &inner) == PLAN_OK);
    PlanJsonError error;
    char *text = plan_to_json(f.plan, test_plan_producer(), &error);
    assert(text != NULL && strstr(text, "\"format_version\": 5") != NULL && strstr(text, "\"next_note_id\": 9") != NULL);
    assert(strstr(text, "\"source\": 1") != NULL && strstr(text, "\"operation\": \"copy\"") != NULL);
    PiecePlan *read = NULL;
    assert(plan_from_json(test_knowledge(), text, composer_plan_read_hooks(), &read, &error) == PLAN_OK);
    assert(plan_next_note_id(read) == 9);
    PlanNote before[16], after[16];
    size_t n = all_notes(f.plan, before, 16);
    assert(n == 8 && all_notes(read, after, 16) == n);
    for (size_t i = 0; i < n; i++) {
        assert(before[i].id == after[i].id && before[i].source == after[i].source && before[i].operation == after[i].operation);
        assert(before[i].midi == after[i].midi);
    }
    char *again = plan_to_json(read, test_plan_producer(), &error);
    assert(again != NULL && strcmp(again, text) == 0);
    free(again);
    plan_destroy(read);
    free(text);
    plan_destroy(f.plan);
}

static void test_format_three_reads_ids_in_file_order(void) {
    Fixture f = make_fixture();
    PlanJsonError error;
    char *four = plan_to_json(f.plan, test_plan_producer(), &error);
    assert(four != NULL);
    char *three = test_plan_text_as_format_three(four);
    assert(three != NULL && strstr(three, "\"format_version\": 3") != NULL && strstr(three, "next_note_id") == NULL);
    PiecePlan *read = NULL;
    assert(plan_from_json(test_knowledge(), three, composer_plan_read_hooks(), &read, &error) == PLAN_OK);
    PlanNote notes[16];
    size_t n = all_notes(read, notes, 16);
    assert(n == 5 && plan_next_note_id(read) == 6);
    unsigned seen = 0;
    for (size_t i = 0; i < n; i++) {
        assert(notes[i].id >= 1 && notes[i].id <= 5 && notes[i].source == PLAN_NOTE_NONE && notes[i].operation == PLAN_NOTE_FROM_NONE);
        seen |= 1u << notes[i].id;
    }
    assert(seen == 0x3E); /* ids 1 to 5, each once */
    /* the melody comes before the bass in the file, so its notes are 1 to 3 */
    LaneSpanId melody = PLAN_LANE_SPAN_NONE, bass = PLAN_LANE_SPAN_NONE;
    for (LaneSpanId id = 1; id <= plan_lane_span_count(read); id++) {
        melody = plan_lane_span_lane(read, id) == PLAN_LANE_MELODY ? id : melody;
        bass = plan_lane_span_lane(read, id) == PLAN_LANE_BASS ? id : bass;
    }
    assert(note_at(read, melody, 0)->id == 1 && note_at(read, melody, 2)->id == 3 && note_at(read, bass, 0)->id == 4);
    plan_destroy(read);
    free(three);
    free(four);
    plan_destroy(f.plan);
}

/* The format 4 text with one member of the document or of a note changed. */
static char *with_changes(const char *text, size_t note, const char *member, double value, bool document) {
    cJSON *root = cJSON_Parse(text);
    assert(root != NULL);
    if (document) {
        cJSON_ReplaceItemInObjectCaseSensitive(root, member, cJSON_CreateNumber(value));
    } else {
        cJSON *notes = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(root, "melody"), 0),
                                                                                           "composer"),
                                                        "payload");
        notes = cJSON_GetObjectItemCaseSensitive(notes, "notes");
        cJSON_ReplaceItemInObjectCaseSensitive(cJSON_GetArrayItem(notes, (int)note), member, cJSON_CreateNumber(value));
    }
    char *out = cJSON_Print(root);
    cJSON_Delete(root);
    assert(out != NULL);
    return out;
}

static void test_refusals(void) {
    Fixture f = make_fixture();
    PlanJsonError error;
    char *text = plan_to_json(f.plan, test_plan_producer(), &error);
    assert(text != NULL);
    PiecePlan *read = NULL;
    /* a repeated id */
    char *repeated = with_changes(text, 1, "id", 1, false);
    assert(plan_from_json(test_knowledge(), repeated, composer_plan_read_hooks(), &read, &error) == PLAN_ERR_FORMAT && read == NULL);
    assert(strstr(error.path, "notes[1].id") != NULL || strstr(error.path, "notes/1/id") != NULL || strstr(error.path, "[1].id") != NULL);
    free(repeated);
    /* an id below 1, and one that is not an integer */
    char *zero = with_changes(text, 0, "id", 0, false);
    assert(plan_from_json(test_knowledge(), zero, composer_plan_read_hooks(), &read, &error) == PLAN_ERR_FORMAT && strstr(error.path, "id") != NULL);
    free(zero);
    char *half = with_changes(text, 0, "id", 1.5, false);
    assert(plan_from_json(test_knowledge(), half, composer_plan_read_hooks(), &read, &error) == PLAN_ERR_FORMAT);
    free(half);
    /* a next_note_id that is not above every id */
    char *low = with_changes(text, 0, "next_note_id", 5, true);
    assert(plan_from_json(test_knowledge(), low, composer_plan_read_hooks(), &read, &error) == PLAN_ERR_FORMAT && strcmp(error.path, "next_note_id") == 0);
    free(low);
    char *none = with_changes(text, 0, "next_note_id", 0, true);
    assert(plan_from_json(test_knowledge(), none, composer_plan_read_hooks(), &read, &error) == PLAN_ERR_FORMAT && strcmp(error.path, "next_note_id") == 0);
    free(none);
    /* a version 4 file with no next_note_id, and a note with no id */
    cJSON *root = cJSON_Parse(text);
    cJSON_DeleteItemFromObjectCaseSensitive(root, "next_note_id");
    char *missing = cJSON_Print(root);
    assert(plan_from_json(test_knowledge(), missing, composer_plan_read_hooks(), &read, &error) == PLAN_ERR_FORMAT && strcmp(error.path, "next_note_id") == 0);
    free(missing);
    cJSON_Delete(root);
    /* a next_note_id above every id is kept */
    char *high = with_changes(text, 0, "next_note_id", 100, true);
    assert(plan_from_json(test_knowledge(), high, composer_plan_read_hooks(), &read, &error) == PLAN_OK && plan_next_note_id(read) == 100);
    plan_destroy(read);
    free(high);
    free(text);
    plan_destroy(f.plan);
}

/* The plan of a request through composer_api_piece or composer_api_theme. */
static PiecePlan *plan_of_response(const ComposerResponse *response) {
    PiecePlan *plan = NULL;
    PlanJsonError error;
    assert(response->plan_json != NULL && plan_from_json(test_knowledge(), response->plan_json, composer_plan_read_hooks(), &plan, &error) == PLAN_OK);
    return plan;
}

static void test_a_return_names_its_source(void) {
    const char *request = "{\"request\": {\"schema\": \"ursatz-piece-request\", \"schema_version\": 2, \"form\": \"small-rondo-ababa\", "
                          "\"key\": \"C\", \"mode\": \"major\", \"meter\": \"4/4\", \"bars\": 68, \"motive\": null}, \"seed\": 1}";
    ComposerResponse response;
    ComposerStatus status = composer_api_piece(test_context(), request, NULL, &response);
    if (status != COMPOSER_OK) {
        fprintf(stderr, "%s (%s)\n", response.error.message, response.error.path);
    }
    assert(status == COMPOSER_OK);
    PiecePlan *plan = plan_of_response(&response);
    composer_response_destroy(&response);
    PlanNote notes[4096];
    size_t n = all_notes(plan, notes, 4096);
    size_t returned = 0;
    for (size_t i = 0; i < n; i++) {
        if (notes[i].operation != PLAN_NOTE_FROM_RETURN) {
            continue;
        }
        returned++;
        LaneSpanId at, from;
        assert(notes[i].source != PLAN_NOTE_NONE && notes[i].source < notes[i].id);
        assert(plan_note_by_id(plan, notes[i].id, &at) != NULL);
        const PlanNote *source = plan_note_by_id(plan, notes[i].source, &from);
        assert(source != NULL && from != at && source->operation != PLAN_NOTE_FROM_RETURN);
    }
    assert(returned > 0);
    plan_destroy(plan);
}

static void test_a_given_theme_has_fresh_ids_and_no_source(void) {
    char *path_text = test_must_read_file(COMPOSER_FIXTURE_DIR "/theme/one-motive.json", NULL);
    cJSON *theme_request = cJSON_Parse(path_text);
    free(path_text);
    assert(theme_request && cJSON_AddNumberToObject(theme_request, "seed", 1));
    char *printed = cJSON_PrintUnformatted(theme_request);
    cJSON_Delete(theme_request);
    ComposerResponse theme;
    assert(composer_api_theme(test_context(), printed, NULL, &theme) == COMPOSER_OK);
    free(printed);
    PiecePlan *theme_plan = plan_of_response(&theme);
    PlanNote theme_notes[1024];
    size_t theme_count = all_notes(theme_plan, theme_notes, 1024);

    char *piece_text = test_must_read_file(COMPOSER_FIXTURE_DIR "/piece/theme-fill.json", NULL);
    cJSON *piece = cJSON_Parse(piece_text);
    free(piece_text);
    assert(piece != NULL);
    cJSON *section;
    bool replaced = false;
    cJSON_ArrayForEach(section, cJSON_GetObjectItemCaseSensitive(piece, "sections")) {
        cJSON *fill = cJSON_GetObjectItemCaseSensitive(section, "fill");
        const cJSON *kind = cJSON_GetObjectItemCaseSensitive(fill, "kind");
        if (cJSON_IsString(kind) && strcmp(kind->valuestring, "theme") == 0) {
            cJSON *inline_plan = cJSON_Parse(theme.plan_json);
            assert(inline_plan && cJSON_ReplaceItemInObject(fill, "plan", inline_plan));
            cJSON_DeleteItemFromObjectCaseSensitive(fill, "source");
            replaced = true;
        }
    }
    assert(replaced);
    cJSON *wrapped = cJSON_CreateObject();
    cJSON_AddItemToObject(wrapped, "request", piece);
    cJSON_AddNumberToObject(wrapped, "seed", 1);
    char *piece_request = cJSON_PrintUnformatted(wrapped);
    cJSON_Delete(wrapped);
    ComposerResponse built;
    ComposerStatus status = composer_api_piece(test_context(), piece_request, NULL, &built);
    free(piece_request);
    if (status != COMPOSER_OK) {
        fprintf(stderr, "%s (%s)\n", built.error.message, built.error.path);
    }
    assert(status == COMPOSER_OK);
    PiecePlan *plan = plan_of_response(&built);
    composer_response_destroy(&built);
    /* every Given note (the theme, copied in) is made from nothing here: a fresh id and no source */
    size_t given = 0;
    for (LaneSpanId id = 1; id <= plan_lane_span_count(plan); id++) {
        PlanLaneKind lane = plan_lane_span_lane(plan, id);
        if (!plan_has_lane_span(plan, id) || plan_lane_span_state(plan, id) != PLAN_STATE_GIVEN ||
            (lane != PLAN_LANE_BASS && lane != PLAN_LANE_MELODY && lane != PLAN_LANE_INNER_VOICE)) {
            continue;
        }
        const PlanNotes *notes = &plan_lane_span_payload(plan, id)->notes;
        for (size_t i = 0; i < notes->note_count; i++) {
            assert(notes->notes[i].id != PLAN_NOTE_NONE && notes->notes[i].source == PLAN_NOTE_NONE &&
                   notes->notes[i].operation == PLAN_NOTE_FROM_NONE);
            given++;
        }
    }
    assert(given > 0 && theme_count > 0);
    composer_response_destroy(&theme);
    plan_destroy(theme_plan);
    plan_destroy(plan);
}

static void test_a_theme_links_melody_notes_to_its_motive(void) {
    char *text = test_must_read_file(COMPOSER_FIXTURE_DIR "/theme/one-motive.json", NULL);
    cJSON *request = cJSON_Parse(text);
    free(text);
    assert(request && cJSON_AddNumberToObject(request, "seed", 1));
    char *printed = cJSON_PrintUnformatted(request);
    cJSON_Delete(request);
    ComposerResponse response;
    assert(composer_api_theme(test_context(), printed, NULL, &response) == COMPOSER_OK);
    free(printed);
    PiecePlan *plan = plan_of_response(&response);
    composer_response_destroy(&response);
    size_t linked = 0;
    for (LaneSpanId id = 1; id <= plan_lane_span_count(plan); id++) {
        if (!plan_has_lane_span(plan, id) || plan_lane_span_lane(plan, id) != PLAN_LANE_MELODY) {
            continue;
        }
        const PlanNotes *notes = &plan_lane_span_payload(plan, id)->notes;
        for (size_t i = 0; i < notes->note_count; i++) {
            const PlanNote *note = &notes->notes[i];
            if (note->operation != PLAN_NOTE_FROM_MOTIVE) {
                continue;
            }
            LaneSpanId from;
            const PlanNote *source = plan_note_by_id(plan, note->source, &from);
            assert(source != NULL && from != id && plan_lane_span_lane(plan, from) == PLAN_LANE_MELODY);
            /* the motive's note lives in a motive node's melody span, or in the idea's before it that an exact restatement repeats */
            PlanNodeKind kind = plan_node_kind(plan, plan_lane_span_owner(plan, from));
            assert(kind == PLAN_NODE_MOTIVE || kind == PLAN_NODE_IDEA);
            linked++;
        }
    }
    assert(linked > 0);
    plan_destroy(plan);
}

int main(void) {
    test_ids_in_insertion_order();
    test_a_rewrite_keeps_ids();
    test_a_copy_within_the_plan();
    test_ids_are_not_reused_after_removal();
    test_json_round_trip();
    test_format_three_reads_ids_in_file_order();
    test_refusals();
    test_a_return_names_its_source();
    test_a_given_theme_has_fresh_ids_and_no_source();
    test_a_theme_links_melody_notes_to_its_motive();
    printf("test_plan_note_ids: all tests passed\n");
    return 0;
}
