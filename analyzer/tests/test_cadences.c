#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_analyzer/analysis_document.h>
#include <ursatz_analyzer/analysis_pipeline.h>
#include <ursatz_analyzer/markdown_report.h>
#include <ursatz_analyzer/midi_pipeline.h>

#include "cJSON.h"

#include "test_support.h"

/*
 * Cadences, boundary cadences and cadence-confirmed key changes on the five reference pieces (Ursatz's
 * framework-v1-corpus).
 *
 * Every cadence claim is checked for what the document promises: valid locations, note ids that exist, once each and
 * in order, an arrival and a dominant inside the claim's span, a type the library names, a key region that exists. The
 * key changes are checked against the cadences: a change into a new key is a modulation (or weak modulation) exactly
 * when a cadence arrives in the new region, a return takes the extent of its excursion, and cadence_confirmed marks the
 * modulations.
 *
 * The counts are what Ursatz's chain gives (the library's, not hand readings; the hand marks are Ursatz's own test):
 *
 *   Bach 114     none in the Analyzer's report (P40 B6: the imperfect authentic cadence of m. 18 is gone with the
 *                bass evidence rules and the held bass fix, and the two perfect authentic cadences of the P1 B3
 *                result were gone on harmonic spans before): the chain's cadences read the spans, and P41 reads
 *                them again.
 *   Clementi 36  hc 3, iac 1, four cadences (HC m4, IAC m8, HC m23 with the perfect authentic cadence of m24 as its competing
 *                reading, HC m27; the P40 result was hc 1, pac 3, iac 1: P41 claims cadences only at phrase points, R-390, R-393;
 *                the PACs of m15, m31 and m38 wait on chord readings and keys, P42).
 *   Mozart 545   16 cadences in all (15 before P42, 23 before P41, which claims cadences only at phrase points; HC m12 and PAC m28 close its first two
 *                phrases, R-393); the chain finds no boundary cadence. The cause is not the outer-line index (the chord and voice
 *                segmentations are equal on this piece, P24 B7a Notes) and has not been found.
 *
 * Competing readings (R-090, R-091): every claim has a share in thousandths; a claim with an alternative has the two
 * shares summing to 1000. Since P41 the competing readings are two cadences: Clementi 36 m. 23 beat 1 is a half cadence (500)
 * or the perfect authentic cadence of m. 24 beat 1 (500); Field 1 m. 61 beat 1 is a half cadence (650) or the imperfect
 * authentic cadence of m. 62 beat 1 (350). The report prints both readings. (Field 5 m. 8 beat 3 and Field 1 m. 60 beat 1,
 * the evaded cadences with a prolongation reading, are not at a phrase point and are no longer claimed.) Mozart 545 m. 4 has
 * a V to I6 the chain does not find: B3 deviation DEV-103.
 *
 * Key changes: since P42 the chain places keys on phrases and confirms the changes with the cadences it claimed (R-397), so on
 * every piece a change is a modulation exactly where a claimed cadence arrives in the new region.
 */

static int failures = 0;

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition);          \
            failures++;                                                                            \
        }                                                                                          \
    } while (0)

typedef struct {
    const char *name;
    const char *file;
} Piece;

static const Piece PIECES[] = {{"Bach 114", "Bach 114.mid"},
                               {"Clementi 36", "Clementi 36.mid"},
                               {"Mozart 545", "Mozart 545.mid"},
                               {"Field 1", "Field 1.mid"},
                               {"Field 5", "Field 5.mid"}};

static const char *const TYPES[] = {"perfect-authentic", "imperfect-authentic", "half",       "subverted",
                                    "plagal",            "deceptive",           "authentic-unspecified", "unknown"};

static bool rational_equals(const cJSON *rational, long num, long den) {
    return (long)test_json_member(rational, "num")->valuedouble * den == num * (long)test_json_member(rational, "den")->valuedouble;
}

static bool position_valid(const cJSON *position) {
    return cJSON_IsObject(position) && test_json_member(position, "measure")->valuedouble >= 0 &&
           test_json_member(position, "beat")->valuedouble >= 1;
}

static size_t count_type(const cJSON *cadences, const char *type) {
    size_t count = 0;
    const cJSON *cadence;
    cJSON_ArrayForEach(cadence, cadences) {
        count += strcmp(test_json_member(cadence, "type")->valuestring, type) == 0 ? 1 : 0;
    }
    return count;
}

