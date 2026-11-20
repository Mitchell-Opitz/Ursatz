/* The layers a piece and a composed theme keep (P63 B9, R-644): the Analyzer's realization and reading beside each analyzed piece, the Composer's beside
 * each composed version, and the transaction's refusal of a reading that names another realization. The piece is Mozart 545's MIDI; the theme is composed
 * from two motifs, and its MIDI is the one a compose without layers gives. */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <cJSON.h>
#include "theme_body.h"

#include <common/files.h>
#include <common/json.h>
#include <ursatz_composer/composer_api.h>
#include <ursatz_workshop/workshop_api.h>

#include "analysis/analysis.h"
#include "base/knowledge.h"
#include "composition/composer_client.h"
#include "composition/motifs.h"
#include "composition/themes.h"
#include "items/collections.h"
#include "items/layers.h"
#include "items/layout.h"
#include "items/store.h"
#include "items/subjects.h"
#include "items/txn.h"

#ifndef URSATZ_WORKSHOP_TEST_MOZART
#error "URSATZ_WORKSHOP_TEST_MOZART must name Mozart 545's MIDI file"
#endif

static int g_failures = 0;

#define CHECK(cond, what)                                                       \
    do {                                                                        \
        if (!(cond)) {                                                          \
            g_failures++;                                                       \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, what);      \
        } else {                                                                \
            printf("PASS: %s\n", what);                                         \
        }                                                                       \
    } while (0)

static const char *text(const cJSON *object, const char *name) {
    const cJSON *item = object ? cJSON_GetObjectItemCaseSensitive(object, name) : NULL;
    return cJSON_IsString(item) ? item->valuestring : "";
}

static char *read_all(const char *path, size_t *size) {
    char *data = NULL;
    return common_file_read(path, &data, size, NULL) == URSATZ_OK ? data : NULL;
}

static cJSON *read_json(const char *path) {
    cJSON *value = NULL;
    return common_json_read_file(path, &value, NULL) == URSATZ_OK ? value : NULL;
}

static void motif(const char *name, const char *project, const char *notes, char *out) {
    static const char kBody[] =
        "{\"uses\":[],\"motif\":{\"role\":\"melodic\",\"form\":\"full\",\"notes\":\"%s\",\"meter\":\"4/4\",\"material\":\"omt.material.natural-minor\","
        "\"pickup\":\"0\",\"features\":[],\"analysis\":null,\"derived\":null}}";
    char json[1024];
    snprintf(json, sizeof(json), kBody, notes);
    cJSON *body = cJSON_Parse(json);
    char analysis_error[WORKSHOP_MOTIF_ANALYSIS_ERROR_SIZE];
    char refusal[512] = "";
    char refusal_path[128] = "";
    assert(workshop_motif_analyze_body(body, NULL, analysis_error, refusal, sizeof(refusal), refusal_path, sizeof(refusal_path)) ==
           WORKSHOP_MOTIF_ANALYSIS_DONE);
    cJSON *item = NULL;
    assert(workshop_material_item_create_noted("motif", name, project, body, NULL, &item) == URSATZ_OK);
    snprintf(out, 64, "%s", text(item, "id"));
    cJSON_Delete(item);
    cJSON_Delete(body);
}

