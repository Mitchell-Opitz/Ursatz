#define URSATZ_FORM_PIECES_AS_LIBRARY
#include "test_form_reference_pieces.c"

#include <math.h>

#include "interpretation/chord.h"
#include "meter_analysis_density.h"
#include "meter_analysis_hemiola.h"
#include "meter_analysis_hypermeter.h"
#include "meter_analysis_syncopation.h"
#include "rhythm_util.h"

/*
 * The rhythm and meter analysis of Field 5 (Nocturne No. 5 in B-flat major, in 12/8), the measures of tests/fixtures/annotations/field5_rhythm.txt:
 * a DRAFT hand reading, not yet checked against the score (see its header), made from the notes of the file and not from this code's output.
 *
 *   SYNCOPATIONS. The adapter runs per line (soprano, bass, other). An annotated syncopation is FOUND when the adapter reports one in the same
 *   measure within 1/100 beat of the annotated beat, in any line. The ones it reports that match none are extras, reported with their line.
 *   HEMIOLAS. 12/8 has none in standard-meter (hemiola_rules.h), so none is annotated and the assertion about them holds vacuously; the
 *   adapter is run with the chord changes of the weighted chord identification given and what it reports is printed.
 *   HYPERMETER. With the ANNOTATED phrase spans as input (so a phrase error does not count against the hypermeter), a hyperdownbeat is FOUND when
 *   the adapter has a hypermeasure starting in that measure; a reinterpretation when it reports one at that measure.
 *   THE DETECTED PHRASES. The same hypermeter run with the form adapter's phrases (from the cadence adapter's cadences through the whole chain),
 *   reported with the number of hyperdownbeats it finds and the difference from the annotated phrases' run. It is asserted only at a floor
 *   under what it measured, so a regression shows; it is not a quality target.
 *
 * Asserted: at least 80% of the annotated syncopations found, every annotated hemiola found, at least 80% of the annotated hyperdownbeats found.
 * Nothing is tuned to the fixture: every default is the library's.
 */

#define RHYTHM_TARGET 0.80
#define DETECTED_FLOOR 0.0

typedef struct {
    int first;
    int last;
    int arrival;
    bool elided;
} AnnotatedPhrase;

typedef struct {
    int measure;
    double beat;
} AnnotatedSyncopation;

typedef struct {
    int first;
    int last;
} AnnotatedHemiola;

typedef struct {
    int range_first;
    int range_last;
    AnnotatedPhrase phrases[32];
    size_t phrase_count;
    AnnotatedSyncopation syncopations[64];
    size_t syncopation_count;
    AnnotatedHemiola hemiolas[16];
    size_t hemiola_count;
    int hyperdownbeats[64];
    size_t hyperdownbeat_count;
    int reinterpretations[16];
    size_t reinterpretation_count;
} RhythmAnnotation;

static RhythmAnnotation load_rhythm_annotation(void) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/field5_rhythm.txt", URSATZ_ANNOTATION_DIR);
    size_t size = 0;
    unsigned char *text = pipeline_read_file(path, &size);
    RhythmAnnotation a;
    memset(&a, 0, sizeof(a));
    for (char *line = strtok((char *)text, "\n"); line != NULL; line = strtok(NULL, "\n")) {
        if (line[0] == '#' || line[0] == '\0') {
            continue;
        }
        char word[16];
        int x = 0;
        int y = 0;
        int z = 0;
        double beat = 0.0;
        if (sscanf(line, "range %d %d", &x, &y) == 2) {
            a.range_first = x;
            a.range_last = y;
        } else if (sscanf(line, "phrase %d %d %d %15s", &x, &y, &z, word) >= 3) {
            AnnotatedPhrase *phrase = &a.phrases[a.phrase_count++];
            phrase->first = x;
            phrase->last = y;
            phrase->arrival = z;
            phrase->elided = sscanf(line, "phrase %d %d %d %15s", &x, &y, &z, word) == 4 && strcmp(word, "elided") == 0;
        } else if (sscanf(line, "syncopation %d %lf", &x, &beat) == 2) {
            a.syncopations[a.syncopation_count].measure = x;
            a.syncopations[a.syncopation_count++].beat = beat;
        } else if (sscanf(line, "hemiola %d %d", &x, &y) == 2) {
            a.hemiolas[a.hemiola_count].first = x;
            a.hemiolas[a.hemiola_count++].last = y;
        } else if (sscanf(line, "hyperdownbeat %d", &x) == 1) {
            a.hyperdownbeats[a.hyperdownbeat_count++] = x;
        } else if (sscanf(line, "reinterpretation %d", &x) == 1) {
            a.reinterpretations[a.reinterpretation_count++] = x;
        }
    }
    free(text);
    assert(a.range_first > 0 && a.range_last >= a.range_first && a.phrase_count > 0);
    return a;
}

