#include <string.h>

#include <ursatz_composer/plan_state.h>

#include "harmony/harmony_internal.h"

/*
 * The chords of developed motives (R-344): what the motives stage wrote on each idea's motive node (its motive_tones)
 * read as chords over the cells of a harmony target.
 */

/* The motive_tones constraint of a node, or NULL. */
static const PlanMotiveTones *node_tones(const PiecePlan *plan, NodeId node) {
    for (size_t i = 0; i < plan_node_constraint_count(plan, node); i++) {
        const PlanConstraint *constraint = plan_node_constraint_at(plan, node, i);
        if (constraint->kind == PLAN_CONSTRAINT_MOTIVE_TONES) {
            return &constraint->params.motive_tones;
        }
    }
    return NULL;
}

/* One diatonic chord of the numeral, one cell long. False for a chord the harmony operators do not write as such. */
static bool tone_chord(KeyStructureMode mode, const char *numeral, PhraseChord *out) {
    HarmonyChord parsed;
    if (!harmony_roman_parse(mode, numeral, &parsed)) {
        return false;
    }
    return parsed.leading_tone ? harmony_make_leading_tone(mode, parsed.inversion, 1, out)
                               : harmony_make_chord(mode, parsed.degree, parsed.inversion, parsed.seventh, false, 1, out);
}

/* The cell of the request that holds the time (counted from the plan's start), or false when it is outside. */
static bool cell_at(const PhraseRequest *request, Rational time, size_t *cell) {
    Rational relative;
    if (rational_sub(time, request->span.start, &relative) != RATIONAL_OK || relative.num < 0) {
        return false;
    }
    for (size_t c = request->cells; c > 0; c--) {
        Rational offset;
        if (harmony_metre_cell_offset(&request->metre, c - 1, &offset) && rational_cmp(offset, relative) <= 0) {
            *cell = c - 1;
            return true;
        }
    }
    return false;
}

/* Whether a chord can stand on an anchor of the shape. */
static bool anchor_keeps(PlanHarmonyAnchor anchor, const PhraseChord *chord) {
    if (anchor == PLAN_HARMONY_ANCHOR_TONIC) {
        return !chord->applied && chord->chord.degree == 1;
    }
    if (anchor == PLAN_HARMONY_ANCHOR_DOMINANT) {
        return !chord->applied && chord->chord.degree == 5;
    }
    return true;
}

/*
 * Frees the cells a motive chord may not hold (R-344): the cadence's, and a shape anchor the chord breaks. The cadence of
 * a target is its last downbeat's chord and the dominant before it (a half cadence's has no dominant before it), the whole
 * target for the cadential shape; a prolongation has none. The anchors are the first and the last cell.
 */
static void free_cells(PhraseRequest *request) {
    bool prolongation = request->has_shape && request->shape.kind == PLAN_HARMONY_SHAPE_PROLONGATION;
    if (!prolongation) {
        size_t from = harmony_metre_last_cell(&request->metre);
        if (request->has_shape && request->shape.kind == PLAN_HARMONY_SHAPE_CADENTIAL) {
            from = 0;
        } else if (request->cadence != CADENCE_TYPE_HALF && from > 0) {
            from--;
        }
        for (size_t cell = from; cell < request->cells; cell++) {
            request->motive_fixed[cell] = false;
        }
    }
    if (request->has_shape && request->cells > 0) {
        size_t last = request->cells - 1;
        if (request->motive_fixed[0] && !anchor_keeps(request->shape.starts_on, &request->motive_chord[0])) {
            request->motive_fixed[0] = false;
        }
        if (request->motive_fixed[last] && !anchor_keeps(request->shape.ends_on, &request->motive_chord[last])) {
            request->motive_fixed[last] = false;
        }
    }
    request->has_motive = false;
    for (size_t cell = 0; cell < request->cells; cell++) {
        request->has_motive = request->has_motive || request->motive_fixed[cell];
    }
}

void harmony_motive_read(const PiecePlan *plan, PhraseRequest *request) {
    LaneSpanId ids[HARMONY_MAX_CHORDS];
    request->has_motive = false;
    memset(request->motive_fixed, 0, sizeof(request->motive_fixed));
    if (request->cells > HARMONY_GIVEN_CELLS) {
        return;
    }
    size_t found = plan_lane_find(plan, PLAN_LANE_MELODY, 0, request->span, ids, HARMONY_MAX_CHORDS);
    if (found > HARMONY_MAX_CHORDS) {
        return;
    }
    for (size_t i = 0; i < found; i++) {
        PlanConstraint ref;
        if (!harmony_find_constraint(plan, ids[i], plan_lane_span_owner(plan, ids[i]), PLAN_CONSTRAINT_MOTIVE_REF, &ref) ||
            !plan_has_node(plan, ref.params.motive.motive)) {
            continue;
        }
        const PlanMotiveTones *tones = node_tones(plan, ref.params.motive.motive);
        if (!tones || tones->chord_count == 0) {
            continue;
        }
        PlanSpan melody = plan_lane_span_span(plan, ids[i]);
        Rational melody_end;
        if (plan_span_end(melody, &melody_end) != PLAN_OK) {
            continue;
        }
        for (size_t c = 0; c < tones->chord_count; c++) {
            const PlanToneChord *tone = &tones->chords[c];
            Rational start;
            Rational end;
            PhraseChord chord;
            /* A chord holds its cells until the next chord's onset, the last until the motive's end (the idea's end). */
            if (rational_add(melody.start, tone->onset, &start) != RATIONAL_OK) {
                continue;
            }
            if (c + 1 < tones->chord_count) {
                if (rational_add(melody.start, tones->chords[c + 1].onset, &end) != RATIONAL_OK) {
                    continue;
                }
            } else {
                end = melody_end;
            }
            size_t first;
            if (!tone_chord(request->mode, tone->numeral, &chord)) {
                continue;
            }
            if (rational_cmp(start, request->span.start) < 0) {
                first = 0;
            } else if (!cell_at(request, start, &first)) {
                continue;
            }
            for (size_t cell = first; cell < request->cells; cell++) {
                Rational offset;
                Rational at;
                if (!harmony_metre_cell_offset(&request->metre, cell, &offset) ||
                    rational_add(request->span.start, offset, &at) != RATIONAL_OK || rational_cmp(at, end) >= 0) {
                    break;
                }
                request->motive_chord[cell] = chord;
                request->motive_fixed[cell] = true;
                request->has_motive = true;
            }
        }
    }
    if (request->has_motive) {
        free_cells(request);
    }
}
