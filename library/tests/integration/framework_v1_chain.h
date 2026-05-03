#ifndef URSATZ_TESTS_INTEGRATION_FRAMEWORK_V1_CHAIN_H
#define URSATZ_TESTS_INTEGRATION_FRAMEWORK_V1_CHAIN_H

/*
 * The full chain over one Framework v1 reference piece, and the expected values it is checked against (test glue).
 *
 * THE CHAIN. Import, the piece's MeterMap (Bach 114's file declares 4/4 and the piece is 3/4, so a 3/4 map replaces it, as in the other
 * reference-piece tests), adaptive segments, weights, local key regions (with the file's key signature events), weighted chord
 * identification in each key region's key and mode (a minor region is read in the minor vocabulary),
 * the Roman numeral adapter, outer voices, the cadence adapter, and the L3.6 standard-form adapter (phrases, ideas, themes, expansions) on
 * the cadences the chain found. No stage is given an answer: the key is not the annotation's home key, and the cadences are not the hand
 * cadences. Every default is the library's.
 *
 * It includes test_form_reference_pieces.c as a library, the way test_rhythm_field5.c does, so the helpers of the cadence and form tests are
 * the same code those tests run.
 *
 * THE EXPECTED VALUES are tests/fixtures/annotations/<piece>_v1_expected.txt (the format is in the header of those files).
 */

#define URSATZ_FORM_PIECES_AS_LIBRARY
#include "test_form_reference_pieces.c"

#include "io/midi_key_signature.h"

/* --- the expected values --- */

typedef struct {
    int first;
    int last;
    int key_index;
    bool unscored;
} XRegion;

typedef struct {
    int measure;
    int extent; /* KeyChangeExtent */
    bool is_return;
} XChange;

typedef struct {
    int measure;
    double beat;
    CadenceType type;
    char type_text[24];
} XCadence;

typedef struct {
    int measure;
    CadenceType type;
    char type_text[24];
} XEnd;

typedef struct {
    PhraseLevelForm form;
    int first;
    int last;
    bool has_parts;
    int a_first;
    int a_last;
    int b_first;
    int b_last;
} XTheme;

typedef struct {
    StandardFormExpansionKind kind;
    int first;
    int last;
    int measures;
} XExpansion;

typedef struct {
    char name[24];
    int first;
    int last;
    bool is_return;
} XOccurrence;

typedef struct {
    char piece[24];
    int beats;
    long beat_num;
    long beat_den;
    int home_key;
    int keys_first, keys_last;
    int cadence_first, cadence_last;
    int form_first, form_last;
    XRegion regions[24];
    size_t region_count;
    XChange changes[16];
    size_t change_count;
    XCadence cadences[24];
    size_t cadence_count;
    XEnd ends[24];
    size_t end_count;
    XTheme themes[8];
    size_t theme_count;
    XExpansion expansions[4];
    size_t expansion_count;
    XOccurrence occurrences[16];
    size_t occurrence_count;
} V1Expected;