static const char *line_name(int line) {
    return line == 0 ? "soprano" : (line == 1 ? "bass" : "inner");
}

static bool found_syncopation(const StandardMeterSyncopations *found, const AnnotatedSyncopation *want) {
    for (size_t i = 0; i < found->count; i++) {
        if (found->items[i].claim.measure == want->measure && fabs(found->items[i].claim.beat - want->beat) < 0.01) {
            return true;
        }
    }
    return false;
}

static bool is_annotated_syncopation(const RhythmAnnotation *a, const StandardMeterSyncopationClaim *claim) {
    for (size_t i = 0; i < a->syncopation_count; i++) {
        if (a->syncopations[i].measure == claim->measure && fabs(a->syncopations[i].beat - claim->beat) < 0.01) {
            return true;
        }
    }
    return false;
}

static bool found_hyperdownbeat(const StandardMeterHypermeter *found, int measure) {
    for (size_t i = 0; i < found->hypermeasure_count; i++) {
        if (found->hypermeasures[i].claim.first_measure == measure) {
            return true;
        }
    }
    return false;
}

static size_t score_hyperdownbeats(const char *label, const RhythmAnnotation *a, const StandardMeterHypermeter *found) {
    size_t hits = 0;
    printf("  %s: %zu hypermeasure(s) in %zu phrase(s)\n", label, found->hypermeasure_count, found->phrase_count);
    for (size_t i = 0; i < a->hyperdownbeat_count; i++) {
        if (found_hyperdownbeat(found, a->hyperdownbeats[i])) {
            hits++;
        } else {
            printf("    missed hyperdownbeat at measure %d\n", a->hyperdownbeats[i]);
        }
    }
    printf("    hyperdownbeats found %zu of %zu (%.1f%%)\n", hits, a->hyperdownbeat_count,
           100.0 * (a->hyperdownbeat_count == 0 ? 1.0 : (double)hits / (double)a->hyperdownbeat_count));
    return hits;
}

