#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"

#include <ursatz_analyzer/analysis_document.h>
#include <ursatz_analyzer/analysis_result.h>
#include <ursatz_analyzer/input_pipeline.h>
#include <ursatz_analyzer/markdown_report.h>
#include <ursatz_analyzer/result_chain.h>

#include <ursatz_chain/analysis_chain.h>

#include "core/entity.h"
#include "interpretation/interpretation.h"
#include "interpretation/uncertainty.h"

#include "test_support.h"

/*
 * The analysis result API (analysis_result.h) on the five reference pieces (Ursatz framework-v1 corpus):
 *   - the claims of each area against the document's entries of that area (or, where one area is several of the
 *     document's sections, against the report counts the document is built from),
 *   - every claim has a target and a scalar confidence,
 *   - a callback that refuses stops the run with ANALYZER_ERR_CANCELLED and nothing written,
 *   - a partial stage set gives the chain's claims and no export,
 *   - the document and the Markdown report of the result equal the snapshots of tests/snapshots.
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

static int failures = 0;

#define CHECK(condition, ...)                       \
    do {                                            \
        if (!(condition)) {                         \
            fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
            fprintf(stderr, __VA_ARGS__);           \
            fprintf(stderr, "\n");                  \
            failures++;                             \
        }                                           \
    } while (0)

static size_t array_length(const cJSON *root, const char *key) {
    const cJSON *array = cJSON_GetObjectItemCaseSensitive(root, key);
    return cJSON_IsArray(array) ? (size_t)cJSON_GetArraySize(array) : 0;
}

/* The chord entries of the document that carry a reading (label other than "no match") or a labelled Roman numeral. */
static size_t chord_entries(const cJSON *root, bool numerals) {
    const cJSON *chords = cJSON_GetObjectItemCaseSensitive(root, "chords");
    size_t count = 0;
    const cJSON *chord;
    cJSON_ArrayForEach(chord, chords) {
        if (numerals) {
            const cJSON *numeral = cJSON_GetObjectItemCaseSensitive(chord, "roman_numeral");
            const cJSON *reason = cJSON_GetObjectItemCaseSensitive(numeral, "unlabelled_reason");
            count += cJSON_IsObject(numeral) && (!reason || cJSON_IsNull(reason));
        } else {
            const cJSON *label = cJSON_GetObjectItemCaseSensitive(chord, "label");
            count += cJSON_IsString(label) && strcmp(label->valuestring, "no match") != 0;
        }
    }
    return count;
}

static void strip_run_metadata(cJSON *root) {
    cJSON *metadata = cJSON_GetObjectItemCaseSensitive(root, "metadata");
    if (cJSON_IsObject(metadata)) {
        cJSON_DeleteItemFromObjectCaseSensitive(metadata, "analyzer_version");
        cJSON_DeleteItemFromObjectCaseSensitive(metadata, "ursatz_commit");
        cJSON_DeleteItemFromObjectCaseSensitive(metadata, "input");
    }
}

static void check_every_claim(const char *slug, const AnalyzerResult *result) {
    for (int area = 0; area < ANALYZER_AREA_COUNT; area++) {
        size_t count = analyzer_result_claim_count(result, (AnalyzerArea)area);
        for (size_t i = 0; i < count; i++) {
            const Interpretation *claim = analyzer_result_claim(result, (AnalyzerArea)area, i);
            const EntityID *target = claim ? interpretation_target_id(claim) : NULL;
            const Uncertainty *uncertainty = claim ? interpretation_uncertainty(claim) : NULL;
            CHECK(claim != NULL, "%s: area %d claim %zu is NULL", slug, area, i);
            CHECK(target && entity_id_value(target) && entity_id_value(target)[0] != '\0',
                  "%s: area %d claim %zu has no target", slug, area, i);
            CHECK(uncertainty && uncertainty_kind(uncertainty) == UNCERTAINTY_KIND_SCALAR_CONFIDENCE,
                  "%s: area %d claim %zu has no scalar confidence", slug, area, i);
        }
        CHECK(analyzer_result_claim(result, (AnalyzerArea)area, count) == NULL, "%s: area %d claim past the count", slug,
              area);
    }
}