static int fv1_key_index(const char *tonic, const char *mode) {
    static const char *const NAMES[12] = {"C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};
    for (int pc = 0; pc < 12; pc++) {
        if (strcmp(tonic, NAMES[pc]) == 0) {
            return common_practice_key_index_of(pc, strcmp(mode, "minor") == 0 ? COMMON_PRACTICE_KEY_MINOR
                                                                                : COMMON_PRACTICE_KEY_MAJOR);
        }
    }
    assert(!"unknown tonic in the expected file");
    return -1;
}

static int fv1_extent_of(const char *text) {
    for (int e = 0; e < (int)KEY_CHANGE_EXTENT_COUNT; e++) {
        if (strcmp(text, key_change_extent_name((KeyChangeExtent)e)) == 0) {
            return e;
        }
    }
    assert(!"unknown extent in the expected file");
    return -1;
}

static CadenceType fv1_cadence_type(const char *text) {
    return strcmp(text, "pac") == 0         ? CADENCE_TYPE_PERFECT_AUTHENTIC
           : strcmp(text, "iac") == 0       ? CADENCE_TYPE_IMPERFECT_AUTHENTIC
           : strcmp(text, "hc") == 0        ? CADENCE_TYPE_HALF
           : strcmp(text, "deceptive") == 0 ? CADENCE_TYPE_DECEPTIVE
           : strcmp(text, "plagal") == 0    ? CADENCE_TYPE_PLAGAL
                                            : CADENCE_TYPE_SUBVERTED;
}

static void fv1_load_expected(const char *prefix, V1Expected *x) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s_v1_expected.txt", URSATZ_ANNOTATION_DIR, prefix);
    size_t size = 0;
    unsigned char *text = pipeline_read_file(path, &size);
    memset(x, 0, sizeof(*x));
    x->home_key = -1;
    for (char *line = strtok((char *)text, "\n"); line != NULL; line = strtok(NULL, "\n")) {
        if (line[0] == '#' || line[0] == '\0') {
            continue;
        }
        char word[24] = "";
        char a[24] = "";
        char b[24] = "";
        char c[24] = "";
        int i1 = 0;
        int i2 = 0;
        int i3 = 0;
        int i4 = 0;
        sscanf(line, "%23s", word);
        if (strcmp(word, "piece") == 0) {
            assert(sscanf(line, "piece %23s", x->piece) == 1);
        } else if (strcmp(word, "meter") == 0) {
            assert(sscanf(line, "meter %d %ld/%ld", &x->beats, &x->beat_num, &x->beat_den) == 3);
        } else if (strcmp(word, "home") == 0) {
            assert(sscanf(line, "home %23s %23s", a, b) == 2);
            x->home_key = fv1_key_index(a, b);
        } else if (strcmp(word, "keys-scope") == 0) {
            assert(sscanf(line, "keys-scope %d %d", &x->keys_first, &x->keys_last) == 2);
        } else if (strcmp(word, "cadence-scope") == 0) {
            assert(sscanf(line, "cadence-scope %d %d", &x->cadence_first, &x->cadence_last) == 2);
        } else if (strcmp(word, "form-scope") == 0) {
            assert(sscanf(line, "form-scope %d %d", &x->form_first, &x->form_last) == 2);
        } else if (strcmp(word, "region") == 0) {
            assert(sscanf(line, "region %d %d %23s %23s", &i1, &i2, a, b) == 4 && x->region_count < 24);
            XRegion *r = &x->regions[x->region_count++];
            r->first = i1;
            r->last = i2;
            r->key_index = fv1_key_index(a, b);
        } else if (strcmp(word, "unscored") == 0) {
            assert(sscanf(line, "unscored %d %d", &i1, &i2) == 2 && x->region_count < 24);
            XRegion *r = &x->regions[x->region_count++];
            r->first = i1;
            r->last = i2;
            r->unscored = true;
        } else if (strcmp(word, "change") == 0) {
            int n = sscanf(line, "change %d %23s %23s", &i1, a, b);
            assert(n >= 2 && x->change_count < 16);
            XChange *ch = &x->changes[x->change_count++];
            ch->measure = i1;
            ch->extent = fv1_extent_of(a);
            ch->is_return = n == 3 && strcmp(b, "return") == 0;
        } else if (strcmp(word, "cadence") == 0) {
            double beat = 0.0;
            assert(sscanf(line, "cadence %d %lf %23s", &i1, &beat, a) == 3 && x->cadence_count < 24);
            XCadence *cd = &x->cadences[x->cadence_count++];
            cd->measure = i1;
            cd->beat = beat;
            cd->type = fv1_cadence_type(a);
            snprintf(cd->type_text, sizeof(cd->type_text), "%s", a);
        } else if (strcmp(word, "phrase-end") == 0) {
            assert(sscanf(line, "phrase-end %d %23s", &i1, a) == 2 && x->end_count < 24);
            XEnd *e = &x->ends[x->end_count++];
            e->measure = i1;
            e->type = fv1_cadence_type(a);
            snprintf(e->type_text, sizeof(e->type_text), "%s", a);
        } else if (strcmp(word, "theme") == 0) {
            assert(x->theme_count < 8);
            XTheme *t = &x->themes[x->theme_count++];
            int n = sscanf(line, "theme %23s %d %d %23s %d %d %d %d", a, &i1, &i2, b, &t->a_first, &t->a_last, &t->b_first,
                           &t->b_last);
            assert(n == 3 || (n == 8 && strcmp(b, "parts") == 0));
            t->form = form_of_text(a);
            t->first = i1;
            t->last = i2;
            t->has_parts = n == 8;
        } else if (strcmp(word, "expansion") == 0) {
            assert(sscanf(line, "expansion %23s %d %d %d", a, &i1, &i2, &i3) == 4 && x->expansion_count < 4);
            XExpansion *e = &x->expansions[x->expansion_count++];
            e->kind = strcmp(a, "internal") == 0 ? STANDARD_FORM_EXPANSION_INTERNAL
                                                 : (strcmp(a, "external") == 0 ? STANDARD_FORM_EXPANSION_EXTERNAL
                                                                               : STANDARD_FORM_EXPANSION_CONTRACTION);
            e->first = i1;
            e->last = i2;
            e->measures = i3;
        } else if (strcmp(word, "original") == 0 || strcmp(word, "return") == 0) {
            assert(sscanf(line, "%*s %23s %d %d", a, &i1, &i2) == 3 && x->occurrence_count < 16);
            XOccurrence *o = &x->occurrences[x->occurrence_count++];
            snprintf(o->name, sizeof(o->name), "%s", a);
            o->first = i1;
            o->last = i2;
            o->is_return = strcmp(word, "return") == 0;
        } else {
            assert(!"unrecognised line in the expected file");
        }
        (void)c;
        (void)i4;
        (void)i3;
    }
    free(text);
    assert(x->beats > 0 && x->home_key >= 0 && x->keys_last >= x->keys_first && x->cadence_last >= x->cadence_first);
}

