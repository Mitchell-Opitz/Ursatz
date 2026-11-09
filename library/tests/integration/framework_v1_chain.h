#ifndef URSATZ_TESTS_INTEGRATION_FRAMEWORK_V1_CHAIN_H
#define URSATZ_TESTS_INTEGRATION_FRAMEWORK_V1_CHAIN_H

/*
 * The full chain over one Framework v1 reference piece, and the expected values it is checked against.
 *
 * THE CHAIN is analysis_chain_run (ursatz_chain) to the form stage on the piece's notes. The piece's MeterMap is the file's, except
 * that Bach 114's file declares 4/4 and the piece is 3/4, so a 3/4 map replaces it, as in the other reference-piece tests. No stage is
 * given an answer: the key is not the annotation's home key (it is only the home key of the boundary cadences, the degrees and the
 * motives, as in the library), and the cadences are not the hand cadences. Every default is the library's.
 *
 * It includes test_form_reference_pieces.c as a library, the way test_rhythm_field5.c does, so the helpers of the cadence and form tests are
 * the same code those tests run.
 *
 * THE EXPECTED VALUES are tests/fixtures/annotations/<piece>_v1_expected.txt (the format is in the header of those files).
 */

#define URSATZ_FORM_PIECES_AS_LIBRARY
#include "test_form_reference_pieces.c"

#include "io/midi_importer.h"
#include "ursatz_chain/analysis_chain.h"

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
    bool optional; /* may be missing without a deviation; found with its type, it is not an extra */
} XCadence;

/* One side of a `readings` line: a cadence type word, or `prolongation`. */
typedef struct {
    int measure;
    double beat;
    bool prolongation;
    CadenceType type; /* meaningful when not a prolongation */
    char text[24];
} XReadingSide;

