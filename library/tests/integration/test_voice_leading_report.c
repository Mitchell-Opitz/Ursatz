#include "analysis_motion.h"
#include "analysis_perfects.h"
#include "analysis_resolution.h"
#include "voice_leading_util.h"

/*
 * Voice leading on separated voices: standard-counterpoint's motion classification and parallel and direct perfect intervals, and
 * standard-harmony's seventh and leading tone resolution, over a voice-pitch grid built by voice_leading_util.h.
 *
 *  1. THE PLANTED-ERROR FIXTURE (tests/fixtures/voice_leading/errors_satb.txt, synthetic): a short SATB progression with six
 *     planted errors, run on the TRUE parts. Asserted: exactly those six are found (two parallel fifths, one parallel octave, one
 *     direct octave, one unresolved seventh, one unresolved leading tone), each in the voices and segments the fixture says.
 *  2. THE CHORALE (tests/fixtures/annotations/chorale_satb.txt, with the hand-labelled chords of
 *     tests/fixtures/voice_leading/chorale_satb_harmony.txt): run on the TRUE parts and on the voices L2.7 assigns when the four
 *     parts are merged into one stream. Every violation of each run is printed. With the true parts, no parallel fifths or
 *     octaves are asserted except the ones listed below, each verified by hand against the fixture's notes. The separated-voice
 *     run is reported against the true-part run: what differs follows the accuracy of voice separation.
 *
 * URSATZ_ANNOTATION_DIR and URSATZ_VOICE_LEADING_DIR come from CMake.
 */

typedef struct {
    char keys[128][96];
    size_t count;
    size_t motions;
    size_t type_count[MOTION_TYPE_COUNT];
} Report;

static void add_key(Report *r, const char *key) {
    assert(r->count < 128);
    snprintf(r->keys[r->count++], sizeof(r->keys[0]), "%s", key);
}

static bool has_key(const Report *r, const char *key) {
    for (size_t i = 0; i < r->count; i++) {
        if (strcmp(r->keys[i], key) == 0) {
            return true;
        }
    }
    return false;
}

static void run_adapters(const VlPiece *piece, const VlAssignment *assignment, Report *report, bool print) {
    memset(report, 0, sizeof(*report));
    VlSegments seg;
    vl_build(piece, assignment, &seg);

    StandardCounterpointPreparedGrid prepared;
    assert(standard_counterpoint_grid_prepare(&seg.cp_grid, &prepared) == STANDARD_COUNTERPOINT_GRID_OK);
    StandardCounterpointMotions motions;
    assert(standard_counterpoint_classify_motions(&prepared, &motions) == STANDARD_COUNTERPOINT_MOTION_OK);
    report->motions = motions.count;
    for (size_t i = 0; i < motions.count; i++) {
        report->type_count[motions.items[i].type]++;
    }
    StandardCounterpointViolations perfects;
    assert(standard_counterpoint_check_perfects(&prepared, &perfects) == STANDARD_COUNTERPOINT_PERFECTS_OK);
    for (size_t i = 0; i < perfects.count; i++) {
        const StandardCounterpointViolation *v = &perfects.items[i];
        char key[96];
        snprintf(key, sizeof(key), "%s %zu %zu %zu %zu", standard_counterpoint_violation_rule_name(v->rule),
                 v->segment_from, v->segment_to, v->upper_voice, v->lower_voice);
        add_key(report, key);
        if (print) {
            printf("    %s, segments %zu-%zu (eighths %ld-%ld), voices %zu and %zu, uncertainty %.2f\n",
                   standard_counterpoint_violation_rule_name(v->rule), v->segment_from, v->segment_to,
                   seg.start[v->segment_from], seg.end[v->segment_to], v->upper_voice, v->lower_voice, v->uncertainty);
        }
    }

    HarmonySegment *segments = NULL;
    assert(harmony_analysis_prepare(seg.inputs, seg.segment_count, &segments) == HARMONY_ANALYSIS_OK);
    HarmonyResolutions resolutions;
    assert(harmony_analysis_check_resolutions(segments, seg.segment_count, &seg.harmony_grid, NULL, &resolutions) ==
           HARMONY_RESOLUTION_OK);
    for (size_t i = 0; i < resolutions.count; i++) {
        const HarmonyResolutionResult *v = &resolutions.items[i];
        char key[96];
        snprintf(key, sizeof(key), "%s %zu %zu %zu -1", harmony_resolution_rule_name(v->rule), v->segment_from,
                 v->segment_to, v->voice);
        add_key(report, key);
        if (print) {
            printf("    %s, segments %zu-%zu (eighths %ld-%ld), voice %zu, uncertainty %.2f\n",
                   harmony_resolution_rule_name(v->rule), v->segment_from, v->segment_to, seg.start[v->segment_from],
                   seg.end[v->segment_to], v->voice, v->uncertainty);
        }
    }

    harmony_resolutions_destroy(&resolutions);
    harmony_analysis_destroy(segments);
    standard_counterpoint_violations_destroy(&perfects);
    standard_counterpoint_motions_destroy(&motions);
    standard_counterpoint_grid_destroy(&prepared);
    vl_destroy(&seg, piece->note_count);
}

