#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>
#include <io/midi_importer.h>
#include <model/time/duration.h>
#include <model/time/musical_time.h>
#include <model/time/temporal_anchor.h>
#include <plan/plan.h>
#include <plan/plan_json.h>
#include <reading/reading.h>
#include <reading/reading_json.h>
#include <realization/movement.h>
#include <realization/performer_part.h>
#include <realization/realization_json.h>
#include <realization/work.h>
#include <ursatz_composer/composer_api.h>

#include "check/hearing_result.h"
#include "support/test_composer.h"
#include "support/test_knowledge.h"
#include "support/test_support.h"
#include "ursatz_test.h"

#ifndef COMPOSER_FIXTURE_DIR
#error "COMPOSER_FIXTURE_DIR is not defined"
#endif

/*
 * The layers of a composed result (R-634, docs/api.md "outputs.layers"): a theme request with "layers" returns "realization" and
 * "reading" with the plan; the reading's target digest is realization_digest of the realization read back; the realization's performer
 * notes are the notes of the response's MIDI file (key, onset, duration); the reading's cadence claims are the cadences the Composer's own
 * hearing of the response's plan finds; and the same request without the flag gives the document it gave before (no such members), which
 * is the layered document without them. A request for the layers of variations is refused.
 */

static char *request_text(bool layers) {
    char *text = test_must_read_file(COMPOSER_FIXTURE_DIR "/theme/one-motive.json", NULL);
    cJSON *request = cJSON_Parse(text);
    free(text);
    assert(request);
    assert(cJSON_AddNumberToObject(request, "seed", 1) && cJSON_AddBoolToObject(request, "midi", true));
    if (layers) {
        assert(cJSON_AddBoolToObject(request, "layers", true));
    }
    char *printed = cJSON_PrintUnformatted(request);
    cJSON_Delete(request);
    assert(printed);
    return printed;
}

static bool same_note(const PerformerNote *note, const NoteEvent *event, long pitch) {
    return note->midi == pitch &&
           musical_time_same(note->onset, *temporal_anchor_position(note_event_temporal_anchor(event))) &&
           duration_same(note->duration, *note_event_duration(event));
}

/* Every performer note of the work is one note of the MIDI file, and the file has no other. */
static void expect_notes_of_midi(const Work *work, const ComposerResponse *response) {
    MidiImport import;
    assert(response->midi && midi_importer_import_complete(response->midi, response->midi_size, &import) == MIDI_IMPORT_OK);
    bool *used = calloc(import.count, sizeof(bool));
    assert(used);
    size_t notes = 0;
    for (size_t m = 0; m < work_movement_count(work); m++) {
        const Movement *movement = work_movement_at(work, m);
        for (size_t p = 0; p < movement_part_count(movement); p++) {
            const PerformerPart *part = movement_part_at(movement, p);
            for (size_t n = 0; n < performer_part_note_count(part); n++, notes++) {
                const PerformerNote *note = performer_part_note_at(part, n);
                bool matched = false;
                for (size_t j = 0; j < import.count && !matched; j++) {
                    if (!used[j] && same_note(note, import.events[j], pitch_numeric_value_position_numerator(import.pitch_values[j]))) {
                        used[j] = matched = true;
                    }
                }
                assert(matched);
            }
        }
    }
    assert(notes == import.count && notes > 0);
    free(used);
    midi_import_destroy(&import);
}

/* The cadence claims of the reading equal those of the hearing of the plan: type and arrival, in order. */
static void expect_cadences_heard(const Reading *reading, const char *plan_json) {
    char path[] = "api_layers_plan.json";
    test_write_text(path, plan_json);
    Plan *plan = NULL;
    PlanJsonError error;
    assert(test_read_plan_file(path, &plan, &error) == PLAN_OK);
    ComposerHearing *hearing = NULL;
    assert(hearing_create(plan, test_knowledge(), &hearing) == HEARING_OK);
    const Reading *heard = NULL;
    assert(hearing_piece_reading(hearing, &heard) == HEARING_OK);
    size_t count = reading_cadence_count(heard);
    assert(count > 0 && reading_cadence_count(reading) == count);
    for (size_t i = 0; i < count; i++) {
        const ReadingCadence *a = reading_cadence_at(reading, i);
        const ReadingCadence *b = reading_cadence_at(heard, i);
        assert(a->type == b->type && rational_cmp(a->arrival, b->arrival) == 0);
    }
    hearing_destroy(hearing);
    plan_destroy(plan);
}

