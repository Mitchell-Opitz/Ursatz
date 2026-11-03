#include <ursatz_composer/voicing.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <standard-voicing/range_rules.h>
#include <standard-voicing/voicing_pitch.h>
#include <semantics/voice_part.h>

#include <ursatz_composer/plan_state.h>

#include "texture/voicing_internal.h"

/* The pitches a spelling table covers. */
#define TABLE_LOW 24
#define TABLE_HIGH 108

VoicingParams voicing_params_default(void) {
    VoicingParams params = {VOICING_DEFAULT_CANDIDATES, VOICING_DEFAULT_BEAM};
    return params;
}

/* ------------------------------------------------------------ reading */

void voicing_context_free(VoicingContext *context) {
    if (!context) {
        return;
    }
    for (size_t i = 0; i < VOICING_MAX_CHORDS; i++) {
        for (size_t k = 0; k < 4; k++) {
            pitch_spelling_destroy(context->tone[i][k]);
        }
        for (size_t p = 0; p < 128; p++) {
            pitch_spelling_destroy(context->spell[i][p]);
        }
    }
    pitch_spelling_destroy(context->key_leading);
    pitch_spelling_destroy(context->key_tonic);
    memset(context, 0, sizeof(*context));
}

/* The melody over the span when it is to be written: one open span over the same time as the target. */
static bool read_melody_span(const PiecePlan *plan, PlanSpan span, LaneSpanId *out) {
    LaneSpanId ids[2];
    size_t found = plan_lane_find(plan, PLAN_LANE_MELODY, 0, span, ids, 2);
    if (found != 1 || plan_lane_span_state(plan, ids[0]) != PLAN_STATE_CONSTRAINED) {
        return false;
    }
    PlanSpan other = plan_lane_span_span(plan, ids[0]);
    if (rational_cmp(other.start, span.start) != 0 || rational_cmp(other.length, span.length) != 0) {
        return false;
    }
    *out = ids[0];
    return true;
}

/* The pitch of the line sounding at a position, or -1 for none. */
static int line_pitch_at(const AccompLine *lines, size_t count, Rational position) {
    for (size_t i = 0; i < count; i++) {
        if (rational_cmp(lines[i].onset, position) <= 0 && rational_cmp(position, lines[i].end) < 0) {
            return lines[i].pitch;
        }
    }
    return -1;
}

static bool read_chords(const PiecePlan *plan, VoicingContext *out, Rational *cell_length) {
    PhraseChord chords[HARMONY_MAX_CHORDS];
    PhraseMetre metre;
    LaneSpanId harmony[HARMONY_MAX_CHORDS];
    size_t count;
    if (!harmony_read_lane(plan, out->span, out->key.mode, chords, &count, &metre) || count == 0 ||
        count > HARMONY_MAX_CHORDS ||
        plan_lane_find(plan, PLAN_LANE_HARMONY, 0, out->span, harmony, HARMONY_MAX_CHORDS) != count ||
        !position_is_positive(out->span.length) || !position_is_positive(metre.cell_length)) {
        return false;
    }
    out->length = out->span.length;
    Rational at = POSITION_ZERO;
    size_t cells = 0;
    for (size_t i = 0; i < count; i++) {
        Rational chord_from;
        Rational chord_length;
        if (plan_lane_span_state(plan, harmony[i]) == PLAN_STATE_CONSTRAINED ||
            !harmony_cells_of_span(&metre, cells, chords[i].cells, &chord_from, &chord_length) ||
            !position_is_positive(chord_length)) {
            return false;
        }
        out->chord[i] = chords[i];
        out->chord_start[i] = at;
        at = position_add(at, chord_length);
        cells += chords[i].cells;
    }
    out->chord_count = count;
    out->chord_start[count] = at;
    *cell_length = metre.cell_length;
    return rational_cmp(at, out->length) == 0;
}

