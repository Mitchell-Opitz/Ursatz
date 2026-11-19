#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <chain/chain_result.h>
#include <io/imported_score.h>

#include "ursatz_test.h"
#include "test_knowledge.h"

/*
 * The tempo and dynamics markings of the report, on a constructed score (tests/musicxml/markings.musicxml, four
 * measures in 4/4): "Allegro" with quarter = 120 and p in measure 1, the text "cresc." in measure 2, f in measure 3,
 * "rit." and mp in measure 4. Asserted: each marking at its measure, the library's group, rank and change for each, the
 * comparison against the previous level marking, and that a file with no markings (the
 * two-part score) reports none.
 */

/* The marking starts at the beginning of the given measure of the 4/4 piece (measure 1 is time 0). */
static bool starts_measure(long numerator, unsigned long denominator, int measure) {
    return numerator == (long)(measure - 1) * (long)denominator;
}

static bool analyze(const char *dir, const char *name, ImportedScore *notes, ChainResult *report) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", dir, name);
    if (imported_score_import_file(path, IMPORTED_FORMAT_MUSICXML, notes, NULL) != IMPORTED_SCORE_OK || !test_analyze_report(notes, report)) {
        fprintf(stderr, "cannot analyze %s\n", path);
        test_failure_count++;
        return false;
    }
    return true;
}

static void check_markings(const char *dir) {
    ImportedScore notes;
    ChainResult report;
    if (!analyze(dir, "markings.musicxml", &notes, &report)) {
        return;
    }

    CHECK(report.tempo_marking_count == 2);
    if (report.tempo_marking_count == 2) {
        const AnalysisTempoMarking *allegro = &report.tempo_markings[0];
        CHECK(allegro->text && strcmp(allegro->text, "Allegro") == 0);
        CHECK(starts_measure(allegro->start.num, allegro->start.den, 1));
        CHECK(starts_measure(allegro->end.num, allegro->end.den, 4));
        CHECK(allegro->has_metronome && allegro->beat_unit.num == 1 && allegro->beat_unit.den == 4);
        CHECK(allegro->bpm.num == 120 && allegro->bpm.den == 1);
        CHECK(allegro->has_rank && allegro->group && strcmp(allegro->group, "fast") == 0);
        CHECK(strcmp(allegro->change, "none") == 0);
        const AnalysisTempoMarking *rit = &report.tempo_markings[1];
        CHECK(rit->text && strcmp(rit->text, "rit.") == 0);
        CHECK(starts_measure(rit->start.num, rit->start.den, 4));
        CHECK(!rit->has_metronome);
        CHECK(strcmp(rit->change, "gradually-slower") == 0);
    }

    CHECK(report.dynamic_count == 4);
    if (report.dynamic_count == 4) {
        const AnalysisDynamic *p = &report.dynamics[0];
        const AnalysisDynamic *cresc = &report.dynamics[1];
        const AnalysisDynamic *f = &report.dynamics[2];
        const AnalysisDynamic *mp = &report.dynamics[3];
        CHECK(strcmp(p->marking, "p") == 0 && starts_measure(p->start.num, p->start.den, 1));
        CHECK(p->has_rank && p->rank < 0 && strcmp(p->change, "none") == 0 && p->relation == NULL);
        CHECK(strcmp(cresc->marking, "cresc.") == 0 && starts_measure(cresc->start.num, cresc->start.den, 2));
        CHECK(!cresc->has_rank && strcmp(cresc->change, "louder") == 0 && cresc->relation == NULL);
        CHECK(strcmp(f->marking, "f") == 0 && starts_measure(f->start.num, f->start.den, 3));
        CHECK(f->has_rank && f->rank > 0 && f->relation && strcmp(f->relation, "louder") == 0);
        CHECK(strcmp(mp->marking, "mp") == 0 && starts_measure(mp->start.num, mp->start.den, 4));
        CHECK(mp->relation && strcmp(mp->relation, "quieter") == 0);
    }

    chain_result_destroy(&report);
    imported_score_destroy(&notes);
}

static void check_no_markings(const char *dir) {
    ImportedScore notes;
    ChainResult report;
    if (!analyze(dir, "two-parts.musicxml", &notes, &report)) {
        return;
    }
    CHECK(report.tempo_marking_count == 0);
    CHECK(report.dynamic_count == 0);
    chain_result_destroy(&report);
    imported_score_destroy(&notes);
}

int main(void) {
    check_markings(URSATZ_MUSICXML_DIR);
    check_no_markings(URSATZ_MUSICXML_DIR);
    return test_finish("test_markings");
}
