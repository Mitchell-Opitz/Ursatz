/* Stale analyses (R-277): a stored analysis whose schema, Analyzer version or Ursatz commit differs from the build is
 * analyzed again the first time it is opened (and only then), by the detail, the style build and the link worker; an
 * old meta.json without ursatz_commit gets it from its document; the list reports the flag from meta.json. Runs in its
 * own working directory (pieces/, styles/, links/, collections/, data/ and the material and subject stores). */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#define MKDIR(path) _mkdir(path)
#else
#include <sys/stat.h>
#include <unistd.h>
#define MKDIR(path) mkdir(path, 0755)
#endif

#include <cJSON.h>

#include <ursatz_analyzer/version.h>

#include "service/analysis_service.h"
#include "service/collection_service.h"
#include "service/link_service.h"
#include "service/piece_metadata.h"
#include "service/material_service.h"
#include "service/style_service.h"
#include "service/subject_service.h"

#ifndef URSATZ_GUI_TEST_LINK_FIXTURES
#error "URSATZ_GUI_TEST_LINK_FIXTURES must name the link fixtures directory"
#endif

static int failures = 0;

#define CHECK(cond, msg)                                            \
    do {                                                            \
        if (!(cond)) {                                              \
            fprintf(stderr, "FAIL: %s (line %d)\n", msg, __LINE__); \
            failures++;                                             \
        }                                                           \
    } while (0)

static char g_materials[64];
static char g_subjects[64];

static char *read_text(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        return NULL;
    }
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);
    char *data = malloc((size_t)length + 1);
    if (data && fread(data, 1, (size_t)length, file) != (size_t)length) {
        free(data);
        data = NULL;
    }
    if (data) {
        data[length] = '\0';
    }
    fclose(file);
    return data;
}

static bool write_json(const char *path, const cJSON *value) {
    char *text = cJSON_Print(value);
    FILE *file = text ? fopen(path, "wb") : NULL;
    bool ok = file && fwrite(text, 1, strlen(text), file) == strlen(text);
    if (file) {
        fclose(file);
    }
    free(text);
    return ok;
}

static cJSON *read_json(const char *path) {
    char *text = read_text(path);
    cJSON *value = text ? cJSON_Parse(text) : NULL;
    free(text);
    return value;
}

static bool import_piece(const char *name, char *out_id, size_t out_size) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", URSATZ_GUI_TEST_LINK_FIXTURES, name);
    char *text = read_text(path);
    char *id = NULL;
    bool ok = text && ursatz_gui_analysis_import_and_save("pieces", (const unsigned char *)text, strlen(text), name,
                                                          &id) == URSATZ_GUI_ANALYSIS_OK;
    free(text);
    if (ok) {
        snprintf(out_id, out_size, "%s", id);
    }
    free(id);
    return ok;
}

static void piece_path(const char *id, const char *file, char *out, size_t size) {
    snprintf(out, size, "pieces/%s/%s", id, file);
}

static const char *str(const cJSON *object, const char *name) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
    return cJSON_IsString(item) ? item->valuestring : "";
}

/* The analyzer_version of the stored document, "" when unreadable. */
static void document_analyzer_version(const char *id, char *out, size_t size) {
    char path[400];
    piece_path(id, "analysis.json", path, sizeof(path));
    cJSON *document = read_json(path);
    snprintf(out, size, "%s", str(cJSON_GetObjectItemCaseSensitive(document, "metadata"), "analyzer_version"));
    cJSON_Delete(document);
}

/* Makes the piece stale by Analyzer version in both files. */
static void make_stale(const char *id) {
    char path[400];
    piece_path(id, "analysis.json", path, sizeof(path));
    cJSON *document = read_json(path);
    cJSON *metadata = cJSON_GetObjectItemCaseSensitive(document, "metadata");
    cJSON_ReplaceItemInObjectCaseSensitive(metadata, "analyzer_version", cJSON_CreateString("0.0.1-old"));
    CHECK(write_json(path, document), "write the stale document");
    cJSON_Delete(document);
    piece_path(id, "meta.json", path, sizeof(path));
    cJSON *meta = read_json(path);
    cJSON_ReplaceItemInObjectCaseSensitive(meta, "analyzer_version", cJSON_CreateString("0.0.1-old"));
    CHECK(write_json(path, meta), "write the stale meta");
    cJSON_Delete(meta);
}