static void test_piece(void) {
    size_t size = 0;
    char *midi = read_all(URSATZ_WORKSHOP_TEST_MOZART, &size);
    assert(midi);
    char *id = NULL;
    assert(workshop_analysis_import_and_save(WORKSHOP_DIR_PIECES, (const unsigned char *)midi, size, "Mozart 545.mid", &id) == URSATZ_OK);
    free(midi);
    char path[512];
    snprintf(path, sizeof(path), "%s/%s/realization.json", WORKSHOP_DIR_PIECES, id);
    CHECK(common_path_exists(path), "the piece's folder holds realization.json");
    snprintf(path, sizeof(path), "%s/%s/readings/analyzer.json", WORKSHOP_DIR_PIECES, id);
    CHECK(common_path_exists(path), "and readings/analyzer.json");
    cJSON *reading = read_json(path);
    const cJSON *target = cJSON_GetObjectItemCaseSensitive(reading, "target");
    CHECK(strcmp(text(target, "digest"), "e8c0e66e5eeed3e1") == 0, "the reading names Mozart 545's realization by its digest (e8c0e66e5eeed3e1)");
    CHECK(strcmp(text(cJSON_GetObjectItemCaseSensitive(reading, "source"), "kind"), "analyzer") == 0, "and is the Analyzer's");
    cJSON_Delete(reading);

    /* the answer describes the layers */
    cJSON *answer = NULL;
    CHECK(workshop_library_piece_json(WORKSHOP_DIR_PIECES, id, true, &answer) == URSATZ_OK, "the piece's answer is made");
    const cJSON *layers = cJSON_GetObjectItemCaseSensitive(answer, "layers");
    CHECK(strcmp(text(cJSON_GetObjectItemCaseSensitive(layers, "realization"), "digest"), "e8c0e66e5eeed3e1") == 0 &&
              cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(layers, "readings")) == 1 &&
              strcmp(text(cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(layers, "readings"), 0), "source"), "analyzer") == 0,
          "the answer has layers: the digest, the movements and the Analyzer's reading");
    cJSON_Delete(answer);

    /* a re-analysis writes both again, in one record with the analysis */
    CHECK(workshop_analysis_reanalyze(WORKSHOP_DIR_PIECES, id) == URSATZ_OK, "a re-analysis runs");
    snprintf(path, sizeof(path), "%s/%s/readings/analyzer.json", WORKSHOP_DIR_PIECES, id);
    reading = read_json(path);
    CHECK(strcmp(text(cJSON_GetObjectItemCaseSensitive(reading, "target"), "digest"), "e8c0e66e5eeed3e1") == 0, "and the layers stay the same");
    cJSON_Delete(reading);

    /* a commit with a reading of another digest is refused, and nothing is made */
    char realization_path[512];
    snprintf(realization_path, sizeof(realization_path), "%s/%s/realization.json", WORKSHOP_DIR_PIECES, id);
    char *realization = read_all(realization_path, NULL);
    snprintf(path, sizeof(path), "%s/%s/readings/analyzer.json", WORKSHOP_DIR_PIECES, id);
    char *reading_text = read_all(path, NULL);
    char *at = strstr(reading_text, "e8c0e66e5eeed3e1");
    assert(at);
    memcpy(at, "0000000000000000", 16);
    WorkshopTxn *txn = workshop_txn_begin(NULL, "library.import", "michelle", "piece-bad", 0);
    const char *pending = NULL;
    char file[600];
    CHECK(txn && workshop_txn_stage_folder(txn, WORKSHOP_DIR_PIECES "/piece-bad", &pending) == URSATZ_OK, "a piece with a wrong reading is staged");
    snprintf(file, sizeof(file), "%s/realization.json", pending);
    assert(common_file_write_atomic(file, realization, strlen(realization), NULL) == URSATZ_OK);
    snprintf(file, sizeof(file), "%s/readings", pending);
    assert(common_dir_make(file, NULL) == URSATZ_OK);
    snprintf(file, sizeof(file), "%s/readings/wrong.json", pending);
    assert(common_file_write_atomic(file, reading_text, strlen(reading_text), NULL) == URSATZ_OK);
    UrsatzError error;
    memset(&error, 0, sizeof(error));
    CHECK(workshop_txn_commit(txn, &error) == URSATZ_ERR_FORMAT && strstr(error.message, "readings/wrong.json") != NULL &&
              strstr(error.message, "not the one beside it") != NULL,
          "a commit with a reading of another digest is refused, naming the reading");
    CHECK(!common_path_exists(WORKSHOP_DIR_PIECES "/piece-bad"), "and the piece is not there");
    /* a realization that does not read is refused too */
    txn = workshop_txn_begin(NULL, "library.import", "michelle", "piece-worse", 0);
    CHECK(txn && workshop_txn_stage_file(txn, WORKSHOP_DIR_PIECES "/piece-worse/realization.json", "{}", 2) == URSATZ_OK, "a broken realization is staged");
    memset(&error, 0, sizeof(error));
    CHECK(workshop_txn_commit(txn, &error) == URSATZ_ERR_FORMAT && strstr(error.message, "not a realization") != NULL, "and refused");
    free(reading_text);
    free(realization);
    free(id);
}