static void check_claims(const Piece *piece, const ImportedNotes *notes, const AnalysisReport *report,
                         const cJSON *document) {
    const cJSON *cadences = test_json_member(document, "cadences");
    const cJSON *regions = test_json_member(test_json_member(document, "keys"), "regions");
    CHECK((size_t)cJSON_GetArraySize(cadences) == report->cadence_count);
    double previous_arrival = -1.0;
    const cJSON *cadence;
    cJSON_ArrayForEach(cadence, cadences) {
        const cJSON *span = test_json_member(cadence, "span");
        const cJSON *location = test_json_member(cadence, "location");
        const cJSON *arrival = test_json_member(cadence, "arrival");
        const cJSON *dominant = test_json_member(cadence, "dominant");
        CHECK(strcmp(test_json_member(cadence, "framework")->valuestring, "standard-harmony-v1") == 0);
        CHECK(position_valid(test_json_member(location, "start")) && position_valid(test_json_member(location, "end")));
        CHECK(position_valid(test_json_member(test_json_member(arrival, "location"), "start")) &&
              position_valid(test_json_member(test_json_member(arrival, "location"), "end")));
        CHECK(test_within(test_json_member(arrival, "span"), span) && test_within(test_json_member(dominant, "span"), span));
        CHECK(test_rational_of(test_json_member(test_json_member(dominant, "span"), "start")) <=
              test_rational_of(test_json_member(test_json_member(arrival, "span"), "start")));

        bool known = false;
        for (size_t t = 0; t < sizeof(TYPES) / sizeof(TYPES[0]); t++) {
            known = known || strcmp(test_json_member(cadence, "type")->valuestring, TYPES[t]) == 0;
        }
        CHECK(known);
        CHECK(strcmp(test_json_member(cadence, "label")->valuestring, test_json_member(cadence, "type")->valuestring) == 0);

        const cJSON *ids = test_json_member(cadence, "note_ids");
        CHECK(cJSON_GetArraySize(ids) > 0);
        double last_id = -1.0;
        const cJSON *id;
        cJSON_ArrayForEach(id, ids) {
            CHECK(id->valuedouble > last_id && id->valuedouble < (double)notes->count);
            last_id = id->valuedouble;
        }

        const cJSON *region = test_json_member(cadence, "region");
        CHECK(cJSON_IsNull(region) || (cJSON_IsNumber(region) && region->valuedouble < cJSON_GetArraySize(regions)));
        const cJSON *soprano = test_json_member(cadence, "soprano_degree");
        CHECK(cJSON_IsNull(soprano) || cJSON_IsString(soprano));
        const char *strength = test_json_member(cadence, "arrival_beat_strength")->valuestring;
        CHECK(strcmp(strength, "strong") == 0 || strcmp(strength, "medium") == 0 || strcmp(strength, "weak") == 0);
        if (cJSON_IsTrue(test_json_member(cadence, "boundary"))) {
            CHECK(strcmp(test_json_member(cadence, "type")->valuestring, "half") == 0);
        }

        double arrival_start = test_rational_of(test_json_member(test_json_member(arrival, "span"), "start"));
        CHECK(arrival_start >= previous_arrival);
        previous_arrival = arrival_start;
    }
    (void)piece;
}

/* A change into a new key is a modulation exactly when a cadence arrives in the new region (R-044); a return takes
   the extent of the excursion before it. */
