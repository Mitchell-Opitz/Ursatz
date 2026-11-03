#include <ursatz_composer/seam_hear.h>

#include <string.h>

#include <standard-harmony/harmonic_function.h>
#include <standard-harmony/harmony_functions.h>

#include <ursatz_composer/constraint.h>
#include <ursatz_composer/harmony_phrase.h>

#include "harmony/harmony_internal.h"
#include "util/util.h"

/* The largest melodic move across the seam, in semitones, and the leap a step the other way must follow. */
#define SEAM_MAX_MOVE 12
#define SEAM_LEAP 7
#define SEAM_STEP 2

NodeId seam_section_at(const PiecePlan *plan, Rational time) {
    for (NodeId id = 1; id <= plan_node_count(plan); id++) {
        if (plan_has_node(plan, id) && plan_node_kind(plan, id) == PLAN_NODE_SECTION &&
            rational_cmp(plan_node_span(plan, id).start, time) == 0 &&
            plan_node_has_constraint(plan, id, PLAN_CONSTRAINT_SEAM)) {
            return id;
        }
    }
    return PLAN_NODE_NONE;
}

/* The function of a chord: the one given, else the numeral's in the mode. */
static bool function_of(PlanKeyMode mode, bool has_given, HarmonicFunction given, const char *roman,
                        HarmonicFunction *out) {
    HarmonyChord chord;
    if (has_given) {
        *out = given;
        return true;
    }
    return harmony_roman_parse(mode, roman, &chord) && harmony_function_of(harmony_mode_of(mode), &chord, out);
}

static bool progression_holds(const SeamNotes *notes) {
    if (!notes->has_chords) {
        return false;
    }
    HarmonicFunction before;
    HarmonicFunction after;
    if (!function_of(notes->key.mode, notes->before_has_function, notes->before_function, notes->before_roman, &before) ||
        !function_of(notes->key.mode, notes->after_has_function, notes->after_function, notes->after_roman, &after)) {
        return false;
    }
    switch (before) {
    case HARMONIC_FUNCTION_TONIC:
        /* A tonic goes to a predominant, a dominant or a tonic (a repeated tonic included). */
        return after == HARMONIC_FUNCTION_PREDOMINANT || after == HARMONIC_FUNCTION_DOMINANT ||
               after == HARMONIC_FUNCTION_TONIC;
    case HARMONIC_FUNCTION_PREDOMINANT:
        return after == HARMONIC_FUNCTION_DOMINANT;
    case HARMONIC_FUNCTION_DOMINANT:
        return after == HARMONIC_FUNCTION_TONIC;
    default:
        break;
    }
    return false;
}

static int sign(int value) {
    return value > 0 ? 1 : (value < 0 ? -1 : 0);
}

/* Whether the interval class between bass and melody is a perfect one (a unison, an octave or a fifth). */
static bool perfect(int bass, int melody) {
    int interval = ((melody - bass) % 12 + 12) % 12;
    return interval == 0 || interval == 7;
}

static bool no_parallels_hold(const SeamNotes *notes) {
    if (notes->bass_before < 0 || notes->bass_after < 0 || notes->melody_before < 0 || notes->melody_after < 0) {
        return true;
    }
    int bass_move = notes->bass_after - notes->bass_before;
    int melody_move = notes->melody_after - notes->melody_before;
    int before = ((notes->melody_before - notes->bass_before) % 12 + 12) % 12;
    int after = ((notes->melody_after - notes->bass_after) % 12 + 12) % 12;
    /* Parallel: both voices move the same way and stay on the same perfect interval. */
    return !(perfect(notes->bass_before, notes->melody_before) && before == after && bass_move != 0 &&
             melody_move != 0 && sign(bass_move) == sign(melody_move));
}

static bool step_holds(const SeamNotes *notes) {
    if (notes->melody_before < 0 || notes->melody_after < 0) {
        return true;
    }
    int move = notes->melody_after - notes->melody_before;
    if (util_absolute(move) > SEAM_MAX_MOVE) {
        return false;
    }
    /* A leap into the seam is answered by a step the other way across it. */
    if (notes->melody_previous >= 0) {
        int into = notes->melody_before - notes->melody_previous;
        if (util_absolute(into) > SEAM_LEAP && !(util_absolute(move) <= SEAM_STEP && sign(move) == -sign(into))) {
            return false;
        }
    }
    /* A leap across it is answered by a step the other way after it. */
    if (util_absolute(move) > SEAM_LEAP && notes->melody_next >= 0) {
        int out = notes->melody_next - notes->melody_after;
        if (!(util_absolute(out) <= SEAM_STEP && out != 0 && sign(out) == -sign(move))) {
            return false;
        }
    }
    return true;
}

static bool leading_tone_holds(const SeamNotes *notes) {
    if (notes->melody_before < 0) {
        return true;
    }
    int tonic = notes->key.tonic_pitch_class;
    bool seventh = ((notes->melody_before % 12) + 12) % 12 == (tonic + 11) % 12;
    if (!seventh) {
        return true;
    }
    return notes->melody_after < 0 || ((notes->melody_after % 12) + 12) % 12 == tonic;
}

void seam_hear_notes(const SeamNotes *notes, SeamHeard *out) {
    memset(out, 0, sizeof(*out));
    out->progression = progression_holds(notes);
    out->no_parallels = no_parallels_hold(notes);
    out->step = step_holds(notes);
    out->leading_tone = leading_tone_holds(notes);
    out->held = (size_t)out->progression + (size_t)out->no_parallels + (size_t)out->step + (size_t)out->leading_tone;
}

