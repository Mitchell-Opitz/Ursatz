#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_analyzer/analysis_document.h>
#include <ursatz_analyzer/analysis_pipeline.h>
#include <ursatz_analyzer/input_pipeline.h>
#include <ursatz_analyzer/notation_reading.h>

#include "cJSON.h"

/*
 * The meter check: Bach 114 as the file declares it (4/4) is offered 3/4 at scale 2; read as 3/4 at scale 2 it is
 * offered nothing; Mozart 545 is offered nothing. Clementi 36 is offered 4/4 at scale 2 (half-length bars): the check
 * is offered, never applied, and the harmony of that piece does not settle it. The claim is the first of the rhythm
 * section, has no notes, and carries the same readings as the report. The document is also validated against the
 * schema by test_analysis_document_schema.
 */

static char path_buffer[512];

static const char *musicxml(const char *name) {
    snprintf(path_buffer, sizeof path_buffer, "%s/%s", URSATZ_MUSICXML_DIR, name);
    return path_buffer;
}

typedef struct {
    ImportedNotes notes;
    AnalysisReport report;
    cJSON *document;
} Analysis;

static void analyze(const char *name, const NotationReading *reading, Analysis *analysis) {
    memset(analysis, 0, sizeof(*analysis));
    assert(input_pipeline_import_file(musicxml(name), INPUT_FORMAT_MUSICXML, &analysis->notes) == INPUT_PIPELINE_OK);
    if (reading) {
        assert(notation_reading_apply(&analysis->notes, reading) == NOTATION_READING_OK);
    }
    assert(run_analysis_pipeline(&analysis->notes, &analysis->report));
    char *json = analysis_document_render(musicxml(name), &analysis->notes, &analysis->report);
    assert(json);
    analysis->document = cJSON_Parse(json);
    free(json);
    assert(analysis->document);
}

static void analysis_destroy(Analysis *analysis) {
    cJSON_Delete(analysis->document);
    analysis_report_destroy(&analysis->report);
    imported_notes_destroy(&analysis->notes);
}

static const cJSON *member(const cJSON *object, const char *key) {
    return cJSON_GetObjectItemCaseSensitive(object, key);
}

static double number(const cJSON *object, const char *key) {
    const cJSON *item = member(object, key);
    assert(cJSON_IsNumber(item));
    return item->valuedouble;
}

/* A reading of the claim is the meter, scale and pickup it names. */
static bool reading_is(const cJSON *reading, int beats, int beat_type, double scale, double pickup) {
    const cJSON *meter = member(reading, "meter");
    return number(meter, "numerator") == beats && number(meter, "denominator") == beat_type &&
           number(member(reading, "scale"), "num") / number(member(reading, "scale"), "den") == scale &&
           number(member(reading, "pickup"), "num") / number(member(reading, "pickup"), "den") == pickup;
}

static const cJSON *claim_of(const Analysis *analysis) {
    const cJSON *rhythm = member(analysis->document, "rhythm");
    assert(cJSON_IsArray(rhythm) && cJSON_GetArraySize(rhythm) > 0);
    const cJSON *claim = cJSON_GetArrayItem(rhythm, 0);
    assert(strcmp(member(claim, "label")->valuestring, "meter-fit") == 0);
    int count = 0;
    const cJSON *other;
    cJSON_ArrayForEach(other, rhythm) {
        count += strcmp(member(other, "label")->valuestring, "meter-fit") == 0 ? 1 : 0;
    }
    assert(count == 1);
    return claim;
}

