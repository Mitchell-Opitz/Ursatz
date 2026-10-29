#include <assert.h>
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "analysis/outer_voice.h"
#include "analysis_cadence.h"
#include "applied_chords.h"
#include "chord_input.h"
#include "key_regions.h"
#include "roman_numeral_adapter.h"
#include "time/note_value.h"
#include "time/time_signature.h"
#include "weighted_pipeline.h"

/*
 * Cadences from real music for the five reference pieces (Bach 114, Mozart 545, Clementi 36, Field 1, Field 5), against
 * tests/fixtures/annotations/<piece>_cadences.txt: DRAFT hand readings, not yet checked against the scores (see their
 * headers), written from the pitch content of the files and not from this code's output.
 *
 * Two runs per piece. A cadence is FOUND when the adapter reports one whose arrival starts within one beat of an
 * annotated arrival; its type is RIGHT when it equals the annotated type (PAC and IAC are separate types). A reported
 * cadence that matches no annotation, inside a stretch of hand chords, is a FALSE POSITIVE.
 *
 * 1. THE CADENCE ADAPTER ALONE. The chords come from the hand numerals (degree, quality, inversion, applied target);
 *    the meter, beat strength and soprano are the real ones from the L2 pipeline (outer voices). Chords outside a hand
 *    stretch are not read. Asserted over all five pieces: at least 80% of the annotated cadences are found, and at
 *    least 80% of the found ones have the right type.
 * 2. THE WHOLE CHAIN. Weighted chord identification (the home key, whole piece), local key regions, the
 *    standard-tonality adapter, then the cadence adapter. The earlier stages miss much of the harmony. Reported with the
 *    numbers, asserted only at a floor under the measured level, so a regression shows; it is not a quality target.
 *
 * Nothing is tuned to the fixtures: every default is the library's. URSATZ_CORPUS_DIR and URSATZ_ANNOTATION_DIR come
 * from CMake.
 */

#define TARGET 0.80
#define CHAIN_RECALL_FLOOR 0.05 /* the whole chain found 1 of 10 when this was written */

typedef struct {
    double at;    /* whole notes */
    double until; /* whole notes */
    int tonic;
    int degree;
    bool seventh;
    bool diminished;
    int inversion;
    int applied_target;
} HandChord;

typedef struct {
    double at;
    CadenceType type;
    char text[12];
} Expected;

typedef struct {
    int beats;
    long beat_num;
    long beat_den;
    int home;
    HandChord chords[64];
    size_t chord_count;
    Expected cadences[16];
    size_t cadence_count;
    double beat_length;
} Annotation;

typedef struct {
    const char *name;
    const char *midi;
    const char *fixture;
    bool score_three_four;
} Piece;

static const Piece PIECES[5] = {{"Bach 114", "Bach 114.mid", "bach114_cadences.txt", true},
                                {"Mozart 545", "Mozart 545.mid", "mozart545_cadences.txt", false},
                                {"Clementi 36", "Clementi 36.mid", "clementi36_cadences.txt", false},
                                {"Field 1", "Field 1.mid", "field1_cadences.txt", false},
                                {"Field 5", "Field 5.mid", "field5_cadences.txt", false}};

