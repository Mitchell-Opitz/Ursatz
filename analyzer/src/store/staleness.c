#include <ursatz_analyzer/staleness.h>

#include <string.h>

#include <cJSON.h>

#include <ursatz_analyzer/analysis_document.h>
#include <ursatz_analyzer/version.h>

AnalyzerDocumentStatus analyzer_document_status_of(int schema_version, const char *analyzer_version,
                                                   const char *producer_key) {
    if (!analyzer_version || !producer_key) {
        return ANALYZER_DOCUMENT_UNREADABLE;
    }
    if (schema_version != URSATZ_ANALYZER_DOCUMENT_SCHEMA_VERSION) {
        return ANALYZER_DOCUMENT_STALE_SCHEMA;
    }
    if (strcmp(analyzer_version, ursatz_analyzer_version()) != 0) {
        return ANALYZER_DOCUMENT_STALE_ANALYZER;
    }
    if (strcmp(producer_key, ursatz_analyzer_producer_key()) != 0) {
        return ANALYZER_DOCUMENT_STALE_URSATZ;
    }
    return ANALYZER_DOCUMENT_CURRENT;
}

AnalyzerDocumentStatus analyzer_document_status(const char *document_json) {
    if (!document_json) {
        return ANALYZER_DOCUMENT_UNREADABLE;
    }
    cJSON *root = cJSON_Parse(document_json);
    const cJSON *schema = cJSON_GetObjectItemCaseSensitive(root, "schema_version");
    const cJSON *metadata = cJSON_GetObjectItemCaseSensitive(root, "metadata");
    const cJSON *analyzer = cJSON_GetObjectItemCaseSensitive(metadata, "analyzer_version");
    const cJSON *key = cJSON_GetObjectItemCaseSensitive(metadata, "producer_key");
    AnalyzerDocumentStatus status = ANALYZER_DOCUMENT_UNREADABLE;
    if (cJSON_IsNumber(schema) && cJSON_IsString(analyzer) && (int)schema->valuedouble != URSATZ_ANALYZER_DOCUMENT_SCHEMA_VERSION) {
        /* Another schema version is stale whatever else the document holds: a v11 document has no producer_key. */
        status = ANALYZER_DOCUMENT_STALE_SCHEMA;
    } else if (cJSON_IsNumber(schema) && cJSON_IsString(analyzer) && cJSON_IsString(key)) {
        status = analyzer_document_status_of((int)schema->valuedouble, analyzer->valuestring, key->valuestring);
    }
    cJSON_Delete(root);
    return status;
}