static void check_key_changes(const cJSON *document, const AnalysisReport *report) {
    const cJSON *cadences = test_json_member(document, "cadences");
    const cJSON *regions = test_json_member(test_json_member(document, "keys"), "regions");
    for (size_t i = 0; i < report->key_change_count; i++) {
        const AnalysisKeyChange *change = &report->key_changes[i];
        bool modulation = change->extent == ANALYSIS_KEY_CHANGE_WEAK_MODULATION ||
                          change->extent == ANALYSIS_KEY_CHANGE_MODULATION;
        if (change->is_return) {
            CHECK(i > 0 && change->extent == report->key_changes[i - 1].extent);
            CHECK(change->cadence_confirmed == report->key_changes[i - 1].cadence_confirmed);
            continue;
        }
        const cJSON *into = test_json_member(cJSON_GetArrayItem(regions, (int)change->to_region), "span");
        bool arrives = false;
        const cJSON *cadence;
        cJSON_ArrayForEach(cadence, cadences) {
            double arrival = test_rational_of(test_json_member(test_json_member(test_json_member(cadence, "arrival"), "span"), "start"));
            arrives = arrives || (arrival >= test_rational_of(test_json_member(into, "start")) - 1e-12 &&
                                  arrival < test_rational_of(test_json_member(into, "end")) - 1e-12);
        }
        CHECK(modulation == arrives);
        CHECK(change->cadence_confirmed == modulation);
    }
    /* the document lists what the report has */
    size_t tonicizations = 0;
    size_t modulations = 0;
    for (size_t i = 0; i < report->key_change_count; i++) {
        const AnalysisKeyChange *change = &report->key_changes[i];
        bool is_modulation = change->extent == ANALYSIS_KEY_CHANGE_WEAK_MODULATION ||
                             change->extent == ANALYSIS_KEY_CHANGE_MODULATION;
        modulations += is_modulation ? 1 : 0;
        tonicizations += !is_modulation && !change->is_return ? 1 : 0;
    }
    CHECK((size_t)cJSON_GetArraySize(test_json_member(document, "tonicizations")) == tonicizations);
    CHECK((size_t)cJSON_GetArraySize(test_json_member(document, "modulations")) == modulations);
}

static bool has_cadence_at(const cJSON *cadences, const char *type, long num, long den) {
    const cJSON *cadence;
    cJSON_ArrayForEach(cadence, cadences) {
        if (strcmp(test_json_member(cadence, "type")->valuestring, type) == 0 &&
            rational_equals(test_json_member(test_json_member(test_json_member(cadence, "arrival"), "span"), "start"), num, den)) {
            return true;
        }
    }
    return false;
}

/* The cadence claim whose arrival begins at measure/beat, or NULL. */
static const cJSON *cadence_at(const cJSON *cadences, int measure, int beat) {
    const cJSON *cadence;
    cJSON_ArrayForEach(cadence, cadences) {
        const cJSON *start = test_json_member(test_json_member(test_json_member(cadence, "arrival"), "location"), "start");
        if ((int)test_json_member(start, "measure")->valuedouble == measure && (int)test_json_member(start, "beat")->valuedouble == beat) {
            return cadence;
        }
    }
    return NULL;
}

/* Every claim has a share; an alternative's share completes it to 1000 and its note ids are in order. */
static void check_readings(const cJSON *cadences) {
    const cJSON *cadence;
    cJSON_ArrayForEach(cadence, cadences) {
        const cJSON *share = test_json_member(cadence, "share");
        CHECK(cJSON_IsNumber(share) && share->valuedouble >= 0 && share->valuedouble <= 1000);
        const cJSON *alternative = test_json_member(cadence, "alternative");
        if (!alternative) {
            CHECK(share->valuedouble == 1000);
            continue;
        }
        const char *reading = test_json_member(alternative, "reading")->valuestring;
        CHECK(strcmp(reading, "cadence") == 0 || strcmp(reading, "prolongation") == 0);
        CHECK((strcmp(reading, "prolongation") == 0) == cJSON_IsNull(test_json_member(alternative, "type")));
        CHECK(share->valuedouble + test_json_member(alternative, "share")->valuedouble == 1000);
        CHECK(cJSON_IsNumber(test_json_member(alternative, "measure")) && cJSON_IsNumber(test_json_member(alternative, "beat")));
        CHECK(cJSON_GetArraySize(test_json_member(alternative, "note_ids")) > 0);
    }
}

/* A cadence claim at a measure and beat of this type and share whose alternative is another cadence, with these values. */
static void check_competing_cadence(const cJSON *cadences, int measure, int beat, const char *type, int share, const char *alternative_type,
                                    int alternative_measure, int alternative_beat, int alternative_share) {
    const cJSON *primary = cadence_at(cadences, measure, beat);
    const cJSON *alternative = primary ? test_json_member(primary, "alternative") : NULL;
    CHECK(primary && strcmp(test_json_member(primary, "type")->valuestring, type) == 0);
    CHECK(primary && test_json_member(primary, "share")->valuedouble == share);
    CHECK(alternative && strcmp(test_json_member(alternative, "reading")->valuestring, "cadence") == 0);
    CHECK(alternative && strcmp(test_json_member(alternative, "type")->valuestring, alternative_type) == 0);
    CHECK(alternative && test_json_member(alternative, "share")->valuedouble == alternative_share);
    CHECK(alternative && test_json_member(alternative, "measure")->valuedouble == alternative_measure &&
          test_json_member(alternative, "beat")->valuedouble == alternative_beat);
}