/* Adds a member to the stored document that a new analysis would not write. */
static void add_marker(const char *id) {
    char path[400];
    piece_path(id, "analysis.json", path, sizeof(path));
    cJSON *document = read_json(path);
    cJSON_AddBoolToObject(document, "x_marker", true);
    CHECK(write_json(path, document), "write the marker");
    cJSON_Delete(document);
}

static bool has_marker(const char *id) {
    char path[400];
    piece_path(id, "analysis.json", path, sizeof(path));
    cJSON *document = read_json(path);
    bool marked = cJSON_GetObjectItemCaseSensitive(document, "x_marker") != NULL;
    cJSON_Delete(document);
    return marked;
}

static const char *listed_status(const char *id, char *out, size_t size) {
    UrsatzGuiPieceSummary *pieces = NULL;
    size_t count = 0;
    out[0] = '\0';
    CHECK(ursatz_gui_analysis_list("pieces", &pieces, &count) == URSATZ_GUI_ANALYSIS_OK, "list the pieces");
    for (size_t i = 0; i < count; i++) {
        if (strcmp(pieces[i].id, id) == 0 && pieces[i].analysis_status) {
            snprintf(out, size, "%s", pieces[i].analysis_status);
        }
    }
    for (size_t i = 0; i < count; i++) {
        free(pieces[i].id);
        free(pieces[i].filename);
        free(pieces[i].format);
        free(pieces[i].uploaded_at);
        free(pieces[i].composer);
        free(pieces[i].source);
        ursatz_gui_meta_tags_free(pieces[i].tags, pieces[i].tag_count);
    }
    free(pieces);
    return out;
}

static void test_list_flag(const char *id) {
    char status[32], path[400];
    CHECK(strcmp(listed_status(id, status, sizeof(status)), "current") == 0, "a new piece lists as current");

    piece_path(id, "meta.json", path, sizeof(path));
    cJSON *meta = read_json(path);
    cJSON_ReplaceItemInObjectCaseSensitive(meta, "analyzer_version", cJSON_CreateString("0.0.1-old"));
    write_json(path, meta);
    CHECK(strcmp(listed_status(id, status, sizeof(status)), "stale_analyzer") == 0, "another Analyzer lists stale");

    cJSON_ReplaceItemInObjectCaseSensitive(meta, "analyzer_version", cJSON_CreateString(ursatz_analyzer_version()));
    cJSON_ReplaceItemInObjectCaseSensitive(meta, "ursatz_commit", cJSON_CreateString("0000000"));
    write_json(path, meta);
    CHECK(strcmp(listed_status(id, status, sizeof(status)), "stale_ursatz") == 0, "another Ursatz lists stale");

    cJSON_DeleteItemFromObjectCaseSensitive(meta, "ursatz_commit");
    write_json(path, meta);
    CHECK(strcmp(listed_status(id, status, sizeof(status)), "unreadable") == 0, "no ursatz_commit lists as stale");

    cJSON_ReplaceItemInObjectCaseSensitive(meta, "analyzer_version", cJSON_CreateString(ursatz_analyzer_version()));
    cJSON_AddStringToObject(meta, "ursatz_commit", ursatz_analyzer_ursatz_commit());
    write_json(path, meta);
    CHECK(strcmp(listed_status(id, status, sizeof(status)), "current") == 0, "restored lists as current");
    cJSON_Delete(meta);
}

static void test_detail(const char *id) {
    make_stale(id);
    char version[64];
    document_analyzer_version(id, version, sizeof(version));
    CHECK(strcmp(version, "0.0.1-old") == 0, "the document is stale before the first open");

    UrsatzGuiPieceDetail *detail = NULL;
    CHECK(ursatz_gui_analysis_get_piece("pieces", id, &detail) == URSATZ_GUI_ANALYSIS_OK && detail, "first open");
    CHECK(detail && strcmp(detail->analysis_status, "current") == 0, "the first open serves a current analysis");
    ursatz_gui_piece_detail_destroy(detail);
    document_analyzer_version(id, version, sizeof(version));
    CHECK(strcmp(version, ursatz_analyzer_version()) == 0, "the first open analyzed the piece again");

    add_marker(id);
    detail = NULL;
    CHECK(ursatz_gui_analysis_get_piece("pieces", id, &detail) == URSATZ_GUI_ANALYSIS_OK, "second open");
    ursatz_gui_piece_detail_destroy(detail);
    CHECK(has_marker(id), "the second open does not analyze again");
}

