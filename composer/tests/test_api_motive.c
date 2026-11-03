#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>
#include <ursatz_composer/composer_api.h>

#include "support/test_support.h"

#ifndef COMPOSER_SNAPSHOT_DIR
#error "COMPOSER_SNAPSHOT_DIR is not defined"
#endif
#ifndef COMPOSER_FIXTURE_DIR
#error "COMPOSER_FIXTURE_DIR is not defined"
#endif

/*
 * The motive entries (docs/api.md) against the shapes the GUI runs: each request is the arguments of a CLI snapshot
 * (tests/cli_snapshots.cmake, "GUI shapes"), and its response is that snapshot's JSON, compared as a value as the snapshot
 * test does. The requests and responses are written beside the test, for the schema test.
 */

static void write_pair(const char *tag, const char *request, const ComposerResponse *response) {
    char path[128];
    snprintf(path, sizeof path, "api_motive_request_%s.json", tag);
    test_write_file(path, request);
    snprintf(path, sizeof path, "api_motive_response_%s.json", tag);
    test_write_file(path, response->json);
}

/* The request: the members of options (a JSON object text) and, from the file, "input" with its format and form. */
static char *request_with_input(const char *file, const char *format, const char *form, const char *options) {
    cJSON *request = cJSON_Parse(options);
    assert(request);
    char *text = test_read_file(file, NULL);
    cJSON *input = cJSON_AddObjectToObject(request, "input");
    assert(input && cJSON_AddStringToObject(input, "text", text));
    if (format) {
        assert(cJSON_AddStringToObject(input, "format", format));
    }
    if (form) {
        assert(cJSON_AddStringToObject(input, "form", form));
    }
    char *out = cJSON_PrintUnformatted(request);
    assert(out);
    cJSON_Delete(request);
    free(text);
    return out;
}

static void expect_snapshot(const ComposerResponse *response, const char *snapshot) {
    char path[256];
    snprintf(path, sizeof path, COMPOSER_SNAPSHOT_DIR "/%s", snapshot);
    char *expected = test_read_file(path, NULL);
    cJSON *actual_json = cJSON_Parse(response->json);
    cJSON *expected_json = cJSON_Parse(expected);
    assert(actual_json && expected_json);
    if (!cJSON_Compare(actual_json, expected_json, true)) {
        fprintf(stderr, "the response differs from %s:\n%s\n", snapshot, response->json);
    }
    assert(cJSON_Compare(actual_json, expected_json, true));
    cJSON_Delete(actual_json);
    cJSON_Delete(expected_json);
    free(expected);
}

static void test_gui_shapes(void) {
    ComposerResponse response;
    char *request = request_with_input(COMPOSER_FIXTURE_DIR "/gui/motive-notes.txt", "notes", NULL,
                                       "{\"meter\":\"4/4\",\"key\":\"D minor\",\"pickup\":\"0\",\"harmony\":\"I V@3\","
                                       "\"features\":[\"a:0-2\"],\"role\":\"melodic\",\"tones\":[\"2=passing\"],\"api_version\":1}");
    assert(composer_api_motive(request, NULL, &response) == COMPOSER_OK);
    assert(response.json && !response.midi && !response.musicxml && !response.plan_json);
    expect_snapshot(&response, "motive-notes.stdout.json");
    write_pair("notes", request, &response);
    composer_response_destroy(&response);
    free(request);

    request = request_with_input(COMPOSER_FIXTURE_DIR "/gui/motive-rhythm.txt", NULL, "rhythm",
                                 "{\"meter\":\"3/4\",\"pickup\":\"1/4\"}");
    assert(composer_api_motive(request, NULL, &response) == COMPOSER_OK);
    expect_snapshot(&response, "motive-form-rhythm.stdout.json");
    write_pair("rhythm", request, &response);
    composer_response_destroy(&response);
    free(request);

    request = request_with_input(COMPOSER_FIXTURE_DIR "/gui/motive-degrees.txt", "degrees", NULL,
                                 "{\"key\":\"C minor\",\"meter\":\"6/8\",\"pickup\":0}");
    assert(composer_api_motive(request, NULL, &response) == COMPOSER_OK);
    expect_snapshot(&response, "motive-degrees.stdout.json");
    write_pair("degrees", request, &response);
    composer_response_destroy(&response);
    free(request);

    /* MusicXML and a motive document arrive as text and give the motive the MIDI file does. */
    ComposerResponse other;
    request = request_with_input(COMPOSER_FIXTURE_DIR "/motive_d_minor.musicxml", NULL, NULL, "{}");
    assert(composer_api_motive(request, NULL, &response) == COMPOSER_OK);
    free(request);
    request = request_with_input(COMPOSER_FIXTURE_DIR "/motive_d_minor.json", "json", NULL, "{}");
    assert(composer_api_motive(request, NULL, &other) == COMPOSER_OK);
    free(request);
    assert(strcmp(response.json, other.json) == 0);
    composer_response_destroy(&response);
    composer_response_destroy(&other);
}

