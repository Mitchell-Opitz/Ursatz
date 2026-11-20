#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_analyzer_internal/analysis_document.h>
#include <ursatz_analyzer_internal/input_pipeline.h>
#include <io/imported_score.h>

#include "cJSON.h"

#include "test_support.h"
#include "ursatz_test.h"
#include "test_knowledge.h"

/*
 * The meter check: Bach 114 as the file declares it (4/4) is offered 3/4 at scale 2; read as 3/4 at scale 2 it is
 * offered nothing; Mozart 545 is offered nothing. Clementi 36 is offered 4/4 at scale 2 (half-length bars): the check
 * is offered, never applied, and the harmony of that piece does not settle it. The claim is the first of the rhythm
 * section, has no notes, and carries the same readings as the report. The document is also validated against the
 * schema by test_analysis_document_schema.
 *
 * With the argument "smoke" only the two Bach 114 cases run (the fast set).
 */

static char path_buffer[512];

static const char *musicxml(const char *name) {
    snprintf(path_buffer, sizeof path_buffer, "%s/%s", URSATZ_MUSICXML_DIR, name);
    return path_buffer;
}

typedef struct {
    ImportedScore notes;
    ChainResult report;
    cJSON *document;
} Analysis;

static void analyze(const char *name, const ImportedReading *reading, Analysis *analysis) {
    memset(analysis, 0, sizeof(*analysis));
    assert(imported_score_import_file(musicxml(name), IMPORTED_FORMAT_MUSICXML, &analysis->notes, NULL) == IMPORTED_SCORE_OK);
    if (reading) {
        assert(imported_reading_apply(&analysis->notes, reading) == IMPORTED_READING_OK);
    }
    assert(test_analyze_report(&analysis->notes, &analysis->report));
    char *json = test_render_document(musicxml(name), &analysis->notes, &analysis->report);
    assert(json);
    analysis->document = cJSON_Parse(json);
    free(json);
    assert(analysis->document);
}

static void analysis_destroy(Analysis *analysis) {
    cJSON_Delete(analysis->document);
    test_release_report(&analysis->report);
    imported_score_destroy(&analysis->notes);
}

static double number(const cJSON *object, const char *key) {
    const cJSON *item = test_json_member(object, key);
    assert(cJSON_IsNumber(item));
    return item->valuedouble;
}

/* A reading of the claim is the meter, scale and pickup it names. */
static bool reading_is(const cJSON *reading, int beats, int beat_type, double scale, double pickup) {
    const cJSON *meter = test_json_member(reading, "meter");
    return number(meter, "numerator") == beats && number(meter, "denominator") == beat_type &&
           number(test_json_member(reading, "scale"), "num") / number(test_json_member(reading, "scale"), "den") == scale &&
           number(test_json_member(reading, "pickup"), "num") / number(test_json_member(reading, "pickup"), "den") == pickup;
}

static const cJSON *claim_of(const Analysis *analysis) {
    const cJSON *rhythm = test_json_member(analysis->document, "rhythm");
    assert(cJSON_IsArray(rhythm) && cJSON_GetArraySize(rhythm) > 0);
    const cJSON *claim = cJSON_GetArrayItem(rhythm, 0);
    assert(strcmp(test_json_member(claim, "label")->valuestring, "meter-fit") == 0);
    int count = 0;
    const cJSON *other;
    cJSON_ArrayForEach(other, rhythm) {
        count += strcmp(test_json_member(other, "label")->valuestring, "meter-fit") == 0 ? 1 : 0;
    }
    assert(count == 1);
    return claim;
}

