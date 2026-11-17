#include "chain/chain_inputs_internal.h"

#include <stdlib.h>
#include <string.h>

#include "theory/tonality/tonal_key.h"
#include "theory/harmony/harmony_key_change.h"
#include "theory/meter/segmentation_beat_grid.h"

/* The length of a beat, from the first measure's beat grid. */
static bool beat_duration(const MeterMap *map, Duration *out) {
    MusicalTime *starts = NULL;
    size_t count = 0;
    TimeSpan span;
    if (!standard_meter_segmentation_beat_grid(map, 1, NULL, &starts, &count) || count == 0) {
        return false;
    }
    bool ok = false;
    if (meter_map_measure_span(map, 1, &span) == METER_MAP_OK) {
        Rational first = starts[0].value;
        Rational next = count > 1 ? starts[1].value : span.end.value;
        Rational length;
        ok = rational_sub(next, first, &length) == RATIONAL_OK && duration_of(length, out) == DURATION_OK;
    }
    free(starts);
    return ok;
}

/* The degree (1..7) of an outer line's note in the major key on `tonic`, or 0 for a chromatic or an absent note. */
/* Already the tonality owner's reading (standard_tonality_natural_degree_of_semitone); kept as the one chain wrapper (R-582). */
static int degree_of(const OuterVoiceEntry *entry, int tonic) {
    if (entry == NULL || !entry->present) {
        return 0;
    }
    long position = pitch_numeric_value_position_numerator(entry->pitch);
    int degree = 0;
    int alteration = 0;
    if (!standard_tonality_natural_degree_of_semitone((int)(position - tonic), &degree, &alteration)) {
        return 0;
    }
    return alteration == 0 ? degree : 0;
}

static void claim_intervals(const StandardTonalityRomanClaim *claim, int out[4], size_t *count) {
    memset(out, 0, 4 * sizeof(int));
    if (claim->seventh) {
        *count = standard_tonality_seventh_quality_semitones((StandardTonalitySeventhQuality)claim->quality, out) ? 4 : 0;
    } else {
        *count = standard_tonality_triad_quality_semitones((StandardTonalityTriadQuality)claim->quality, out) ? 3 : 0;
    }
}

ChainInputStatus chain_build_harmony_segments(const ChainSegments *segments, size_t count, const MeterMap *map,
                                              const StandardTonalityChordInput *chords, const StandardTonalityRomanResult *romans,
                                              const OuterVoiceLines *lines, ChainHarmonyInputs *out) {
    if (segments == NULL || map == NULL || chords == NULL || romans == NULL || lines == NULL || out == NULL) {
        return CHAIN_INPUT_ERR_NULL_ARGUMENT;
    }
    if (count == 0 || count > segments->count || count > lines->count) {
        return CHAIN_INPUT_ERR_BAD_NOTES;
    }
    ChainHarmonyInputs built;
    memset(&built, 0, sizeof(built));
    built.count = count;
    built.inputs = calloc(count, sizeof(HarmonySegmentInput));
    built.positions = calloc(count, sizeof(MetricalPosition *));
    if (built.inputs == NULL || built.positions == NULL) {
        chain_harmony_inputs_destroy(&built);
        return CHAIN_INPUT_ERR_ALLOC;
    }
    if (!beat_duration(map, &built.beat_length)) {
        chain_harmony_inputs_destroy(&built);
        return CHAIN_INPUT_ERR_KEYS;
    }
    for (size_t s = 0; s < count; s++) {
        const TimeSpan *window = sonority_time_window(segments->sonorities.items[s].sonority);
        if (metrical_position_from_time(map, &window->start, &built.positions[s]) != METRICAL_POSITION_OK) {
            chain_harmony_inputs_destroy(&built);
            return CHAIN_INPUT_ERR_KEYS;
        }
        HarmonySegmentInput *input = &built.inputs[s];
        AnalysisSegment segment = {segments->ids[s], *window, built.positions[s], chain_attack_strength(map, &window->start)};
        input->segment = segment;
        input->key_ref = "key";
        input->mode = HARMONY_MODE_MAJOR;
        input->has_beat_length = true;
        input->beat_length = built.beat_length;
        if (!romans[s].has_numeral) {
            input->degree = 0;
            input->interval_count = 3;
            continue;
        }
        const StandardTonalityRomanClaim *claim = &romans[s].claim;
        int intervals[4];
        size_t interval_count = 0;
        claim_intervals(claim, intervals, &interval_count);
        memset(input->intervals, 0, sizeof(input->intervals));
        for (size_t k = 0; k < interval_count; k++) {
            input->intervals[k] = intervals[k];
        }
        input->interval_count = interval_count;
        input->degree = claim->degree;
        input->inversion = claim->inversion;
        input->applied_to = claim->applied ? claim->applied_target_degree : 0;
        input->mode = chords[s].mode == STANDARD_TONALITY_ROMAN_KEY_MINOR ? HARMONY_MODE_MINOR : HARMONY_MODE_MAJOR;
        input->uncertainty = romans[s].uncertainty;
        input->bass_degree = degree_of(&lines->bass[s], chords[s].tonic_pitch_class);
        input->soprano_degree = degree_of(&lines->soprano[s], chords[s].tonic_pitch_class);
    }
    *out = built;
    return CHAIN_INPUT_OK;
}