static void test_midi(void) {
    size_t size = 0;
    unsigned char *bytes = (unsigned char *)test_read_file(COMPOSER_FIXTURE_DIR "/motive_d_minor.mid", &size);
    ComposerResponse response;
    const char *request = "{\"meter\":\"4/4\",\"key\":\"C major\"}";
    assert(composer_api_motive_from_midi(bytes, size, request, NULL, &response) == COMPOSER_OK);
    expect_snapshot(&response, "motive-midi.stdout.json");
    write_pair("midi", request, &response);
    composer_response_destroy(&response);

    /* The file's own meter and key, and a pickup counted in its beats. */
    assert(composer_api_motive_from_midi(bytes, size, "{\"pickup\":\"1/2\"}", NULL, &response) == COMPOSER_OK);
    assert(strstr(response.json, "\"pickup\": \"1/2\""));
    composer_response_destroy(&response);
    free(bytes);

    /* Ten bytes that are not a MIDI file. */
    const unsigned char noise[10] = {0x4c, 0x9a, 0x01, 0xff, 0x37, 0x00, 0x80, 0x12, 0xe5, 0x61};
    assert(composer_api_motive_from_midi(noise, sizeof noise, "{}", NULL, &response) == COMPOSER_ERR_UNSATISFIABLE);
    assert(!response.json && response.error.message[0] != '\0');
    composer_response_destroy(&response);
    assert(composer_api_motive_from_midi(NULL, 0, "{}", NULL, &response) == COMPOSER_ERR_REQUEST);
    composer_response_destroy(&response);
}

static void test_render(void) {
    ComposerResponse response;
    char *request = request_with_input(COMPOSER_FIXTURE_DIR "/gui/motive-degrees.txt", "degrees", "full",
                                       "{\"meter\":\"4/4\",\"key\":\"C major\",\"pickup\":\"0\",\"render_key\":\"Eb major\","
                                       "\"tempo\":96,\"musicxml\":true}");
    assert(composer_api_motive_render(request, NULL, &response) == COMPOSER_OK);
    expect_snapshot(&response, "motive-render.stdout.json");
    size_t size = 0;
    char *xml = test_read_file(COMPOSER_SNAPSHOT_DIR "/motive-render.musicxml", &size);
    assert(response.musicxml && strlen(response.musicxml) == size && memcmp(response.musicxml, xml, size) == 0);
    assert(!response.midi && response.midi_size == 0);
    write_pair("render", request, &response);
    composer_response_destroy(&response);
    free(xml);
    free(request);

    /* MIDI as well, and no files without midi or musicxml; the rendered notes are always in the response. */
    request = request_with_input(COMPOSER_FIXTURE_DIR "/gui/motive-degrees.txt", "degrees", NULL,
                                 "{\"meter\":\"3/4\",\"key\":\"D minor\",\"pickup\":1,\"midi\":true,\"musicxml\":true}");
    assert(composer_api_motive_render(request, NULL, &response) == COMPOSER_OK);
    assert(response.midi && response.midi_size > 14 && memcmp(response.midi, "MThd", 4) == 0);
    assert(response.musicxml && strstr(response.musicxml, "<measure number=\"0\" implicit=\"yes\">"));
    assert(strstr(response.json, "\"rendered\""));
    composer_response_destroy(&response);
    free(request);
    request = request_with_input(COMPOSER_FIXTURE_DIR "/gui/motive-degrees.txt", "degrees", NULL,
                                 "{\"meter\":\"3/4\",\"key\":\"D minor\"}");
    assert(composer_api_motive_render(request, NULL, &response) == COMPOSER_OK);
    assert(!response.midi && !response.musicxml && strstr(response.json, "\"rendered\""));
    composer_response_destroy(&response);
    free(request);
}

static void expect_refused(const char *request, ComposerStatus status, const char *path, const char *message) {
    ComposerResponse response;
    ComposerStatus result = composer_api_motive(request, NULL, &response);
    if (result != status || strcmp(response.error.path, path) != 0 || !strstr(response.error.message, message)) {
        fprintf(stderr, "'%s': status %s, path '%s', message '%s'\n", request, composer_status_name(result),
                response.error.path, response.error.message);
    }
    assert(result == status);
    assert(strcmp(response.error.path, path) == 0);
    assert(strstr(response.error.message, message));
    assert(!response.json && !response.midi && !response.musicxml);
    composer_response_destroy(&response);
}

