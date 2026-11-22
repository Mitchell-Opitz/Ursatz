#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"

#include <ursatz_analyzer_internal/analysis_document.h>
#include <ursatz_analyzer_internal/analysis_result.h>
#include <ursatz_analyzer_internal/input_pipeline.h>
#include "result/analysis_result_internal.h"

#include <chain/analysis_chain.h>
#include <io/imported_score.h>
#include <realization/work.h>
#include <reading/reading.h>

#include "test_support.h"
#include "ursatz_test.h"
#include "test_knowledge.h"

/*
 * The analysis result API (analysis_result.h) on the five reference pieces (Ursatz framework-v1 corpus):
 *   - the chain's voice-leading and melody results against the report and the document's sections,
 *   - a partial stage set gives the chain's results of those stages and no export,
 *   - the document of the result equals the snapshots of tests/snapshots.
 *
 *   test_analysis_result          the five pieces
 *   test_analysis_result smoke    Bach 114 only (the valgrind run)
 */

static const struct {
    const char *fixture;
    const char *slug;
} PIECES[] = {
    {"Bach 114.mid", "bach-114"}, {"Mozart 545.mid", "mozart-545"},   {"Clementi 36.mid", "clementi-36"},
    {"Field 1.mid", "field-1"},   {"Field 5.mid", "field-5"},
};

static size_t array_length(const cJSON *root, const char *key) {
    const cJSON *array = cJSON_GetObjectItemCaseSensitive(root, key);
    return cJSON_IsArray(array) ? (size_t)cJSON_GetArraySize(array) : 0;
}

static void strip_run_metadata(cJSON *root) {
    cJSON *metadata = cJSON_GetObjectItemCaseSensitive(root, "metadata");
    if (cJSON_IsObject(metadata)) {
        cJSON_DeleteItemFromObjectCaseSensitive(metadata, "analyzer_version");
        cJSON_DeleteItemFromObjectCaseSensitive(metadata, "producer_key");
        cJSON_DeleteItemFromObjectCaseSensitive(metadata, "input");
    }
}

static void check_chain_against_document(const char *slug, const AnalyzerResult *result, const cJSON *document) {
    const ChainResult *report = analyzer_result_report(result);
    const AnalysisChain *chain = analyzer_result_run_chain(result);

    /* The voice-leading and melody results are the chain's own; the document aggregates them. */
    const AnalysisChainVoiceLeading *leading = analysis_chain_voice_leading(chain);
    size_t voice_results = leading ? leading->motions.count + leading->perfects.count + leading->resolutions.count : 0;
    CHECKF(leading && leading->perfects.count == report->voice_leading.violation_count,
           "%s: violations %zu, report %zu", slug, leading ? leading->perfects.count : 0,
           report->voice_leading.violation_count);
    const AnalysisChainMelody *melody = analysis_chain_melody(chain);
    size_t melody_results = melody ? melody->intervals.count + melody->summaries.count + melody->climaxes.count +
                                         melody->recoveries.count
                                   : 0;
    CHECKF(array_length(document, "voice_leading") > 0 || voice_results == 0, "%s: no voice-leading section", slug);
    CHECKF(array_length(document, "melody") > 0 || melody_results == 0, "%s: no melody section", slug);
}

static void check_snapshots(const char *slug, const char *fixture, const AnalyzerResult *result) {
    char path[512];

    char *json = analysis_document_render_result(fixture, result);
    CHECKF(json != NULL, "%s: no document from the result", slug);
    if (json) {
        cJSON *root = cJSON_Parse(json);
        CHECKF(root != NULL, "%s: the document does not parse", slug);
        if (root) {
            strip_run_metadata(root);
            char *text = cJSON_Print(root);
            snprintf(path, sizeof(path), "%s/%s.analysis.json", URSATZ_SNAPSHOT_DIR, slug);
            char *expected = test_read_file(path, NULL);
            CHECKF(expected && text && strcmp(expected, text) == 0, "%s: the document differs from %s", slug, path);
            free(expected);
            cJSON_free(text);
            cJSON_Delete(root);
        }
        free(json);
    }
}

static void check_piece(const char *fixture, const char *slug) {
    char path[512];
    snprintf(path, sizeof(path), "%s/framework-v1-corpus/%s", URSATZ_FIXTURES_DIR, fixture);
    ImportedScore notes;
    if (imported_score_import_file(path, IMPORTED_FORMAT_AUTO, &notes, NULL) != IMPORTED_SCORE_OK) {
        CHECKF(false, "%s: cannot import %s", slug, path);
        return;
    }

    AnalyzerResult *result = NULL;
    AnalyzerOptions options = test_analyzer_options();
    UrsatzStatus status = analyzer_analyze(&notes, &options, &result);
    CHECKF(status == URSATZ_OK && result != NULL, "%s: analyze returned %d", slug, (int)status);
    if (result) {
        const ChainResult *report = analyzer_result_report(result);
        CHECKF(report && report->entry_count > 0, "%s: empty report view", slug);

        char *json = analysis_document_render_result(fixture, result);
        cJSON *document = json ? cJSON_Parse(json) : NULL;
        CHECKF(document != NULL, "%s: no document", slug);
        if (document) {
            check_chain_against_document(slug, result, document);
            cJSON_Delete(document);
        }
        free(json);
        check_snapshots(slug, fixture, result);
    }
    analyzer_result_destroy(result);
    imported_score_destroy(&notes);
}

