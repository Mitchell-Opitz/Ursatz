#define _POSIX_C_SOURCE 200809L
#include "chorale_chain.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "analysis/outer_voice.h"
#include "analysis_cadence.h"
#include "applied_chords.h"
#include "chord_input.h"
#include "io/musicxml_importer.h"
#include "key_regions.h"
#include "key_vocabulary.h"
#include "metric_position_rules.h"
#include "pitch/pitch_spelling.h"
#include "roman_numeral_adapter.h"
#include "segmentation_beat_grid.h"
#include "semantics/sonority_segmentation.h"
#include "time/duration.h"
#include "time/exact_rational.h"
#include "time/metrical_position.h"
#include "time/temporal_anchor.h"
#include "time/time_span.h"
#include "weighted_chord_identification.h"

/* The glue after the import is the same as tests/analysis/weighted_pipeline.h's (which imports MIDI); it is kept
   here so that the existing test helper stays as it is. */

typedef struct {
    const char *id;
    size_t index;
} NoteKey;

static int compare_keys(const void *a, const void *b) {
    return strcmp(((const NoteKey *)a)->id, ((const NoteKey *)b)->id);
}

static int semitone(const PitchSpelling *spelling) {
    static const int LETTER_SEMITONE[7] = {9, 11, 0, 2, 4, 5, 7};
    return 12 * (pitch_spelling_octave(spelling) + 1) + LETTER_SEMITONE[pitch_spelling_letter(spelling)] +
           pitch_spelling_accidental(spelling);
}

static BeatStrength attack_strength(const MeterMap *map, const MusicalTime *onset) {
    MetricalPosition *position = NULL;
    BeatStrength strength = BEAT_STRENGTH_WEAK;
    if (metrical_position_from_time(map, onset, &position) != METRICAL_POSITION_OK) {
        return strength;
    }
    const TimeSignature *reading = NULL;
    if (metrical_position_reading_signature(map, metrical_position_measure(position), &reading) ==
        METRICAL_POSITION_OK) {
        BeatStrength found;
        if (standard_meter_beat_strength(reading, position, &found)) {
            strength = found;
        }
    }
    metrical_position_destroy(position);
    return strength;
}

static double as_double(ExactRational r) {
    return (double)r.n / (double)r.d;
}

typedef struct {
    MusicXmlImport imported;
    bool have_import;
    ScoreLayout layout;
    bool have_layout;
    MeterMap *map;
    AdaptiveSonorities sonorities;
    bool have_sonorities;
    CommonPracticeMember *members;
    CommonPracticeSegment *segments;
    size_t segment_count;
    char (*segment_ids)[32];
    double *starts;
    double *ends;
    CommonPracticeKeySegment *key_segments;
    MusicalTime **boundaries; /* pass 2: the phrase ends found from pass 1, owned copies */
    size_t boundary_count;
    CommonPracticeKeyRegions regions;
    bool have_regions;
    CommonPracticeWeightedResult *results; /* one per segment */
    Key **annotation_keys;
    size_t annotation_key_count;
    StandardTonalityKeyRegionInput *chain_region_inputs;
    StandardTonalityKeyRegionInput *annotation_region_inputs;
    StandardTonalitySegmentInput *segment_inputs;
    int *segment_pitches; /* the members' pitches, segment after segment, for the chord bass */
    StandardTonalityChordInput *chain_inputs;
    StandardTonalityRomanResult *chain_romans;
    StandardTonalityChordInput *annotation_inputs;
    StandardTonalityRomanResult *annotation_romans;
    RomanTextAnalysis annotation;
    bool have_annotation;
} Run;

