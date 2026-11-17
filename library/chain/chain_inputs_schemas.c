#include "chain/chain_inputs_internal.h"

#include <stdio.h>
#include <string.h>

#include "model/time/meter_arithmetic.h"
#include "model/pitch/midi_pitch.h"

/* --- the galant segments --- */

void chain_galant_segments_destroy(ChainGalantSegments *segments) {
    if (segments == NULL) {
        return;
    }
    galant_segments_destroy(&segments->list);
    chain_release_spans(segments->spans, segments->span_count);
    memset(segments, 0, sizeof(*segments));
}

/* The length of a beat at `time`, in whole notes (a compound meter's beat is its dotted unit); a quarter when it cannot be read. */
static void beat_length_at(const MeterMap *map, const MusicalTime *time, long *numerator, unsigned long *denominator) {
    Rational length;
    int measure = 0;
    *numerator = 1;
    *denominator = 4;
    if (meter_map_measure_index_at(map, time, &measure) != METER_MAP_OK ||
        meter_map_beat_length(map, measure, &length) != METER_MAP_OK) {
        return;
    }
    *numerator = (long)length.num;
    *denominator = (unsigned long)length.den;
}

static int pitch_class_of(const OuterVoiceEntry *entry) {
    long position = pitch_numeric_value_position_numerator(entry->pitch);
    return pitch_class_of_midi((int)position);
}

static double clamp01(double x) {
    return x < 0.0 ? 0.0 : (x > 1.0 ? 1.0 : x);
}

static bool same_harmony(const GalantSegment *a, const GalantSegment *b) {
    return strcmp(a->key_id, b->key_id) == 0 && a->has_bass == b->has_bass && a->has_soprano == b->has_soprano &&
           (!a->has_bass || galant_degree_equal(a->bass, b->bass)) &&
           (!a->has_soprano || galant_degree_equal(a->soprano, b->soprano)) && a->has_numeral == b->has_numeral &&
           (!a->has_numeral || (galant_degree_equal(a->root, b->root) && a->inversion == b->inversion));
}

ChainInputStatus chain_build_galant_segments(const ChainSegments *segments, size_t count, const MeterMap *map,
                                             const OuterVoiceLines *lines, int home_key_index,
                                             const StandardTonalityChordInput *chords, const StandardTonalityRomanResult *romans,
                                             ChainGalantSegments *out) {
    if (segments == NULL || map == NULL || lines == NULL || chords == NULL || romans == NULL || out == NULL) {
        return CHAIN_INPUT_ERR_NULL_ARGUMENT;
    }
    int home_pc = common_practice_key_index_tonic(home_key_index);
    if (count == 0 || count > segments->count || count > lines->count || home_pc < 0) {
        return CHAIN_INPUT_ERR_BAD_NOTES;
    }
    ChainGalantSegments build;
    memset(&build, 0, sizeof(build));
    galant_segments_init(&build.list);
    ChainInputStatus status = CHAIN_INPUT_OK;
    for (size_t s = 0; s < count && status == CHAIN_INPUT_OK; s++) {
        const TimeSpan *window = sonority_time_window(segments->sonorities.items[s].sonority);
        int tonic_pc = chords[s].tonic_pitch_class;
        bool minor = chords[s].mode == STANDARD_TONALITY_ROMAN_KEY_MINOR;
        GalantSegment segment;
        memset(&segment, 0, sizeof(segment));
        AnalysisSegment analysis = {segments->ids[s], *window, NULL, chain_attack_strength(map, time_span_start(window))};
        segment.segment = analysis;
        beat_length_at(map, time_span_start(window), &segment.beat_numerator, &segment.beat_denominator);
        snprintf(segment.key_id, sizeof(segment.key_id), "key-%d%s", tonic_pc, minor ? "m" : "");
        segment.key = galant_degree_of_semitones(tonic_pc - home_pc);
        if (lines->bass[s].present) {
            segment.has_bass = true;
            segment.bass = galant_degree_of_semitones(pitch_class_of(&lines->bass[s]) - tonic_pc);
        }
        if (lines->soprano[s].present) {
            segment.has_soprano = true;
            segment.soprano = galant_degree_of_semitones(pitch_class_of(&lines->soprano[s]) - tonic_pc);
        }
        segment.uncertainty = 0.5;
        if (romans[s].has_numeral) {
            const StandardTonalityRomanClaim *claim = &romans[s].claim;
            int lowered = minor && (claim->degree == 3 || claim->degree == 6 || claim->degree == 7) &&
                                  !romans[s].reading.raised_leading_tone
                              ? -1
                              : 0;
            segment.has_numeral = true;
            segment.root = (GalantPitchDegree){claim->degree, lowered};
            if (claim->applied) {
                int target_lowered = minor && (claim->applied_target_degree == 3 || claim->applied_target_degree == 6 ||
                                               claim->applied_target_degree == 7)
                                         ? -1
                                         : 0;
                segment.root = galant_degree_plus((GalantPitchDegree){claim->applied_target_degree, target_lowered},
                                                  (GalantPitchDegree){claim->degree, 0});
                segment.applied_to = claim->applied_target_degree;
            }
            segment.quality = claim->quality;
            segment.seventh = claim->seventh;
            segment.inversion = claim->inversion;
            segment.uncertainty = clamp01(romans[s].uncertainty);
        }
        /* a held chord continues the segment before it */
        if (build.list.count > 0) {
            GalantSegment *last = &build.list.items[build.list.count - 1];
            if (same_harmony(last, &segment)) {
                TimeSpan merged;
                if (time_span_make(last->segment.span.start, window->end, &merged) != TIME_SPAN_OK) {
                    status = CHAIN_INPUT_ERR_ALLOC;
                    break;
                }
                last->segment.span = merged;
                last->uncertainty = last->uncertainty > segment.uncertainty ? last->uncertainty : segment.uncertainty;
                continue;
            }
        }
        if (galant_segments_add(&build.list, &segment) != GALANT_ANALYSIS_OK) {
            status = CHAIN_INPUT_ERR_BAD_NOTES;
        }
    }
    if (status != CHAIN_INPUT_OK) {
        chain_galant_segments_destroy(&build);
        return status;
    }
    *out = build;
    return CHAIN_INPUT_OK;
}
