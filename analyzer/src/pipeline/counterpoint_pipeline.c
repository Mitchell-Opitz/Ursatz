#include "pipeline/counterpoint_pipeline.h"

#include <stdlib.h>
#include <string.h>

#include "analysis/voice_separation.h"
#include "segmentation_beat_grid.h"
#include "time/musical_time.h"
#include "time/temporal_anchor.h"

/* The notes of one voice, as ImportedNotes indices in onset order. */
typedef struct {
    size_t *notes;
    size_t count;
} VoiceNotes;

typedef struct {
    VoiceNotes upper;
    VoiceNotes lower;
    double uncertainty;
} TwoVoices;

static void two_voices_destroy(TwoVoices *voices) {
    free(voices->upper.notes);
    free(voices->lower.notes);
    memset(voices, 0, sizeof(*voices));
}

/* a/b compared with c/d for positive denominators: negative, zero or positive. */
static int compare_rational(long long a, long long b, long long c, long long d) {
    long long left = a * d;
    long long right = c * b;
    return left < right ? -1 : (left > right ? 1 : 0);
}

static const MusicalTime *onset_of(const ImportedNotes *notes, size_t note) {
    return temporal_anchor_position(note_event_temporal_anchor(notes->events[note]));
}

static const ImportedNotes *sort_notes;

static int compare_onset(const void *left, const void *right) {
    size_t a = *(const size_t *)left;
    size_t b = *(const size_t *)right;
    const MusicalTime *time_a = onset_of(sort_notes, a);
    const MusicalTime *time_b = onset_of(sort_notes, b);
    int order = compare_rational((long long)musical_time_numerator(time_a), (long long)musical_time_denominator(time_a),
                                 (long long)musical_time_numerator(time_b), (long long)musical_time_denominator(time_b));
    if (order != 0) {
        return order;
    }
    long pitch_a = pitch_numeric_value_position_numerator(sort_notes->pitch_values[a]);
    long pitch_b = pitch_numeric_value_position_numerator(sort_notes->pitch_values[b]);
    return pitch_a < pitch_b ? -1 : (pitch_a > pitch_b ? 1 : 0);
}

static void sort_voice(const ImportedNotes *notes, VoiceNotes *voice) {
    sort_notes = notes;
    qsort(voice->notes, voice->count, sizeof(size_t), compare_onset);
}

/* The sum of the voice's pitches over its note count, compared across voices without dividing. */
static long pitch_sum(const ImportedNotes *notes, const VoiceNotes *voice) {
    long sum = 0;
    for (size_t i = 0; i < voice->count; i++) {
        sum += pitch_numeric_value_position_numerator(notes->pitch_values[voice->notes[i]]);
    }
    return sum;
}

/* The input's two streams as the voices, the higher-sounding one on top. */
static CounterpointCheckResult voices_from_streams(const ImportedNotes *notes, TwoVoices *out) {
    size_t first = notes->streams[0];
    VoiceNotes voices[2] = {{malloc(notes->count * sizeof(size_t)), 0}, {malloc(notes->count * sizeof(size_t)), 0}};
    if (!voices[0].notes || !voices[1].notes) {
        free(voices[0].notes);
        free(voices[1].notes);
        return COUNTERPOINT_CHECK_ERR_ALLOC;
    }
    for (size_t i = 0; i < notes->count; i++) {
        VoiceNotes *voice = &voices[notes->streams[i] == first ? 0 : 1];
        voice->notes[voice->count++] = i;
    }
    sort_voice(notes, &voices[0]);
    sort_voice(notes, &voices[1]);
    /* the higher mean pitch is the upper voice: sum_a / count_a against sum_b / count_b */
    bool first_is_upper = pitch_sum(notes, &voices[0]) * (long)voices[1].count >=
                          pitch_sum(notes, &voices[1]) * (long)voices[0].count;
    out->upper = voices[first_is_upper ? 0 : 1];
    out->lower = voices[first_is_upper ? 1 : 0];
    out->uncertainty = 0;
    return COUNTERPOINT_CHECK_OK;
}

