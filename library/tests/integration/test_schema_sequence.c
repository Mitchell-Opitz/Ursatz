#define main cadence_pieces_unused_main
#include "test_cadences_reference_pieces.c"
#undef main

#include "galant_analysis_match.h"
#include "scale_analysis_detect.h"
#include "schema_util.h"
#include "ursatz_chain/chain_inputs.h"

/*
 * Schema and sequence detection (L3.10) on a hand-written galant fixture, tests/fixtures/galant/galant_fixture.txt: 24 measures in C major planted with a
 * Romanesca, a Prinner, a Do-Re-Mi, a Meyer, a Fonte, a Monte, a Romanesca in G, a cycle of descending fifths and an ascending 5-6, each with its span in
 * galant_annotation.txt (written with the fixture, not from the output of the adapters).
 *
 *   THE ADAPTERS ALONE. The segments are the fixture's own (the outer-voice degrees, the local keys and the numerals as written, the meter), so
 *   nothing of the L2/L3 chain counts against them. A planted item is FOUND when the adapter reports the same name (schema) or kind (sequence) with its start
 *   and its end each within one beat of the annotated ones. Asserted: every planted schema and sequence found. Reported: the matches that are none of the
 *   planted ones (the false positives).
 *   THE WHOLE CHAIN. The fixture is written as a MIDI file; its outer voices (L2.6), local key regions (L2.8) and Roman numerals (L3.1) come from the
 *   real chain, and the adapters run on them. Reported with the numbers; asserted only at a floor under what it measured, so a regression shows.
 *   MOZART 545 AND CLEMENTI 36. The whole chain, printed without asserting.
 *
 * Nothing is tuned to the fixture: every default is the library's.
 */

#define CHAIN_FLOOR_FOUND 4 /* the whole chain found 5 of 9 when this was written */

typedef struct {
    bool schema;
    char name[40];
    double start; /* in beats from the start, quarter notes */
    double end;
} Planted;

typedef struct {
    Planted items[32];
    size_t count;
} PlantedList;

static PlantedList load_planted(void) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/galant_annotation.txt", URSATZ_FIXTURE_DIR);
    size_t size = 0;
    unsigned char *text = pipeline_read_file(path, &size);
    PlantedList list;
    memset(&list, 0, sizeof(list));
    for (char *line = strtok((char *)text, "\n"); line != NULL; line = strtok(NULL, "\n")) {
        if (line[0] == '#' || line[0] == '\0') {
            continue;
        }
        char type[16];
        char name[40];
        int m1 = 0;
        int m2 = 0;
        double b1 = 0.0;
        double b2 = 0.0;
        if (sscanf(line, "%15s %39s %d %lf %d %lf", type, name, &m1, &b1, &m2, &b2) == 6) {
            Planted *p = &list.items[list.count++];
            p->schema = strcmp(type, "schema") == 0;
            snprintf(p->name, sizeof(p->name), "%s", name);
            p->start = (m1 - 1) * 4.0 + (b1 - 1.0);
            p->end = (m2 - 1) * 4.0 + (b2 - 1.0);
        }
    }
    free(text);
    assert(list.count > 0);
    return list;
}

static double beats_of(const MusicalTime *time) {
    return 4.0 * (double)musical_time_numerator(time) / (double)musical_time_denominator(time);
}

static void describe(const MeterMap *map, const GalantSegmentList *list, int first, int last, char *out, size_t size) {
    int m1 = 0;
    int m2 = 0;
    double b1 = 0.0;
    double b2 = 0.0;
    schema_where(map, time_span_start(list->items[first].span), &m1, &b1);
    schema_where(map, time_span_start(list->items[last].span), &m2, &b2);
    snprintf(out, size, "measure %d beat %.2f to measure %d beat %.2f", m1, b1, m2, b2);
}

typedef struct {
    size_t found;
    size_t extras;
} Outcome;

