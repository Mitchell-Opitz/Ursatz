#define _POSIX_C_SOURCE 200809L
#include "chorale_chain.h"
#include "../integration/reference_pieces.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "analysis/outer_voice.h"
#include "standard-harmony/analysis/analysis_cadence.h"
#include "standard-tonality/analysis/applied_chords.h"
#include "standard-tonality/analysis/chord_input.h"
#include "core/rational.h"
#include "io/import_notes.h"
#include "io/musicxml_importer.h"
#include "common-practice-minimal/key_regions.h"
#include "common-practice-minimal/key_vocabulary.h"
#include "standard-meter/metric_position_rules.h"
#include "standard-tonality/analysis/roman_numeral_adapter.h"
#include "standard-meter/segmentation_beat_grid.h"
#include "time/duration.h"
#include "time/metrical_position.h"
#include "time/time_rational.h"
#include "time/time_span.h"
#include "ursatz_chain/analysis_chain.h"
#include "common-practice-minimal/weighted_chord_identification.h"
#include "../rational_test_util.h"

/* The analysis (segments, key regions in two passes, chords, Roman numerals) is analysis_chain_run on the imported notes. What stays here
   is the harness's own: the phrase ends it finds between the passes (cadence ends, on outer degrees read from the segment members), the
   RomanText annotation's numerals and the scoring. */

typedef struct {
    MusicXmlImport imported;
    bool have_import;
    ScoreLayout layout;
    bool have_layout;
    MeterMap *map;
    AnalysisNote *notes;
    size_t note_count;
    AnalysisKeyEvent *key_events;
    size_t key_event_count;
    AnalysisChain *first;  /* pass 1: the key regions from the pitch classes alone */
    AnalysisChain *chain;  /* pass 2: the key regions again, on pass 1's chords and the cadence ends as boundaries */
    CommonPracticeKeySegment *key_segments;
    size_t segment_count;
    MusicalTime **boundaries; /* the phrase ends found from pass 1, owned copies */
    size_t boundary_count;
    const CommonPracticeKeyRegions *regions;
    const StandardTonalityChordInput *chain_inputs;
    const StandardTonalityRomanResult *chain_romans;
    const double *starts;
    const double *ends;
    Key **annotation_keys;
    size_t annotation_key_count;
    StandardTonalityKeyRegionInput *annotation_region_inputs;
    StandardTonalitySegmentInput *segment_inputs;
    int *segment_pitches; /* the members' pitches, segment after segment, for the chord bass */
    StandardTonalityChordInput *annotation_inputs;
    StandardTonalityRomanResult *annotation_romans;
    RomanTextAnalysis annotation;
    bool have_annotation;
} Run;

static void run_destroy(Run *r) {
    if (r->annotation_romans != NULL) {
        standard_tonality_roman_results_destroy(r->annotation_romans, r->segment_count);
    }
    standard_tonality_chord_inputs_destroy(r->annotation_inputs);
    free(r->segment_inputs);
    free(r->segment_pitches);
    free(r->annotation_region_inputs);
    for (size_t i = 0; i < r->annotation_key_count; i++) {
        key_destroy(r->annotation_keys[i]);
    }
    free(r->annotation_keys);
    for (size_t i = 0; i < r->boundary_count; i++) {
        musical_time_destroy(r->boundaries[i]);
    }
    free(r->boundaries);
    chain_key_segments_destroy(r->key_segments);
    analysis_chain_destroy(r->chain);
    analysis_chain_destroy(r->first);
    free(r->key_events);
    import_analysis_notes_destroy(r->notes, r->note_count);
    if (r->map != NULL) {
        meter_map_destroy(r->map);
    }
    if (r->have_layout) {
        score_layout_destroy(&r->layout);
    }
    if (r->have_import) {
        musicxml_import_destroy(&r->imported);
    }
    if (r->have_annotation) {
        romantext_destroy(&r->annotation);
    }
}

static char *read_all(const char *path, size_t *size) {
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long length = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buffer = (char *)malloc((size_t)length + 1);
    if (buffer == NULL) {
        fclose(f);
        return NULL;
    }
    *size = fread(buffer, 1, (size_t)length, f);
    buffer[*size] = '\0';
    fclose(f);
    return buffer;
}

