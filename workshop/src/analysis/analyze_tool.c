#include "analysis/analyze_tool.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_analyzer/analyzer_api.h>

#include <io/imported_score.h>
#include <plan/plan_json.h>
#include <reading/reading_json.h>
#include <text/text_view.h>
#include <reading/reading_plan.h>
#include <realization/realization_json.h>

#include "base/knowledge.h"

/* The minimal model composition of the Analyzer's reading (R-635): the plan its sections, keys and chords give, as the text of a plan file. */
static UrsatzStatus model_plan_text(const AnalyzerResult *result, const KnowledgeSnapshot *knowledge, char **out, UrsatzError *error) {
    const Work *work = analyzer_result_realization(result);
    Plan *plan = NULL;
    ReadingPlanError plan_error;
    memset(&plan_error, 0, sizeof(plan_error));
    if (reading_to_plan(analyzer_result_reading(result), work_movement_at(work, 0), knowledge, &plan, &plan_error) != REALIZATION_OK) {
        return ursatz_error_set(error, URSATZ_ERR_UNSATISFIABLE, "", "%s", plan_error.message);
    }
    PlanProducer producer;
    memset(&producer, 0, sizeof(producer));
    snprintf(producer.name, sizeof(producer.name), "%s", "ursatz-analyzer");
    snprintf(producer.version, sizeof(producer.version), "%s", analyzer_version());
    snprintf(producer.producer_key, sizeof(producer.producer_key), "%s", analyzer_producer_key());
    snprintf(producer.knowledge_hash, sizeof(producer.knowledge_hash), "%s", knowledge_snapshot_hash(knowledge));
    PlanJsonError json_error;
    *out = plan_to_json(plan, &producer, &json_error);
    plan_destroy(plan);
    return *out ? URSATZ_OK : ursatz_error_set(error, URSATZ_ERR_INTERNAL, "", "cannot write the plan of the analysis");
}

static bool add_text(cJSON *object, const char *name, char *text) {
    bool ok = text && cJSON_AddStringToObject(object, name, text);
    free(text);
    return ok;
}