static bool near_planted(const Planted *p, double start, double end) {
    return fabs(start - p->start) <= 1.0 + 1e-9 && fabs(end - p->end) <= 1.0 + 1e-9;
}

/* Scores the matches against the planted list and prints the numbers. */
static Outcome score(const char *label, const MeterMap *map, const GalantSegmentList *list, const GalantSchemaMatches *schemas,
                     const ScaleSequenceMatches *sequences, const PlantedList *planted, bool print_extras) {
    Outcome outcome = {0, 0};
    for (size_t i = 0; i < planted->count; i++) {
        const Planted *p = &planted->items[i];
        bool found = false;
        if (p->schema) {
            for (size_t k = 0; k < schemas->count; k++) {
                const GalantSchemaClaim *c = &schemas->items[k].claim;
                found = found || (strcmp(c->name, p->name) == 0 &&
                                  near_planted(p, beats_of(time_span_start(list->items[c->first_segment].span)),
                                               beats_of(time_span_end(list->items[c->last_segment].span))));
            }
        } else {
            for (size_t k = 0; k < sequences->count; k++) {
                const ScaleSequenceClaim *c = &sequences->items[k].claim;
                found = found || (strcmp(c->kind_name, p->name) == 0 &&
                                  near_planted(p, beats_of(time_span_start(list->items[c->first_segment].span)),
                                               beats_of(time_span_end(list->items[c->last_segment].span))));
            }
        }
        outcome.found += found ? 1 : 0;
        if (!found) {
            printf("    missed %s %s at beat %.1f to %.1f\n", p->schema ? "schema" : "sequence", p->name, p->start, p->end);
        }
    }
    for (size_t k = 0; k < schemas->count; k++) {
        const GalantSchemaClaim *c = &schemas->items[k].claim;
        bool planted_one = false;
        for (size_t i = 0; i < planted->count; i++) {
            planted_one = planted_one || (planted->items[i].schema && strcmp(c->name, planted->items[i].name) == 0 &&
                                          near_planted(&planted->items[i], beats_of(time_span_start(list->items[c->first_segment].span)),
                                                       beats_of(time_span_end(list->items[c->last_segment].span))));
        }
        if (!planted_one) {
            outcome.extras++;
            if (print_extras) {
                char where[96];
                describe(map, list, c->first_segment, c->last_segment, where, sizeof(where));
                printf("    not planted: schema %s, %s, confidence %.2f\n", c->name, where, c->confidence);
            }
        }
    }
    for (size_t k = 0; k < sequences->count; k++) {
        const ScaleSequenceClaim *c = &sequences->items[k].claim;
        bool planted_one = false;
        for (size_t i = 0; i < planted->count; i++) {
            planted_one = planted_one || (!planted->items[i].schema && strcmp(c->kind_name, planted->items[i].name) == 0 &&
                                          near_planted(&planted->items[i], beats_of(time_span_start(list->items[c->first_segment].span)),
                                                       beats_of(time_span_end(list->items[c->last_segment].span))));
        }
        if (!planted_one) {
            outcome.extras++;
            if (print_extras) {
                char where[96];
                describe(map, list, c->first_segment, c->last_segment, where, sizeof(where));
                printf("    not planted: sequence %s (%d units%s), %s, confidence %.2f\n", c->kind_name, c->units,
                       c->rule_backed ? "" : ", not rule-backed", where, c->confidence);
            }
        }
    }
    printf("  %s: planted items found %zu of %zu; %zu schema(s) and %zu sequence(s) reported, %zu not planted\n", label, outcome.found,
           planted->count, schemas->count, sequences->count, outcome.extras);
    return outcome;
}

/* --- the chain --- */

typedef struct {
    CommonPracticeWeightedResults chords;
    CommonPracticeKeySegment *key_segments;
    CommonPracticeKeyRegions regions;
    StandardTonalityKeyRegionInput *region_inputs;
    StandardTonalitySegmentInput *segment_inputs;
    StandardTonalityChordInput *tonal_inputs;
    size_t tonal_count;
    StandardTonalityRomanResult *romans;
    size_t count;
    int *tonic_pc;
    bool *minor;
} Chain;

