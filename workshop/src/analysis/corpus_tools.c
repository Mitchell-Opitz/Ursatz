#include "analysis/corpus_tools.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_analyzer/analyzer_api.h>

#include <theory/style/style_model_file.h>

#include "base/knowledge.h"
#include <common/files.h>

typedef struct {
    cJSON *pieces;
    bool ok;
} PieceCollector;

static void collect_piece(const char *path, AnalyzerCorpusPieceStatus status, const char *error, const AnalyzerCorpusPieceReport *report,
                          void *user) {
    PieceCollector *collector = user;
    if (!collector->ok) {
        return;
    }
    const char *state = status == ANALYZER_CORPUS_PIECE_ANALYZED ? "analyzed"
                        : status == ANALYZER_CORPUS_PIECE_SKIPPED ? "skipped"
                                                                  : "failed";
    cJSON *piece = cJSON_CreateObject();
    cJSON *warnings = NULL;
    bool ok = piece && cJSON_AddStringToObject(piece, "state", state) && cJSON_AddStringToObject(piece, "path", path) &&
              (!error || cJSON_AddStringToObject(piece, "error", error)) &&
              (!report->message || cJSON_AddStringToObject(piece, "import_message", report->message)) &&
              (warnings = cJSON_AddArrayToObject(piece, "warnings")) != NULL;
    for (size_t i = 0; ok && i < report->warning_count; i++) {
        cJSON *line = cJSON_CreateString(report->warnings[i]);
        ok = line != NULL;
        if (ok) {
            cJSON_AddItemToArray(warnings, line);
        }
    }
    if (!ok) {
        cJSON_Delete(piece);
        collector->ok = false;
        return;
    }
    cJSON_AddItemToArray(collector->pieces, piece);
}

UrsatzStatus workshop_corpus_run(const char *dir, const char *out, bool resume, cJSON **result, UrsatzError *error) {
    *result = NULL;
    char *default_out = out ? NULL : common_path_join_alloc(dir, ".ursatz-corpus");
    if (!out && !default_out) {
        return ursatz_error_set(error, URSATZ_ERR_ALLOC, "", "out of memory");
    }
    cJSON *answer = cJSON_CreateObject();
    PieceCollector collector = {answer ? cJSON_AddArrayToObject(answer, "pieces") : NULL, true};
    if (!collector.pieces) {
        cJSON_Delete(answer);
        free(default_out);
        return ursatz_error_set(error, URSATZ_ERR_ALLOC, "", "out of memory");
    }
    AnalyzerCorpusOptions options = {dir, out, resume, collect_piece, &collector, workshop_knowledge_snapshot()};
    AnalyzerCorpusSummary summary;
    memset(&summary, 0, sizeof(summary));
    UrsatzStatus ran = analyzer_corpus_run(&options, &summary, error);
    if (ran == URSATZ_OK) {
        if (!collector.ok || !cJSON_AddNumberToObject(answer, "analyzed", (double)summary.analyzed) ||
            !cJSON_AddNumberToObject(answer, "skipped", (double)summary.skipped) ||
            !cJSON_AddNumberToObject(answer, "failed", (double)summary.failed) ||
            !cJSON_AddStringToObject(answer, "out", out ? out : default_out)) {
            ran = ursatz_error_set(error, URSATZ_ERR_ALLOC, "", "out of memory");
        }
    }
    free(default_out);
    if (ran != URSATZ_OK) {
        cJSON_Delete(answer);
        return ran;
    }
    *result = answer;
    return URSATZ_OK;
}

UrsatzStatus workshop_style_profile(const char *dir, const char *name, const char *model_path, cJSON **result, UrsatzError *error) {
    *result = NULL;
    StyleProfile *profile = style_profile_create();
    char *index_path = common_path_join_alloc(dir, "corpus-index.json");
    char *default_model = model_path ? NULL : common_path_join_alloc(dir, "style-model.json");
    if (!profile || !index_path || (!model_path && !default_model)) {
        style_profile_destroy(profile);
        free(index_path);
        free(default_model);
        return ursatz_error_set(error, URSATZ_ERR_ALLOC, "", "out of memory");
    }
    const char *target = model_path ? model_path : default_model;
    size_t stale = 0;
    if (analyzer_corpus_stale_count(index_path, &stale) != URSATZ_OK) {
        stale = 0; /* an unreadable index is the profile's refusal below */
    }
    free(index_path);
    UrsatzStatus status = URSATZ_OK;
    StyleProfileResult added = analyzer_style_profile_add_corpus(profile, dir);
    if (added != STYLE_PROFILE_OK) {
        status = ursatz_error_set(error, added == STYLE_PROFILE_ERR_ALLOC ? URSATZ_ERR_ALLOC : URSATZ_ERR_IO, "/params/dir",
                                  "cannot read the corpus in '%s'", dir);
    } else if (style_model_file_write(profile, name, analyzer_version(), target) != STYLE_MODEL_FILE_OK) {
        status = ursatz_error_set(error, URSATZ_ERR_IO, "/params/out", "cannot write '%s'", target);
    } else {
        cJSON *answer = cJSON_CreateObject();
        if (!answer || !cJSON_AddStringToObject(answer, "path", target) || !cJSON_AddStringToObject(answer, "name", name) ||
            !cJSON_AddNumberToObject(answer, "pieces", (double)style_profile_piece_count(profile)) ||
            !cJSON_AddNumberToObject(answer, "stale", (double)stale)) {
            cJSON_Delete(answer);
            status = ursatz_error_set(error, URSATZ_ERR_ALLOC, "", "out of memory");
        } else {
            *result = answer;
        }
    }
    style_profile_destroy(profile);
    free(default_model);
    return status;
}