/* --- the chain --- */

typedef struct {
    const FormPiece *fp;
    V1Expected x;
    MeterMap *own;
    WeightedPipeline p;
    const MeterMap *map;
    size_t count; /* segments of the chain: those that start inside the keys scope */
    Annotation shim; /* only the beat length is read from it (make_inputs) */

    CommonPracticeKeySegment *key_segments;
    MidiKeySignatureEvents scanned;
    CommonPracticeKeySignatureEvent *events;
    CommonPracticeKeyRegions regions;
    CommonPracticeWeightedResult *chords; /* one per segment */
    StandardTonalityKeyRegionInput *region_inputs;
    StandardTonalitySegmentInput *segment_inputs;
    StandardTonalityChordInput *tonal_inputs;
    size_t tonal_count;
    StandardTonalityRomanResult *romans;
    OuterVoiceLines lines;
    Inputs in;
    HarmonySegment *harmony;
    HarmonyCadences cadences;
    FormBuild build;
    FormAnalysis form;
} Chain;

static double fv1_time_double(const MusicalTime *t) {
    return (double)musical_time_numerator(t) / (double)musical_time_denominator(t);
}

/* The region holding the segment that starts at `start` whole notes: the last whose start is not after it. */
static size_t fv1_region_at(const CommonPracticeKeyRegions *regions, double start) {
    size_t found = 0;
    for (size_t i = 0; i < regions->region_count; i++) {
        if (fv1_time_double(time_span_start(regions->regions[i].span)) <= start + 1e-12) {
            found = i;
        }
    }
    return found;
}