static void fail(char *message, size_t size, const char *text) {
    snprintf(message, size, "%s", text);
}

/* The region holding time `start`: the last whose start is not after it. */
static size_t region_at(const CommonPracticeKeyRegions *regions, double t) {
    size_t found = 0;
    for (size_t i = 0; i < regions->region_count; i++) {
        Rational start = test_rational_of_time(time_span_start(regions->regions[i].span));
        if (rational_to_double(start) <= t + 1e-12) {
            found = i;
        }
    }
    return found;
}

/* The numerals in the annotation's keys, over the chain's segments, chords and weights (the chain's own are in the chain's keys). */
static bool read_numerals(Run *r) {
    size_t n = r->segment_count;
    const ChainSegments *segments = analysis_chain_segments(r->chain);
    const CommonPracticeWeightedResult *results = analysis_chain_chords(r->chain);
    r->segment_inputs = (StandardTonalitySegmentInput *)calloc(n, sizeof(StandardTonalitySegmentInput));
    r->annotation_region_inputs =
        (StandardTonalityKeyRegionInput *)calloc(r->annotation.key_count + 1, sizeof(*r->annotation_region_inputs));
    r->annotation_keys = (Key **)calloc(r->annotation.key_count + 1, sizeof(Key *));
    size_t pitch_total = 0;
    for (size_t i = 0; i < n; i++) {
        pitch_total += segments->segments[i].member_count;
    }
    r->segment_pitches = (int *)calloc(pitch_total > 0 ? pitch_total : 1, sizeof(int));
    if (r->segment_pitches == NULL || r->segment_inputs == NULL || r->annotation_region_inputs == NULL || r->annotation_keys == NULL) {
        return false;
    }
    pitch_total = 0;
    for (size_t i = 0; i < n; i++) {
        r->segment_inputs[i].segment_id = segments->ids[i];
        r->segment_inputs[i].start = r->key_segments[i].start;
        r->segment_inputs[i].chord = results[i].matched ? results[i].best_chord : NULL;
        r->segment_inputs[i].weights = r->key_segments[i].weights.pitch_class;
        r->segment_inputs[i].pitches = &r->segment_pitches[pitch_total];
        r->segment_inputs[i].pitch_count = segments->segments[i].member_count;
        for (size_t j = 0; j < segments->segments[i].member_count; j++) {
            r->segment_pitches[pitch_total++] = segments->segments[i].members[j].pitch;
        }
    }
    size_t count = 0;
    /* the annotation's keys as kernel Keys; where two keys are written at one time (an analyst's second reading
       of one chord), the later one holds, as in key_at, and the regions need distinct starts */
    size_t region_total = 0;
    for (size_t i = 0; i < r->annotation.key_count; i++) {
        if (i + 1 < r->annotation.key_count &&
            musical_time_equals(r->annotation.keys[i].time, r->annotation.keys[i + 1].time)) {
            continue;
        }
        const RomanTextKey *k = &r->annotation.keys[i];
        CommonPracticeKeyMode mode = k->minor ? COMMON_PRACTICE_KEY_MINOR : COMMON_PRACTICE_KEY_MAJOR;
        int index = common_practice_key_index_of(k->tonic_pitch_class, mode);
        char id[48];
        snprintf(id, sizeof(id), "annotation-key-%zu", region_total);
        EntityID *context = NULL;
        if (entity_id_create(id, &context) != ENTITY_ID_OK ||
            common_practice_build_key(mode, common_practice_key_index_tonic_name(index), context,
                                      &r->annotation_keys[region_total]) != KEY_OK) {
            return false;
        }
        r->annotation_key_count++;
        r->annotation_region_inputs[region_total].key = r->annotation_keys[region_total];
        r->annotation_region_inputs[region_total].start = r->annotation.keys[i].time;
        r->annotation_region_inputs[region_total].uncertainty = 0.0;
        region_total++;
    }
    if (standard_tonality_build_chord_inputs(r->segment_inputs, n, r->annotation_region_inputs, region_total,
                                             &r->annotation_inputs, &count) != STANDARD_TONALITY_CHORD_INPUT_OK ||
        standard_tonality_build_roman_numerals(r->annotation_inputs, count, &r->annotation_romans) !=
            STANDARD_TONALITY_ROMAN_OK) {
        return false;
    }
    return true;
}