/* The tones of chord i, their spellings, and the leading tone to tell standard-voicing of. */
static bool read_tones(VoicingContext *out, size_t i) {
    const PhraseChord *chord = &out->chord[i];
    int root_degree;
    if (!accomp_chord_tones(&out->key, chord, out->pcs[i], &out->tone_count[i])) {
        return false;
    }
    root_degree = chord->applied ? harmony_applied_root_degree(harmony_mode_of(out->key.mode), &chord->applied_chord)
                                 : chord->chord.degree;
    if (root_degree < 1 || root_degree > 7) {
        return false;
    }
    for (size_t k = 0; k < out->tone_count[i]; k++) {
        out->tone_degree[i][k] = (root_degree - 1 + 2 * (int)k) % 7 + 1;
        out->tone[i][k] = harmony_spell_pitch(&out->key, out->tone_degree[i][k], 60 + out->pcs[i][k]);
        if (!out->tone[i][k]) {
            return false;
        }
    }
    for (int pitch = TABLE_LOW; pitch <= TABLE_HIGH; pitch++) {
        for (size_t k = 0; k < out->tone_count[i]; k++) {
            if (pitch % 12 == out->pcs[i][k]) {
                out->spell[i][pitch] = harmony_spell_pitch(&out->key, out->tone_degree[i][k], pitch);
                if (!out->spell[i][pitch]) {
                    return false;
                }
            }
        }
    }
    out->leading[i] = out->key_leading;
    if (chord->applied) {
        out->leading[i] = chord->applied_chord.kind == APPLIED_CHORD_KIND_SECONDARY_LEADING_TONE ? out->tone[i][0]
                                                                                                    : out->tone[i][1];
    }
    return true;
}

/*
 * The chord before the target, when it is voiced: the harmony span that ends where the target starts, written, with a
 * note in each of the four voices at its start. Then it becomes chord 0 of the context and the target's chords follow.
 * True with the context as it was when there is none.
 */
static bool read_lead_in(const PiecePlan *plan, VoicingContext *out, Rational cell_length) {
    static const PlanLaneKind LANE[4] = {PLAN_LANE_MELODY, PLAN_LANE_INNER_VOICE, PLAN_LANE_INNER_VOICE, PLAN_LANE_BASS};
    static const int VOICE[4] = {0, 1, 2, 0};
    PlanSpan range = {{0, 1}, out->span.start};
    LaneSpanId *ids;
    LaneSpanId before;
    PlanSpan before_span;
    Rational before_end;
    PhraseChord chord;
    PlanKey key;
    int pitch[4];
    Rational origin;
    Rational length;
    size_t found;
    if (out->span.start.num <= 0 || out->chord_count >= VOICING_MAX_CHORDS) {
        return true;
    }
    found = plan_lane_find(plan, PLAN_LANE_HARMONY, 0, range, NULL, 0);
    ids = found ? malloc(found * sizeof(*ids)) : NULL;
    if (!ids) {
        return found == 0;
    }
    plan_lane_find(plan, PLAN_LANE_HARMONY, 0, range, ids, found);
    before = ids[found - 1];
    free(ids);
    before_span = plan_lane_span_span(plan, before);
    if (plan_span_end(before_span, &before_end) != PLAN_OK || rational_cmp(before_end, out->span.start) != 0 ||
        plan_lane_span_state(plan, before) == PLAN_STATE_CONSTRAINED ||
        !harmony_read_chord(plan, before, out->key.mode, cell_length, &chord) ||
        (harmony_find_key(plan, PLAN_LANE_SPAN_NONE, PLAN_NODE_NONE, before_span, &key) &&
         (key.tonic_pitch_class != out->key.tonic_pitch_class || key.mode != out->key.mode)) ||
        !position_is_positive(before_span.length)) {
        return true;
    }
    origin = before_span.start;
    length = before_span.length;
    for (size_t voice = 0; voice < 4; voice++) {
        AccompLine lines[ACCOMP_MAX_LINE];
        size_t count;
        if (!accomp_read_line(plan, LANE[voice], VOICE[voice], before_span, origin, length, lines, &count) ||
            (pitch[voice] = line_pitch_at(lines, count, POSITION_ZERO)) < TABLE_LOW || pitch[voice] > TABLE_HIGH) {
            return true;
        }
    }
    /* Chord 0 is the lead-in; the target's chords move up one. */
    memmove(&out->chord[1], &out->chord[0], out->chord_count * sizeof(out->chord[0]));
    memmove(&out->chord_start[1], &out->chord_start[0], (out->chord_count + 1) * sizeof(out->chord_start[0]));
    out->chord[0] = chord;
    out->chord_start[0] = position_sub(POSITION_ZERO, length);
    out->chord_count++;
    out->first = 1;
    memcpy(out->lead, pitch, sizeof(out->lead));
    out->bass[0] = pitch[VOICING_BASS];
    return true;
}