static int tonic_of(const char *name) {
    static const char *const NAMES[12] = {"C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};
    for (int pc = 0; pc < 12; pc++) {
        if (strcmp(name, NAMES[pc]) == 0) {
            return pc;
        }
    }
    assert(!"unknown tonic");
    return -1;
}

static int roman_degree(const char *letters, size_t length) {
    static const char *const NAMES[7] = {"i", "ii", "iii", "iv", "v", "vi", "vii"};
    char lower[8] = {0};
    assert(length < sizeof(lower));
    for (size_t i = 0; i < length; i++) {
        lower[i] = (char)tolower((unsigned char)letters[i]);
    }
    for (int d = 0; d < 7; d++) {
        if (strcmp(lower, NAMES[d]) == 0) {
            return d + 1;
        }
    }
    return -1;
}

static void parse_numeral(const char *text, HandChord *chord) {
    size_t n = 0;
    while (text[n] == 'I' || text[n] == 'V' || text[n] == 'i' || text[n] == 'v') {
        n++;
    }
    chord->degree = roman_degree(text, n);
    assert(chord->degree > 0);
    const char *rest = text + n;
    chord->diminished = *rest == 'o';
    if (chord->diminished) {
        rest++;
    }
    char figure[16] = {0};
    size_t f = 0;
    while (*rest != '\0' && (isdigit((unsigned char)*rest) || *rest == '/')) {
        figure[f++] = *rest++;
        assert(f < sizeof(figure));
    }
    chord->applied_target = 0;
    if (f > 0 && figure[f - 1] == '/') {
        figure[--f] = '\0';
        chord->applied_target = roman_degree(rest, strlen(rest));
        assert(chord->applied_target > 0);
    } else {
        assert(*rest == '\0');
    }
    chord->seventh = strcmp(figure, "7") == 0 || strcmp(figure, "6/5") == 0 || strcmp(figure, "4/3") == 0 ||
                     strcmp(figure, "4/2") == 0;
    if (strcmp(figure, "") == 0 || strcmp(figure, "7") == 0) {
        chord->inversion = 0;
    } else if (strcmp(figure, "6") == 0 || strcmp(figure, "6/5") == 0) {
        chord->inversion = 1;
    } else if (strcmp(figure, "6/4") == 0 || strcmp(figure, "4/3") == 0) {
        chord->inversion = 2;
    } else if (strcmp(figure, "4/2") == 0) {
        chord->inversion = 3;
    } else {
        assert(!"unknown figure");
    }
}

static double time_of(const Annotation *a, int measure, double beat) {
    return (((double)(measure - 1) * (double)a->beats + (beat - 1.0)) * (double)a->beat_num) / (double)a->beat_den;
}

static Annotation load_annotation(const char *fixture) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", URSATZ_ANNOTATION_DIR, fixture);
    size_t size = 0;
    unsigned char *text = pipeline_read_file(path, &size);
    Annotation a;
    memset(&a, 0, sizeof(a));
    for (char *line = strtok((char *)text, "\n"); line != NULL; line = strtok(NULL, "\n")) {
        if (line[0] == '#' || line[0] == '\0') {
            continue;
        }
        char word[16] = "";
        sscanf(line, "%15s", word);
        if (strcmp(word, "meter") == 0) {
            assert(sscanf(line, "%*s %d %ld/%ld", &a.beats, &a.beat_num, &a.beat_den) == 3);
            a.beat_length = (double)a.beat_num / (double)a.beat_den;
        } else if (strcmp(word, "home") == 0) {
            char name[4] = "";
            assert(sscanf(line, "%*s %3s", name) == 1);
            a.home = tonic_of(name);
        } else if (strcmp(word, "chord") == 0) {
            int measure = 0;
            double beat = 0.0;
            char tonic[4] = "";
            char numeral[24] = "";
            assert(sscanf(line, "%*s %d %lf %3s %23s", &measure, &beat, tonic, numeral) == 4);
            HandChord *c = &a.chords[a.chord_count++];
            assert(a.chord_count <= 64 && a.beats > 0);
            c->at = time_of(&a, measure, beat);
            c->until = c->at;
            c->tonic = tonic_of(tonic);
            parse_numeral(numeral, c);
            if (a.chord_count >= 2 && a.chords[a.chord_count - 2].until == a.chords[a.chord_count - 2].at) {
                a.chords[a.chord_count - 2].until = c->at;
            }
        } else if (strcmp(word, "end") == 0) {
            int measure = 0;
            double beat = 0.0;
            assert(sscanf(line, "%*s %d %lf", &measure, &beat) == 2 && a.chord_count > 0);
            a.chords[a.chord_count - 1].until = time_of(&a, measure, beat);
        } else if (strcmp(word, "cadence") == 0) {
            int measure = 0;
            double beat = 0.0;
            Expected *e = &a.cadences[a.cadence_count++];
            assert(sscanf(line, "%*s %d %lf %11s", &measure, &beat, e->text) == 3 && a.cadence_count <= 16);
            e->at = time_of(&a, measure, beat);
            e->type = strcmp(e->text, "pac") == 0         ? CADENCE_TYPE_PERFECT_AUTHENTIC
                      : strcmp(e->text, "iac") == 0       ? CADENCE_TYPE_IMPERFECT_AUTHENTIC
                      : strcmp(e->text, "hc") == 0        ? CADENCE_TYPE_HALF
                      : strcmp(e->text, "deceptive") == 0 ? CADENCE_TYPE_DECEPTIVE
                      : strcmp(e->text, "plagal") == 0    ? CADENCE_TYPE_PLAGAL
                                                          : CADENCE_TYPE_SUBVERTED;
        }
    }
    free(text);
    assert(a.beats > 0 && a.chord_count > 0);
    return a;
}