/* `readings <m> <beat> <reading> <m> <beat> <reading> [prefer 1|2]`: one cadence point with two competing readings (C7, C9). */
typedef struct {
    XReadingSide side[2];
    int prefer; /* 0: none; 1 or 2: that side must have the higher share */
} XReadings;

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
    XReadings readings[8];
    size_t readings_count;
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
            int n = sscanf(line, "cadence %d %lf %23s %23s", &i1, &beat, a, b);
            assert(n >= 3 && x->cadence_count < 24);
            assert(n == 3 || strcmp(b, "optional") == 0);
            XCadence *cd = &x->cadences[x->cadence_count++];
            cd->measure = i1;
            cd->beat = beat;
            cd->type = fv1_cadence_type(a);
            snprintf(cd->type_text, sizeof(cd->type_text), "%s", a);
            cd->optional = n == 4;
        } else if (strcmp(word, "readings") == 0) {
            int m1 = 0;
            int m2 = 0;
            double b1 = 0.0;
            double b2 = 0.0;
            char r1[24] = "";
            char r2[24] = "";
            char keyword[24] = "";
            int prefer = 0;
            int n = sscanf(line, "readings %d %lf %23s %d %lf %23s %23s %d", &m1, &b1, r1, &m2, &b2, r2, keyword, &prefer);
            assert((n == 6 || n == 8) && x->readings_count < 8);
            assert(n == 6 || (strcmp(keyword, "prefer") == 0 && (prefer == 1 || prefer == 2)));
            XReadings *rd = &x->readings[x->readings_count++];
            const int measures[2] = {m1, m2};
            const double beats[2] = {b1, b2};
            const char *texts[2] = {r1, r2};
            for (int s = 0; s < 2; s++) {
                rd->side[s].measure = measures[s];
                rd->side[s].beat = beats[s];
                rd->side[s].prolongation = strcmp(texts[s], "prolongation") == 0;
                rd->side[s].type = rd->side[s].prolongation ? CADENCE_TYPE_SUBVERTED : fv1_cadence_type(texts[s]);
                snprintf(rd->side[s].text, sizeof(rd->side[s].text), "%s", texts[s]);
            }
            rd->prefer = n == 8 ? prefer : 0;
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

/* The full chain over one piece: analysis_chain_run to the form (cadences and key regions included) on the piece's notes, with the file's
   key signature events, in the keys scope of the expected file. The fields view the chain's results. */
typedef struct {
    const FormPiece *fp;
    V1Expected x;
    MeterMap *own;
    WeightedPipeline p;
    const MeterMap *map;
    size_t count; /* segments of the chain: those that start inside the keys scope */
    double beat_length; /* in whole notes */

    AnalysisKeyEvent *key_events;
    size_t key_event_count;
    AnalysisChain *chain;
    CommonPracticeKeySegment *key_segments;
    CommonPracticeKeyRegions regions;
    const CommonPracticeWeightedResult *chords; /* one per segment */
    const StandardTonalityChordInput *tonal_inputs;
    const StandardTonalityRomanResult *romans;
    OuterVoiceLines lines;
    HarmonyCadences cadences;
    FormAnalysis form;
} Chain;

static double fv1_time_double(const MusicalTime *t) {
    return (double)musical_time_numerator(t) / (double)musical_time_denominator(t);
}

static void fv1_chain_run(Chain *c, const FormPiece *fp) {
    memset(c, 0, sizeof(*c));
    c->fp = fp;
    fv1_load_expected(fp->prefix, &c->x);
    const V1Expected *x = &c->x;
    c->own = fp->piece.score_three_four ? three_four_map() : NULL;
    c->p = pipeline_run(URSATZ_CORPUS_DIR, fp->piece.midi, c->own);
    c->map = c->own != NULL ? c->own : c->p.meter.meter_map;
    c->beat_length = (double)x->beat_num / (double)x->beat_den;

    c->key_event_count = c->p.import.key_signatures.count;
    c->key_events = (AnalysisKeyEvent *)calloc(c->key_event_count + 1, sizeof(AnalysisKeyEvent));
    assert(c->key_events != NULL);
    for (size_t i = 0; i < c->key_event_count; i++) {
        const MidiKeySignatureEvent *event = &c->p.import.key_signatures.events[i];
        c->key_events[i].time = event->position;
        assert(analysis_key_from_fifths(event->sharps_flats, KEY_STRUCTURE_MODE_MAJOR, &c->key_events[i].key));
    }
    AnalysisNoteSet set = {c->p.notes, c->p.note_count, c->map};
    AnalysisChainConfig config = analysis_chain_config_default();
    config.stages = ANALYSIS_CHAIN_STAGE_FORM;
    config.first_measure = x->keys_first;
    config.last_measure = x->keys_last;
    config.home_key_index = x->home_key;
    assert(analysis_chain_run(&set, c->key_events, c->key_event_count, &config, &c->chain) == ANALYSIS_CHAIN_OK);

    c->count = analysis_chain_segment_count(c->chain);
    assert(c->count > 0);
    c->regions = *analysis_chain_key_regions(c->chain);
    c->chords = analysis_chain_chords(c->chain);
    c->tonal_inputs = analysis_chain_chord_inputs(c->chain);
    c->romans = analysis_chain_numerals(c->chain);
    c->lines = *analysis_chain_outer_voices(c->chain);
    c->cadences = *analysis_chain_cadences(c->chain);
    c->form.phrases = *analysis_chain_phrases(c->chain);
    c->form.segmentation = *analysis_chain_ideas(c->chain);
    c->form.themes = *analysis_chain_themes(c->chain);
    c->form.expansions = *analysis_chain_expansions(c->chain);
    assert(chain_build_key_segments(analysis_chain_segments(c->chain), c->count, NULL, &c->key_segments) == CHAIN_INPUT_OK);
    for (size_t s = 0; s < c->count; s++) {
        c->key_segments[s].chord = common_practice_key_chord_of(&c->chords[s]);
    }
}

static void fv1_chain_destroy(Chain *c) {
    chain_key_segments_destroy(c->key_segments);
    analysis_chain_destroy(c->chain);
    free(c->key_events);
    pipeline_destroy(&c->p);
    if (c->own != NULL) {
        meter_map_destroy(c->own);
    }
}

/* The measure and beat (1-based) a segment starts at. The cadences index the chain's own segments (analysis_chain_segments, harmonic spans
   merged), not the pipeline's, which runs without the CHORDS stage and is never merged into spans. */
static void fv1_where(const Chain *c, size_t segment, int *measure, double *beat) {
    const TimeSpan *window = sonority_time_window(analysis_chain_segments(c->chain)->sonorities.items[segment].sonority);
    MetricalPosition *position = NULL;
    assert(metrical_position_from_time(c->map, time_span_start(window), &position) == METRICAL_POSITION_OK);
    *measure = metrical_position_measure(position);
    const Duration *offset = metrical_position_offset(position);
    *beat = 1.0 + ((double)duration_numerator(offset) / (double)duration_denominator(offset)) / c->beat_length;
    metrical_position_destroy(position);
}

/* The time in whole notes of measure `m`, beat `b` of the piece. */
static double fv1_time_of(const Chain *c, int measure, double beat) {
    TimeSpan *span = NULL;
    assert(meter_map_measure_span(c->map, measure, &span) == METER_MAP_OK);
    double start = fv1_time_double(time_span_start(span));
    time_span_destroy(span);
    return start + (beat - 1.0) * c->beat_length;
}

#endif