/* ------------------------------------------------------------- the plan */

/* A time one tick (1/4096 of a whole note) before another. */
static Rational before_tick(Rational time) {
    Rational out = {0, 1};
    if (rational_make(time.num * 4096 - (long)time.den, time.den * 4096UL, &out) != RATIONAL_OK) {
        out.num = 0;
        out.den = 1;
    }
    return out;
}

/* The lane span of a lane and voice that sounds over the tick starting at `at`, or none. */
static LaneSpanId span_at(const PiecePlan *plan, PlanLaneKind lane, int voice, Rational at) {
    LaneSpanId ids[8];
    Rational tick = {1, 4096};
    PlanSpan range = {at, tick};
    size_t found = plan_lane_find(plan, lane, voice, range, ids, 8);
    for (size_t i = 0; i < found && i < 8; i++) {
        if (plan_lane_span_state(plan, ids[i]) != PLAN_STATE_CONSTRAINED) {
            return ids[i];
        }
    }
    return PLAN_LANE_SPAN_NONE;
}

static bool copy_roman(const PiecePlan *plan, LaneSpanId id, char *roman, HarmonicFunction *function,
                       bool *has_function) {
    if (id == PLAN_LANE_SPAN_NONE) {
        return false;
    }
    const PlanHarmony *harmony = &plan_lane_span_payload(plan, id)->harmony;
    memcpy(roman, harmony->roman_numeral, PLAN_ROMAN_NUMERAL_MAX);
    *function = harmony->function;
    *has_function = harmony->has_function;
    return true;
}

SeamHearResult seam_hear(const PiecePlan *plan, NodeId seam, SeamHeard *out) {
    SeamNotes notes;
    if (!plan || !out) {
        return SEAM_HEAR_ERR_NULL;
    }
    if (!plan_has_node(plan, seam) || !plan_node_has_constraint(plan, seam, PLAN_CONSTRAINT_SEAM)) {
        return SEAM_HEAR_ERR_NOT_SEAM;
    }
    Rational start = plan_node_span(plan, seam).start;
    Rational before = before_tick(start);
    LaneSpanId keys[2];
    memset(&notes, 0, sizeof(notes));
    notes.bass_before = notes.bass_after = notes.melody_previous = notes.melody_before = notes.melody_after =
        notes.melody_next = -1;
    PlanSpan key_range = {start, {1, 4096}};
    if (plan_lane_find(plan, PLAN_LANE_KEY, 0, key_range, keys, 2) < 1) {
        return SEAM_HEAR_ERR_NOT_WRITTEN;
    }
    notes.key = plan_lane_span_payload(plan, keys[0])->key;
    notes.has_chords =
        copy_roman(plan, span_at(plan, PLAN_LANE_HARMONY, 0, before), notes.before_roman, &notes.before_function,
                   &notes.before_has_function) &&
        copy_roman(plan, span_at(plan, PLAN_LANE_HARMONY, 0, start), notes.after_roman, &notes.after_function,
                   &notes.after_has_function);
    if (!notes.has_chords) {
        return SEAM_HEAR_ERR_NOT_WRITTEN;
    }
    LaneSpanId id = span_at(plan, PLAN_LANE_BASS, 0, before);
    if (id != PLAN_LANE_SPAN_NONE && plan_lane_span_payload(plan, id)->notes.note_count > 0) {
        const PlanNotes *bass = &plan_lane_span_payload(plan, id)->notes;
        notes.bass_before = bass->notes[bass->note_count - 1].midi_pitch;
    }
    id = span_at(plan, PLAN_LANE_BASS, 0, start);
    if (id != PLAN_LANE_SPAN_NONE && plan_lane_span_payload(plan, id)->notes.note_count > 0) {
        notes.bass_after = plan_lane_span_payload(plan, id)->notes.notes[0].midi_pitch;
    }
    id = span_at(plan, PLAN_LANE_MELODY, 0, before);
    if (id != PLAN_LANE_SPAN_NONE && plan_lane_span_payload(plan, id)->notes.note_count > 0) {
        const PlanNotes *melody = &plan_lane_span_payload(plan, id)->notes;
        notes.melody_before = melody->notes[melody->note_count - 1].midi_pitch;
        notes.melody_previous = melody->note_count > 1 ? melody->notes[melody->note_count - 2].midi_pitch : -1;
    }
    id = span_at(plan, PLAN_LANE_MELODY, 0, start);
    if (id == PLAN_LANE_SPAN_NONE) {
        /* A melody that opens with a rest starts after the first tick. */
        LaneSpanId ids[8];
        PlanSpan seam_span = plan_node_span(plan, seam);
        size_t found = plan_lane_find(plan, PLAN_LANE_MELODY, 0, seam_span, ids, 8);
        id = found > 0 ? ids[0] : PLAN_LANE_SPAN_NONE;
    }
    if (id != PLAN_LANE_SPAN_NONE && plan_lane_span_payload(plan, id)->notes.note_count > 0) {
        const PlanNotes *melody = &plan_lane_span_payload(plan, id)->notes;
        notes.melody_after = melody->notes[0].midi_pitch;
        notes.melody_next = melody->note_count > 1 ? melody->notes[1].midi_pitch : -1;
    }
    seam_hear_notes(&notes, out);
    return SEAM_HEAR_OK;
}