static void check_options(const char *fixture) {
    char path[512];
    snprintf(path, sizeof(path), "%s/framework-v1-corpus/%s", URSATZ_FIXTURES_DIR, fixture);
    ImportedScore notes;
    if (imported_score_import_file(path, IMPORTED_FORMAT_AUTO, &notes, NULL) != IMPORTED_SCORE_OK) {
        CHECKF(false, "cannot import %s", path);
        return;
    }

    /* The defaults: all stages, no knowledge. */
    AnalyzerOptions defaults = analyzer_options_default();
    CHECKF(defaults.stages == ANALYSIS_CHAIN_STAGE_ALL && defaults.knowledge == NULL,
           "the default options are not all stages with no knowledge");

    /* Arguments. */
    AnalyzerResult *result = (AnalyzerResult *)0x1;
    CHECKF(analyzer_analyze(NULL, NULL, &result) == URSATZ_ERR_REQUEST && result == NULL, "NULL notes");
    AnalyzerOptions with_knowledge = test_analyzer_options();
    CHECKF(analyzer_analyze(&notes, &with_knowledge, NULL) == URSATZ_ERR_REQUEST, "NULL out");
    CHECKF(analyzer_analyze(&notes, NULL, &result) == URSATZ_ERR_REQUEST && result == NULL, "no options, no knowledge");
    AnalyzerOptions no_knowledge = analyzer_options_default();
    CHECKF(analyzer_analyze(&notes, &no_knowledge, &result) == URSATZ_ERR_REQUEST && result == NULL, "no knowledge");
    AnalyzerOptions none = test_analyzer_options();
    none.stages = 0;
    CHECKF(analyzer_analyze(&notes, &none, &result) == URSATZ_ERR_REQUEST && result == NULL, "no stages");
    analyzer_result_destroy(NULL);
    CHECKF(analyzer_result_run_chain(NULL) == NULL && analyzer_result_report(NULL) == NULL, "chain or report of NULL");

    /* A partial stage set: the chain's results, an empty report view, no export. */
    AnalyzerOptions texture = test_analyzer_options();
    texture.stages = ANALYSIS_CHAIN_STAGE_TEXTURE;
    result = NULL;
    CHECKF(analyzer_analyze(&notes, &texture, &result) == URSATZ_OK && result, "texture stage only");
    if (result) {
        const AnalysisChain *chain = analyzer_result_run_chain(result);
        CHECKF(analysis_chain_texture(chain) != NULL, "texture results from the texture stage");
        CHECKF(analysis_chain_cadences(chain) == NULL, "cadence results without the stage");
        CHECKF(analyzer_result_report(result)->progression_count == 0, "progressions without the report");
        CHECKF(analyzer_result_report(result)->entry_count == 0, "report entries from a partial chain");
        CHECKF(analyzer_document_build(fixture, result) == NULL, "a document from a partial chain");
    }
    analyzer_result_destroy(result);
    imported_score_destroy(&notes);
}

