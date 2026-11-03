#ifndef URSATZ_COMPOSER_TESTS_ACCOMPANIMENT_CHECK_H
#define URSATZ_COMPOSER_TESTS_ACCOMPANIMENT_CHECK_H

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>

#include <ursatz_composer/lanes.h>
#include <ursatz_composer/plan_state.h>

#include "harmony/harmony_internal.h"
#include "melody/melody_internal.h"

/*
 * What a written accompaniment is held to, worked out here from the plan's lanes and not from the operator: every
 * note a chord tone of the harmony sounding at its start (the chord read with harmony_read_lane and its pitch classes
 * with melody_chord_mask, the code the melody checker uses), inside the range, above every bass note sounding while it
 * does and below every melody note sounding while it does.
 */

#define CHECK_TICKS 128L
#define CHECK_MAX_NOTES 512

typedef struct {
    long onset;
    long end;
    int pitch;
} CheckNote;

static inline long check_ticks(Rational time) {
    assert((time.num * CHECK_TICKS) % (long)time.den == 0);
    return time.num * CHECK_TICKS / (long)time.den;
}

/* The notes of a lane voice over span, in ticks of the plan, in time order of their spans. */
static inline size_t check_notes(const PiecePlan *plan, PlanLaneKind lane, int voice, PlanSpan span, CheckNote *out) {
    LaneSpanId ids[256];
    size_t found = plan_lane_find(plan, lane, voice, span, ids, 256);
    size_t count = 0;
    assert(found <= 256);
    for (size_t i = 0; i < found; i++) {
        long base = check_ticks(plan_lane_span_span(plan, ids[i]).start);
        const PlanNotes *notes = &plan_lane_span_payload(plan, ids[i])->notes;
        assert(plan_lane_span_state(plan, ids[i]) != PLAN_STATE_CONSTRAINED);
        for (size_t n = 0; n < notes->note_count; n++) {
            assert(count < CHECK_MAX_NOTES);
            out[count].onset = base + check_ticks(notes->notes[n].offset);
            out[count].end = out[count].onset + check_ticks(notes->notes[n].duration);
            out[count].pitch = notes->notes[n].midi_pitch;
            count++;
        }
    }
    return count;
}

/* Checks every inner voice over span against the rules above; returns how many notes it checked. */
static inline size_t check_accompaniment(const PiecePlan *plan, PlanSpan span, PlanKey key, int low, int high) {
    PhraseChord chords[HARMONY_MAX_CHORDS];
    PhraseMetre metre;
    LaneSpanId chord_ids[HARMONY_MAX_CHORDS];
    unsigned masks[HARMONY_MAX_CHORDS];
    long starts[HARMONY_MAX_CHORDS + 1];
    size_t chord_count;
    CheckNote bass[CHECK_MAX_NOTES];
    CheckNote melody[CHECK_MAX_NOTES];
    CheckNote voice[CHECK_MAX_NOTES];
    size_t checked = 0;

    assert(harmony_read_lane(plan, span, key.mode, chords, &chord_count, &metre));
    assert(plan_lane_find(plan, PLAN_LANE_HARMONY, 0, span, chord_ids, HARMONY_MAX_CHORDS) == chord_count);
    for (size_t i = 0; i < chord_count; i++) {
        unsigned seventh;
        assert(melody_chord_mask(&key, &chords[i], &masks[i], &seventh));
        starts[i] = check_ticks(plan_lane_span_span(plan, chord_ids[i]).start);
    }
    starts[chord_count] = check_ticks(span.start) + check_ticks(span.length);
    size_t bass_count = check_notes(plan, PLAN_LANE_BASS, 0, span, bass);
    size_t melody_count = check_notes(plan, PLAN_LANE_MELODY, 0, span, melody);
    assert(bass_count > 0 && melody_count > 0);

    for (int v = 1; v <= 3; v++) {
        size_t voice_count = check_notes(plan, PLAN_LANE_INNER_VOICE, v, span, voice);
        for (size_t n = 0; n < voice_count; n++) {
            size_t chord = 0;
            for (size_t c = 0; c < chord_count; c++) {
                chord = voice[n].onset >= starts[c] ? c : chord;
            }
            assert(masks[chord] & (1u << (voice[n].pitch % 12)));
            assert(voice[n].pitch >= low && voice[n].pitch <= high);
            for (size_t b = 0; b < bass_count; b++) {
                if (bass[b].onset < voice[n].end && bass[b].end > voice[n].onset) {
                    assert(voice[n].pitch > bass[b].pitch);
                }
            }
            for (size_t m = 0; m < melody_count; m++) {
                if (melody[m].onset < voice[n].end && melody[m].end > voice[n].onset) {
                    assert(voice[n].pitch < melody[m].pitch);
                }
            }
            checked++;
        }
    }
    return checked;
}

#endif