static void test_backfill(const char *id) {
    char path[400];
    piece_path(id, "meta.json", path, sizeof(path));
    cJSON *meta = read_json(path);
    cJSON_DeleteItemFromObjectCaseSensitive(meta, "ursatz_commit");
    write_json(path, meta);
    cJSON_Delete(meta);
    add_marker(id);

    UrsatzGuiPieceDetail *detail = NULL;
    CHECK(ursatz_gui_analysis_get_piece("pieces", id, &detail) == URSATZ_GUI_ANALYSIS_OK, "open an old meta.json");
    ursatz_gui_piece_detail_destroy(detail);
    meta = read_json(path);
    CHECK(strcmp(str(meta, "ursatz_commit"), ursatz_analyzer_ursatz_commit()) == 0,
          "ursatz_commit comes from the document");
    cJSON_Delete(meta);
    CHECK(has_marker(id), "filling ursatz_commit does not analyze again");
}

static void test_style(const char *id) {
    make_stale(id);
    add_marker(id);
    const char *ids[] = {id};
    UrsatzGuiStyleSource source = {0};
    source.kind = URSATZ_GUI_STYLE_SOURCE_PIECES;
    source.piece_ids = ids;
    source.piece_count = 1;
    char *style_id = NULL;
    UrsatzGuiStyleSkipped *skipped = NULL;
    size_t skipped_count = 0;
    UrsatzGuiStyleResult result = ursatz_gui_style_build("Stale", &source, &style_id, &skipped, &skipped_count);
    CHECK(result == URSATZ_GUI_STYLE_OK && skipped_count == 0, "the style builds from a stale piece");
    CHECK(!has_marker(id), "the style build analyzed the stale piece again");
    free(style_id);
    ursatz_gui_style_skipped_free(skipped, skipped_count);
}

static void test_link(const char *id, const char *other) {
    make_stale(id);
    add_marker(id);
    add_marker(other);
    UrsatzGuiLinkScope scope = {0};
    scope.kind = URSATZ_GUI_LINK_SCOPE_ALL;
    char *run_id = NULL;
    CHECK(ursatz_gui_link_run_start(&scope, &run_id) == URSATZ_GUI_LINK_OK && run_id, "the link run starts");
    for (int waited = 0; run_id && waited < 600000; waited += 50) {
        cJSON *run = NULL;
        if (ursatz_gui_link_run_get(run_id, &run) != URSATZ_GUI_LINK_OK) {
            break;
        }
        bool running = strcmp(str(run, "status"), "running") == 0;
        cJSON_Delete(run);
        if (!running) {
            break;
        }
#ifdef _WIN32
        Sleep(50);
#else
        usleep(50 * 1000);
#endif
    }
    free(run_id);
    CHECK(!has_marker(id), "the link worker analyzed the stale piece again");
    CHECK(has_marker(other), "the link worker left the current piece alone");
}

int main(void) {
    snprintf(g_materials, sizeof(g_materials), "materials_%d", (int)time(NULL));
    snprintf(g_subjects, sizeof(g_subjects), "subjects_%d", (int)time(NULL));
#ifdef _WIN32
    system("rmdir /s /q pieces styles collections data links materials_* subjects_* 2> nul");
#else
    system("rm -rf pieces styles collections data links materials_* subjects_*");
#endif
    MKDIR("pieces");
    MKDIR("data");
    CHECK(ursatz_gui_collection_service_init("collections", "data/corpus.db"), "init collections");
    CHECK(ursatz_gui_material_service_init(g_materials), "init materials");
    CHECK(ursatz_gui_subject_service_init(g_subjects), "init subjects");
    CHECK(ursatz_gui_link_service_init("links"), "init links");

    char a[64], b[64], c[64];
    if (import_piece("a.musicxml", a, sizeof(a)) && import_piece("b.musicxml", b, sizeof(b)) &&
        import_piece("c.musicxml", c, sizeof(c))) {
        test_list_flag(a);
        test_detail(a);
        test_backfill(a);
        test_style(b);
        test_link(c, a);
    } else {
        CHECK(false, "import the fixtures");
    }

    ursatz_gui_link_service_shutdown();
    ursatz_gui_subject_service_shutdown();
    ursatz_gui_material_service_shutdown();
    ursatz_gui_collection_service_shutdown();
#ifdef _WIN32
    system("rmdir /s /q pieces styles collections data links materials_* subjects_* 2> nul");
#else
    system("rm -rf pieces styles collections data links materials_* subjects_*");
#endif
    if (failures) {
        fprintf(stderr, "%d check(s) failed\n", failures);
        return 1;
    }
    printf("stale analysis: ok\n");
    return 0;
}