static bool same_set(const int *a, size_t an, const int *b, size_t bn) {
    if (an != bn) {
        return false;
    }
    for (size_t i = 0; i < an; i++) {
        if (a[i] != b[i]) {
            return false;
        }
    }
    return true;
}

static bool numeral_matches(const RomanTextChord *want, const StandardTonalityRomanResult *got) {
    if (!got->has_numeral) {
        return false;
    }
    const StandardTonalityRomanClaim *claim = &got->claim;
    if (claim->applied != want->applied || claim->inversion != want->inversion || claim->seventh != want->seventh) {
        return false;
    }
    if (want->applied) {
        if (claim->applied_target_degree != want->target_degree || want->target_alteration != 0 ||
            claim->degree != want->degree) {
            return false;
        }
    } else if (claim->degree != want->degree || (want->alteration != 0 && !romantext_is_raised_sixth_diminished(want))) {
        return false;
    }
    int root = 0;
    int want_intervals[4];
    size_t want_count = romantext_expected_chord(want, &root, want_intervals);
    int got_intervals[4];
    size_t got_count = 0;
    claim_intervals(claim, got_intervals, &got_count);
    return same_set(want_intervals, want_count, got_intervals, got_count);
}

/* --- phrase ends --- */

static const int DEGREE_OF_OFFSET[12] = {1, 0, 2, 0, 3, 4, 0, 5, 0, 6, 0, 7};

/* The degree (1..7) of a pitch in the major key on `tonic`, or 0 for a chromatic note. */
static int degree_of_pitch(int pitch, int tonic) {
    return DEGREE_OF_OFFSET[(((pitch - tonic) % 12) + 12) % 12];
}

/* The length of a beat, from the first measure's beat grid. */
static bool beat_duration(const MeterMap *map, Duration **out) {
    MusicalTime **starts = NULL;
    size_t count = 0;
    TimeSpan *span = NULL;
    if (!standard_meter_segmentation_beat_grid(map, 1, NULL, &starts, &count) || count == 0) {
        return false;
    }
    bool ok = false;
    if (meter_map_measure_span(map, 1, &span) == METER_MAP_OK) {
        Rational next = count > 1 ? test_rational_of_time(starts[1]) : test_rational_of_time(time_span_end(span));
        Rational length = {0, 1};
        ok = rational_sub(next, test_rational_of_time(starts[0]), &length) == 0 &&
             duration_create((long)length.num, (unsigned long)length.den, out) == DURATION_OK;
        time_span_destroy(span);
    }
    for (size_t b = 0; b < count; b++) {
        musical_time_destroy(starts[b]);
    }
    free(starts);
    return ok;
}

/* The ends of the chain's cadences, as the phrase ends for the second pass. The kernel's phrase boundary detection
   takes a detected cadence as a boundary whatever its type, and a chorale has no second signal, so the boundaries are
   the cadences' arrival ends. The outer voices are the highest and lowest pitch of each segment. */