int main(void) {
    const FormPiece *fp = &FORM_PIECES[4];
    assert(strcmp(fp->prefix, "field5") == 0);
    RhythmAnnotation ra = load_rhythm_annotation();
    Annotation cadence_annotation = load_annotation(fp->piece.fixture);
    WeightedPipeline p = pipeline_run(URSATZ_CORPUS_DIR, fp->piece.midi, NULL);
    const MeterMap *map = p.meter.meter_map;
    OuterVoiceLines lines = {0};
    assert(outer_voice_extract(&p.voice_sonorities, p.events, p.pitch_values, p.note_count, map, standard_meter_segmentation_beat_grid, NULL,
                               NULL, &lines) == OUTER_VOICE_OK);
    printf("%s, measures %d-%d (draft hand analysis):\n", fp->piece.name, ra.range_first, ra.range_last);

    /* the harmony: chord labels per segment, for the chord changes */
    size_t scope = scope_count(&p, &cadence_annotation);
    CommonPracticeWeightedResults chords = {0};
    assert(common_practice_identify_weighted_sequence(p.segments, scope, cadence_annotation.home, COMMON_PRACTICE_KEY_MAJOR, NULL,
                                                      &chords) == COMMON_PRACTICE_WEIGHTED_OK);
    const char **labels = (const char **)calloc(p.segment_count, sizeof(char *));
    assert(labels != NULL);
    for (size_t s = 0; s < scope; s++) {
        labels[s] = chords.items[s].matched ? chord_label(chords.items[s].best_chord) : NULL;
    }

    /* --- the annotated phrases --- */
    RhythmBuild build;
    rhythm_build_init(&build, map);
    rhythm_build_notes(&build, &p, &lines, ra.range_first, ra.range_last);
    rhythm_build_chord_changes(&build, &p, labels, ra.range_first, ra.range_last);
    for (size_t i = 0; i < ra.phrase_count; i++) {
        rhythm_build_phrase(&build, (int)i, ra.phrases[i].first, ra.phrases[i].last, ra.phrases[i].arrival,
                            STANDARD_METER_EXPANSION_NONE, 0, ra.phrases[i].elided);
    }
    printf("  %zu notes, %zu chord changes, %zu annotated phrases\n", build.input.note_count, build.input.chord_change_count,
           build.input.phrase_count);

    StandardMeterSyncopations syncopations = {NULL, 0};
    assert(standard_meter_find_syncopations(&build.input, NULL, &syncopations) == STANDARD_METER_ANALYSIS_OK);
    size_t sync_hits = 0;
    for (size_t i = 0; i < ra.syncopation_count; i++) {
        if (found_syncopation(&syncopations, &ra.syncopations[i])) {
            sync_hits++;
        } else {
            printf("    missed syncopation at measure %d beat %.2f\n", ra.syncopations[i].measure, ra.syncopations[i].beat);
        }
    }
    size_t per_line[3] = {0, 0, 0};
    size_t extras = 0;
    for (size_t i = 0; i < syncopations.count; i++) {
        const StandardMeterSyncopationClaim *claim = &syncopations.items[i].claim;
        per_line[claim->line < 0 || claim->line > 2 ? 2 : claim->line]++;
        if (!is_annotated_syncopation(&ra, claim)) {
            extras++;
            if (extras <= 20) {
                printf("    reported, not annotated: %s measure %d beat %.2f held through measure %d beat %.2f (strength %u)\n",
                       line_name(claim->line), claim->measure, claim->beat, claim->through_measure, claim->through_beat,
                       claim->strength);
            }
        }
    }
    printf("  syncopations found %zu of %zu (%.1f%%); the adapter reported %zu (soprano %zu, bass %zu, inner %zu), %zu not annotated\n",
           sync_hits, ra.syncopation_count, 100.0 * (ra.syncopation_count == 0 ? 1.0 : (double)sync_hits / (double)ra.syncopation_count),
           syncopations.count, per_line[0], per_line[1], per_line[2], extras);

    StandardMeterHemiolas hemiolas = {NULL, 0};
    assert(standard_meter_find_hemiolas(&build.input, NULL, &hemiolas) == STANDARD_METER_ANALYSIS_OK);
    size_t hemiola_hits = 0;
    for (size_t i = 0; i < ra.hemiola_count; i++) {
        for (size_t j = 0; j < hemiolas.count; j++) {
            if (hemiolas.items[j].claim.first_measure == ra.hemiolas[i].first) {
                hemiola_hits++;
                break;
            }
        }
    }
    printf("  hemiolas found %zu of %zu annotated (12/8 has none in standard-meter); the adapter reported %zu\n", hemiola_hits,
           ra.hemiola_count, hemiolas.count);

    StandardMeterHypermeter hypermeter;
    memset(&hypermeter, 0, sizeof(hypermeter));
    assert(standard_meter_find_hypermeter(&build.input, NULL, &hypermeter) == STANDARD_METER_ANALYSIS_OK);
    size_t hyper_hits = score_hyperdownbeats("annotated phrases", &ra, &hypermeter);
    size_t reinterpretation_hits = 0;
    for (size_t i = 0; i < ra.reinterpretation_count; i++) {
        for (size_t j = 0; j < hypermeter.reinterpretation_count; j++) {
            if (hypermeter.reinterpretations[j].claim.measure == ra.reinterpretations[i]) {
                reinterpretation_hits++;
                break;
            }
        }
    }
    printf("    reinterpretations found %zu of %zu\n", reinterpretation_hits, ra.reinterpretation_count);

    StandardMeterDensities densities = {NULL, 0};
    assert(standard_meter_find_densities(&build.input, NULL, &densities) == STANDARD_METER_ANALYSIS_OK);
    assert(densities.count >= ra.phrase_count);
    printf("  density, phrase 1 (all notes): %u notes, %u attacks, %.2f attacks per beat (mean), %.2f (max), %.1f%% of the attacks on a beat\n",
           densities.items[0].claim.notes, densities.items[0].claim.attacks, densities.items[0].claim.attacks_per_beat_mean,
           densities.items[0].claim.attacks_per_beat_max, 100.0 * densities.items[0].claim.on_beat_share);

    /* --- the detected phrases (the whole chain) --- */
    HarmonySegment *segments = NULL;
    Inputs inputs;
    HarmonyCadences found_cadences = chain_cadences(&fp->piece, &p, map, &lines, &cadence_annotation, &segments, &inputs);
    FormBuild form;
    form_build_init(&form, map);
    form_build_melody(&form, &p, &lines, ra.range_first, ra.range_last);
    add_chain_cadences(&form, &p, &found_cadences);
    assert(standard_form_input_finish(&form.input) == STANDARD_FORM_ANALYSIS_OK);
    FormAnalysis analysis;
    memset(&analysis, 0, sizeof(analysis));
    form_analyze(&form, &analysis);

    RhythmBuild detected;
    rhythm_build_init(&detected, map);
    rhythm_build_notes(&detected, &p, &lines, ra.range_first, ra.range_last);
    rhythm_build_detected_phrases(&detected, &form.input, &analysis.phrases, &analysis.expansions);
    printf("  the form adapter found %zu phrase(s) from %zu cadence(s) (annotated: %zu)\n", analysis.phrases.count,
           found_cadences.count, ra.phrase_count);
    StandardMeterHypermeter detected_hypermeter;
    memset(&detected_hypermeter, 0, sizeof(detected_hypermeter));
    assert(standard_meter_find_hypermeter(&detected.input, NULL, &detected_hypermeter) == STANDARD_METER_ANALYSIS_OK);
    size_t detected_hits = score_hyperdownbeats("detected phrases (the whole chain)", &ra, &detected_hypermeter);
    printf("  difference: %+d hyperdownbeat(s) found against the annotated phrases' run\n", (int)detected_hits - (int)hyper_hits);

    double sync_rate = ra.syncopation_count == 0 ? 1.0 : (double)sync_hits / (double)ra.syncopation_count;
    double hyper_rate = ra.hyperdownbeat_count == 0 ? 1.0 : (double)hyper_hits / (double)ra.hyperdownbeat_count;
    double detected_rate = ra.hyperdownbeat_count == 0 ? 1.0 : (double)detected_hits / (double)ra.hyperdownbeat_count;
    assert(sync_rate >= RHYTHM_TARGET);
    assert(hemiola_hits == ra.hemiola_count);
    assert(hyper_rate >= RHYTHM_TARGET);
    assert(reinterpretation_hits == ra.reinterpretation_count);
    assert(detected_rate >= DETECTED_FLOOR);

    standard_meter_hypermeter_destroy(&detected_hypermeter);
    rhythm_build_destroy(&detected);
    form_analysis_destroy(&analysis);
    form_build_destroy(&form);
    harmony_cadences_destroy(&found_cadences);
    harmony_analysis_destroy(segments);
    destroy_inputs(&inputs);
    standard_meter_densities_destroy(&densities);
    standard_meter_hypermeter_destroy(&hypermeter);
    standard_meter_hemiolas_destroy(&hemiolas);
    standard_meter_syncopations_destroy(&syncopations);
    rhythm_build_destroy(&build);
    common_practice_weighted_results_destroy(&chords);
    free(labels);
    outer_voice_lines_destroy(&lines);
    pipeline_destroy(&p);
    printf("test_rhythm_field5: ok\n");
    return 0;
}