static void test_refusals(void) {
    const char *notes = "\"input\":{\"text\":\"D4:q F4:e A4:e D5:h\"}";
    char request[512];
    snprintf(request, sizeof request, "{%s,\"meter\":\"9/5\"}", notes);
    expect_refused(request, COMPOSER_ERR_REQUEST, "/meter", "meter '9/5'");
    snprintf(request, sizeof request, "{%s,\"meter\":\"4/4\",\"key\":\"H major\"}", notes);
    expect_refused(request, COMPOSER_ERR_REQUEST, "/key", "unknown key 'H major'");
    snprintf(request, sizeof request, "{%s,\"meter\":\"4/4\",\"key\":\"D minor\",\"harmony\":\"I V6\"}", notes);
    expect_refused(request, COMPOSER_ERR_REQUEST, "/harmony", "token 2");
    snprintf(request, sizeof request, "{%s,\"meter\":\"4/4\",\"key\":\"D minor\",\"features\":[\"a:0-1\",\"a:2-3\"]}", notes);
    expect_refused(request, COMPOSER_ERR_REQUEST, "/features/1", "label 'a' is used twice");
    snprintf(request, sizeof request, "{%s,\"meter\":\"4/4\",\"key\":\"D minor\",\"role\":\"bass\"}", notes);
    expect_refused(request, COMPOSER_ERR_REQUEST, "/role", "melodic or accompaniment");
    snprintf(request, sizeof request, "{%s,\"meter\":\"4/4\",\"key\":\"D minor\",\"tones\":[\"2=zzz\"]}", notes);
    expect_refused(request, COMPOSER_ERR_REQUEST, "/tones/0", "the role is chord");
    snprintf(request, sizeof request, "{%s,\"meter\":\"4/4\",\"key\":\"D minor\",\"harmony\":\"I V@2\",\"features\":[\"a:0-9\"]}", notes);
    expect_refused(request, COMPOSER_ERR_UNSATISFIABLE, "/input/text", "features[0]");
    snprintf(request, sizeof request, "{%s}", notes);
    expect_refused(request, COMPOSER_ERR_UNSATISFIABLE, "/input/text", "cannot load the input");
    snprintf(request, sizeof request, "{\"input\":{\"format\":\"xml\",\"text\":\"D4:q\"},\"meter\":\"4/4\"}");
    expect_refused(request, COMPOSER_ERR_REQUEST, "/input/format", "format must be");
    snprintf(request, sizeof request, "{\"input\":{\"form\":\"rhythm\",\"format\":\"contour\",\"text\":\"0 1\"},\"meter\":\"4/4\"}");
    expect_refused(request, COMPOSER_ERR_REQUEST, "/input/form", "form rhythm reads a rhythm");
    expect_refused("{\"meter\":\"4/4\"}", COMPOSER_ERR_REQUEST, "/input", "input must be");
    expect_refused("{\"input\":{},\"meter\":\"4/4\"}", COMPOSER_ERR_REQUEST, "/input/text", "text must be a string");
    expect_refused("{\"input\":{\"text\":\"D4:q\"},\"api_version\":3}", COMPOSER_ERR_REQUEST, "/api_version", "api_version");

    /* Render members are refused on the render entry only by their values. */
    ComposerResponse response;
    snprintf(request, sizeof request, "{%s,\"meter\":\"4/4\",\"key\":\"D minor\",\"render_key\":\"H\"}", notes);
    assert(composer_api_motive_render(request, NULL, &response) == COMPOSER_ERR_REQUEST);
    assert(strcmp(response.error.path, "/render_key") == 0);
    composer_response_destroy(&response);
    snprintf(request, sizeof request, "{%s,\"meter\":\"4/4\",\"render_key\":\"G minor\"}", notes);
    assert(composer_api_motive_render(request, NULL, &response) == COMPOSER_ERR_UNSATISFIABLE);
    assert(strcmp(response.error.path, "/render_key") == 0 && strstr(response.error.message, "it has no key"));
    composer_response_destroy(&response);
    snprintf(request, sizeof request, "{%s,\"meter\":\"4/4\",\"key\":\"D minor\",\"tempo\":5,\"midi\":true}", notes);
    assert(composer_api_motive_render(request, NULL, &response) == COMPOSER_ERR_REQUEST);
    assert(strcmp(response.error.path, "/tempo") == 0 && !response.midi);
    composer_response_destroy(&response);
}

static bool cancel_now(void *user) {
    (void)user;
    return true;
}

static void test_job(void) {
    ComposerJob job = {NULL, cancel_now, NULL};
    ComposerResponse response;
    assert(composer_api_motive("{\"input\":{\"text\":\"D4:q\"},\"meter\":\"4/4\"}", &job, &response) == COMPOSER_ERR_CANCELLED);
    assert(!response.json);
    composer_response_destroy(&response);
}

int main(void) {
    test_gui_shapes();
    test_midi();
    test_render();
    test_refusals();
    test_job();
    return 0;
}