static void test_theme(const char *general) {
    char a[64];
    char b[64];
    motif("A", general, "C4:e Eb4:e G4:e Eb4:e C4:e Eb4:e G4:e C5:e", a);
    motif("B", general, "G4:e F4:e Eb4:e D4:e F4:e Eb4:e D4:e C4:e", b);
    char json[2048];
    theme_body_text(json, sizeof(json), a, b, 5);
    cJSON *body = cJSON_Parse(json);
    cJSON *theme = NULL;
    assert(workshop_material_item_create_noted("theme", "Theme", general, body, NULL, &theme) == URSATZ_OK);
    char id[64];
    snprintf(id, sizeof(id), "%s", text(theme, "id"));
    cJSON_Delete(theme);
    cJSON_Delete(body);

    cJSON *item = NULL;
    UrsatzError error;
    memset(&error, 0, sizeof(error));
    CHECK(workshop_theme_compose(id, true, 1, false, NULL, &item, &error) == URSATZ_OK, "the theme composes");
    cJSON_Delete(item);
    cJSON *layers = NULL;
    CHECK(workshop_material_version_layers(id, 2, &layers) == URSATZ_OK && cJSON_IsObject(layers) &&
              strlen(text(cJSON_GetObjectItemCaseSensitive(layers, "realization"), "digest")) == 16 &&
              strcmp(text(cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(layers, "readings"), 0), "source"), "composer") == 0,
          "its version holds the realization and the Composer's reading");
    cJSON_Delete(layers);
    CHECK(workshop_material_version_layers(id, 1, &layers) == URSATZ_OK && cJSON_IsNull(layers), "a version made without them reports layers: null");
    cJSON_Delete(layers);

    char dir[512];
    char path[600];
    assert(workshop_material_version_dir(id, 2, dir, sizeof(dir)));
    snprintf(path, sizeof(path), "%s/realization.json", dir);
    CHECK(common_path_exists(path), "realization.json is in the version's folder");
    snprintf(path, sizeof(path), "%s/readings/composer.json", dir);
    CHECK(common_path_exists(path), "and readings/composer.json");

    /* the MIDI is the one the same request gives without layers */
    snprintf(path, sizeof(path), "%s/request.json", dir);
    cJSON *request = read_json(path);
    assert(request);
    cJSON_AddBoolToObject(request, "midi", 1);
    cJSON_AddBoolToObject(request, "musicxml", 1);
    cJSON_AddBoolToObject(request, "plain_midi", 0);
    cJSON_AddBoolToObject(request, "plans", 0);
    cJSON_AddBoolToObject(request, "check", 0);
    cJSON_AddStringToObject(request, "seed", "1");
    char *request_text = cJSON_PrintUnformatted(request);
    cJSON_Delete(request);
    ComposerResponse response;
    memset(&response, 0, sizeof(response));
    CHECK(composer_api_theme(workshop_composer_context_global(), request_text, NULL, &response) == URSATZ_OK && response.midi, "the theme is composed without layers");
    free(request_text);
    snprintf(path, sizeof(path), "%s/theme.mid", dir);
    size_t size = 0;
    char *stored = read_all(path, &size);
    CHECK(stored && size == response.midi_size && memcmp(stored, response.midi, size) == 0, "its MIDI is byte for byte the version's: the layers change no composed output");
    free(stored);
    composer_response_destroy(&response);
}

int main(void) {
    char home[512];
    assert(getcwd(home, sizeof(home)));
    char folder[600];
    snprintf(folder, sizeof(folder), "%s/layers_%d", home, (int)getpid());
    WorkshopConfig config = {folder, NULL, 9501, 2, 2};
    WorkshopConfig resolved;
    char storage[512];
    UrsatzError error;
    memset(&error, 0, sizeof(error));
    assert(workshop_settings_resolve(&config, &resolved, storage, sizeof(storage), &error) == URSATZ_OK);
    Workshop *workshop = NULL;
    assert(workshop_open(&resolved, &workshop, &error) == URSATZ_OK);
    workshop_close(workshop);
    assert(chdir(folder) == 0);
    assert(workshop_knowledge_init());
    assert(workshop_material_service_init());
    cJSON *projects = NULL;
    assert(workshop_material_project_list(&projects) == URSATZ_OK);
    char general[64];
    snprintf(general, sizeof(general), "%s", text(cJSON_GetArrayItem(projects, 0), "id"));
    cJSON_Delete(projects);
    assert(workshop_subject_service_init());
    assert(ursatz_gui_collection_service_init());

    test_piece();
    test_theme(general);

    workshop_subject_service_shutdown();
    workshop_collection_service_shutdown();
    workshop_material_service_shutdown();
    workshop_knowledge_shutdown();
    assert(chdir(home) == 0);
    char command[700];
    snprintf(command, sizeof(command), "rm -rf '%s'", folder);
    if (system(command) != 0) {
        return 1;
    }
    printf("workshop_layers: %d failures\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
