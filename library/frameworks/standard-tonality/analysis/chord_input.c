#include "chord_input.h"

#include <stdlib.h>
#include "pitch/midi_pitch.h"

/* a < b for two MusicalTimes (positive denominators): cross-multiplied, so no division. */
static bool time_before(const MusicalTime *a, const MusicalTime *b) {
    long long left = (long long)musical_time_numerator(a) * (long long)musical_time_denominator(b);
    long long right = (long long)musical_time_numerator(b) * (long long)musical_time_denominator(a);
    return left < right;
}

/* The region containing `start`: the last region that starts at or before it. -1 if none. */
static long region_at(const StandardTonalityKeyRegionInput *regions, size_t region_count, const MusicalTime *start) {
    long found = -1;
    for (size_t i = 0; i < region_count; i++) {
        if (time_before(start, regions[i].start)) {
            break;
        }
        found = (long)i;
    }
    return found;
}

static double chord_uncertainty_of(const Chord *chord) {
    const Uncertainty *uncertainty = interpretation_uncertainty(chord_interpretation(chord));
    if (uncertainty == NULL || uncertainty_kind(uncertainty) != UNCERTAINTY_KIND_SCALAR_CONFIDENCE) {
        return 1.0;
    }
    unsigned long denominator = uncertainty_scalar_confidence_denominator(uncertainty);
    if (denominator == 0) {
        return 1.0;
    }
    double confidence = (double)uncertainty_scalar_confidence_numerator(uncertainty) / (double)denominator;
    double value = 1.0 - confidence;
    return value < 0.0 ? 0.0 : (value > 1.0 ? 1.0 : value);
}

static bool is_chord_member(const StandardTonalityChordInput *input, int pitch_class) {
    int interval = pitch_class_of_midi(pitch_class - input->root_pitch_class);
    for (size_t i = 0; i < input->interval_count; i++) {
        if (input->intervals[i] == interval) {
            return true;
        }
    }
    return false;
}

/* The bass register: a chord tone counts as the bass only within an octave of the lowest sounding pitch. */
#define STANDARD_TONALITY_BASS_REGISTER 12

/* The bass from the lowest sounding chord tone; else the previous segment's bass if still a member; else -1. */
static int chord_bass_of(const StandardTonalityChordInput *input, const StandardTonalitySegmentInput *segment,
                         int previous_bass) {
    int floor = segment->pitches[0];
    for (size_t i = 1; i < segment->pitch_count; i++) {
        if (segment->pitches[i] < floor) {
            floor = segment->pitches[i];
        }
    }
    bool found = false;
    int lowest = 0;
    for (size_t i = 0; i < segment->pitch_count; i++) {
        int pitch = segment->pitches[i];
        if (pitch - floor < STANDARD_TONALITY_BASS_REGISTER && is_chord_member(input, pitch_class_of_midi(pitch)) &&
            (!found || pitch < lowest)) {
            lowest = pitch;
            found = true;
        }
    }
    if (found) {
        return pitch_class_of_midi(lowest);
    }
    if (previous_bass >= 0 && is_chord_member(input, previous_bass)) {
        return previous_bass;
    }
    return -1;
}

/* Reads the chord's and key's structure into `input`; false (has_numeral stays false) if either is missing. */
static void read_structures(StandardTonalityChordInput *input, const Chord *chord, const Key *key) {
    ChordStructure chord_structure_value;
    KeyStructure key_structure_value;
    if (chord == NULL || key == NULL || !chord_structure(chord, &chord_structure_value) ||
        !key_structure(key, &key_structure_value) || chord_structure_value.root_pitch_class < 0 ||
        chord_structure_value.interval_count == 0) {
        return;
    }
    input->has_numeral = true;
    input->chord = chord;
    input->root_pitch_class = chord_structure_value.root_pitch_class;
    input->bass_pitch_class = chord_structure_value.bass_pitch_class;
    input->sounding_bass_pitch_class = chord_structure_value.bass_pitch_class;
    input->interval_count = chord_structure_value.interval_count;
    for (size_t i = 0; i < chord_structure_value.interval_count; i++) {
        input->intervals[i] = chord_structure_value.root_relative_intervals[i];
    }
    input->tonic_pitch_class = key_structure_value.tonic_pitch_class;
    input->mode = key_structure_value.mode == KEY_STRUCTURE_MODE_MINOR ? STANDARD_TONALITY_ROMAN_KEY_MINOR
                                                                       : STANDARD_TONALITY_ROMAN_KEY_MAJOR;
    input->chord_uncertainty = chord_uncertainty_of(chord);
}

StandardTonalityChordInputStatus standard_tonality_build_chord_inputs(
    const StandardTonalitySegmentInput *segments, size_t segment_count,
    const StandardTonalityKeyRegionInput *regions, size_t region_count,
    StandardTonalityChordInput **out, size_t *out_count) {
    if (out == NULL || out_count == NULL || (segments == NULL && segment_count > 0) ||
        (regions == NULL && region_count > 0)) {
        return STANDARD_TONALITY_CHORD_INPUT_ERR_NULL_ARGUMENT;
    }
    for (size_t i = 0; i < region_count; i++) {
        if (regions[i].key == NULL || regions[i].start == NULL ||
            (i > 0 && !time_before(regions[i - 1].start, regions[i].start))) {
            return STANDARD_TONALITY_CHORD_INPUT_ERR_BAD_REGION;
        }
    }
    for (size_t i = 0; i < segment_count; i++) {
        if (segments[i].segment_id == NULL || segments[i].start == NULL) {
            return STANDARD_TONALITY_CHORD_INPUT_ERR_BAD_SEGMENT;
        }
    }

    StandardTonalityChordInput *inputs =
        (StandardTonalityChordInput *)calloc(segment_count > 0 ? segment_count : 1, sizeof(*inputs));
    if (inputs == NULL) {
        return STANDARD_TONALITY_CHORD_INPUT_ERR_ALLOC;
    }

    int previous_bass = -1;
    for (size_t i = 0; i < segment_count; i++) {
        StandardTonalityChordInput *input = &inputs[i];
        input->segment_id = segments[i].segment_id;
        input->start = segments[i].start;
        input->bass_pitch_class = -1;
        input->sounding_bass_pitch_class = -1;
        input->region_index = 0;
        if (segments[i].weights != NULL) {
            input->has_weights = true;
            for (int pc = 0; pc < 12; pc++) {
                input->weights[pc] = segments[i].weights[pc];
            }
        }
        long region = region_at(regions, region_count, segments[i].start);
        if (region >= 0) {
            input->region_index = (size_t)region;
            input->key_uncertainty = regions[region].uncertainty;
            read_structures(input, segments[i].chord, regions[region].key);
        }
        if (input->has_numeral && segments[i].pitches != NULL && segments[i].pitch_count > 0) {
            input->bass_pitch_class = chord_bass_of(input, &segments[i], previous_bass);
        }
        previous_bass = input->has_numeral ? input->bass_pitch_class : -1;
    }

    *out = inputs;
    *out_count = segment_count;
    return STANDARD_TONALITY_CHORD_INPUT_OK;
}

void standard_tonality_chord_inputs_destroy(StandardTonalityChordInput *inputs) {
    free(inputs);
}