static void test_planted_errors(void) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/errors_satb.txt", URSATZ_VOICE_LEADING_DIR);
    VlPiece piece;
    memset(&piece, 0, sizeof(piece));
    vl_load(path, &piece);
    assert(piece.note_count == 52 && piece.expected_count == 6 && piece.chord_count == 13);
    VlAssignment truth;
    vl_true_parts(&piece, &truth);

    Report report;
    printf("Planted-error fixture (synthetic, true parts):\n");
    run_adapters(&piece, &truth, &report, true);
    assert(report.motions == 12 * 6); /* 13 segments, 6 voice pairs, every voice sounding throughout */
    assert(report.count == piece.expected_count);
    for (size_t i = 0; i < piece.expected_count; i++) {
        const VlExpected *e = &piece.expected[i];
        char key[96];
        snprintf(key, sizeof(key), "%s %d %d %d %d", e->rule, e->from, e->to, e->upper, e->lower);
        assert(has_key(&report, key));
    }
    printf("  all six planted errors found, and nothing else\n");
}

/* The parallel fifths and octaves the chorale's true parts make, each verified by hand against the notes of
   tests/fixtures/annotations/chorale_satb.txt (segments are the attack boundaries of all four parts; voices 0 S, 1 A, 2 T,
   3 B). The fixture is a hand-written draft, so these are faults of the draft, not of the analysis. */
static const char *const VERIFIED_PARALLELS[] = {
    "parallel-octaves 1 2 0 3",   /* S C5-D5 over B C3-D3 */
    "parallel-fifths 1 2 1 3",    /* A G4-A4 over B C3-D3 (twelfths) */
    "parallel-octaves 8 9 0 3",   /* S G4-C5 over B G2-C3 */
    "parallel-octaves 9 10 0 2",  /* S C5-B4 over T C4-B3 */
    "parallel-fifths 11 12 1 3",  /* A D4-E4 over B G2-A2 (twelfths) */
    "parallel-octaves 20 21 0 3", /* S C5-D5 over B C3-D3 */
    "parallel-octaves 22 23 0 3", /* S D5-C5 over B D3-C3 */
    "parallel-octaves 27 28 0 3", /* S C5-B4 over B C3-B2 */
    NULL};

static bool is_parallel(const char *key) {
    return strncmp(key, "parallel-", 9) == 0;
}

static void test_chorale(void) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/chorale_satb.txt", URSATZ_ANNOTATION_DIR);
    VlPiece piece;
    memset(&piece, 0, sizeof(piece));
    vl_load(path, &piece);
    snprintf(path, sizeof(path), "%s/chorale_satb_harmony.txt", URSATZ_VOICE_LEADING_DIR);
    vl_load(path, &piece);
    assert(piece.note_count > 100 && piece.chord_count == 32 && piece.total == 64);

    VlAssignment truth;
    VlAssignment separated;
    vl_true_parts(&piece, &truth);
    vl_separated(&piece, &separated);
    size_t agree = 0;
    for (size_t i = 0; i < piece.note_count; i++) {
        agree += separated.voice[i] == truth.voice[i] ? 1 : 0;
    }

    Report true_run;
    Report separated_run;
    printf("Chorale (hand-written, draft), true parts:\n");
    run_adapters(&piece, &truth, &true_run, true);
    printf("  %zu motions: contrary %zu, similar %zu, parallel %zu, oblique %zu, static %zu\n", true_run.motions,
           true_run.type_count[MOTION_TYPE_CONTRARY], true_run.type_count[MOTION_TYPE_SIMILAR],
           true_run.type_count[MOTION_TYPE_PARALLEL], true_run.type_count[MOTION_TYPE_OBLIQUE],
           true_run.type_count[MOTION_TYPE_STATIC]);
    printf("  %zu violation(s) with the true parts\n", true_run.count);

    printf("Chorale, L2.7's separated voices (%zu of %zu notes in their true voice):\n", agree, piece.note_count);
    run_adapters(&piece, &separated, &separated_run, true);
    printf("  %zu violation(s) with the separated voices\n", separated_run.count);

    size_t only_true = 0;
    size_t only_separated = 0;
    for (size_t i = 0; i < true_run.count; i++) {
        if (!has_key(&separated_run, true_run.keys[i])) {
            printf("  only with the true parts: %s\n", true_run.keys[i]);
            only_true++;
        }
    }
    for (size_t i = 0; i < separated_run.count; i++) {
        if (!has_key(&true_run, separated_run.keys[i])) {
            printf("  only with the separated voices: %s\n", separated_run.keys[i]);
            only_separated++;
        }
    }
    printf("  the separated run finds %zu the true run does not, and misses %zu\n", only_separated, only_true);

    /* with the true parts: no parallel fifths or octaves other than the hand-verified ones */
    for (size_t i = 0; i < true_run.count; i++) {
        if (!is_parallel(true_run.keys[i])) {
            continue;
        }
        bool verified = false;
        for (size_t v = 0; VERIFIED_PARALLELS[v] != NULL; v++) {
            verified = verified || strcmp(VERIFIED_PARALLELS[v], true_run.keys[i]) == 0;
        }
        if (!verified) {
            printf("  UNVERIFIED parallel with the true parts: %s\n", true_run.keys[i]);
        }
        assert(verified);
    }
}

int main(void) {
    test_planted_errors();
    test_chorale();
    printf("test_voice_leading_report: ok\n");
    return 0;
}
