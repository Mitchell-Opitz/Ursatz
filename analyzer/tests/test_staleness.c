#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>

#include <ursatz_analyzer/analysis_document.h>
#include <ursatz_analyzer/staleness.h>
#include <ursatz_analyzer/version.h>

#include "ursatz_test.h"

/*
 * analyzer_document_status_of over each status, and analyzer_document_status over a document from the snapshots
 * (which hold no analyzer_version or producer_key; the test sets them) with each key changed in turn.
 */

/* The snapshot document with schema_version, metadata.analyzer_version and metadata.producer_key replaced. A NULL
   string leaves the member out. */
static char *variant(const char *snapshot, int schema_version, const char *analyzer, const char *commit) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", URSATZ_SNAPSHOT_DIR, snapshot);
    char *text = test_read_file(path, NULL);
    cJSON *root = text ? cJSON_Parse(text) : NULL;
    free(text);
    cJSON *metadata = cJSON_GetObjectItemCaseSensitive(root, "metadata");
    if (!cJSON_IsObject(metadata)) {
        cJSON_Delete(root);
        return NULL;
    }
    cJSON_DeleteItemFromObjectCaseSensitive(root, "schema_version");
    cJSON_AddNumberToObject(root, "schema_version", schema_version);
    cJSON_DeleteItemFromObjectCaseSensitive(metadata, "analyzer_version");
    cJSON_DeleteItemFromObjectCaseSensitive(metadata, "producer_key");
    if (analyzer) {
        cJSON_AddStringToObject(metadata, "analyzer_version", analyzer);
    }
    if (commit) {
        cJSON_AddStringToObject(metadata, "producer_key", commit);
    }
    char *printed = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return printed;
}

static AnalyzerDocumentStatus status_of_variant(const char *snapshot, int schema_version, const char *analyzer,
                                                const char *commit) {
    char *json = variant(snapshot, schema_version, analyzer, commit);
    AnalyzerDocumentStatus status = analyzer_document_status(json);
    free(json);
    return status;
}

int main(void) {
    const char *analyzer = ursatz_analyzer_version();
    const char *commit = ursatz_analyzer_producer_key();
    const int schema = URSATZ_ANALYZER_DOCUMENT_SCHEMA_VERSION;

    CHECKF(analyzer_document_status_of(schema, analyzer, commit) == ANALYZER_DOCUMENT_CURRENT, "all keys match");
    CHECKF(analyzer_document_status_of(schema - 1, analyzer, commit) == ANALYZER_DOCUMENT_STALE_SCHEMA, "old schema");
    CHECKF(analyzer_document_status_of(schema, "0.0.1", commit) == ANALYZER_DOCUMENT_STALE_ANALYZER, "old analyzer");
    CHECKF(analyzer_document_status_of(schema, analyzer, "0000") == ANALYZER_DOCUMENT_STALE_URSATZ, "other commit");
    CHECKF(analyzer_document_status_of(schema - 1, "0.0.1", "0000") == ANALYZER_DOCUMENT_STALE_SCHEMA,
           "schema is named first");
    CHECKF(analyzer_document_status_of(schema, "0.0.1", "0000") == ANALYZER_DOCUMENT_STALE_ANALYZER,
           "analyzer is named before Ursatz");
    CHECKF(analyzer_document_status_of(schema, NULL, commit) == ANALYZER_DOCUMENT_UNREADABLE, "no analyzer version");
    CHECKF(analyzer_document_status_of(schema, analyzer, NULL) == ANALYZER_DOCUMENT_UNREADABLE, "no commit");

    const char *snapshots[] = {"bach-114.analysis.json", "mozart-545.analysis.json"};
    for (size_t i = 0; i < sizeof snapshots / sizeof *snapshots; i++) {
        const char *name = snapshots[i];
        CHECKF(status_of_variant(name, schema, analyzer, commit) == ANALYZER_DOCUMENT_CURRENT, "%s: current", name);
        CHECKF(status_of_variant(name, schema - 1, analyzer, commit) == ANALYZER_DOCUMENT_STALE_SCHEMA,
               "%s: schema", name);
        CHECKF(status_of_variant(name, schema, "0.0.1", commit) == ANALYZER_DOCUMENT_STALE_ANALYZER,
               "%s: analyzer", name);
        CHECKF(status_of_variant(name, schema, analyzer, "0000") == ANALYZER_DOCUMENT_STALE_URSATZ,
               "%s: changed commit", name);
        CHECKF(status_of_variant(name, schema, NULL, commit) == ANALYZER_DOCUMENT_UNREADABLE,
               "%s: no analyzer version", name);
        CHECKF(status_of_variant(name, schema, analyzer, NULL) == ANALYZER_DOCUMENT_UNREADABLE,
               "%s: no commit", name);
    }

    CHECKF(analyzer_document_status(NULL) == ANALYZER_DOCUMENT_UNREADABLE, "NULL text");
    CHECKF(analyzer_document_status("") == ANALYZER_DOCUMENT_UNREADABLE, "empty text");
    CHECKF(analyzer_document_status("{") == ANALYZER_DOCUMENT_UNREADABLE, "not JSON");
    CHECKF(analyzer_document_status("[]") == ANALYZER_DOCUMENT_UNREADABLE, "not an object");
    CHECKF(analyzer_document_status("{\"schema_version\": 12}") == ANALYZER_DOCUMENT_UNREADABLE, "no metadata");
    CHECKF(analyzer_document_status("{\"metadata\": {}}") == ANALYZER_DOCUMENT_UNREADABLE, "no schema_version");
    CHECKF(analyzer_document_status("{\"schema_version\": \"9\", \"metadata\": {\"analyzer_version\": \"a\", "
                                    "\"producer_key\": \"b\"}}") == ANALYZER_DOCUMENT_UNREADABLE,
           "schema_version is not a number");
    CHECKF(analyzer_document_status("{\"schema_version\": 11, \"metadata\": {\"analyzer_version\": \"a\", "
                                    "\"ursatz_commit\": \"b\"}}") == ANALYZER_DOCUMENT_STALE_SCHEMA,
           "a v11 document reads stale, not unreadable");

    return test_finish("test_staleness");
}