/* The hand chord sounding at a time, or NULL outside every stretch. */
static const HandChord *chord_at(const Annotation *a, double t) {
    for (size_t i = 0; i < a->chord_count; i++) {
        if (a->chords[i].at <= t + 1e-9 && t < a->chords[i].until - 1e-9) {
            return &a->chords[i];
        }
    }
    return NULL;
}

static MeterMap *three_four_map(void) {
    NoteValue *quarter = NULL;
    assert(note_value_create(2, &quarter) == NOTE_VALUE_OK);
    TimeSignature *three_four = NULL;
    assert(time_signature_create(3, quarter, &three_four) == TIME_SIGNATURE_OK);
    note_value_destroy(quarter);
    MusicalTime *zero = NULL;
    assert(musical_time_create(0, 1, &zero) == MUSICAL_TIME_OK);
    MeterMapChangePoint point = {three_four, zero};
    EntityID *id = NULL;
    assert(entity_id_create("bach-114-score-meter", &id) == ENTITY_ID_OK);
    MeterMap *map = NULL;
    assert(meter_map_create(id, &point, 1, &map) == METER_MAP_OK);
    time_signature_destroy(three_four);
    musical_time_destroy(zero);
    return map;
}

/* --- segment inputs --- */

typedef struct {
    HarmonySegmentInput *inputs;
    MetricalPosition **positions;
    Duration *beat_length;
    size_t count;
} Inputs;

static Inputs make_inputs(const WeightedPipeline *p, const MeterMap *map, const Annotation *a, size_t count) {
    Inputs in;
    in.count = count;
    in.inputs = (HarmonySegmentInput *)calloc(count, sizeof(HarmonySegmentInput));
    in.positions = (MetricalPosition **)calloc(count, sizeof(MetricalPosition *));
    assert(in.inputs != NULL && in.positions != NULL);
    assert(duration_create(a->beat_num, (unsigned long)a->beat_den, &in.beat_length) == DURATION_OK);
    for (size_t s = 0; s < count; s++) {
        const TimeSpan *window = sonority_time_window(p->sonorities.items[s].sonority);
        assert(metrical_position_from_time(map, time_span_start(window), &in.positions[s]) == METRICAL_POSITION_OK);
        HarmonySegmentInput *input = &in.inputs[s];
        input->segment_id = p->segment_ids[s];
        input->span = window;
        input->position = in.positions[s];
        input->beat_strength = chain_attack_strength(map, time_span_start(window));
        input->key_ref = "key";
        input->mode = HARMONY_MODE_MAJOR;
        input->beat_length = in.beat_length;
    }
    return in;
}

static void destroy_inputs(Inputs *in) {
    for (size_t s = 0; s < in->count; s++) {
        metrical_position_destroy(in->positions[s]);
    }
    duration_destroy(in->beat_length);
    free(in->positions);
    free(in->inputs);
}