/*
 * A soprano the voicing writes ends the cadence the melody span asks for, as the recognizer hears it: a perfect authentic
 * cadence on the tonic, an imperfect one on the third of the tonic chord.
 */
static void limit_cadence_soprano(const PiecePlan *plan, VoicingContext *out) {
    size_t last = out->chord_count - 1;
    for (size_t i = 0; i < out->chord_count; i++) {
        out->soprano_mask[i] = 0xFFFu;
    }
    for (size_t i = 0; i < plan_lane_span_constraint_count(plan, out->melody_span); i++) {
        const PlanConstraint *constraint = plan_lane_span_constraint_at(plan, out->melody_span, i);
        if (constraint->kind != PLAN_CONSTRAINT_CADENCE) {
            continue;
        }
        if (constraint->params.cadence == CADENCE_TYPE_PERFECT_AUTHENTIC) {
            out->soprano_mask[last] = 1u << out->key.tonic_pitch_class;
        } else if (constraint->params.cadence == CADENCE_TYPE_IMPERFECT_AUTHENTIC) {
            out->soprano_mask[last] = 1u << ((out->key.tonic_pitch_class + (out->key.mode == PLAN_MODE_MINOR ? 3 : 4)) % 12);
        }
    }
}

bool voicing_read(const PiecePlan *plan, LaneSpanId target, VoicingContext *out) {
    PlanConstraint constraint;
    AccompLine melody[ACCOMP_MAX_LINE];
    AccompLine bass[ACCOMP_MAX_LINE];
    size_t melody_count = 0;
    size_t bass_count = 0;
    Rational cell_length;
    Rational origin;
    if (!plan || !out || !plan_has_lane_span(plan, target) || plan_lane_span_lane(plan, target) != PLAN_LANE_INNER_VOICE ||
        plan_lane_span_voice(plan, target) != 1 || plan_lane_span_state(plan, target) != PLAN_STATE_CONSTRAINED) {
        return false;
    }
    memset(out, 0, sizeof(*out));
    out->length = POSITION_ZERO;
    for (size_t i = 0; i <= VOICING_MAX_CHORDS; i++) {
        out->chord_start[i] = POSITION_ZERO;
    }
    out->target = target;
    out->owner = plan_lane_span_owner(plan, target);
    out->span = plan_lane_span_span(plan, target);
    if (!harmony_find_constraint(plan, target, out->owner, PLAN_CONSTRAINT_VOICING, &constraint) ||
        !harmony_find_key(plan, target, out->owner, out->span, &out->key)) {
        return false;
    }
    out->write_soprano = constraint.params.voicing.write_soprano;
    origin = out->span.start;
    if (!read_chords(plan, out, &cell_length) || !read_lead_in(plan, out, cell_length) ||
        !accomp_read_line(plan, PLAN_LANE_BASS, 0, out->span, origin, out->length, bass, &bass_count) ||
        !accomp_read_voice(plan, 2, out->span, &out->tenor_span)) {
        return false;
    }
    if (out->write_soprano ? !read_melody_span(plan, out->span, &out->melody_span)
                           : !accomp_read_line(plan, PLAN_LANE_MELODY, 0, out->span, origin, out->length, melody,
                                               &melody_count)) {
        return false;
    }
    if (out->write_soprano) {
        limit_cadence_soprano(plan, out);
    }
    out->key_tonic = harmony_spell_tonic(&out->key, 4);
    out->key_leading = harmony_spell_pitch(&out->key, 7, 60 + (out->key.tonic_pitch_class + 11) % 12);
    if (!out->key_tonic || !out->key_leading) {
        return false;
    }
    for (size_t i = 0; i < out->chord_count; i++) {
        int bass_pc;
        Rational start = out->chord_start[i];
        Rational end = out->chord_start[i + 1];
        if (!read_tones(out, i)) {
            return false;
        }
        if (i < out->first) {
            /* The chord before the target: voiced already, with the bass read with the lead-in. */
            out->inversion[i] = -1;
            for (size_t k = 0; k < out->tone_count[i]; k++) {
                out->inversion[i] = out->pcs[i][k] == out->bass[i] % 12 ? (int)k : out->inversion[i];
            }
            out->soprano[i] = out->lead[VOICING_SOPRANO];
            out->chord_ok[i] = out->inversion[i] >= 0 && out->spell[i][out->bass[i]] &&
                               out->spell[i][out->lead[VOICING_SOPRANO]] && out->spell[i][out->lead[VOICING_ALTO]] &&
                               out->spell[i][out->lead[VOICING_TENOR]];
            continue;
        }
        out->bass[i] = line_pitch_at(bass, bass_count, start);
        out->inversion[i] = -1;
        out->soprano[i] = -1;
        out->chord_ok[i] = out->bass[i] >= TABLE_LOW && out->bass[i] <= TABLE_HIGH;
        bass_pc = out->bass[i] % 12;
        for (size_t k = 0; out->chord_ok[i] && k < out->tone_count[i]; k++) {
            if (out->pcs[i][k] == bass_pc) {
                out->inversion[i] = (int)k;
            }
        }
        out->chord_ok[i] = out->chord_ok[i] && out->inversion[i] >= 0;
        if (out->write_soprano) {
            continue;
        }
        /* The melody sounding at the start of the chord, else the first chord tone that sounds in it. */
        for (size_t pass = 0; pass < 2 && out->soprano[i] < 0; pass++) {
            for (size_t n = 0; n < melody_count && out->soprano[i] < 0; n++) {
                const AccompLine *line = &melody[n];
                bool sounds = pass == 0 ? rational_cmp(line->onset, start) <= 0 && rational_cmp(start, line->end) < 0
                                        : rational_cmp(line->onset, start) > 0 && rational_cmp(line->onset, end) < 0;
                if (sounds && line->pitch >= TABLE_LOW && line->pitch <= TABLE_HIGH && out->spell[i][line->pitch]) {
                    out->soprano[i] = line->pitch;
                }
            }
        }
        out->chord_ok[i] = out->chord_ok[i] && out->soprano[i] >= 0;
    }
    return true;
}