static void fv1_chain_run(Chain *c, const FormPiece *fp) {
    memset(c, 0, sizeof(*c));
    c->fp = fp;
    fv1_load_expected(fp->prefix, &c->x);
    const V1Expected *x = &c->x;
    c->own = fp->piece.score_three_four ? three_four_map() : NULL;
    c->p = pipeline_run(URSATZ_CORPUS_DIR, fp->piece.midi, c->own);
    c->map = c->own != NULL ? c->own : c->p.meter.meter_map;
    c->shim.beats = x->beats;
    c->shim.beat_num = x->beat_num;
    c->shim.beat_den = x->beat_den;
    c->shim.beat_length = (double)x->beat_num / (double)x->beat_den;

    /* the segments of the keys scope */
    TimeSpan *last_measure = NULL;
    assert(meter_map_measure_span(c->map, x->keys_last, &last_measure) == METER_MAP_OK);
    double scope_end = fv1_time_double(time_span_end(last_measure));
    time_span_destroy(last_measure);
    while (c->count < c->p.segment_count && c->p.starts[c->count] < scope_end - 1e-12) {
        c->count++;
    }
    assert(c->count > 0);
    size_t count = c->count;

    /* weights, and the key regions with the file's key signature events */
    CommonPracticeWeightedConfig weighting = common_practice_weighted_config_default();
    c->key_segments = (CommonPracticeKeySegment *)calloc(count, sizeof(CommonPracticeKeySegment));
    assert(c->key_segments != NULL);
    for (size_t s = 0; s < count; s++) {
        const TimeSpan *window = sonority_time_window(c->p.sonorities.items[s].sonority);
        c->key_segments[s].start = time_span_start(window);
        c->key_segments[s].end = time_span_end(window);
        c->key_segments[s].source = &c->p.segments[s];
        assert(common_practice_weight_segment(c->p.segments[s].members, c->p.segments[s].member_count, &weighting,
                                              &c->key_segments[s].weights) == COMMON_PRACTICE_WEIGHTED_OK);
    }
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", URSATZ_CORPUS_DIR, fp->piece.midi);
    size_t size = 0;
    unsigned char *data = pipeline_read_file(path, &size);
    assert(midi_key_signature_scan_all(data, size, &c->scanned) == MIDI_KEY_SIGNATURE_SCAN_ALL_OK);
    free(data);
    c->events = (CommonPracticeKeySignatureEvent *)calloc(c->scanned.count + 1, sizeof(CommonPracticeKeySignatureEvent));
    assert(c->events != NULL);
    for (size_t i = 0; i < c->scanned.count; i++) {
        c->events[i].position = c->scanned.events[i].position;
        c->events[i].sharps_flats = c->scanned.events[i].sharps_flats;
    }
    assert(key_regions_estimate(c->key_segments, count, c->map, c->events, c->scanned.count, NULL, &c->regions) ==
           COMMON_PRACTICE_KEY_REGIONS_OK);

    /* chords in each region's own key and mode */
    c->chords = (CommonPracticeWeightedResult *)calloc(count, sizeof(CommonPracticeWeightedResult));
    assert(c->chords != NULL);
    size_t s = 0;
    while (s < count) {
        size_t region = fv1_region_at(&c->regions, c->p.starts[s]);
        size_t end = s + 1;
        while (end < count && fv1_region_at(&c->regions, c->p.starts[end]) == region) {
            end++;
        }
        int key_index = c->regions.regions[region].key_index;
        int tonic = common_practice_key_index_tonic(key_index);
        CommonPracticeKeyMode mode = common_practice_key_index_mode(key_index);
        CommonPracticeWeightedResults group = {0};
        assert(common_practice_identify_weighted_sequence(&c->p.segments[s], end - s, tonic, mode, NULL, &group) ==
               COMMON_PRACTICE_WEIGHTED_OK);
        for (size_t i = 0; i < group.count; i++) {
            c->chords[s + i] = group.items[i];
        }
        free(group.items);
        s = end;
    }

    /* Roman numerals in the chain's keys */
    c->region_inputs = (StandardTonalityKeyRegionInput *)calloc(c->regions.region_count, sizeof(StandardTonalityKeyRegionInput));
    c->segment_inputs = (StandardTonalitySegmentInput *)calloc(count, sizeof(StandardTonalitySegmentInput));
    assert(c->region_inputs != NULL && c->segment_inputs != NULL);
    for (size_t i = 0; i < c->regions.region_count; i++) {
        c->region_inputs[i].key = c->regions.regions[i].key;
        c->region_inputs[i].start = time_span_start(c->regions.regions[i].span);
        c->region_inputs[i].uncertainty = c->regions.regions[i].uncertainty;
    }
    for (size_t i = 0; i < count; i++) {
        c->segment_inputs[i].segment_id = c->p.segment_ids[i];
        c->segment_inputs[i].start = c->key_segments[i].start;
        c->segment_inputs[i].chord = c->chords[i].matched ? c->chords[i].best_chord : NULL;
        c->segment_inputs[i].weights = c->key_segments[i].weights.pitch_class;
    }
    assert(standard_tonality_build_chord_inputs(c->segment_inputs, count, c->region_inputs, c->regions.region_count,
                                                &c->tonal_inputs, &c->tonal_count) == STANDARD_TONALITY_CHORD_INPUT_OK);
    assert(standard_tonality_build_roman_numerals(c->tonal_inputs, c->tonal_count, &c->romans) == STANDARD_TONALITY_ROMAN_OK);

    /* outer voices and the cadences */
    assert(outer_voice_extract(&c->p.voice_sonorities, c->p.events, c->p.pitch_values, c->p.note_count, c->map,
                               standard_meter_segmentation_beat_grid, NULL, NULL, &c->lines) == OUTER_VOICE_OK);
    c->in = make_inputs(&c->p, c->map, &c->shim, count);
    for (size_t i = 0; i < count; i++) {
        HarmonySegmentInput *input = &c->in.inputs[i];
        if (!c->romans[i].has_numeral) {
            input->degree = 0;
            input->interval_count = 3;
            continue;
        }
        const StandardTonalityRomanClaim *claim = &c->romans[i].claim;
        int intervals[4];
        size_t interval_count = 0;
        intervals_of(claim, intervals, &interval_count);
        set_intervals(input, intervals, interval_count);
        input->degree = claim->degree;
        input->inversion = claim->inversion;
        input->applied_to = claim->applied ? claim->applied_target_degree : 0;
        input->mode = c->tonal_inputs[i].mode == STANDARD_TONALITY_ROMAN_KEY_MINOR ? HARMONY_MODE_MINOR : HARMONY_MODE_MAJOR;
        input->uncertainty = c->romans[i].uncertainty;
        input->bass_degree = degree_of(&c->lines.bass[i], c->tonal_inputs[i].tonic_pitch_class);
        input->soprano_degree = degree_of(&c->lines.soprano[i], c->tonal_inputs[i].tonic_pitch_class);
    }
    assert(harmony_analysis_prepare(c->in.inputs, count, &c->harmony) == HARMONY_ANALYSIS_OK);
    assert(harmony_analysis_find_cadences(c->harmony, count, NULL, &c->cadences) == HARMONY_CADENCE_ANALYSIS_OK);

    /* the L3.6 adapter over the keys scope, on the cadences the chain found */
    form_build_init(&c->build, c->map);
    form_build_melody(&c->build, &c->p, &c->lines, x->keys_first, x->keys_last);
    add_chain_cadences(&c->build, &c->p, &c->cadences);
    assert(standard_form_input_finish(&c->build.input) == STANDARD_FORM_ANALYSIS_OK);
    form_analyze(&c->build, &c->form);
}