static bool find_phrase_ends(Run *r) {
    size_t n = r->segment_count;
    const ChainSegments *segs = analysis_chain_segments(r->first);
    const StandardTonalityChordInput *first_inputs = analysis_chain_chord_inputs(r->first);
    const StandardTonalityRomanResult *first_romans = analysis_chain_numerals(r->first);
    HarmonySegmentInput *inputs = (HarmonySegmentInput *)calloc(n, sizeof(HarmonySegmentInput));
    MetricalPosition **positions = (MetricalPosition **)calloc(n, sizeof(MetricalPosition *));
    Duration *beat_length = NULL;
    HarmonySegment *prepared = NULL;
    HarmonyCadences cadences = {NULL, 0};
    bool ok = inputs != NULL && positions != NULL && beat_duration(r->map, &beat_length);
    for (size_t i = 0; i < n && ok; i++) {
        const TimeSpan *window = sonority_time_window(segs->sonorities.items[i].sonority);
        ok = metrical_position_from_time(r->map, time_span_start(window), &positions[i]) == METRICAL_POSITION_OK;
        if (!ok) {
            break;
        }
        HarmonySegmentInput *input = &inputs[i];
        input->segment_id = segs->ids[i];
        input->span = window;
        input->position = positions[i];
        input->beat_strength = chain_attack_strength(r->map, time_span_start(window));
        input->key_ref = "key";
        input->mode = HARMONY_MODE_MAJOR;
        input->beat_length = beat_length;
        if (!first_romans[i].has_numeral) {
            input->degree = 0;
            input->interval_count = 3;
            continue;
        }
        const StandardTonalityRomanClaim *claim = &first_romans[i].claim;
        int intervals[4];
        size_t interval_count = 0;
        claim_intervals(claim, intervals, &interval_count);
        for (size_t k = 0; k < interval_count; k++) {
            input->intervals[k] = intervals[k];
        }
        input->interval_count = interval_count;
        input->degree = claim->degree;
        input->inversion = claim->inversion;
        input->applied_to = claim->applied ? claim->applied_target_degree : 0;
        input->mode = first_inputs[i].mode == STANDARD_TONALITY_ROMAN_KEY_MINOR ? HARMONY_MODE_MINOR : HARMONY_MODE_MAJOR;
        input->uncertainty = first_romans[i].uncertainty;
        int low = 1000;
        int high = -1000;
        for (size_t j = 0; j < segs->segments[i].member_count; j++) {
            int pitch = segs->segments[i].members[j].pitch;
            low = pitch < low ? pitch : low;
            high = pitch > high ? pitch : high;
        }
        input->bass_degree = segs->segments[i].member_count > 0 ? degree_of_pitch(low, first_inputs[i].tonic_pitch_class) : 0;
        input->soprano_degree = segs->segments[i].member_count > 0 ? degree_of_pitch(high, first_inputs[i].tonic_pitch_class) : 0;
    }
    ok = ok && harmony_analysis_prepare(inputs, n, &prepared) == HARMONY_ANALYSIS_OK &&
         harmony_analysis_find_cadences(prepared, n, NULL, &cadences) == HARMONY_CADENCE_ANALYSIS_OK;
    if (ok && cadences.count > 0) {
        r->boundaries = (MusicalTime **)calloc(cadences.count, sizeof(MusicalTime *));
        ok = r->boundaries != NULL;
        for (size_t k = 0; k < cadences.count && ok; k++) {
            const MusicalTime *end = r->key_segments[cadences.items[k].arrival_last].end;
            ok = musical_time_create(musical_time_numerator(end), musical_time_denominator(end),
                                     &r->boundaries[r->boundary_count]) == MUSICAL_TIME_OK;
            r->boundary_count += ok ? 1 : 0;
        }
    }
    harmony_cadences_destroy(&cadences);
    harmony_analysis_destroy(prepared);
    for (size_t i = 0; i < n && positions != NULL; i++) {
        metrical_position_destroy(positions[i]);
    }
    duration_destroy(beat_length);
    free(positions);
    free(inputs);
    return ok;
}

/* --- the comparison --- */


static int compare_double(const void *a, const void *b) {
    double x = *(const double *)a;
    double y = *(const double *)b;
    return x < y ? -1 : (x > y ? 1 : 0);
}

static const RomanTextChord *chord_at(const RomanTextAnalysis *a, double t) {
    const RomanTextChord *found = NULL;
    for (size_t i = 0; i < a->chord_count; i++) {
        Rational at = test_rational_of_time(a->chords[i].time);
        if (rational_to_double(at) <= t + 1e-12) {
            found = &a->chords[i];
        }
    }
    return found;
}

static const RomanTextKey *key_at(const RomanTextAnalysis *a, double t) {
    const RomanTextKey *found = NULL;
    for (size_t i = 0; i < a->key_count; i++) {
        Rational at = test_rational_of_time(a->keys[i].time);
        if (rational_to_double(at) <= t + 1e-12) {
            found = &a->keys[i];
        }
    }
    return found;
}