static void check_counts(const char *slug, const AnalyzerResult *result, const cJSON *document) {
    const AnalysisReport *report = analyzer_result_report(result);
    const AnalysisChain *chain = analyzer_result_chain(result);
    size_t claims[ANALYZER_AREA_COUNT];
    for (int area = 0; area < ANALYZER_AREA_COUNT; area++) {
        claims[area] = analyzer_result_claim_count(result, (AnalyzerArea)area);
    }
    const cJSON *keys = cJSON_GetObjectItemCaseSensitive(document, "keys");
    CHECK(claims[ANALYZER_AREA_KEYS] == array_length(keys, "regions"), "%s: key claims %zu, document %zu", slug,
          claims[ANALYZER_AREA_KEYS], array_length(keys, "regions"));
    CHECK(claims[ANALYZER_AREA_CHORDS] == chord_entries(document, false), "%s: chord claims %zu, document %zu", slug,
          claims[ANALYZER_AREA_CHORDS], chord_entries(document, false));
    CHECK(claims[ANALYZER_AREA_NUMERALS] == chord_entries(document, true), "%s: numeral claims %zu, document %zu", slug,
          claims[ANALYZER_AREA_NUMERALS], chord_entries(document, true));
    CHECK(claims[ANALYZER_AREA_CADENCES] == array_length(document, "cadences"), "%s: cadence claims %zu, document %zu",
          slug, claims[ANALYZER_AREA_CADENCES], array_length(document, "cadences"));
    CHECK(claims[ANALYZER_AREA_PHRASES] == array_length(document, "phrases"), "%s: phrase claims %zu, document %zu", slug,
          claims[ANALYZER_AREA_PHRASES], array_length(document, "phrases"));
    CHECK(claims[ANALYZER_AREA_THEMES] == array_length(document, "themes"), "%s: theme claims %zu, document %zu", slug,
          claims[ANALYZER_AREA_THEMES], array_length(document, "themes"));
    CHECK(claims[ANALYZER_AREA_MOTIVES] == array_length(document, "motives"), "%s: motive claims %zu, document %zu", slug,
          claims[ANALYZER_AREA_MOTIVES], array_length(document, "motives"));
    CHECK(claims[ANALYZER_AREA_TEXTURE] == array_length(document, "texture"), "%s: texture claims %zu, document %zu", slug,
          claims[ANALYZER_AREA_TEXTURE], array_length(document, "texture"));
    CHECK(claims[ANALYZER_AREA_SCHEMAS] == array_length(document, "schemas"), "%s: schema claims %zu, document %zu", slug,
          claims[ANALYZER_AREA_SCHEMAS], array_length(document, "schemas"));
    CHECK(claims[ANALYZER_AREA_PROGRESSIONS] == array_length(document, "progressions"),
          "%s: progression claims %zu, document %zu", slug, claims[ANALYZER_AREA_PROGRESSIONS],
          array_length(document, "progressions"));

    /* The document's rhythm section holds the meter check, the densities, syncopations, hemiolas, hypermeter and the
       tuplets; the result splits it into the METER and RHYTHM areas. */
    const AnalysisChainMeter *meter = analysis_chain_meter(chain);
    size_t meter_claims = (report->meter_check.present ? 1 : 0) + report->density_count + report->syncopation_count +
                          report->hemiola_count + (meter ? meter->hypermeter.hypermeasure_count : 0) +
                          (meter ? meter->hypermeter.reinterpretation_count : 0);
    CHECK(claims[ANALYZER_AREA_METER] == meter_claims, "%s: meter claims %zu, report %zu", slug,
          claims[ANALYZER_AREA_METER], meter_claims);
    CHECK(claims[ANALYZER_AREA_RHYTHM] == report->tuplet_count, "%s: rhythm claims %zu, tuplets %zu", slug,
          claims[ANALYZER_AREA_RHYTHM], report->tuplet_count);

    /* The voice-leading and melody claims are the chain's own results; the document aggregates them. */
    const AnalysisChainVoiceLeading *leading = analysis_chain_voice_leading(chain);
    size_t voice_claims = leading ? leading->motions.count + leading->perfects.count + leading->resolutions.count : 0;
    CHECK(claims[ANALYZER_AREA_VOICE_LEADING] == voice_claims, "%s: voice-leading claims %zu, chain %zu", slug,
          claims[ANALYZER_AREA_VOICE_LEADING], voice_claims);
    CHECK(leading && leading->perfects.count == report->voice_leading.violation_count,
          "%s: violations %zu, report %zu", slug, leading ? leading->perfects.count : 0,
          report->voice_leading.violation_count);
    const AnalysisChainMelody *melody = analysis_chain_melody(chain);
    size_t melody_claims = melody ? melody->intervals.count + melody->summaries.count + melody->climaxes.count +
                                        melody->recoveries.count
                                  : 0;
    CHECK(claims[ANALYZER_AREA_MELODY] == melody_claims, "%s: melody claims %zu, chain %zu", slug,
          claims[ANALYZER_AREA_MELODY], melody_claims);
    CHECK(array_length(document, "voice_leading") > 0 || voice_claims == 0, "%s: no voice-leading section", slug);
    CHECK(array_length(document, "melody") > 0 || melody_claims == 0, "%s: no melody section", slug);
}