static void chain_run(Chain *chain, const WeightedPipeline *p, const MeterMap *map, int home, size_t count) {
    memset(chain, 0, sizeof(*chain));
    chain->count = count;
    assert(common_practice_identify_weighted_sequence(p->segments, count, home, COMMON_PRACTICE_KEY_MAJOR, NULL, &chain->chords) ==
           COMMON_PRACTICE_WEIGHTED_OK);
    CommonPracticeWeightedConfig weighting = common_practice_weighted_config_default();
    chain->key_segments = (CommonPracticeKeySegment *)calloc(count, sizeof(CommonPracticeKeySegment));
    assert(chain->key_segments != NULL);
    for (size_t s = 0; s < count; s++) {
        const TimeSpan *window = sonority_time_window(p->sonorities.items[s].sonority);
        chain->key_segments[s].start = time_span_start(window);
        chain->key_segments[s].end = time_span_end(window);
        chain->key_segments[s].source = &p->segments[s];
        assert(common_practice_weight_segment(p->segments[s].members, p->segments[s].member_count, &weighting,
                                              &chain->key_segments[s].weights) == COMMON_PRACTICE_WEIGHTED_OK);
    }
    assert(key_regions_estimate(chain->key_segments, count, map, NULL, 0, NULL, &chain->regions) == COMMON_PRACTICE_KEY_REGIONS_OK);
    chain->region_inputs = (StandardTonalityKeyRegionInput *)calloc(chain->regions.region_count, sizeof(StandardTonalityKeyRegionInput));
    chain->segment_inputs = (StandardTonalitySegmentInput *)calloc(count, sizeof(StandardTonalitySegmentInput));
    assert(chain->region_inputs != NULL && chain->segment_inputs != NULL);
    for (size_t i = 0; i < chain->regions.region_count; i++) {
        chain->region_inputs[i].key = chain->regions.regions[i].key;
        chain->region_inputs[i].start = time_span_start(chain->regions.regions[i].span);
        chain->region_inputs[i].uncertainty = chain->regions.regions[i].uncertainty;
    }
    for (size_t s = 0; s < count; s++) {
        chain->segment_inputs[s].segment_id = p->segment_ids[s];
        chain->segment_inputs[s].start = chain->key_segments[s].start;
        chain->segment_inputs[s].chord = chain->chords.items[s].matched ? chain->chords.items[s].best_chord : NULL;
        chain->segment_inputs[s].weights = chain->key_segments[s].weights.pitch_class;
    }
    assert(standard_tonality_build_chord_inputs(chain->segment_inputs, count, chain->region_inputs, chain->regions.region_count,
                                                &chain->tonal_inputs, &chain->tonal_count) == STANDARD_TONALITY_CHORD_INPUT_OK);
    assert(standard_tonality_build_roman_numerals(chain->tonal_inputs, chain->tonal_count, &chain->romans) == STANDARD_TONALITY_ROMAN_OK);
    assert(chain->tonal_count == count);
    chain->tonic_pc = (int *)calloc(count, sizeof(int));
    chain->minor = (bool *)calloc(count, sizeof(bool));
    assert(chain->tonic_pc != NULL && chain->minor != NULL);
    for (size_t s = 0; s < count; s++) {
        chain->tonic_pc[s] = chain->tonal_inputs[s].tonic_pitch_class;
        chain->minor[s] = chain->tonal_inputs[s].mode == STANDARD_TONALITY_ROMAN_KEY_MINOR;
    }
}

static void chain_destroy(Chain *chain) {
    standard_tonality_roman_results_destroy(chain->romans, chain->tonal_count);
    standard_tonality_chord_inputs_destroy(chain->tonal_inputs);
    free(chain->segment_inputs);
    free(chain->region_inputs);
    common_practice_key_regions_destroy(&chain->regions);
    free(chain->key_segments);
    common_practice_weighted_results_destroy(&chain->chords);
    free(chain->tonic_pc);
    free(chain->minor);
    memset(chain, 0, sizeof(*chain));
}