static long segment_at(const Run *r, double t) {
    for (size_t s = 0; s < r->segment_count; s++) {
        if (r->starts[s] <= t + 1e-12 && t < r->ends[s] - 1e-12) {
            return (long)s;
        }
    }
    return -1;
}

static void compare(const Run *r, ChoraleScore *out) {
    memset(out, 0, sizeof(*out));
    out->segments = r->segment_count;
    out->annotated_chords = r->annotation.chord_count;
    out->unsupported_chords = r->annotation.unsupported_chords;
    out->key_regions = r->regions->region_count;

    size_t capacity = r->segment_count * 2 + r->annotation.chord_count + r->annotation.key_count + 4;
    double *cuts = (double *)malloc(capacity * sizeof(double));
    if (cuts == NULL || r->annotation.chord_count == 0) {
        free(cuts);
        return;
    }
    size_t n = 0;
    double first = rational_to_double(test_rational_of_time(r->annotation.chords[0].time));
    double end = rational_to_double(r->layout.end);
    cuts[n++] = first;
    cuts[n++] = end;
    for (size_t s = 0; s < r->segment_count; s++) {
        cuts[n++] = r->starts[s];
        cuts[n++] = r->ends[s];
    }
    for (size_t i = 0; i < r->annotation.chord_count; i++) {
        cuts[n++] = rational_to_double(test_rational_of_time(r->annotation.chords[i].time));
    }
    for (size_t i = 0; i < r->annotation.key_count; i++) {
        cuts[n++] = rational_to_double(test_rational_of_time(r->annotation.keys[i].time));
    }
    qsort(cuts, n, sizeof(double), compare_double);

    for (size_t i = 0; i + 1 < n; i++) {
        double a = cuts[i];
        double b = cuts[i + 1];
        if (a < first - 1e-12 || b > end + 1e-12 || b - a < 1e-12) {
            continue;
        }
        double w = b - a;
        const RomanTextChord *chord = chord_at(&r->annotation, a);
        const RomanTextKey *key = key_at(&r->annotation, a);
        long s = segment_at(r, a);
        out->annotated_time += w;
        if (key != NULL && s >= 0) {
            const CommonPracticeKeyRegion *region = &r->regions->regions[region_at(r->regions, r->starts[s])];
            int tonic = common_practice_key_index_tonic(region->key_index);
            bool minor = common_practice_key_index_mode(region->key_index) == COMMON_PRACTICE_KEY_MINOR;
            if (tonic == key->tonic_pitch_class && minor == key->minor) {
                out->key_right += w;
            }
        }
        if (chord == NULL) {
            continue;
        }
        if (!chord->supported) {
            out->unsupported_time += w;
            continue;
        }
        out->chord_time += w;
        if (s < 0) {
            continue;
        }
        int root = 0;
        int want_intervals[4];
        size_t want_count = romantext_expected_chord(chord, &root, want_intervals);
        const StandardTonalityChordInput *chain = &r->chain_inputs[s];
        if (chain->has_numeral) {
            if (chain->root_pitch_class == root) {
                out->root_right += w;
            }
            if (same_set(want_intervals, want_count, chain->intervals, chain->interval_count)) {
                out->quality_right += w;
            }
        }
        if (numeral_matches(chord, &r->annotation_romans[s])) {
            out->numeral_right += w;
        }
        if (numeral_matches(chord, &r->chain_romans[s])) {
            out->numeral_chain_key_right += w;
        }
    }
    free(cuts);
}