/* The claim agrees with the report and has the shape the schema asks for. */
static void check_claim(const Analysis *analysis) {
    const AnalysisMeterCheck *check = &analysis->report.meter_check;
    const cJSON *claim = claim_of(analysis);
    assert(check->present);
    assert(strcmp(member(claim, "framework")->valuestring, "standard-meter-v1") == 0);
    assert(cJSON_GetArraySize(member(claim, "note_ids")) == 0);
    assert(fabs(number(claim, "uncertainty") - check->uncertainty) < 1e-9);
    assert(check->uncertainty >= 0.0 && check->uncertainty <= 1.0);

    const cJSON *declared = member(claim, "declared");
    assert(reading_is(declared, check->effective.beats, check->effective.beat_type,
                      (double)check->effective.scale_numerator / (double)check->effective.scale_denominator,
                      (double)check->effective.pickup_numerator / (double)check->effective.pickup_denominator));
    assert(fabs(number(declared, "score") - check->effective.score) < 1e-9);
    assert(check->effective.score >= 0.0 && check->effective.score <= 1.0);

    const cJSON *alternative = member(claim, "alternative");
    if (check->has_alternative) {
        assert(cJSON_IsObject(alternative) && check->has_other);
        assert(check->other.score - check->effective.score >= 0.15 - 1e-9);
        assert(fabs(number(alternative, "score") - check->other.score) < 1e-9);
        assert(fabs(check->uncertainty - (1.0 - (check->other.score - check->effective.score))) < 1e-9);
    } else {
        assert(cJSON_IsNull(alternative));
        assert(!check->has_other || check->other.score - check->effective.score < 0.15);
    }
}

/* The file declares 4/4; the harmony is in 3/4 at twice the note values. */
static void test_bach_as_declared(void) {
    Analysis analysis;
    analyze("bach-114.musicxml", NULL, &analysis);
    check_claim(&analysis);
    const cJSON *claim = claim_of(&analysis);
    assert(reading_is(member(claim, "declared"), 4, 4, 1.0, 0.0));
    assert(reading_is(member(claim, "alternative"), 3, 4, 2.0, 0.0));
    assert(analysis.report.meter_check.other.score > analysis.report.meter_check.effective.score + 0.4);
    /* offered, never applied: the analysis still reads the file's 4/4 */
    assert(!analysis.notes.source.reading_applied);
    analysis_destroy(&analysis);
}

/* With the alternative applied the check runs on the effective meter and offers nothing. */
static void test_bach_with_reading_applied(void) {
    NotationReading reading = notation_reading_none();
    assert(notation_reading_parse_meter("3/4", &reading) && notation_reading_parse_scale("2", &reading));
    Analysis analysis;
    analyze("bach-114.musicxml", &reading, &analysis);
    check_claim(&analysis);
    const cJSON *claim = claim_of(&analysis);
    assert(reading_is(member(claim, "declared"), 3, 4, 1.0, 0.0));
    assert(cJSON_IsNull(member(claim, "alternative")));
    assert(!analysis.report.meter_check.has_alternative);
    analysis_destroy(&analysis);
}

static void test_mozart_has_no_alternative(void) {
    Analysis analysis;
    analyze("mozart-545.musicxml", NULL, &analysis);
    check_claim(&analysis);
    const cJSON *claim = claim_of(&analysis);
    assert(reading_is(member(claim, "declared"), 4, 4, 1.0, 0.0));
    assert(cJSON_IsNull(member(claim, "alternative")));
    analysis_destroy(&analysis);
}

/* The half-length bars of 4/4 are as good a reading of Clementi's harmony as its bars: offered, with a low certainty. */
static void test_clementi_alternative_is_offered_only(void) {
    Analysis analysis;
    analyze("clementi-36.musicxml", NULL, &analysis);
    check_claim(&analysis);
    const cJSON *claim = claim_of(&analysis);
    assert(reading_is(member(claim, "declared"), 4, 4, 1.0, 0.0));
    assert(analysis.report.meter_check.has_alternative);
    assert(reading_is(member(claim, "alternative"), 4, 4, 2.0, 0.0));
    assert(number(claim, "uncertainty") > 0.5);
    assert(!analysis.notes.source.reading_applied);
    analysis_destroy(&analysis);
}

/* A piece without chord segments has no claim and no meter check. */
static void test_empty_score(void) {
    ImportedNotes notes;
    if (input_pipeline_import_file(musicxml("empty-score.musicxml"), INPUT_FORMAT_MUSICXML, &notes) !=
        INPUT_PIPELINE_OK) {
        return; /* the importer refuses an empty score: nothing to check */
    }
    AnalysisReport report;
    assert(run_analysis_pipeline(&notes, &report));
    assert(!report.meter_check.present);
    analysis_report_destroy(&report);
    imported_notes_destroy(&notes);
}

int main(void) {
    test_bach_as_declared();
    test_bach_with_reading_applied();
    test_mozart_has_no_alternative();
    test_clementi_alternative_is_offered_only();
    test_empty_score();
    printf("test_meter_check: ok\n");
    return 0;
}