void chain_harmony_inputs_destroy(ChainHarmonyInputs *inputs) {
    if (inputs == NULL) {
        return;
    }
    for (size_t s = 0; s < inputs->count && inputs->positions != NULL; s++) {
        metrical_position_destroy(inputs->positions[s]);
    }
    free(inputs->positions);
    free(inputs->inputs);
    memset(inputs, 0, sizeof(*inputs));
}

ChainInputStatus chain_add_boundary_cadences(const HarmonySegment *prepared, size_t count, const CommonPracticeKeyRegions *regions,
                                             int home_key_index, HarmonyCadences *cadences) {
    if (prepared == NULL || regions == NULL || cadences == NULL) {
        return CHAIN_INPUT_ERR_NULL_ARGUMENT;
    }
    Rational *starts = calloc(regions->region_count + 1, sizeof(Rational));
    if (starts == NULL) {
        return CHAIN_INPUT_ERR_ALLOC;
    }
    size_t n = 0;
    for (size_t i = 1; i < regions->region_count; i++) {
        if (regions->regions[i].key_index == home_key_index && regions->regions[i - 1].key_index != home_key_index) {
            starts[n++] = regions->regions[i].span.start.value;
        }
    }
    HarmonyCadenceAnalysisStatus status = harmony_analysis_add_boundary_cadences(prepared, count, NULL, starts, n, cadences);
    free(starts);
    return status == HARMONY_CADENCE_ANALYSIS_OK ? CHAIN_INPUT_OK : CHAIN_INPUT_ERR_KEYS;
}

ChainInputStatus chain_confirm_key_changes(CommonPracticeKeyRegions *regions, const Rational *segment_starts, const HarmonyCadences *cadences) {
    if (regions == NULL || segment_starts == NULL || cadences == NULL) {
        return CHAIN_INPUT_ERR_NULL_ARGUMENT;
    }
    Rational *arrivals = calloc(cadences->count + 1, sizeof(Rational));
    if (arrivals == NULL) {
        return CHAIN_INPUT_ERR_ALLOC;
    }
    for (size_t k = 0; k < cadences->count; k++) {
        arrivals[k] = segment_starts[cadences->items[k].arrival_first];
    }
    ChainInputStatus status = CHAIN_INPUT_OK;
    for (size_t i = 0; i < regions->change_count && status == CHAIN_INPUT_OK; i++) {
        CommonPracticeKeyChange *change = &regions->changes[i];
        if (change->is_return) {
            continue;
        }
        const CommonPracticeKeyRegion *into = &regions->regions[change->to_region];
        KeyChangeExtent confirmed = change->extent;
        if (!harmony_key_change_confirm(change->extent, into->span.start.value,
                                        into->span.end.value, arrivals, cadences->count, &confirmed)) {
            status = CHAIN_INPUT_ERR_KEYS;
            break;
        }
        change->extent = confirmed;
        change->cadence_confirmed = key_change_extent_is_modulation(confirmed);
    }
    for (size_t i = 1; i < regions->change_count && status == CHAIN_INPUT_OK; i++) {
        if (regions->changes[i].is_return) {
            regions->changes[i].extent = regions->changes[i - 1].extent;
            regions->changes[i].cadence_confirmed = regions->changes[i - 1].cadence_confirmed;
        }
    }
    free(arrivals);
    return status;
}
