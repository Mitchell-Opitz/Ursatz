#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/composer.h>
#include <ursatz_composer/composer_api.h>
#include <ursatz_composer/plan_json.h>

#include "support/test_support.h"

#ifndef COMPOSER_SNAPSHOT_DIR
#error "COMPOSER_SNAPSHOT_DIR is not defined"
#endif

/*
 * The API core (docs/api.md): the version strings and the versions entry, the export entry (the plan of a CLI snapshot
 * gives the snapshot's MIDI byte for byte), the refusals of a wrong api_version and a malformed request with the member at
 * fault, and composer_response_destroy twice. The responses are written beside the test, for the schema test.
 */

static void write_response(const char *name, const ComposerResponse *response) {
    char path[128];
    snprintf(path, sizeof path, "api_%s_response_%s.json", name, "core");
    test_write_file(path, response->json);
}

static char *export_request(const char *plan_text, const char *options) {
    size_t length = strlen(plan_text) + strlen(options) + 32;
    char *request = malloc(length);
    assert(request);
    snprintf(request, length, "{\"plan\":%s,%s}", plan_text, options);
    return request;
}

static void test_versions(void) {
    assert(strcmp(composer_api_version(), "1.7") == 0);
    assert(strcmp(composer_status_name(COMPOSER_OK), "ok") == 0);
    assert(strcmp(composer_status_name(COMPOSER_ERR_REQUEST), "request") == 0);
    assert(strcmp(composer_status_name(COMPOSER_ERR_UNSATISFIABLE), "unsatisfiable") == 0);
    assert(strcmp(composer_status_name(COMPOSER_ERR_CANCELLED), "cancelled") == 0);
    assert(strcmp(composer_status_name(COMPOSER_ERR_ALLOC), "alloc") == 0);
    assert(strcmp(composer_status_name(COMPOSER_ERR_INTERNAL), "internal") == 0);

    ComposerResponse response;
    assert(composer_api_versions(&response) == COMPOSER_OK);
    assert(response.json && !response.plan_json && !response.midi && !response.musicxml && response.midi_size == 0);
    assert(strstr(response.json, "\"composer_version\":\"" ) != NULL);
    char expected[64];
    snprintf(expected, sizeof expected, "\"composer_version\":\"%s\"", composer_version());
    assert(strstr(response.json, expected));
    assert(strstr(response.json, "\"api_version\":\"1.7\""));
    assert(strstr(response.json, "\"analyzer_version\":\""));
    assert(strstr(response.json, "\"ursatz_commit\":\""));
    snprintf(expected, sizeof expected, "\"plan_format_version\":%d", URSATZ_COMPOSER_PLAN_FORMAT_VERSION);
    assert(strstr(response.json, expected));
    write_response("versions", &response);
    composer_response_destroy(&response);
}