/* ------------------------------------------------------- enumeration */

bool voicing_options(const VoicingContext *context, size_t chord, VoicingOption *out, size_t max, size_t *count) {
    int range[VOICING_PARTS][2];
    VoicePart parts[VOICING_PARTS] = {VOICE_PART_SOPRANO, VOICE_PART_ALTO, VOICE_PART_TENOR};
    size_t found = 0;
    *count = 0;
    if (chord >= context->chord_count || !context->chord_ok[chord]) {
        return false;
    }
    if (chord < context->first) {
        /* The chord before the target has the voicing it was written with, and no other. */
        if (max < 1) {
            return false;
        }
        memcpy(out[0].pitch, context->lead, sizeof(out[0].pitch));
        *count = 1;
        return true;
    }
    for (size_t part = 0; part < VOICING_PARTS; part++) {
        if (!standard_voicing_range_semitones(parts[part], &range[part][0], &range[part][1])) {
            return false;
        }
        /* standard-voicing counts semitones from C0 (C4 is 48), MIDI from C-1 (C4 is 60). */
        range[part][0] += STANDARD_VOICING_SEMITONES_PER_OCTAVE;
        range[part][1] += STANDARD_VOICING_SEMITONES_PER_OCTAVE;
    }
    if (!context->write_soprano) {
        range[VOICING_SOPRANO][0] = range[VOICING_SOPRANO][1] = context->soprano[chord];
    }
    for (int s = range[VOICING_SOPRANO][0]; s <= range[VOICING_SOPRANO][1]; s++) {
        if (!context->spell[chord][s]) {
            continue;
        }
        for (int a = range[VOICING_ALTO][0]; a <= range[VOICING_ALTO][1] && a <= s; a++) {
            if (!context->spell[chord][a]) {
                continue;
            }
            for (int t = range[VOICING_TENOR][0]; t <= range[VOICING_TENOR][1] && t <= a; t++) {
                int pitch[VOICING_PARTS] = {s, a, t};
                if (!context->spell[chord][t] || !voicing_chord_valid(context, chord, pitch)) {
                    continue;
                }
                if (found == max) {
                    return false;
                }
                memcpy(out[found++].pitch, pitch, sizeof(pitch));
            }
        }
    }
    *count = found;
    return true;
}