static void check_snapshots(const char *slug, const char *fixture, const AnalyzerResult *result) {
    char path[512];

    char *json = analysis_document_render_result(fixture, result);
    CHECK(json != NULL, "%s: no document from the result", slug);
    if (json) {
        cJSON *root = cJSON_Parse(json);
        CHECK(root != NULL, "%s: the document does not parse", slug);
        if (root) {
            strip_run_metadata(root);
            char *text = cJSON_Print(root);
            snprintf(path, sizeof(path), "%s/%s.analysis.json", URSATZ_SNAPSHOT_DIR, slug);
            char *expected = test_read_file(path);
            CHECK(expected && text && strcmp(expected, text) == 0, "%s: the document differs from %s", slug, path);
            free(expected);
            cJSON_free(text);
            cJSON_Delete(root);
        }
        free(json);
    }

    char *markdown = markdown_report_render_result(fixture, result);
    CHECK(markdown != NULL, "%s: no report from the result", slug);
    if (markdown) {
        snprintf(path, sizeof(path), "%s/%s.report.md", URSATZ_SNAPSHOT_DIR, slug);
        char *expected = test_read_file(path);
        CHECK(expected && strcmp(expected, markdown) == 0, "%s: the report differs from %s", slug, path);
        free(expected);
        free(markdown);
    }
}

static void check_piece(const char *fixture, const char *slug) {
    char path[512];
    snprintf(path, sizeof(path), "%s/framework-v1-corpus/%s", URSATZ_ANALYZER_FIXTURE_DIR, fixture);
    ImportedNotes notes;
    if (input_pipeline_import_file(path, INPUT_FORMAT_AUTO, &notes) != INPUT_PIPELINE_OK) {
        CHECK(false, "%s: cannot import %s", slug, path);
        return;
    }

    AnalyzerResult *result = NULL;
    AnalyzerStatus status = analyzer_analyze(&notes, NULL, &result);
    CHECK(status == ANALYZER_OK && result != NULL, "%s: analyze returned %d", slug, (int)status);
    if (result) {
        check_every_claim(slug, result);
        const AnalysisReport *report = analyzer_result_report(result);
        CHECK(report && report->entry_count > 0, "%s: empty report view", slug);

        char *json = analysis_document_render_result(fixture, result);
        cJSON *document = json ? cJSON_Parse(json) : NULL;
        CHECK(document != NULL, "%s: no document", slug);
        if (document) {
            check_counts(slug, result, document);
            cJSON_Delete(document);
        }
        free(json);
        check_snapshots(slug, fixture, result);
    }
    analyzer_result_destroy(result);
    imported_notes_destroy(&notes);
}

typedef struct {
    int calls;
    int refuse_from; /* the call number (from 1) at which the callback refuses */
} CancelState;

static bool cancel_callback(void *context) {
    CancelState *state = context;
    state->calls++;
    return state->calls >= state->refuse_from;
}