static void fv1_chain_destroy(Chain *c) {
    form_analysis_destroy(&c->form);
    form_build_destroy(&c->build);
    harmony_cadences_destroy(&c->cadences);
    harmony_analysis_destroy(c->harmony);
    destroy_inputs(&c->in);
    outer_voice_lines_destroy(&c->lines);
    standard_tonality_roman_results_destroy(c->romans, c->tonal_count);
    standard_tonality_chord_inputs_destroy(c->tonal_inputs);
    free(c->segment_inputs);
    free(c->region_inputs);
    for (size_t i = 0; i < c->count; i++) {
        common_practice_weighted_result_destroy(&c->chords[i]);
    }
    free(c->chords);
    common_practice_key_regions_destroy(&c->regions);
    free(c->events);
    midi_key_signature_events_destroy(&c->scanned);
    free(c->key_segments);
    pipeline_destroy(&c->p);
    if (c->own != NULL) {
        meter_map_destroy(c->own);
    }
}

/* The measure and beat (1-based) a segment starts at. */
static void fv1_where(const Chain *c, size_t segment, int *measure, double *beat) {
    const TimeSpan *window = sonority_time_window(c->p.sonorities.items[segment].sonority);
    MetricalPosition *position = NULL;
    assert(metrical_position_from_time(c->map, time_span_start(window), &position) == METRICAL_POSITION_OK);
    *measure = metrical_position_measure(position);
    const Duration *offset = metrical_position_offset(position);
    *beat = 1.0 + ((double)duration_numerator(offset) / (double)duration_denominator(offset)) / c->shim.beat_length;
    metrical_position_destroy(position);
}

/* The time in whole notes of measure `m`, beat `b` of the piece. */
static double fv1_time_of(const Chain *c, int measure, double beat) {
    TimeSpan *span = NULL;
    assert(meter_map_measure_span(c->map, measure, &span) == METER_MAP_OK);
    double start = fv1_time_double(time_span_start(span));
    time_span_destroy(span);
    return start + (beat - 1.0) * c->shim.beat_length;
}

#endif