/* The stages that depend on the key regions: freed between the two passes and at the end. */
static void pass_destroy(Run *r) {
    if (r->annotation_romans != NULL) {
        standard_tonality_roman_results_destroy(r->annotation_romans, r->segment_count);
    }
    if (r->chain_romans != NULL) {
        standard_tonality_roman_results_destroy(r->chain_romans, r->segment_count);
    }
    standard_tonality_chord_inputs_destroy(r->annotation_inputs);
    standard_tonality_chord_inputs_destroy(r->chain_inputs);
    free(r->segment_inputs);
    free(r->segment_pitches);
    free(r->annotation_region_inputs);
    free(r->chain_region_inputs);
    for (size_t i = 0; i < r->annotation_key_count; i++) {
        key_destroy(r->annotation_keys[i]);
    }
    free(r->annotation_keys);
    if (r->results != NULL) {
        for (size_t s = 0; s < r->segment_count; s++) {
            common_practice_weighted_result_destroy(&r->results[s]);
        }
        free(r->results);
    }
    if (r->have_regions) {
        common_practice_key_regions_destroy(&r->regions);
    }
    r->annotation_romans = NULL;
    r->chain_romans = NULL;
    r->annotation_inputs = NULL;
    r->chain_inputs = NULL;
    r->segment_inputs = NULL;
    r->segment_pitches = NULL;
    r->annotation_region_inputs = NULL;
    r->chain_region_inputs = NULL;
    r->annotation_keys = NULL;
    r->annotation_key_count = 0;
    r->results = NULL;
    r->have_regions = false;
}