static void set_intervals(HarmonySegmentInput *input, const int *intervals, size_t count) {
    memset(input->intervals, 0, sizeof(input->intervals));
    for (size_t i = 0; i < count; i++) {
        input->intervals[i] = intervals[i];
    }
    input->interval_count = count;
}

static void input_from_chord(const HandChord *chord, HarmonySegmentInput *input) {
    static const int MAJOR[3] = {0, 4, 7};
    static const int MINOR[3] = {0, 3, 7};
    static const int DIM[3] = {0, 3, 6};
    static const int DOMINANT7[4] = {0, 4, 7, 10};
    static const int QUALITY_OF_DEGREE[7] = {0, 1, 1, 0, 0, 1, 2};
    static const int MEMBER[4] = {0, 2, 4, 6};
    input->inversion = chord->inversion;
    input->applied_to = chord->applied_target;
    if (chord->applied_target != 0) {
        input->degree = chord->diminished ? 7 : 5;
        if (chord->diminished) {
            set_intervals(input, DIM, 3);
        } else {
            set_intervals(input, chord->seventh ? DOMINANT7 : MAJOR, chord->seventh ? 4 : 3);
        }
        input->bass_degree = 0;
        return;
    }
    input->degree = chord->degree;
    if (chord->seventh) {
        set_intervals(input, DOMINANT7, 4);
    } else {
        int quality = QUALITY_OF_DEGREE[chord->degree - 1];
        set_intervals(input, quality == 0 ? MAJOR : (quality == 1 ? MINOR : DIM), 3);
    }
    input->bass_degree = (chord->degree - 1 + MEMBER[chord->inversion]) % 7 + 1;
}

/* The degree (1..7) of an outer-voice pitch in the major key on `tonic`, or 0 for a chromatic or an absent note. */
static int degree_of(const OuterVoiceEntry *entry, int tonic) {
    static const int DEGREE_OF_OFFSET[12] = {1, 0, 2, 0, 3, 4, 0, 5, 0, 6, 0, 7};
    if (!entry->present) {
        return 0;
    }
    long position = pitch_numeric_value_position_numerator(entry->pitch);
    return DEGREE_OF_OFFSET[(((position - tonic) % 12) + 12) % 12];
}

/* --- scoring --- */

typedef struct {
    size_t annotated;
    size_t found;
    size_t right;
    size_t reported;
    size_t false_positives;
} Score;

static void add(Score *total, const Score *s) {
    total->annotated += s->annotated;
    total->found += s->found;
    total->right += s->right;
    total->reported += s->reported;
    total->false_positives += s->false_positives;
}

static void print_where(const Annotation *a, double t) {
    double beats = t / ((double)a->beat_num / (double)a->beat_den);
    long measure = (long)(beats / (double)a->beats + 1e-9) + 1;
    printf("measure %ld beat %.2f", measure, 1.0 + beats - (double)(measure - 1) * (double)a->beats);
}

static Score evaluate(const Annotation *a, const WeightedPipeline *p, const HarmonyCadences *cadences, bool print) {
    Score score = {a->cadence_count, 0, 0, 0, 0};
    bool *used = (bool *)calloc(cadences->count > 0 ? cadences->count : 1, sizeof(bool));
    assert(used != NULL);
    for (size_t e = 0; e < a->cadence_count; e++) {
        bool found = false;
        for (size_t c = 0; c < cadences->count; c++) {
            double arrival = p->starts[cadences->items[c].arrival_first];
            if (fabs(arrival - a->cadences[e].at) <= a->beat_length + 1e-9) {
                found = true;
                used[c] = true;
                score.found++;
                if (cadences->items[c].type == a->cadences[e].type) {
                    score.right++;
                } else if (print) {
                    printf("  wrong type at ");
                    print_where(a, a->cadences[e].at);
                    printf(": annotated %s, got %s\n", a->cadences[e].text, cadence_type_label(cadences->items[c].type));
                }
                break;
            }
        }
        if (!found && print) {
            printf("  missed %s at ", a->cadences[e].text);
            print_where(a, a->cadences[e].at);
            printf("\n");
        }
    }
    for (size_t c = 0; c < cadences->count; c++) {
        double arrival = p->starts[cadences->items[c].arrival_first];
        if (chord_at(a, arrival) == NULL) {
            continue; /* outside the hand stretches: not scored */
        }
        score.reported++;
        if (!used[c]) {
            score.false_positives++;
            if (print) {
                printf("  false positive %s at ", cadence_type_label(cadences->items[c].type));
                print_where(a, arrival);
                printf("\n");
            }
        }
    }
    free(used);
    return score;
}