/* ------------------------------------------------------- the operator */

static bool operator_applies(const PiecePlan *plan, FillTarget target, void *context) {
    VoicingContext *read;
    bool applies;
    (void)context;
    if (target.kind != FILL_TARGET_LANE_SPAN) {
        return false;
    }
    read = calloc(1, sizeof(*read));
    applies = read && voicing_read(plan, (LaneSpanId)target.id, read);
    voicing_context_free(read);
    free(read);
    return applies;
}

static void candidate_label(const VoicingCandidate *candidate, size_t first, char *label, size_t size) {
    int used = snprintf(label, size, "satb");
    for (size_t i = first; used > 0 && (size_t)used + 1 < size && i < candidate->chord_count && i < first + 3; i++) {
        used += snprintf(label + used, size - (size_t)used, " %d-%d-%d", candidate->pitch[i][VOICING_SOPRANO],
                         candidate->pitch[i][VOICING_ALTO], candidate->pitch[i][VOICING_TENOR]);
    }
}

#define MAX_CANDIDATES 16

static bool operator_generate(const PiecePlan *plan, FillTarget target, FillRng *rng, void *context,
                              FillCandidateList *out) {
    const VoicingParams *params = context;
    size_t wanted = params && params->candidates > 0 ? params->candidates : VOICING_DEFAULT_CANDIDATES;
    size_t beam = params && params->beam > 0 ? params->beam : VOICING_DEFAULT_BEAM;
    VoicingContext *read = calloc(1, sizeof(*read));
    VoicingCandidate *found[MAX_CANDIDATES];
    size_t found_count = 0;
    bool ok = read && target.kind == FILL_TARGET_LANE_SPAN && voicing_read(plan, (LaneSpanId)target.id, read);
    if (wanted > MAX_CANDIDATES) {
        wanted = MAX_CANDIDATES;
    }
    ok = ok && voicing_search(read, rng, wanted, beam, found, &found_count);
    for (size_t i = 0; i < found_count; i++) {
        char label[FILL_LABEL_MAX];
        candidate_label(found[i], read->first, label, sizeof(label));
        /* The candidate list owns the payload, also when it cannot be added. */
        ok = fill_candidates_add(out, label, found[i], free) && ok;
    }
    voicing_context_free(read);
    free(read);
    return ok;
}