static void run_destroy(Run *r) {
    pass_destroy(r);
    for (size_t i = 0; i < r->boundary_count; i++) {
        musical_time_destroy(r->boundaries[i]);
    }
    free(r->boundaries);
    free(r->key_segments);
    free(r->starts);
    free(r->ends);
    free(r->segment_ids);
    free(r->segments);
    free(r->members);
    if (r->have_sonorities) {
        adaptive_sonorities_destroy(&r->sonorities);
    }
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

/* The beat a segment [start, end) lies inside, as a non-zero index (measure and beat), or 0 when it spans beats or
   the grid cannot be read. */
static long beat_index(const MeterMap *map, ExactRational start, ExactRational end) {
    MusicalTime *at = er_to_time(start);
    int measure = 0;
    bool found = at != NULL && meter_map_measure_index_at(map, at, &measure) == METER_MAP_OK;
    musical_time_destroy(at);
    MusicalTime **starts = NULL;
    size_t count = 0;
    if (!found || !standard_meter_segmentation_beat_grid(map, measure, NULL, &starts, &count)) {
        return 0;
    }
    TimeSpan *span = NULL;
    long index = 0;
    if (meter_map_measure_span(map, measure, &span) == METER_MAP_OK) {
        ExactRational measure_end = er_of_time(time_span_end(span));
        for (size_t b = 0; b < count; b++) {
            ExactRational beat_start = er_of_time(starts[b]);
            ExactRational beat_end = b + 1 < count ? er_of_time(starts[b + 1]) : measure_end;
            if (er_cmp(beat_start, start) <= 0 && er_cmp(start, beat_end) < 0) {
                if (er_cmp(end, beat_end) <= 0) {
                    index = ((long)measure + 1) * 1000 + (long)b + 1;
                }
                break;
            }
        }
        time_span_destroy(span);
    }
    for (size_t b = 0; b < count; b++) {
        musical_time_destroy(starts[b]);
    }
    free(starts);
    return index;
}

/* Segments, members and weights from the imported notes. */
static bool build_segments(Run *r, char *message, size_t message_size) {
    MusicXmlImport *imp = &r->imported;
    if (sonority_builder_build_adaptive(imp->events, imp->count, r->map, standard_meter_segmentation_beat_grid, NULL,
                                        NULL, &r->sonorities) != AD_SEGMENTATION_OK) {
        fail(message, message_size, "adaptive segmentation failed");
        return false;
    }
    r->have_sonorities = true;
    NoteKey *keys = (NoteKey *)malloc(imp->count * sizeof(NoteKey));
    if (keys == NULL) {
        return false;
    }
    for (size_t i = 0; i < imp->count; i++) {
        keys[i].id = entity_id_value(note_event_id(imp->events[i]));
        keys[i].index = i;
    }
    qsort(keys, imp->count, sizeof(NoteKey), compare_keys);

    size_t total = 0;
    for (size_t i = 0; i < r->sonorities.count; i++) {
        total += sonority_event_id_count(r->sonorities.items[i].sonority);
    }
    r->segment_count = r->sonorities.count;
    r->members = (CommonPracticeMember *)calloc(total > 0 ? total : 1, sizeof(CommonPracticeMember));
    r->segments = (CommonPracticeSegment *)calloc(r->segment_count, sizeof(CommonPracticeSegment));
    r->segment_ids = calloc(r->segment_count, sizeof(char[32]));
    r->starts = (double *)calloc(r->segment_count, sizeof(double));
    r->ends = (double *)calloc(r->segment_count, sizeof(double));
    if (r->members == NULL || r->segments == NULL || r->segment_ids == NULL || r->starts == NULL || r->ends == NULL) {
        free(keys);
        return false;
    }
    size_t next = 0;
    for (size_t s = 0; s < r->segment_count; s++) {
        const AdaptiveSonority *segment = &r->sonorities.items[s];
        const TimeSpan *window = sonority_time_window(segment->sonority);
        ExactRational start = er_of_time(time_span_start(window));
        ExactRational end = er_of_time(time_span_end(window));
        r->starts[s] = as_double(start);
        r->ends[s] = as_double(end);
        snprintf(r->segment_ids[s], sizeof(r->segment_ids[s]), "seg-%zu", s);
        size_t count = sonority_event_id_count(segment->sonority);
        r->segments[s].members = &r->members[next];
        r->segments[s].member_count = count;
        r->segments[s].id = r->segment_ids[s];
        r->segments[s].beat = beat_index(r->map, start, end);
        for (size_t j = 0; j < count; j++) {
            NoteKey probe = {entity_id_value(sonority_event_id_at(segment->sonority, j)), 0};
            const NoteKey *found = (const NoteKey *)bsearch(&probe, keys, imp->count, sizeof(NoteKey), compare_keys);
            if (found == NULL) {
                free(keys);
                fail(message, message_size, "a segment names a note that was not imported");
                return false;
            }
            const NoteEvent *note = imp->events[found->index];
            const MusicalTime *onset_time = temporal_anchor_position(note_event_temporal_anchor(note));
            ExactRational onset = er_of_time(onset_time);
            ExactRational release = {0, 1};
            ExactRational overlap = {0, 1};
            if (er_add(onset, er_of_duration(note_event_duration(note)), &release) != 0) {
                free(keys);
                return false;
            }
            ExactRational from = er_cmp(onset, start) > 0 ? onset : start;
            ExactRational to = er_cmp(release, end) < 0 ? release : end;
            if (er_cmp(to, from) <= 0 || er_sub(to, from, &overlap) != 0) {
                free(keys);
                fail(message, message_size, "a segment holds a note that does not sound in it");
                return false;
            }
            CommonPracticeMember *member = &r->members[next++];
            member->note_id = entity_id_value(note_event_id(note));
            member->pitch = semitone(note_event_optional_pitch_spelling(note));
            member->held = segment->held[j];
            member->duration = as_double(overlap);
            member->strength = member->held ? BEAT_STRENGTH_STRONG : attack_strength(r->map, onset_time);
            const VoiceReference *voice = note_event_optional_voice(note);
            member->voice_id = voice != NULL ? entity_id_value(voice_reference_target_id(voice)) : NULL;
            member->onset = as_double(onset);
        }
    }
    free(keys);
    return true;
}

/* The key regions. The first call weights the segments; a later call reuses them (and the chords set on them). */
static bool estimate_keys(Run *r, const MusicalTime *const *boundaries, size_t boundary_count) {
    CommonPracticeWeightedConfig weighting = common_practice_weighted_config_default();
    bool weigh = r->key_segments == NULL;
    if (weigh) {
        r->key_segments = (CommonPracticeKeySegment *)calloc(r->segment_count, sizeof(CommonPracticeKeySegment));
    }
    if (r->key_segments == NULL) {
        return false;
    }
    for (size_t s = 0; s < r->segment_count && weigh; s++) {
        const TimeSpan *window = sonority_time_window(r->sonorities.items[s].sonority);
        r->key_segments[s].start = time_span_start(window);
        r->key_segments[s].end = time_span_end(window);
        r->key_segments[s].source = &r->segments[s];
        if (common_practice_weight_segment(r->segments[s].members, r->segments[s].member_count, &weighting,
                                           &r->key_segments[s].weights) != COMMON_PRACTICE_WEIGHTED_OK) {
            return false;
        }
    }
    MusicXmlImport *imp = &r->imported;
    CommonPracticeKeySignatureEvent *events = NULL;
    if (imp->key_signature_count > 0) {
        events = (CommonPracticeKeySignatureEvent *)calloc(imp->key_signature_count, sizeof(*events));
        if (events == NULL) {
            return false;
        }
        for (size_t i = 0; i < imp->key_signature_count; i++) {
            events[i].position = imp->key_signatures[i].position;
            events[i].sharps_flats = imp->key_signatures[i].sharps_flats;
        }
    }
    CommonPracticeKeyRegionsConfig config = common_practice_key_regions_config_default();
    config.phrase_boundaries = boundaries;
    config.phrase_boundary_count = boundary_count;
    CommonPracticeKeyRegionsStatus status = key_regions_estimate(r->key_segments, r->segment_count, r->map, events,
                                                                  imp->key_signature_count, &config, &r->regions);
    free(events);
    if (status != COMMON_PRACTICE_KEY_REGIONS_OK) {
        return false;
    }
    r->have_regions = true;
    return true;
}

/* The region holding time `start`: the last whose start is not after it. */
static size_t region_at(const CommonPracticeKeyRegions *regions, double t) {
    size_t found = 0;
    for (size_t i = 0; i < regions->region_count; i++) {
        ExactRational start = er_of_time(time_span_start(regions->regions[i].span));
        if (as_double(start) <= t + 1e-12) {
            found = i;
        }
    }
    return found;
}

static bool identify_chords(Run *r) {
    r->results = (CommonPracticeWeightedResult *)calloc(r->segment_count, sizeof(CommonPracticeWeightedResult));
    if (r->results == NULL) {
        return false;
    }
    size_t s = 0;
    while (s < r->segment_count) {
        size_t region = region_at(&r->regions, r->starts[s]);
        size_t end = s + 1;
        while (end < r->segment_count && region_at(&r->regions, r->starts[end]) == region) {
            end++;
        }
        int key_index = r->regions.regions[region].key_index;
        int tonic = common_practice_key_index_tonic(key_index);
        CommonPracticeKeyMode mode = common_practice_key_index_mode(key_index);
        CommonPracticeWeightedResults group = {0};
        if (common_practice_identify_weighted_sequence(&r->segments[s], end - s, tonic, mode, NULL, &group) !=
            COMMON_PRACTICE_WEIGHTED_OK) {
            return false;
        }
        for (size_t i = 0; i < group.count; i++) {
            r->results[s + i] = group.items[i]; /* ownership moves to r->results */
        }
        free(group.items);
        s = end;
    }
    return true;
}

static bool read_numerals(Run *r) {
    size_t n = r->segment_count;
    r->segment_inputs = (StandardTonalitySegmentInput *)calloc(n, sizeof(StandardTonalitySegmentInput));
    r->chain_region_inputs = (StandardTonalityKeyRegionInput *)calloc(r->regions.region_count, sizeof(*r->chain_region_inputs));
    r->annotation_region_inputs =
        (StandardTonalityKeyRegionInput *)calloc(r->annotation.key_count + 1, sizeof(*r->annotation_region_inputs));
    r->annotation_keys = (Key **)calloc(r->annotation.key_count + 1, sizeof(Key *));
    size_t pitch_total = 0;
    for (size_t i = 0; i < n; i++) {
        pitch_total += r->segments[i].member_count;
    }
    r->segment_pitches = (int *)calloc(pitch_total > 0 ? pitch_total : 1, sizeof(int));
    if (r->segment_pitches == NULL || r->segment_inputs == NULL || r->chain_region_inputs == NULL ||
        r->annotation_region_inputs == NULL || r->annotation_keys == NULL) {
        return false;
    }
    pitch_total = 0;
    for (size_t i = 0; i < n; i++) {
        r->segment_inputs[i].segment_id = r->segment_ids[i];
        r->segment_inputs[i].start = r->key_segments[i].start;
        r->segment_inputs[i].chord = r->results[i].matched ? r->results[i].best_chord : NULL;
        r->segment_inputs[i].weights = r->key_segments[i].weights.pitch_class;
        r->segment_inputs[i].pitches = &r->segment_pitches[pitch_total];
        r->segment_inputs[i].pitch_count = r->segments[i].member_count;
        for (size_t j = 0; j < r->segments[i].member_count; j++) {
            r->segment_pitches[pitch_total++] = r->segments[i].members[j].pitch;
        }
    }
    for (size_t i = 0; i < r->regions.region_count; i++) {
        r->chain_region_inputs[i].key = r->regions.regions[i].key;
        r->chain_region_inputs[i].start = time_span_start(r->regions.regions[i].span);
        r->chain_region_inputs[i].uncertainty = r->regions.regions[i].uncertainty;
    }
    size_t count = 0;
    if (standard_tonality_build_chord_inputs(r->segment_inputs, n, r->chain_region_inputs, r->regions.region_count,
                                             &r->chain_inputs, &count) != STANDARD_TONALITY_CHORD_INPUT_OK ||
        standard_tonality_build_roman_numerals(r->chain_inputs, count, &r->chain_romans) != STANDARD_TONALITY_ROMAN_OK) {
        return false;
    }
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
    count = 0;
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

static void claim_intervals(const StandardTonalityRomanClaim *claim, int out[4], size_t *count) {
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
        ExactRational next = count > 1 ? er_of_time(starts[1]) : er_of_time(time_span_end(span));
        ExactRational length = {0, 1};
        ok = er_sub(next, er_of_time(starts[0]), &length) == 0 &&
             duration_create((long)length.n, (unsigned long)length.d, out) == DURATION_OK;
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
    HarmonySegmentInput *inputs = (HarmonySegmentInput *)calloc(n, sizeof(HarmonySegmentInput));
    MetricalPosition **positions = (MetricalPosition **)calloc(n, sizeof(MetricalPosition *));
    Duration *beat_length = NULL;
    HarmonySegment *prepared = NULL;
    HarmonyCadences cadences = {NULL, 0};
    bool ok = inputs != NULL && positions != NULL && beat_duration(r->map, &beat_length);
    for (size_t i = 0; i < n && ok; i++) {
        const TimeSpan *window = sonority_time_window(r->sonorities.items[i].sonority);
        ok = metrical_position_from_time(r->map, time_span_start(window), &positions[i]) == METRICAL_POSITION_OK;
        if (!ok) {
            break;
        }
        HarmonySegmentInput *input = &inputs[i];
        input->segment_id = r->segment_ids[i];
        input->span = window;
        input->position = positions[i];
        input->beat_strength = attack_strength(r->map, time_span_start(window));
        input->key_ref = "key";
        input->mode = HARMONY_MODE_MAJOR;
        input->beat_length = beat_length;
        if (!r->chain_romans[i].has_numeral) {
            input->degree = 0;
            input->interval_count = 3;
            continue;
        }
        const StandardTonalityRomanClaim *claim = &r->chain_romans[i].claim;
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
        input->mode = r->chain_inputs[i].mode == STANDARD_TONALITY_ROMAN_KEY_MINOR ? HARMONY_MODE_MINOR : HARMONY_MODE_MAJOR;
        input->uncertainty = r->chain_romans[i].uncertainty;
        int low = 1000;
        int high = -1000;
        for (size_t j = 0; j < r->segments[i].member_count; j++) {
            int pitch = r->segments[i].members[j].pitch;
            low = pitch < low ? pitch : low;
            high = pitch > high ? pitch : high;
        }
        input->bass_degree = r->segments[i].member_count > 0 ? degree_of_pitch(low, r->chain_inputs[i].tonic_pitch_class) : 0;
        input->soprano_degree = r->segments[i].member_count > 0 ? degree_of_pitch(high, r->chain_inputs[i].tonic_pitch_class) : 0;
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
        ExactRational at = er_of_time(a->chords[i].time);
        if (as_double(at) <= t + 1e-12) {
            found = &a->chords[i];
        }
    }
    return found;
}

static const RomanTextKey *key_at(const RomanTextAnalysis *a, double t) {
    const RomanTextKey *found = NULL;
    for (size_t i = 0; i < a->key_count; i++) {
        ExactRational at = er_of_time(a->keys[i].time);
        if (as_double(at) <= t + 1e-12) {
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
    out->key_regions = r->regions.region_count;

    size_t capacity = r->segment_count * 2 + r->annotation.chord_count + r->annotation.key_count + 4;
    double *cuts = (double *)malloc(capacity * sizeof(double));
    if (cuts == NULL || r->annotation.chord_count == 0) {
        free(cuts);
        return;
    }
    size_t n = 0;
    double first = as_double(er_of_time(r->annotation.chords[0].time));
    double end = as_double(r->layout.end);
    cuts[n++] = first;
    cuts[n++] = end;
    for (size_t s = 0; s < r->segment_count; s++) {
        cuts[n++] = r->starts[s];
        cuts[n++] = r->ends[s];
    }
    for (size_t i = 0; i < r->annotation.chord_count; i++) {
        cuts[n++] = as_double(er_of_time(r->annotation.chords[i].time));
    }
    for (size_t i = 0; i < r->annotation.key_count; i++) {
        cuts[n++] = as_double(er_of_time(r->annotation.keys[i].time));
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
            const CommonPracticeKeyRegion *region = &r->regions.regions[region_at(&r->regions, r->starts[s])];
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
    if (!build_segments(&r, message, message_size) || r.segment_count == 0) {
        if (message[0] == '\0') {
            fail(message, message_size, "no segments");
        }
        goto done;
    }
    /* Pass 1: the key regions from the pitch classes alone, then chords, numerals and cadences in them. Pass 2: the key
       regions again with those chords as evidence and the cadence ends as phrase boundaries, and everything after them
       once more. No further iteration. */
    for (int pass = 1; pass <= 2; pass++) {
        if (!estimate_keys(&r, (const MusicalTime *const *)r.boundaries, r.boundary_count)) {
            fail(message, message_size, "key regions failed");
            goto done;
        }
        if (!identify_chords(&r)) {
            fail(message, message_size, "chord identification failed");
            goto done;
        }
        if (!read_numerals(&r)) {
            fail(message, message_size, "the Roman numeral adapter failed");
            goto done;
        }
        if (pass == 1) {
            if (!find_phrase_ends(&r)) {
                fail(message, message_size, "the cadences failed");
                goto done;
            }
            for (size_t s = 0; s < r.segment_count; s++) {
                r.key_segments[s].chord = common_practice_key_chord_of(&r.results[s]);
            }
            pass_destroy(&r);
        }
    }
    compare(&r, out);
    status = CHORALE_CHAIN_OK;
done:
    run_destroy(&r);
    free(score);
    return status;
}