static size_t scope_count(const WeightedPipeline *p, const Annotation *a) {
    double end = 0.0;
    for (size_t i = 0; i < a->chord_count; i++) {
        end = a->chords[i].until > end ? a->chords[i].until : end;
    }
    size_t count = 0;
    while (count < p->segment_count && p->starts[count] < end + 1e-9) {
        count++;
    }
    return count;
}

static void report(const char *label, const Score *s) {
    printf("  %s: %zu of %zu annotated cadences found, %zu of %zu found with the right type, %zu reported in the "
           "stretches, %zu false positive(s)\n",
           label, s->found, s->annotated, s->right, s->found, s->reported, s->false_positives);
}

static Score run_adapter_alone(const Piece *piece) {
    Annotation a = load_annotation(piece->fixture);
    MeterMap *own = piece->score_three_four ? three_four_map() : NULL;
    WeightedPipeline p = pipeline_run(URSATZ_CORPUS_DIR, piece->midi, own);
    const MeterMap *map = own != NULL ? own : p.meter.meter_map;
    size_t count = scope_count(&p, &a);

    OuterVoiceLines lines = {0};
    assert(outer_voice_extract(&p.voice_sonorities, p.events, p.pitch_values, p.note_count, map,
                               standard_meter_segmentation_beat_grid, NULL, NULL, &lines) == OUTER_VOICE_OK);
    Inputs in = make_inputs(&p, map, &a, count);
    for (size_t s = 0; s < count; s++) {
        const HandChord *chord = chord_at(&a, p.starts[s]);
        if (chord == NULL) {
            in.inputs[s].degree = 0; /* not a chord this framework reads */
            in.inputs[s].interval_count = 3;
            continue;
        }
        input_from_chord(chord, &in.inputs[s]);
        in.inputs[s].soprano_degree = degree_of(&lines.soprano[s], chord->tonic);
    }
    HarmonySegment *segments = NULL;
    assert(harmony_analysis_prepare(in.inputs, count, &segments) == HARMONY_ANALYSIS_OK);
    HarmonyCadences found = {0};
    assert(harmony_analysis_find_cadences(segments, count, NULL, &found) == HARMONY_CADENCE_ANALYSIS_OK);

    printf("%s, the cadence adapter alone on the hand chords (draft hand marks):\n", piece->name);
    Score score = evaluate(&a, &p, &found, true);
    report("alone", &score);

    harmony_cadences_destroy(&found);
    harmony_analysis_destroy(segments);
    outer_voice_lines_destroy(&lines);
    destroy_inputs(&in);
    pipeline_destroy(&p);
    if (own != NULL) {
        meter_map_destroy(own);
    }
    return score;
}

/* --- the whole chain --- */

static void intervals_of(const StandardTonalityRomanClaim *claim, int out[4], size_t *count) {
    static const int TRIADS[4][3] = {{0, 4, 7}, {0, 3, 7}, {0, 3, 6}, {0, 4, 8}};
    static const int SEVENTHS[5][4] = {{0, 4, 7, 11}, {0, 4, 7, 10}, {0, 3, 7, 10}, {0, 3, 6, 10}, {0, 3, 6, 9}};
    memset(out, 0, 4 * sizeof(int));
    if (claim->seventh) {
        memcpy(out, SEVENTHS[claim->quality], 4 * sizeof(int));
        *count = 4;
    } else {
        memcpy(out, TRIADS[claim->quality], 3 * sizeof(int));
        *count = 3;
    }
}

