#include <ursatz_composer/seam_hear.h>

#include <string.h>

#include <standard-counterpoint/harmonic_rules.h>
#include <standard-counterpoint/melodic_rules.h>
#include <standard-harmony/harmonic_function.h>
#include <standard-harmony/harmony_phrase_rules.h>
#include <standard-harmony/harmony_resolution_rules.h>

#include <ursatz_composer/constraint.h>
#include <ursatz_composer/harmony_phrase.h>

#include "harmony/harmony_internal.h"
#include "util/util.h"

/* The leap, in semitones, that a step the other way must follow. */
#define SEAM_LEAP 7

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

/*
 * The chord of a numeral in the mode. A numeral that does not parse (an applied chord) stands for the representative
 * chord of the function the plan gives it: I, IV or V.
 */
static bool chord_of(KeyStructureMode mode, bool has_given, HarmonicFunction given, const char *roman,
                     HarmonyChord *out) {
    if (harmony_roman_parse(mode, roman, out)) {
        return true;
    }
    if (!has_given) {
        return false;
    }
    memset(out, 0, sizeof(*out));
    switch (given) {
    case HARMONIC_FUNCTION_TONIC:
        out->degree = 1;
        return true;
    case HARMONIC_FUNCTION_PREDOMINANT:
        out->degree = 4;
        return true;
    case HARMONIC_FUNCTION_DOMINANT:
        out->degree = 5;
        return true;
    default:
        return false;
    }
}

static bool seam_chords(const SeamNotes *notes, HarmonyChord chords[2]) {
    return notes->has_chords &&
           chord_of(notes->key.mode, notes->before_has_function, notes->before_function, notes->before_roman,
                    &chords[0]) &&
           chord_of(notes->key.mode, notes->after_has_function, notes->after_function, notes->after_roman, &chords[1]);
}

/* The progression keeps the phrase model's order (standard-harmony): no predominant after a dominant, no tonic after a predominant. */
static bool progression_holds(const SeamNotes *notes) {
    HarmonyChord chords[2];
    if (!seam_chords(notes, chords)) {
        return false;
    }
    HarmonyPhrase phrase = {harmony_mode_of(notes->key.mode), chords, 2};
    return harmony_phrase_rule_holds(HARMONY_PHRASE_NO_BACKWARD_MOTION, &phrase);
}

static int sign(int value) {
    return value > 0 ? 1 : (value < 0 ? -1 : 0);
}

/* A pitch spelled as a degree of the key, as the counterpoint rules read intervals; NULL when it cannot be. */
static PitchSpelling *spell(const AnalysisKey *key, int pitch) {
    int degree;
    int alteration;
    if (!util_scale_degree(analysis_key_tonic_pitch_class(key), key->mode == KEY_STRUCTURE_MODE_MINOR, pitch, &degree,
                           &alteration)) {
        return NULL;
    }
    return harmony_spell_pitch(key, degree, pitch);
}

/* No parallel fifths or octaves between bass and melody across the seam (standard-counterpoint). */
static bool no_parallels_hold(const SeamNotes *notes) {
    if (notes->bass_before < 0 || notes->bass_after < 0 || notes->melody_before < 0 || notes->melody_after < 0) {
        return true;
    }
    const int pitches[4] = {notes->melody_before, notes->melody_after, notes->bass_before, notes->bass_after};
    PitchSpelling *spelled[4] = {NULL, NULL, NULL, NULL};
    bool ok = true;
    for (int i = 0; i < 4; i++) {
        spelled[i] = spell(&notes->key, pitches[i]);
        ok = ok && spelled[i] != NULL;
    }
    bool holds = true;
    if (ok) {
        StandardCounterpointLine upper = {(const PitchSpelling *const *)&spelled[0], 2};
        StandardCounterpointLine lower = {(const PitchSpelling *const *)&spelled[2], 2};
        StandardCounterpointPair pair = {&upper, &lower};
        holds = !standard_counterpoint_pair_valid(&pair) ||
                (standard_counterpoint_harmonic_rule_holds(STANDARD_COUNTERPOINT_HARMONIC_RULE_PARALLEL_FIFTHS, &pair) &&
                 standard_counterpoint_harmonic_rule_holds(STANDARD_COUNTERPOINT_HARMONIC_RULE_PARALLEL_OCTAVES, &pair));
    }
    for (int i = 0; i < 4; i++) {
        pitch_spelling_destroy(spelled[i]);
    }
    return holds;
}