/* The report prints each competing pair, the primary first, and calls a subverted cadence evaded. */
static void check_reading_report(const char *path, const ImportedNotes *notes, const AnalysisReport *report,
                                 const char *pair_line, const char *counts) {
    AnalysisDocument *document = analysis_document_build(path, notes, report);
    char *markdown = document ? markdown_report_render(document) : NULL;
    CHECK(markdown != NULL);
    if (markdown) {
        CHECK(strstr(markdown, pair_line) != NULL);
        CHECK(strstr(markdown, "subverted") == NULL);
        CHECK(strstr(markdown, counts) != NULL);
    }
    free(markdown);
    analysis_document_destroy(document);
}

static void check_counts(const Piece *piece, const cJSON *cadences) {
    if (strcmp(piece->name, "Bach 114") == 0) {
        CHECK(count_type(cadences, "perfect-authentic") == 0);
        CHECK(count_type(cadences, "imperfect-authentic") == 0);
        CHECK(cJSON_GetArraySize(cadences) == 0);
    } else if (strcmp(piece->name, "Clementi 36") == 0) {
        CHECK(count_type(cadences, "half") == 3);
        CHECK(count_type(cadences, "perfect-authentic") == 0);
        CHECK(count_type(cadences, "imperfect-authentic") == 1);
        CHECK(count_type(cadences, "subverted") == 0);
        CHECK(cJSON_GetArraySize(cadences) == 4);
    }
}

static void run_piece(const Piece *piece) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/framework-v1-corpus/%s", URSATZ_ANALYZER_FIXTURE_DIR, piece->file);
    ImportedNotes notes;
    AnalysisReport report;
    if (midi_pipeline_import_file(path, &notes) != MIDI_PIPELINE_OK || !run_analysis_pipeline(&notes, &report)) {
        fprintf(stderr, "%s: cannot analyze %s\n", piece->name, path);
        failures++;
        return;
    }
    char *json = analysis_document_render(path, &notes, &report);
    cJSON *document = json ? cJSON_Parse(json) : NULL;
    free(json);
    CHECK(document != NULL);
    if (document) {
        check_claims(piece, &notes, &report, document);
        check_key_changes(document, &report);
        check_counts(piece, test_json_member(document, "cadences"));
        check_readings(test_json_member(document, "cadences"));
        if (strcmp(piece->name, "Clementi 36") == 0) {
            check_competing_cadence(test_json_member(document, "cadences"), 23, 1, "half", 500, "perfect-authentic", 24, 1, 500);
            check_reading_report(path, &notes, &report, "- m23 b1 half cadence (50%), or perfect authentic cadence at m24 b1 (50%)\n",
                                 "half 3, imperfect-authentic 1.");
        }
        if (strcmp(piece->name, "Field 1") == 0) {
            check_competing_cadence(test_json_member(document, "cadences"), 61, 1, "half", 650, "imperfect-authentic", 62, 1, 350);
            check_reading_report(path, &notes, &report, "- m61 b1 half cadence (65%), or imperfect authentic cadence at m62 b1 (35%)\n",
                                 "half 7, imperfect-authentic 1, perfect-authentic 1.");
        }
        if (strcmp(piece->name, "Field 5") == 0) {
            CHECK(cJSON_GetArraySize(test_json_member(document, "cadences")) == 0);
        }
        printf("%s: %zu cadences\n", piece->name, report.cadence_count);
        cJSON_Delete(document);
    }
    analysis_report_destroy(&report);
    imported_notes_destroy(&notes);
}

int main(void) {
    for (size_t i = 0; i < sizeof(PIECES) / sizeof(PIECES[0]); i++) {
        run_piece(&PIECES[i]);
    }
    if (failures > 0) {
        fprintf(stderr, "test_cadences: %d failure(s)\n", failures);
        return 1;
    }
    printf("test_cadences: ok\n");
    return 0;
}