ChoraleChainStatus chorale_chain_run(const char *piece_dir, ChoraleScore *out, char *message, size_t message_size) {
    Run r;
    memset(&r, 0, sizeof(r));
    char path[1024];
    snprintf(path, sizeof(path), "%s/score.musicxml", piece_dir);
    size_t score_size = 0;
    char *score = read_all(path, &score_size);
    if (score == NULL) {
        snprintf(message, message_size, "cannot read %s", path);
        return CHORALE_CHAIN_ERR_IO;
    }
    ChoraleChainStatus status = CHORALE_CHAIN_ERR_CHAIN;
    if (musicxml_import_buffer(score, score_size, &r.imported) != MUSICXML_IMPORT_OK) {
        snprintf(message, message_size, "import failed: %s", r.imported.error_message);
        free(score);
        return CHORALE_CHAIN_ERR_IMPORT;
    }
    r.have_import = true;
    if (score_layout_read(score, score_size, &r.layout) != SCORE_LAYOUT_OK) {
        fail(message, message_size, "the measure layout could not be read");
        status = CHORALE_CHAIN_ERR_LAYOUT;
        goto done;
    }
    r.have_layout = true;
    r.map = score_layout_meter_map(&r.layout, "accuracy-score-meter");
    if (r.map == NULL) {
        fail(message, message_size, "no meter map from the layout");
        status = CHORALE_CHAIN_ERR_LAYOUT;
        goto done;
    }
    snprintf(path, sizeof(path), "%s/analysis.rntxt", piece_dir);
    RomanTextGrid grid = score_layout_grid(&r.layout);
    RomanTextStatus loaded = romantext_load(path, &grid, &r.annotation);
    if (loaded != ROMANTEXT_OK) {
        snprintf(message, message_size, "analysis: %s", r.annotation.error_message);
        status = CHORALE_CHAIN_ERR_ANNOTATION;
        goto done;
    }
    r.have_annotation = true;
    if (!musicxml_import_analysis_notes(&r.imported, &r.notes, &r.note_count)) {
        fail(message, message_size, "no analysis notes from the import");
        goto done;
    }
    r.key_event_count = r.imported.key_signature_count;
    r.key_events = (AnalysisKeyEvent *)calloc(r.key_event_count + 1, sizeof(AnalysisKeyEvent));
    if (r.key_events == NULL) {
        goto done;
    }
    for (size_t i = 0; i < r.key_event_count; i++) {
        r.key_events[i].time = r.imported.key_signatures[i].position;
        if (!analysis_key_from_fifths(r.imported.key_signatures[i].sharps_flats, KEY_STRUCTURE_MODE_MAJOR, &r.key_events[i].key)) {
            goto done;
        }
    }
    AnalysisNoteSet set = {r.notes, r.note_count, r.map};
    /* Pass 1: the key regions from the pitch classes alone, then chords and numerals in them. The phrase ends are found from those
       (cadence ends). Pass 2: the key regions again with those chords as evidence and the cadence ends as boundaries, and everything after
       them once more. No further iteration. */
    AnalysisChainConfig config = analysis_chain_config_default();
    config.stages = ANALYSIS_CHAIN_STAGE_NUMERALS;
    config.first_measure = 0;
    config.refine_keys = false;
    if (analysis_chain_run(&set, r.key_events, r.key_event_count, &config, &r.first) != ANALYSIS_CHAIN_OK) {
        fail(message, message_size, "the first pass failed");
        goto done;
    }
    r.segment_count = analysis_chain_segment_count(r.first);
    if (chain_build_key_segments(analysis_chain_segments(r.first), r.segment_count, NULL, &r.key_segments) != CHAIN_INPUT_OK) {
        fail(message, message_size, "key segments failed");
        goto done;
    }
    if (!find_phrase_ends(&r)) {
        fail(message, message_size, "the cadences failed");
        goto done;
    }
    config.refine_keys = true;
    config.refine_with_given_boundaries = true;
    config.key_regions.phrase_boundaries = (const MusicalTime *const *)r.boundaries;
    config.key_regions.phrase_boundary_count = r.boundary_count;
    if (analysis_chain_run(&set, r.key_events, r.key_event_count, &config, &r.chain) != ANALYSIS_CHAIN_OK) {
        fail(message, message_size, "the second pass failed");
        goto done;
    }
    r.regions = analysis_chain_key_regions(r.chain);
    r.chain_inputs = analysis_chain_chord_inputs(r.chain);
    r.chain_romans = analysis_chain_numerals(r.chain);
    r.starts = analysis_chain_segments(r.chain)->starts;
    r.ends = analysis_chain_segments(r.chain)->ends;
    if (!read_numerals(&r)) {
        fail(message, message_size, "the Roman numeral adapter failed");
        goto done;
    }
    compare(&r, out);
    status = CHORALE_CHAIN_OK;
done:
    run_destroy(&r);
    free(score);
    return status;
}