/* The adapters over the chain of a pipeline; the results are the caller's. */
static void chain_adapters(const WeightedPipeline *p, const OuterVoiceLines *lines, const MeterMap *map, int home, ChainGalantSegments *build,
                           GalantSchemaMatches *schemas, ScaleSequenceMatches *sequences) {
    Chain chain;
    chain_run(&chain, p, map, home, p->segment_count);
    assert(chain_build_galant_segments(analysis_chain_segments(p->chain), chain.count, map, lines,
                                       common_practice_key_index_of(home, COMMON_PRACTICE_KEY_MAJOR), chain.tonal_inputs, chain.romans,
                                       build) == CHAIN_INPUT_OK);
    chain_destroy(&chain);
    assert(galant_schema_match(&build->list, NULL, schemas) == GALANT_ANALYSIS_OK);
    assert(scale_sequence_detect(&build->list, NULL, sequences) == GALANT_ANALYSIS_OK);
}

static void print_matches(const char *name, const MeterMap *map, const ChainGalantSegments *build, const GalantSchemaMatches *schemas,
                          const ScaleSequenceMatches *sequences) {
    printf("%s: %zu segments, %zu schema(s), %zu sequence(s)\n", name, build->list.count, schemas->count, sequences->count);
    for (size_t i = 0; i < schemas->count; i++) {
        const GalantSchemaClaim *c = &schemas->items[i].claim;
        char where[96];
        describe(map, &build->list, c->first_segment, c->last_segment, where, sizeof(where));
        printf("  schema %s, %s, confidence %.2f%s\n", c->name, where, c->confidence, c->bass_only_stages > 0 ? " (a bass-only stage)" : "");
    }
    for (size_t i = 0; i < sequences->count; i++) {
        const ScaleSequenceClaim *c = &sequences->items[i].claim;
        char where[96];
        describe(map, &build->list, c->first_segment, c->last_segment, where, sizeof(where));
        printf("  sequence %s%s, %d units of %d, transposed %+d, %s, confidence %.2f\n", c->kind_name, c->rule_backed ? "" : " (not rule-backed)",
               c->units, c->unit_length, c->transposition_steps, where, c->confidence);
    }
}

/* --- the three runs --- */