/* The kernel's separation of a single stream, which must find two voices and no chord notes. */
static CounterpointCheckResult voices_from_separation(const ImportedNotes *notes, TwoVoices *out) {
    VoiceAssignment assignment = {0};
    VoiceSeparationConfig config = voice_separation_config_default();
    VoiceSeparationResult separated =
        voice_separate(notes->events, notes->pitch_values, notes->count, NULL, notes->meter.meter_map,
                       standard_meter_segmentation_beat_grid, NULL, &config, &assignment);
    if (separated != VOICE_SEPARATION_OK) {
        return separated == VOICE_SEPARATION_ERR_ALLOC ? COUNTERPOINT_CHECK_ERR_ALLOC
                                                       : COUNTERPOINT_CHECK_NOT_TWO_VOICES;
    }
    size_t voice_count = 0;
    double least_confidence = 1;
    bool chord_notes = false;
    for (size_t i = 0; i < assignment.count; i++) {
        const VoiceAssignmentEntry *entry = &assignment.entries[i];
        voice_count = entry->voice + 1 > voice_count ? entry->voice + 1 : voice_count;
        chord_notes = chord_notes || entry->chord_member;
        least_confidence = entry->confidence < least_confidence ? entry->confidence : least_confidence;
    }
    CounterpointCheckResult result = COUNTERPOINT_CHECK_NOT_TWO_VOICES;
    VoiceNotes upper = {malloc(notes->count * sizeof(size_t)), 0};
    VoiceNotes lower = {malloc(notes->count * sizeof(size_t)), 0};
    if (!upper.notes || !lower.notes) {
        result = COUNTERPOINT_CHECK_ERR_ALLOC;
    } else if (voice_count == 2 && !chord_notes) {
        for (size_t i = 0; i < assignment.count; i++) {
            VoiceNotes *voice = assignment.entries[i].voice == 0 ? &upper : &lower;
            voice->notes[voice->count++] = i;
        }
        sort_voice(notes, &upper);
        sort_voice(notes, &lower);
        out->upper = upper;
        out->lower = lower;
        out->uncertainty = 1 - (least_confidence < 0 ? 0 : (least_confidence > 1 ? 1 : least_confidence));
        upper.notes = lower.notes = NULL;
        result = COUNTERPOINT_CHECK_OK;
    }
    free(upper.notes);
    free(lower.notes);
    voice_assignment_destroy(&assignment);
    return result;
}

/* 1 or 2 for that many streams, 3 for more. */
static int stream_count(const ImportedNotes *notes) {
    if (!notes->streams) {
        return 1;
    }
    size_t first = notes->streams[0];
    size_t second = first;
    int count = 1;
    for (size_t i = 0; i < notes->count; i++) {
        size_t stream = notes->streams[i];
        if (stream == first || (count == 2 && stream == second)) {
            continue;
        }
        if (count == 2) {
            return 3;
        }
        second = stream;
        count = 2;
    }
    return count;
}

CounterpointCheckResult counterpoint_check_collect(const ImportedNotes *notes, int species, bool cantus_upper,
                                                   AnalysisReport *report) {
    if (notes->count < 2) {
        return COUNTERPOINT_CHECK_NOT_TWO_VOICES;
    }
    int streams = stream_count(notes);
    if (streams > 2) {
        return COUNTERPOINT_CHECK_NOT_TWO_VOICES;
    }
    TwoVoices voices = {0};
    CounterpointCheckResult result =
        streams == 2 ? voices_from_streams(notes, &voices) : voices_from_separation(notes, &voices);
    if (result == COUNTERPOINT_CHECK_OK) {
        report->counterpoint_check = (AnalysisCounterpointCheck){
            .present = true, .species = species, .cantus_upper = cantus_upper, .uncertainty = voices.uncertainty};
    }
    two_voices_destroy(&voices);
    return result;
}