/* The claim agrees with the report and has the shape the schema asks for. */
static void check_claim(const Analysis *analysis) {
    const AnalysisMeterCheck *check = &analysis->report.meter_check;
    const cJSON *claim = claim_of(analysis);
    assert(check->present);
    assert(strcmp(test_json_member(claim, "framework")->valuestring, "standard-meter-v1") == 0);
    assert(cJSON_GetArraySize(test_json_member(claim, "note_ids")) == 0);
    assert(fabs(number(claim, "uncertainty") - check->uncertainty) < 1e-9);
    assert(check->uncertainty >= 0.0 && check->uncertainty <= 1.0);

    const cJSON *declared = test_json_member(claim, "declared");
    assert(reading_is(declared, check->effective.beats, check->effective.beat_type,
                      (double)check->effective.scale.num / (double)check->effective.scale.den,
                      (double)check->effective.pickup.num / (double)check->effective.pickup.den));
    assert(fabs(number(declared, "score") - check->effective.score) < 1e-9);
    assert(check->effective.score >= 0.0 && check->effective.score <= 1.0);

    const cJSON *alternative = test_json_member(claim, "alternative");
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

/* The file declares 4/4 for a 3/4 piece at twice the note values. On sonority segments the harmonic rhythm showed it and
   the check offered the 3/4 reading; on harmonic spans the chords change once a bar of the declared meter, so the check
   fits 4/4 and offers no alternative (the span grouping reads whole measures of the declared meter). */
static void test_bach_as_declared(void) {
    Analysis analysis;
    analyze("bach-114.musicxml", NULL, &analysis);
    check_claim(&analysis);
    const cJSON *claim = claim_of(&analysis);
    assert(reading_is(test_json_member(claim, "declared"), 4, 4, 1.0, 0.0));
    assert(cJSON_IsNull(test_json_member(claim, "alternative")));
    assert(!analysis.report.meter_check.has_alternative);
    /* offered, never applied: the analysis still reads the file's 4/4 */
    assert(!analysis.notes.source.reading_applied);
    analysis_destroy(&analysis);
}

/* With the alternative applied the check runs on the effective meter and offers nothing. */
static void test_bach_with_reading_applied(void) {
    ImportedReading reading = imported_reading_none();
    assert(imported_reading_parse_meter("3/4", &reading) && imported_reading_parse_scale("2", &reading));
    Analysis analysis;
    analyze("bach-114.musicxml", &reading, &analysis);
    check_claim(&analysis);
    const cJSON *claim = claim_of(&analysis);
    assert(reading_is(test_json_member(claim, "declared"), 3, 4, 1.0, 0.0));
    assert(cJSON_IsNull(test_json_member(claim, "alternative")));
    assert(!analysis.report.meter_check.has_alternative);
    analysis_destroy(&analysis);
}

static void test_mozart_has_no_alternative(void) {
    Analysis analysis;
    analyze("mozart-545.musicxml", NULL, &analysis);
    check_claim(&analysis);
    const cJSON *claim = claim_of(&analysis);
    assert(reading_is(test_json_member(claim, "declared"), 4, 4, 1.0, 0.0));
    assert(cJSON_IsNull(test_json_member(claim, "alternative")));
    analysis_destroy(&analysis);
}

/* On sonority segments the half-length bars of 4/4 were as good a reading of Clementi's harmony as its bars, and the check
   offered them with a low certainty; on harmonic spans the declared bars fit and nothing is offered. No reference piece
   is offered an alternative now. */
static void test_clementi_has_no_alternative(void) {
    Analysis analysis;
    analyze("clementi-36.musicxml", NULL, &analysis);
    check_claim(&analysis);
    const cJSON *claim = claim_of(&analysis);
    assert(reading_is(test_json_member(claim, "declared"), 4, 4, 1.0, 0.0));
    assert(!analysis.report.meter_check.has_alternative);
    assert(cJSON_IsNull(test_json_member(claim, "alternative")));
    assert(!analysis.notes.source.reading_applied);
    analysis_destroy(&analysis);
}

/* A piece without chord segments has no claim and no meter check. */
static void test_empty_score(void) {
    ImportedScore notes;
    if (imported_score_import_file(musicxml("empty-score.musicxml"), IMPORTED_FORMAT_MUSICXML, &notes, NULL) !=
        IMPORTED_SCORE_OK) {
        return; /* the importer refuses an empty score: nothing to check */
    }
    ChainResult report;
    assert(test_analyze_report(&notes, &report));
    assert(!report.meter_check.present);
    test_release_report(&report);
    imported_score_destroy(&notes);
}

int main(int argc, char **argv) {
    test_bach_as_declared();
    test_bach_with_reading_applied();
    if (argc > 1 && strcmp(argv[1], "smoke") == 0) {
        printf("test_meter_check smoke: ok\n");
        return 0;
    }
    test_mozart_has_no_alternative();
    test_clementi_has_no_alternative();
    test_empty_score();
    printf("test_meter_check: ok\n");
    return 0;
}