static void test_export(void) {
    char *plan_text = test_read_file(COMPOSER_SNAPSHOT_DIR "/grow-period.plan.json", NULL);
    size_t midi_size = 0;
    char *midi = test_read_file(COMPOSER_SNAPSHOT_DIR "/grow-period.mid", &midi_size);

    char *request = export_request(plan_text, "\"midi\":true,\"musicxml\":true,\"api_version\":1");
    ComposerResponse response;
    assert(composer_api_export(request, NULL, &response) == COMPOSER_OK);
    free(request);
    assert(response.midi && response.midi_size == midi_size && memcmp(response.midi, midi, midi_size) == 0);
    assert(response.musicxml && strstr(response.musicxml, "<score-partwise"));
    assert(response.json && strstr(response.json, "\"skipped\":["));
    assert(strstr(response.json, "\"reason\":\"source material\""));
    write_response("export", &response);
    composer_response_destroy(&response);

    /* A format not asked for is not made; a tempo is written into the file. */
    request = export_request(plan_text, "\"midi\":true,\"tempo\":\"195/2\"");
    assert(composer_api_export(request, NULL, &response) == COMPOSER_OK);
    free(request);
    assert(response.midi && !response.musicxml && response.midi_size > 0);
    assert(response.midi_size != midi_size);
    composer_response_destroy(&response);

    request = export_request(plan_text, "\"midi\":true,\"plain_midi\":true,\"tempo\":null");
    assert(composer_api_export(request, NULL, &response) == COMPOSER_OK);
    free(request);
    assert(response.midi && response.midi_size < midi_size);
    composer_response_destroy(&response);

    /* The plan file text is the plan as the library writes plan files; with no format asked for, nothing is rendered. */
    request = export_request(plan_text, "\"plan_file\":true");
    assert(composer_api_export(request, NULL, &response) == COMPOSER_OK);
    free(request);
    assert(response.plan_json && strstr(response.plan_json, "\"format\": \"ursatz-composer-plan\""));
    assert(!response.midi && !response.musicxml);
    /* The text is a plan file: it exports to the same MIDI. */
    request = export_request(response.plan_json, "\"midi\":true");
    composer_response_destroy(&response);
    assert(composer_api_export(request, NULL, &response) == COMPOSER_OK);
    free(request);
    assert(response.midi && response.midi_size == midi_size && memcmp(response.midi, midi, midi_size) == 0);
    composer_response_destroy(&response);
    request = export_request(plan_text, "\"plan_file\":true,\"midi\":true");
    assert(composer_api_export(request, NULL, &response) == COMPOSER_OK);
    free(request);
    assert(response.plan_json && response.midi && response.midi_size == midi_size);
    composer_response_destroy(&response);

    free(plan_text);
    free(midi);
}

static void expect_refused(const char *request, ComposerStatus status, const char *path) {
    ComposerResponse response;
    ComposerStatus result = composer_api_export(request, NULL, &response);
    assert(result == status);
    assert(strcmp(response.error.path, path) == 0);
    assert(response.error.message[0] != '\0');
    assert(!response.json && !response.midi && !response.musicxml && !response.plan_json);
    composer_response_destroy(&response);
}

static void test_refusals(void) {
    char *plan_text = test_read_file(COMPOSER_SNAPSHOT_DIR "/grow-period.plan.json", NULL);
    char *request = export_request(plan_text, "\"midi\":true,\"api_version\":2");
    expect_refused(request, COMPOSER_ERR_REQUEST, "/api_version");
    free(request);
    request = export_request(plan_text, "\"midi\":true,\"tempo\":5");
    expect_refused(request, COMPOSER_ERR_REQUEST, "/tempo");
    free(request);
    request = export_request(plan_text, "\"midi\":\"yes\"");
    expect_refused(request, COMPOSER_ERR_REQUEST, "/midi");
    free(request);
    request = export_request(plan_text, "\"plan_file\":\"yes\"");
    expect_refused(request, COMPOSER_ERR_REQUEST, "/plan_file");
    free(request);
    free(plan_text);

    expect_refused("{not json", COMPOSER_ERR_REQUEST, "");
    expect_refused("[]", COMPOSER_ERR_REQUEST, "");
    expect_refused(NULL, COMPOSER_ERR_REQUEST, "");
    expect_refused("{\"midi\":true}", COMPOSER_ERR_REQUEST, "/plan");
    expect_refused("{\"plan\":{\"format\":\"nope\"},\"midi\":true}", COMPOSER_ERR_REQUEST, "/plan/format");
}

static void test_destroy(void) {
    ComposerResponse response;
    assert(composer_api_versions(&response) == COMPOSER_OK);
    composer_response_destroy(&response);
    assert(!response.json && !response.plan_json && !response.midi && !response.musicxml && response.midi_size == 0);
    composer_response_destroy(&response);

    ComposerResponse zeroed;
    memset(&zeroed, 0, sizeof zeroed);
    composer_response_destroy(&zeroed);
    composer_response_destroy(NULL);

    /* A failed entry leaves a response that is safe to destroy. */
    assert(composer_api_export("{}", NULL, &response) == COMPOSER_ERR_REQUEST);
    composer_response_destroy(&response);
    composer_response_destroy(&response);
}

int main(void) {
    test_versions();
    test_export();
    test_refusals();
    test_destroy();
    printf("test_api_core: ok\n");
    return 0;
}