/*
 * The move across the seam is one standard-counterpoint allows, and a leap larger than a fifth into or across it is
 * answered by a step the other way. The answer to a leap has no framework rule: it stays here.
 */
static bool step_holds(const SeamNotes *notes) {
    if (notes->melody_before < 0 || notes->melody_after < 0) {
        return true;
    }
    int move = notes->melody_after - notes->melody_before;
    if (!standard_counterpoint_melodic_semitones_allowed(move)) {
        return false;
    }
    if (notes->melody_previous >= 0) {
        int into = notes->melody_before - notes->melody_previous;
        if (util_absolute(into) > SEAM_LEAP &&
            !(standard_counterpoint_is_step(move) && sign(move) == -sign(into))) {
            return false;
        }
    }
    if (util_absolute(move) > SEAM_LEAP && notes->melody_next >= 0) {
        int out = notes->melody_next - notes->melody_after;
        if (!(standard_counterpoint_is_step(out) && sign(out) == -sign(move))) {
            return false;
        }
    }
    return true;
}

/* The diatonic step of a pitch from the key's tonic: 0 is do, 7 the octave above. */
static int diatonic_step(const AnalysisKey *key, int pitch) {
    int tonic = analysis_key_tonic_pitch_class(key);
    int relative = pitch - tonic;
    int octave = relative >= 0 ? relative / 12 : -((-relative + 11) / 12);
    int degree = 1;
    int alteration;
    util_scale_degree(tonic, key->mode == KEY_STRUCTURE_MODE_MINOR, pitch, &degree, &alteration);
    return octave * 7 + degree - 1;
}

/* The leading tone of the chord before the seam goes up to the tonic when the chord after is one (standard-harmony). */
static bool leading_tone_holds(const SeamNotes *notes) {
    HarmonyChord chords[2];
    if (notes->melody_before < 0 || notes->melody_after < 0 || !seam_chords(notes, chords)) {
        return true;
    }
    HarmonyVoiceResolution resolution;
    memset(&resolution, 0, sizeof(resolution));
    resolution.mode = harmony_mode_of(notes->key.mode);
    resolution.from = chords[0];
    resolution.to = chords[1];
    resolution.from_step = diatonic_step(&notes->key, notes->melody_before);
    resolution.to_step = diatonic_step(&notes->key, notes->melody_after);
    resolution.outer = true;
    return !harmony_resolution_rule_applies(HARMONY_RESOLUTION_LEADING_TONE_RESOLVES, &resolution) ||
           harmony_resolution_rule_holds(HARMONY_RESOLUTION_LEADING_TONE_RESOLVES, &resolution);
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
        notes.bass_before = bass->notes[bass->note_count - 1].midi;
    }
    id = span_at(plan, PLAN_LANE_BASS, 0, start);
    if (id != PLAN_LANE_SPAN_NONE && plan_lane_span_payload(plan, id)->notes.note_count > 0) {
        notes.bass_after = plan_lane_span_payload(plan, id)->notes.notes[0].midi;
    }
    id = span_at(plan, PLAN_LANE_MELODY, 0, before);
    if (id != PLAN_LANE_SPAN_NONE && plan_lane_span_payload(plan, id)->notes.note_count > 0) {
        const PlanNotes *melody = &plan_lane_span_payload(plan, id)->notes;
        notes.melody_before = melody->notes[melody->note_count - 1].midi;
        notes.melody_previous = melody->note_count > 1 ? melody->notes[melody->note_count - 2].midi : -1;
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
        notes.melody_after = melody->notes[0].midi;
        notes.melody_next = melody->note_count > 1 ? melody->notes[1].midi : -1;
    }
    seam_hear_notes(&notes, out);
    return SEAM_HEAR_OK;
}