/* The notes of one part as lane notes, one a chord, held for the chord. */
static PlanResult part_payload(const VoicingContext *context, const VoicingCandidate *candidate, size_t part,
                               PlanNote *notes, PlanLanePayload *payload) {
    for (size_t i = context->first; i < candidate->chord_count; i++) {
        PlanNote *note = &notes[i - context->first];
        note->midi_pitch = candidate->pitch[i][part];
        note->offset = context->chord_start[i];
        note->duration = position_sub(context->chord_start[i + 1], context->chord_start[i]);
        if (position_is_far(note->offset) || position_is_far(note->duration)) {
            return PLAN_ERR_BAD_TIME;
        }
    }
    memset(payload, 0, sizeof(*payload));
    payload->notes.notes = notes;
    payload->notes.note_count = candidate->chord_count - context->first;
    return PLAN_OK;
}

/* The target keeps its id and takes the alto; the tenor and, when written, the soprano go in the spans over the same time. */
static PlanResult operator_apply(PiecePlan *plan, FillTarget target, const FillCandidate *candidate, void *context) {
    const VoicingCandidate *voicing = candidate ? candidate->payload : NULL;
    VoicingContext *read;
    PlanNote *notes;
    PlanResult result = PLAN_OK;
    (void)context;
    if (target.kind != FILL_TARGET_LANE_SPAN || !voicing || voicing->magic != VOICING_PAYLOAD_MAGIC) {
        return PLAN_ERR_NULL;
    }
    read = calloc(1, sizeof(*read));
    notes = calloc(VOICING_MAX_CHORDS, sizeof(*notes));
    if (!read || !notes) {
        free(read);
        free(notes);
        return PLAN_ERR_ALLOC;
    }
    /* The plan must still be what the voicing was made for. */
    if (!voicing_read(plan, (LaneSpanId)target.id, read) || voicing->chord_count != read->chord_count) {
        result = PLAN_ERR_BAD_PAYLOAD;
    }
    for (size_t part = VOICING_SOPRANO; result == PLAN_OK && part < VOICING_PARTS; part++) {
        PlanLanePayload payload;
        LaneSpanId id = part == VOICING_ALTO ? (LaneSpanId)target.id
                        : part == VOICING_TENOR ? read->tenor_span : read->melody_span;
        if (part == VOICING_SOPRANO && !read->write_soprano) {
            continue;
        }
        result = part_payload(read, voicing, part, notes, &payload);
        if (result != PLAN_OK) {
            break;
        }
        if (id != PLAN_LANE_SPAN_NONE) {
            result = plan_lane_span_write(plan, id, &payload);
        } else {
            result = plan_add_lane_span(plan, read->owner, PLAN_LANE_INNER_VOICE, 2, read->span, &payload, &id);
            if (result == PLAN_OK) {
                result = plan_lane_span_set_state(plan, id, PLAN_STATE_REALIZED);
            }
        }
    }
    voicing_context_free(read);
    free(read);
    free(notes);
    return result;
}

FillResult voicing_register(FillOperatorRegistry *registry, const VoicingParams *params) {
    FillOperator operator_;
    memset(&operator_, 0, sizeof(operator_));
    operator_.generator.name = VOICING_OPERATOR_NAME;
    operator_.generator.supported_inputs = "PiecePlan inner voice lane span over a written harmony and bass";
    operator_.generator.supported_outputs = "PiecePlan inner voice lane spans, and a melody lane span";
    operator_.generator.configuration_description = "the number of candidates and the width of the beam";
    operator_.generator.constraints_description = "standard-voicing for every chord, standard-counterpoint and standard-harmony between them";
    operator_.generator.deterministic = true;
    operator_.generator.version = "1";
    operator_.kind = FILL_OPERATOR_FILL;
    operator_.applies = operator_applies;
    operator_.generate = operator_generate;
    operator_.apply = operator_apply;
    operator_.context = (void *)params;
    return fill_registry_register(registry, &operator_);
}