static void run_fixture_alone(const PlantedList *planted) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/galant_fixture.txt", URSATZ_FIXTURE_DIR);
    Fixture fixture = schema_load_fixture(path);
    SchemaBuild build;
    schema_build_init(&build);
    schema_build_from_fixture(&build, &fixture);
    GalantSchemaMatches schemas = {NULL, 0};
    ScaleSequenceMatches sequences = {NULL, 0};
    assert(galant_schema_match(&build.list, NULL, &schemas) == GALANT_ANALYSIS_OK);
    assert(scale_sequence_detect(&build.list, NULL, &sequences) == GALANT_ANALYSIS_OK);
    printf("The galant fixture, %zu segments, 24 measures (hand-written):\n", build.list.count);
    TimeSignature *signature = NULL;
    NoteValue *quarter = NULL;
    MusicalTime *zero = NULL;
    EntityID *id = NULL;
    MeterMap *map = NULL;
    assert(note_value_create(2, &quarter) == NOTE_VALUE_OK && time_signature_create(4, quarter, &signature) == TIME_SIGNATURE_OK);
    assert(musical_time_create(0, 1, &zero) == MUSICAL_TIME_OK && entity_id_create("galant-fixture-meter", &id) == ENTITY_ID_OK);
    MeterMapChangePoint point = {signature, zero};
    assert(meter_map_create(id, &point, 1, &map) == METER_MAP_OK);
    Outcome outcome = score("the adapters alone", map, &build.list, &schemas, &sequences, planted, true);
    assert(outcome.found == planted->count);

    galant_schema_matches_destroy(&schemas);
    scale_sequence_matches_destroy(&sequences);
    meter_map_destroy(map);
    musical_time_destroy(zero);
    time_signature_destroy(signature);
    note_value_destroy(quarter);
    schema_build_destroy(&build);

    /* the whole chain on the same music, as a MIDI file */
    unsigned char *bytes = NULL;
    size_t size = schema_midi_bytes(&fixture, &bytes);
    char midi_path[1024];
    snprintf(midi_path, sizeof(midi_path), "%s/galant_fixture.mid", URSATZ_TEST_OUTPUT_DIR);
    FILE *file = fopen(midi_path, "wb");
    assert(file != NULL && fwrite(bytes, 1, size, file) == size);
    fclose(file);
    free(bytes);
    WeightedPipeline p = pipeline_run(URSATZ_TEST_OUTPUT_DIR, "galant_fixture.mid", NULL);
    const MeterMap *chain_map = p.meter.meter_map;
    OuterVoiceLines lines = {0};
    assert(outer_voice_extract(&p.voice_sonorities, p.events, p.pitch_values, p.note_count, chain_map, standard_meter_segmentation_beat_grid, NULL, NULL,
                               &lines) == OUTER_VOICE_OK);
    ChainGalantSegments chain_build;
    GalantSchemaMatches chain_schemas = {NULL, 0};
    ScaleSequenceMatches chain_sequences = {NULL, 0};
    chain_adapters(&p, &lines, chain_map, fixture.home, &chain_build, &chain_schemas, &chain_sequences);
    Outcome chain_outcome = score("the whole chain (outer voices, key regions, numerals from the MIDI file)", chain_map, &chain_build.list,
                                  &chain_schemas, &chain_sequences, planted, false);
    assert(chain_outcome.found >= CHAIN_FLOOR_FOUND);
    galant_schema_matches_destroy(&chain_schemas);
    scale_sequence_matches_destroy(&chain_sequences);
    chain_galant_segments_destroy(&chain_build);
    outer_voice_lines_destroy(&lines);
    pipeline_destroy(&p);
    schema_fixture_destroy(&fixture);
}

typedef struct {
    const char *name;
    const char *midi;
    const char *fixture;
} RealPiece;

static void run_real_piece(const RealPiece *piece) {
    Annotation a = load_annotation(piece->fixture);
    WeightedPipeline p = pipeline_run(URSATZ_CORPUS_DIR, piece->midi, NULL);
    const MeterMap *map = p.meter.meter_map;
    OuterVoiceLines lines = {0};
    assert(outer_voice_extract(&p.voice_sonorities, p.events, p.pitch_values, p.note_count, map, standard_meter_segmentation_beat_grid, NULL, NULL,
                               &lines) == OUTER_VOICE_OK);
    ChainGalantSegments build;
    GalantSchemaMatches schemas = {NULL, 0};
    ScaleSequenceMatches sequences = {NULL, 0};
    chain_adapters(&p, &lines, map, a.home, &build, &schemas, &sequences);
    print_matches(piece->name, map, &build, &schemas, &sequences);
    galant_schema_matches_destroy(&schemas);
    scale_sequence_matches_destroy(&sequences);
    chain_galant_segments_destroy(&build);
    outer_voice_lines_destroy(&lines);
    pipeline_destroy(&p);
}

int main(void) {
    PlantedList planted = load_planted();
    run_fixture_alone(&planted);
    static const RealPiece PIECES[2] = {{"Mozart 545", "Mozart 545.mid", "mozart545_cadences.txt"},
                                        {"Clementi 36", "Clementi 36.mid", "clementi36_cadences.txt"}};
    printf("The whole chain on the reference pieces (printed, not asserted):\n");
    for (size_t i = 0; i < 2; i++) {
        run_real_piece(&PIECES[i]);
    }
    printf("test_schema_sequence: ok\n");
    return 0;
}
