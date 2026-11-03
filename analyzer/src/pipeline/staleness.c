#include <ursatz_analyzer/staleness.h>

#include <string.h>

#include <cJSON.h>

#include <ursatz_analyzer/analysis_document.h>
#include <ursatz_analyzer/version.h>

AnalyzerDocumentStatus analyzer_document_status_of(int schema_version, const char *analyzer_version,
                                                   const char *ursatz_commit) {
    if (!analyzer_version || !ursatz_commit) {
        return ANALYZER_DOCUMENT_UNREADABLE;
    }
    if (schema_version != URSATZ_ANALYZER_DOCUMENT_SCHEMA_VERSION) {
        return ANALYZER_DOCUMENT_STALE_SCHEMA;
    }
    if (strcmp(analyzer_version, ursatz_analyzer_version()) != 0) {
        return ANALYZER_DOCUMENT_STALE_ANALYZER;
    }
    if (strcmp(ursatz_commit, ursatz_analyzer_ursatz_commit()) != 0) {
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
    const cJSON *commit = cJSON_GetObjectItemCaseSensitive(metadata, "ursatz_commit");
    AnalyzerDocumentStatus status = ANALYZER_DOCUMENT_UNREADABLE;
    if (cJSON_IsNumber(schema) && cJSON_IsString(analyzer) && cJSON_IsString(commit)) {
        status = analyzer_document_status_of((int)schema->valuedouble, analyzer->valuestring, commit->valuestring);
    }
    cJSON_Delete(root);
    return status;
}