static Score run_whole_chain(const Piece *piece) {
    Annotation a = load_annotation(piece->fixture);
    MeterMap *own = piece->score_three_four ? three_four_map() : NULL;
    WeightedPipeline p = pipeline_run(URSATZ_CORPUS_DIR, piece->midi, own);
    const MeterMap *map = own != NULL ? own : p.meter.meter_map;
    size_t count = scope_count(&p, &a);

    CommonPracticeWeightedResults chords = {0};
    assert(common_practice_identify_weighted_sequence(p.segments, count, a.home, COMMON_PRACTICE_KEY_MAJOR, NULL,
                                                      &chords) == COMMON_PRACTICE_WEIGHTED_OK);
    CommonPracticeWeightedConfig weighting = common_practice_weighted_config_default();
    CommonPracticeKeySegment *key_segments = (CommonPracticeKeySegment *)calloc(count, sizeof(CommonPracticeKeySegment));
    assert(key_segments != NULL);
    for (size_t s = 0; s < count; s++) {
        const TimeSpan *window = sonority_time_window(p.sonorities.items[s].sonority);
        key_segments[s].start = time_span_start(window);
        key_segments[s].end = time_span_end(window);
        key_segments[s].source = &p.segments[s];
        assert(common_practice_weight_segment(p.segments[s].members, p.segments[s].member_count, &weighting,
                                              &key_segments[s].weights) == COMMON_PRACTICE_WEIGHTED_OK);
    }
    CommonPracticeKeyRegions regions = {0};
    assert(key_regions_estimate(key_segments, count, map, NULL, 0, NULL, &regions) == COMMON_PRACTICE_KEY_REGIONS_OK);
    StandardTonalityKeyRegionInput *region_inputs =
        (StandardTonalityKeyRegionInput *)calloc(regions.region_count, sizeof(StandardTonalityKeyRegionInput));
    StandardTonalitySegmentInput *segment_inputs =
        (StandardTonalitySegmentInput *)calloc(count, sizeof(StandardTonalitySegmentInput));
    assert(region_inputs != NULL && segment_inputs != NULL);
    for (size_t i = 0; i < regions.region_count; i++) {
        region_inputs[i].key = regions.regions[i].key;
        region_inputs[i].start = time_span_start(regions.regions[i].span);
        region_inputs[i].uncertainty = regions.regions[i].uncertainty;
    }
    for (size_t s = 0; s < count; s++) {
        segment_inputs[s].segment_id = p.segment_ids[s];
        segment_inputs[s].start = key_segments[s].start;
        segment_inputs[s].chord = chords.items[s].matched ? chords.items[s].best_chord : NULL;
        segment_inputs[s].weights = key_segments[s].weights.pitch_class;
    }
    StandardTonalityChordInput *tonal_inputs = NULL;
    size_t tonal_count = 0;
    assert(standard_tonality_build_chord_inputs(segment_inputs, count, region_inputs, regions.region_count,
                                                &tonal_inputs, &tonal_count) == STANDARD_TONALITY_CHORD_INPUT_OK);
    StandardTonalityRomanResult *romans = NULL;
    assert(standard_tonality_build_roman_numerals(tonal_inputs, tonal_count, &romans) == STANDARD_TONALITY_ROMAN_OK);
    OuterVoiceLines lines = {0};
    assert(outer_voice_extract(&p.voice_sonorities, p.events, p.pitch_values, p.note_count, map,
                               standard_meter_segmentation_beat_grid, NULL, NULL, &lines) == OUTER_VOICE_OK);

    Inputs in = make_inputs(&p, map, &a, count);
    for (size_t s = 0; s < count; s++) {
        HarmonySegmentInput *input = &in.inputs[s];
        if (!romans[s].has_numeral) {
            input->degree = 0;
            input->interval_count = 3;
            continue;
        }
        const StandardTonalityRomanClaim *claim = &romans[s].claim;
        int intervals[4];
        size_t interval_count = 0;
        intervals_of(claim, intervals, &interval_count);
        set_intervals(input, intervals, interval_count);
        input->degree = claim->degree;
        input->inversion = claim->inversion;
        input->applied_to = claim->applied ? claim->applied_target_degree : 0;
        input->mode = tonal_inputs[s].mode == STANDARD_TONALITY_ROMAN_KEY_MINOR ? HARMONY_MODE_MINOR : HARMONY_MODE_MAJOR;
        input->uncertainty = romans[s].uncertainty;
        input->bass_degree = degree_of(&lines.bass[s], tonal_inputs[s].tonic_pitch_class);
        input->soprano_degree = degree_of(&lines.soprano[s], tonal_inputs[s].tonic_pitch_class);
    }
    HarmonySegment *segments = NULL;
    assert(harmony_analysis_prepare(in.inputs, count, &segments) == HARMONY_ANALYSIS_OK);
    HarmonyCadences found = {0};
    assert(harmony_analysis_find_cadences(segments, count, NULL, &found) == HARMONY_CADENCE_ANALYSIS_OK);

    printf("%s, the whole chain (draft hand marks):\n", piece->name);
    Score score = evaluate(&a, &p, &found, true);
    report("chain", &score);

    harmony_cadences_destroy(&found);
    harmony_analysis_destroy(segments);
    destroy_inputs(&in);
    outer_voice_lines_destroy(&lines);
    standard_tonality_roman_results_destroy(romans, tonal_count);
    standard_tonality_chord_inputs_destroy(tonal_inputs);
    free(segment_inputs);
    free(region_inputs);
    common_practice_key_regions_destroy(&regions);
    free(key_segments);
    common_practice_weighted_results_destroy(&chords);
    pipeline_destroy(&p);
    if (own != NULL) {
        meter_map_destroy(own);
    }
    return score;
}