static void test_layers(void) {
    char *with_text = request_text(true);
    ComposerResponse layered;
    assert(composer_api_theme(test_context(), with_text, NULL, &layered) == COMPOSER_OK);
    free(with_text);
    cJSON *document = cJSON_Parse(layered.json);
    assert(document);
    cJSON *realization = cJSON_GetObjectItemCaseSensitive(document, "realization");
    cJSON *reading = cJSON_GetObjectItemCaseSensitive(document, "reading");
    assert(cJSON_IsObject(realization) && cJSON_IsObject(reading));

    /* The members read back, and the reading names the realization by its digest. */
    char *realization_text = cJSON_PrintUnformatted(realization);
    char *reading_text = cJSON_PrintUnformatted(reading);
    Work *work = NULL;
    RealizationJsonError realization_error;
    assert(realization_work_from_json(test_knowledge(), realization_text, &work, &realization_error) == REALIZATION_OK);
    Reading *read_back = NULL;
    ReadingJsonError reading_error;
    assert(reading_from_json(test_knowledge(), reading_text, &read_back, &reading_error) == REALIZATION_OK);
    char digest[17];
    assert(realization_digest(work, digest));
    assert(strcmp(reading_target(read_back)->digest, digest) == 0);
    assert(strcmp(reading_source(read_back)->name, "ursatz-composer") == 0);

    expect_notes_of_midi(work, &layered);
    expect_cadences_heard(read_back, layered.plan_json);

    /* The same request without the flag: no members, and the rest of the document as it was. */
    char *plain_text = request_text(false);
    ComposerResponse plain;
    assert(composer_api_theme(test_context(), plain_text, NULL, &plain) == COMPOSER_OK);
    free(plain_text);
    cJSON *plain_document = cJSON_Parse(plain.json);
    assert(plain_document);
    assert(!cJSON_GetObjectItemCaseSensitive(plain_document, "realization") && !cJSON_GetObjectItemCaseSensitive(plain_document, "reading"));
    cJSON_DeleteItemFromObjectCaseSensitive(document, "realization");
    cJSON_DeleteItemFromObjectCaseSensitive(document, "reading");
    assert(cJSON_Compare(document, plain_document, true));
    assert(strcmp(layered.plan_json, plain.plan_json) == 0);
    assert(layered.midi_size == plain.midi_size && memcmp(layered.midi, plain.midi, plain.midi_size) == 0);

    /* The files the schema test validates. */
    test_write_text("api_theme_request_layers.json", with_text = request_text(true));
    free(with_text);
    test_write_text("api_theme_response_layers.json", layered.json);

    cJSON_Delete(plain_document);
    cJSON_Delete(document);
    free(realization_text);
    free(reading_text);
    reading_destroy(read_back);
    work_destroy(work);
    composer_response_destroy(&plain);
    composer_response_destroy(&layered);
}

/* "layers" is a boolean. */
static void test_refusal(void) {
    char *text = test_must_read_file(COMPOSER_FIXTURE_DIR "/theme/one-motive.json", NULL);
    cJSON *request = cJSON_Parse(text);
    free(text);
    assert(request && cJSON_AddStringToObject(request, "layers", "yes"));
    char *printed = cJSON_PrintUnformatted(request);
    cJSON_Delete(request);
    ComposerResponse response;
    assert(composer_api_theme(test_context(), printed, NULL, &response) == COMPOSER_ERR_REQUEST);
    assert(strcmp(response.error.path, "/layers") == 0);
    free(printed);
    composer_response_destroy(&response);
}

int main(void) {
    test_layers();
    test_refusal();
    test_knowledge_release();
    puts("api layers: ok");
    return 0;
}