static void check_options(const char *fixture) {
    char path[512];
    snprintf(path, sizeof(path), "%s/framework-v1-corpus/%s", URSATZ_ANALYZER_FIXTURE_DIR, fixture);
    ImportedNotes notes;
    if (input_pipeline_import_file(path, INPUT_FORMAT_AUTO, &notes) != INPUT_PIPELINE_OK) {
        CHECK(false, "cannot import %s", path);
        return;
    }

    /* The defaults: all stages, no callback. */
    AnalyzerOptions defaults = analyzer_options_default();
    CHECK(defaults.stages == ANALYZER_STAGE_ALL && defaults.cancel == NULL && defaults.cancel_context == NULL,
          "the default options are not all stages without a callback");

    /* A callback that refuses at once: cancelled before the chain run, nothing written. */
    for (int refuse_from = 1; refuse_from <= 4; refuse_from++) {
        CancelState state = {0, refuse_from};
        AnalyzerOptions options = analyzer_options_default();
        options.cancel = cancel_callback;
        options.cancel_context = &state;
        AnalyzerResult *result = (AnalyzerResult *)0x1;
        AnalyzerStatus status = analyzer_analyze(&notes, &options, &result);
        CHECK(status == ANALYZER_ERR_CANCELLED, "refusal at call %d returned %d", refuse_from, (int)status);
        CHECK(result == NULL, "refusal at call %d left a result", refuse_from);
        CHECK(state.calls == refuse_from, "refusal at call %d made %d calls", refuse_from, state.calls);
    }

    /* A callback that never refuses is asked before the chain and between the Analyzer stages. */
    CancelState never = {0, 1 << 30};
    AnalyzerOptions asked = analyzer_options_default();
    asked.cancel = cancel_callback;
    asked.cancel_context = &never;
    AnalyzerResult *result = NULL;
    CHECK(analyzer_analyze(&notes, &asked, &result) == ANALYZER_OK && result, "a callback that accepts stopped the run");
    CHECK(never.calls > 1, "the callback was asked %d times", never.calls);
    analyzer_result_destroy(result);

    /* Arguments. */
    result = (AnalyzerResult *)0x1;
    CHECK(analyzer_analyze(NULL, NULL, &result) == ANALYZER_ERR_ARGUMENT && result == NULL, "NULL notes");
    CHECK(analyzer_analyze(&notes, NULL, NULL) == ANALYZER_ERR_ARGUMENT, "NULL out");
    AnalyzerOptions none = analyzer_options_default();
    none.stages = 0;
    CHECK(analyzer_analyze(&notes, &none, &result) == ANALYZER_ERR_ARGUMENT && result == NULL, "no stages");
    analyzer_result_destroy(NULL);
    CHECK(analyzer_result_claim_count(NULL, ANALYZER_AREA_KEYS) == 0, "claim count of NULL");
    CHECK(analyzer_result_claim(NULL, ANALYZER_AREA_KEYS, 0) == NULL, "claim of NULL");
    CHECK(analyzer_result_chain(NULL) == NULL && analyzer_result_report(NULL) == NULL, "chain or report of NULL");

    /* A partial stage set: the chain's claims, an empty report view, no export. */
    AnalyzerOptions texture = analyzer_options_default();
    texture.stages = ANALYSIS_CHAIN_STAGE_TEXTURE;
    result = NULL;
    CHECK(analyzer_analyze(&notes, &texture, &result) == ANALYZER_OK && result, "texture stage only");
    if (result) {
        CHECK(analyzer_result_claim_count(result, ANALYZER_AREA_TEXTURE) > 0, "texture claims from the texture stage");
        CHECK(analyzer_result_claim_count(result, ANALYZER_AREA_CADENCES) == 0, "cadence claims without the stage");
        CHECK(analyzer_result_claim_count(result, ANALYZER_AREA_PROGRESSIONS) == 0, "progression claims without the report");
        CHECK(analyzer_result_report(result)->entry_count == 0, "report entries from a partial chain");
        CHECK(analysis_document_build_from_result(fixture, result) == NULL, "a document from a partial chain");
        CHECK(markdown_report_render_result(fixture, result) == NULL, "a report from a partial chain");
    }
    analyzer_result_destroy(result);
    imported_notes_destroy(&notes);
}

int main(int argc, char **argv) {
    bool smoke = argc > 1 && strcmp(argv[1], "smoke") == 0;
    size_t count = smoke ? 1 : sizeof(PIECES) / sizeof(PIECES[0]);
    for (size_t i = 0; i < count; i++) {
        check_piece(PIECES[i].fixture, PIECES[i].slug);
        if (i == 0) {
            check_options(PIECES[i].fixture);
        }
    }
    if (failures == 0) {
        printf("analysis result: ok\n");
    }
    return failures == 0 ? 0 : 1;
}