int main(void) {
    Score alone = {0, 0, 0, 0, 0};
    for (size_t i = 0; i < 5; i++) {
        Score s = run_adapter_alone(&PIECES[i]);
        add(&alone, &s);
    }
    double recall = alone.annotated == 0 ? 0.0 : (double)alone.found / (double)alone.annotated;
    double accuracy = alone.found == 0 ? 0.0 : (double)alone.right / (double)alone.found;
    printf("All five pieces, the adapter alone: recall %.1f%% (%zu of %zu), type accuracy %.1f%% (%zu of %zu), %zu "
           "false positive(s) of %zu reported\n",
           recall * 100.0, alone.found, alone.annotated, accuracy * 100.0, alone.right, alone.found,
           alone.false_positives, alone.reported);
    assert(alone.annotated > 0);
    assert(recall >= TARGET);
    assert(accuracy >= TARGET);

    Score chain = {0, 0, 0, 0, 0};
    for (size_t i = 0; i < 5; i++) {
        Score s = run_whole_chain(&PIECES[i]);
        add(&chain, &s);
    }
    double chain_recall = chain.annotated == 0 ? 0.0 : (double)chain.found / (double)chain.annotated;
    double chain_accuracy = chain.found == 0 ? 0.0 : (double)chain.right / (double)chain.found;
    printf("All five pieces, the whole chain: recall %.1f%% (%zu of %zu), type accuracy %.1f%% (%zu of %zu), %zu "
           "false positive(s) of %zu reported\n",
           chain_recall * 100.0, chain.found, chain.annotated, chain_accuracy * 100.0, chain.right, chain.found,
           chain.false_positives, chain.reported);
    assert(chain_recall >= CHAIN_RECALL_FLOOR);
    if (chain_recall < TARGET || chain_accuracy < TARGET) {
        printf("  NOTE: the whole chain is below the %.0f%% target (floor %.0f%% asserted; see the PR)\n", TARGET * 100.0,
               CHAIN_RECALL_FLOOR * 100.0);
    }
    printf("test_cadences_reference_pieces: ok\n");
    return 0;
}