/* The layers of a result (R-628, R-630) and the run of one file (R-656), on Bach 114 with all stages. */
static void check_layers_and_file_runs(const char *fixture, const KnowledgeSnapshot *knowledge) {
    char path[512];
    snprintf(path, sizeof(path), "%s/framework-v1-corpus/%s", URSATZ_FIXTURES_DIR, fixture);
    ImportedScore notes;
    if (imported_score_import_file(path, IMPORTED_FORMAT_AUTO, &notes, NULL) != IMPORTED_SCORE_OK) {
        CHECKF(false, "cannot import %s", path);
        return;
    }
    AnalyzerOptions options = test_analyzer_options();
    AnalyzerResult *result = NULL;
    CHECKF(analyzer_analyze(&notes, &options, &result) == URSATZ_OK && result, "analyze for the layers");
    if (result) {
        /* The realization names its work "w1" with one movement; the reading is the Analyzer's, of that work and movement, over every stage. */
        CHECKF(analyzer_result_realization(NULL) == NULL && analyzer_result_reading(NULL) == NULL, "layers of NULL");
        const Work *work = analyzer_result_realization(result);
        const Reading *reading = analyzer_result_reading(result);
        CHECKF(work && reading, "no realization or reading");
        if (work && reading) {
            CHECKF(strcmp(work_id(work), "w1") == 0 && work_movement_count(work) == 1, "the work is %s with %zu movements", work_id(work),
                   work_movement_count(work));
            const ReadingTarget *target = reading_target(reading);
            const ReadingSource *source = reading_source(reading);
            CHECKF(strcmp(target->work, "w1") == 0 && strcmp(target->movement, "m1") == 0 && strlen(target->digest) == 16,
                   "the reading's target is %s %s %s", target->work, target->movement, target->digest);
            for (const char *digit = target->digest; *digit; digit++) {
                CHECKF((*digit >= '0' && *digit <= '9') || (*digit >= 'a' && *digit <= 'f'), "digest digit %c", *digit);
            }
            CHECKF(source->kind == READING_SOURCE_ANALYZER && strcmp(source->name, "ursatz-analyzer") == 0 &&
                       source->stages == ANALYSIS_CHAIN_STAGE_ALL,
                   "the reading's source");
        }
    }
    analyzer_result_destroy(result);
    imported_score_destroy(&notes);

    /* One file: imported, analyzed with all stages, the result over the caller's notes, and the document of it. */
    ImportedScore run_notes;
    AnalyzerResult *run_result = NULL;
    ImportedScoreResult import_status = IMPORTED_SCORE_ERR_ARGUMENT;
    ImportedReadingResult reading_status = IMPORTED_READING_OK;
    UrsatzStatus analyze_status = URSATZ_OK;
    UrsatzError error;
    memset(&error, 0, sizeof(error));
    CHECKF(analyzer_run_file(path, IMPORTED_FORMAT_AUTO, NULL, knowledge, &run_notes, &run_result, &import_status, &reading_status,
                             &analyze_status, &error) == ANALYZER_RUN_FILE_OK && run_result,
           "run file");
    if (run_result) {
        CHECKF(import_status == IMPORTED_SCORE_OK && run_notes.count > 0 && analyzer_result_report(run_result) != NULL, "the run's result");
        AnalysisDocument *document = analyzer_run_file_document(path, &run_notes, run_result);
        CHECKF(document != NULL, "the run's document");
        if (document) {
            char *json = analyzer_document_to_json(document);
            CHECKF(json && strncmp(json, "{\n", 2) == 0 && strncmp(json + 2 + strspn(json + 2, " \t"), "\"schema_version\"", 16) == 0,
                   "the document starts with its schema version");
            free(json);
            analyzer_document_destroy(document);
        }
    }

    /* An import that fails: the importer's status, and its message with the file as its path (URSATZ_ERR_FORMAT). */
    snprintf(path, sizeof(path), "%s/musicxml/malformed.musicxml", URSATZ_FIXTURES_DIR);
    memset(&error, 0, sizeof(error));
    CHECKF(analyzer_run_file(path, IMPORTED_FORMAT_AUTO, NULL, knowledge, &run_notes, &run_result, &import_status, &reading_status,
                             &analyze_status, &error) == ANALYZER_RUN_FILE_ERR_IMPORT,
           "a malformed file");
    CHECKF(import_status != IMPORTED_SCORE_OK && error.message[0] != '\0' && strcmp(error.path, path) == 0, "the import's error");

    /* A reading that does not hold (a scale of 3): the reading's status, and the notes are released. */
    snprintf(path, sizeof(path), "%s/framework-v1-corpus/%s", URSATZ_FIXTURES_DIR, fixture);
    ImportedReading reading = imported_reading_none();
    reading.scale = rational_ratio(3, 1);
    CHECKF(analyzer_run_file(path, IMPORTED_FORMAT_AUTO, &reading, knowledge, &run_notes, &run_result, &import_status, &reading_status,
                             &analyze_status, &error) == ANALYZER_RUN_FILE_ERR_READING,
           "a reading that is invalid");
    CHECKF(reading_status == IMPORTED_READING_ERR_INVALID, "the reading's status %d", (int)reading_status);

    /* No knowledge: the analysis is refused as a bad request. */
    CHECKF(analyzer_run_file(path, IMPORTED_FORMAT_AUTO, NULL, NULL, &run_notes, &run_result, &import_status, &reading_status,
                             &analyze_status, &error) == ANALYZER_RUN_FILE_ERR_ANALYZE,
           "no knowledge");
    CHECKF(analyze_status == URSATZ_ERR_REQUEST, "the analysis status %d", (int)analyze_status);
}

int main(int argc, char **argv) {
    bool smoke = argc > 1 && strcmp(argv[1], "smoke") == 0;
    size_t count = smoke ? 1 : sizeof(PIECES) / sizeof(PIECES[0]);
    for (size_t i = 0; i < count; i++) {
        check_piece(PIECES[i].fixture, PIECES[i].slug);
        if (i == 0) {
            check_options(PIECES[i].fixture);
            check_layers_and_file_runs(PIECES[i].fixture, test_analyzer_options().knowledge);
        }
    }
    return test_finish("test_analysis_result");
}