UrsatzStatus workshop_analyze(const WorkshopAnalyzeRequest *request, cJSON **out, UrsatzError *error) {
    *out = NULL;
    const KnowledgeSnapshot *knowledge = workshop_knowledge_snapshot();
    ImportedFormat format = IMPORTED_FORMAT_AUTO;
    if (request->input_format && !imported_format_parse(request->input_format, &format)) {
        return ursatz_error_set(error, URSATZ_ERR_REQUEST, "/params/input_format", "input_format is auto, midi or musicxml");
    }
    if (format != IMPORTED_FORMAT_AUTO && format != IMPORTED_FORMAT_MIDI && format != IMPORTED_FORMAT_MUSICXML) {
        return ursatz_error_set(error, URSATZ_ERR_REQUEST, "/params/input_format", "input_format is auto, midi or musicxml");
    }
    ImportedReading reading = imported_reading_none();
    bool reading_set = false;
    if (request->meter) {
        if (!imported_reading_parse_meter(request->meter, &reading)) {
            return ursatz_error_set(error, URSATZ_ERR_REQUEST, "/params/meter", "'%s' is not a meter (3/4, 6/8)", request->meter);
        }
        reading_set = true;
    }
    if (request->value_scale) {
        if (!imported_reading_parse_scale(request->value_scale, &reading)) {
            return ursatz_error_set(error, URSATZ_ERR_REQUEST, "/params/value_scale", "'%s' is not a value scale (2, 1/2)", request->value_scale);
        }
        reading_set = true;
    }
    if (request->pickup) {
        if (!imported_reading_parse_pickup(request->pickup, &reading)) {
            return ursatz_error_set(error, URSATZ_ERR_REQUEST, "/params/pickup", "'%s' is not a pickup (1/4)", request->pickup);
        }
        reading_set = true;
    }

    ImportedScore notes;
    AnalyzerResult *result = NULL;
    UrsatzError import_error = {{0}, {0}};
    ImportedScoreResult import_status = IMPORTED_SCORE_OK;
    ImportedReadingResult reading_status = IMPORTED_READING_OK;
    UrsatzStatus analyze_status = URSATZ_OK;
    switch (analyzer_run_file(request->path, format, reading_set ? &reading : NULL, knowledge, &notes, &result, &import_status,
                              &reading_status, &analyze_status, &import_error)) {
    case ANALYZER_RUN_FILE_OK:
        break;
    case ANALYZER_RUN_FILE_ERR_IMPORT:
        return ursatz_error_set(error, URSATZ_ERR_FORMAT, "/params/path", "%s", import_error.message);
    case ANALYZER_RUN_FILE_ERR_READING:
        if (reading_status == IMPORTED_READING_ERR_INVALID) {
            return ursatz_error_set(error, URSATZ_ERR_REQUEST, "/params/pickup", "the pickup must be shorter than one measure of the meter");
        }
        if (reading_status == IMPORTED_READING_ERR_OVERFLOW) {
            return ursatz_error_set(error, URSATZ_ERR_REQUEST, "/params/value_scale", "a scaled time is too large");
        }
        return ursatz_error_set(error, URSATZ_ERR_ALLOC, "", "out of memory");
    default:
        return ursatz_error_set(error, analyze_status == URSATZ_ERR_ALLOC ? URSATZ_ERR_ALLOC : URSATZ_ERR_INTERNAL, "", "the analysis failed");
    }

    cJSON *object = cJSON_CreateObject();
    cJSON *warnings = object ? cJSON_AddArrayToObject(object, "warnings") : NULL;
    bool ok = warnings != NULL;
    for (size_t i = 0; ok && i < notes.source.warning_line_count; i++) {
        cJSON *line = cJSON_CreateString(notes.source.warnings[i]);
        ok = line != NULL;
        if (ok) {
            cJSON_AddItemToArray(warnings, line);
        }
    }
    UrsatzStatus status = URSATZ_OK;
    if (ok && request->layers) {
        ok = add_text(object, "realization", realization_work_to_json(analyzer_result_realization(result), NULL)) &&
             add_text(object, "reading", reading_to_json(analyzer_result_reading(result), NULL));
    }
    if (ok && request->model_plan) {
        char *plan = NULL;
        status = model_plan_text(result, knowledge, &plan, error);
        if (status == URSATZ_OK) {
            ok = add_text(object, "plan", plan);
        }
    }
    if (ok && status == URSATZ_OK && request->text) {
        AnalysisDocument *document = analyzer_document_build(request->path, result);
        char *json = document ? analyzer_document_to_json(document) : NULL;
        analyzer_document_destroy(document);
        analyzer_result_destroy(result);
        result = NULL;
        if (!json) {
            ok = false;
        } else {
            AnalyzerTextExportOptions options = analyzer_text_export_options_default();
            options.first_measure = request->first_measure;
            options.last_measure = request->last_measure;
            options.title = request->title;
            char *text = NULL;
            UrsatzError export_error = {{0}, {0}};
            UrsatzStatus exported = analyzer_text_export_piece(&notes, json, &options, &text, &export_error);
            free(json);
            if (exported == URSATZ_OK) {
                /* the export is the reading view of the file, framed as every view is (R-663) */
                char expand[512];
                snprintf(expand, sizeof(expand), "ursatz analyze %s --view reading --measures a-b", request->path);
                char *framed = text ? text_view_frame("reading", request->path, text, "nothing the analysis document holds; the notes are the import's", expand)
                                    : NULL;
                free(text);
                ok = add_text(object, "text", framed);
            } else if (exported == URSATZ_ERR_REQUEST) {
                status = ursatz_error_set(error, URSATZ_ERR_REQUEST, "/params/text", "no measure of '%s' falls in the range given", request->path);
            } else if (exported == URSATZ_ERR_INTERNAL) {
                status = ursatz_error_set(error, URSATZ_ERR_INTERNAL, "", "cannot write the score of '%s' as MusicXML to export it", request->path);
            } else {
                ok = false;
            }
        }
        analyzer_result_destroy(result);
        result = NULL;
        imported_score_destroy(&notes);
    } else if (result) {
        /* The document: built from the result, which releases it and the notes. */
        AnalysisDocument *document = analyzer_run_file_document(request->path, &notes, result);
        char *json = document ? analyzer_document_to_json(document) : NULL;
        analyzer_document_destroy(document);
        char *path = analyzer_document_path_for(request->path);
        if (status == URSATZ_OK) {
            ok = ok && json && path && cJSON_AddStringToObject(object, "document", json) && cJSON_AddStringToObject(object, "document_path", path);
        }
        free(json);
        free(path);
    }
    if (status != URSATZ_OK || !ok) {
        cJSON_Delete(object);
        return status != URSATZ_OK ? status : ursatz_error_set(error, URSATZ_ERR_ALLOC, "", "out of memory");
    }
    *out = object;
    return URSATZ_OK;
}
