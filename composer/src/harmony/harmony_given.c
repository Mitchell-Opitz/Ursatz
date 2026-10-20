#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/plan_state.h>
#include <ursatz_composer/seam_hear.h>

#include "harmony/harmony_internal.h"
#include "melody/melody_internal.h"

/*
 * Given material around a harmony target (R-071): what the Given bass and the Given melody over the target ask of
 * its chords, and the Given chords it joins.
 */

/* The state of a lane span is Given. */
static bool given(const PiecePlan *plan, LaneSpanId id) {
    return plan_lane_span_state(plan, id) == PLAN_STATE_GIVEN;
}

static bool time_plus(PlanTime a, PlanTime b, PlanTime *out) {
    return plan_time_add(a, b, out) == PLAN_OK;
}

/* Whether the time is a whole number of bars from the start of the plan. */
static bool on_bar(PlanTime time, PlanTime bar_length) {
    return time.num >= 0 && (time.num * (long)bar_length.den) % ((long)time.den * bar_length.num) == 0;
}

/* The pitch class of the Given note of a lane sounding at the time, or -1; with start_only, only one that starts there. */
static int given_pitch_class(const PiecePlan *plan, PlanLaneKind lane, PlanSpan span, PlanTime at, bool start_only) {
    LaneSpanId ids[32];
    PlanTime tick = {1, 4096};
    PlanSpan here = {at, tick};
    size_t found = plan_lane_find(plan, lane, 0, here, ids, 32);
    (void)span;
    for (size_t i = 0; i < found && i < 32; i++) {
        if (!given(plan, ids[i])) {
            continue;
        }
        PlanSpan lane_span = plan_lane_span_span(plan, ids[i]);
        const PlanNotes *notes = &plan_lane_span_payload(plan, ids[i])->notes;
        for (size_t n = 0; n < notes->note_count; n++) {
            PlanTime onset;
            PlanTime end;
            if (!time_plus(lane_span.start, notes->notes[n].offset, &onset) ||
                !time_plus(onset, notes->notes[n].duration, &end)) {
                continue;
            }
            int from_start = plan_time_compare(onset, at);
            if (start_only ? from_start == 0 : (from_start <= 0 && plan_time_compare(at, end) < 0)) {
                return ((notes->notes[n].midi_pitch % 12) + 12) % 12;
            }
        }
    }
    return -1;
}

/* The Given chords in a row that end where `from` is, and the ones that start where it is. */
static size_t read_chain(const PiecePlan *plan, const PhraseRequest *request, PlanTime from, bool before,
                         PhraseChord *out) {
    PlanSpan window = {from, request->bar_length};
    LaneSpanId ids[HARMONY_JOIN_CHORDS * 2 + 4];
    size_t count = 0;
    PlanTime position = from;
    if (before) {
        PlanTime back = {-request->bar_length.num * (long)(HARMONY_JOIN_CHORDS + 2), request->bar_length.den};
        PlanTime start;
        if (!time_plus(from, back, &start)) {
            return 0;
        }
        if (start.num < 0) {
            start.num = 0;
            start.den = 1;
        }
        window.start = start;
        if (plan_time_make(from.num * (long)start.den - start.num * (long)from.den, from.den * start.den,
                           &window.length) != PLAN_OK || window.length.num <= 0) {
            return 0;
        }
    } else {
        window.length.num = request->bar_length.num * (long)(HARMONY_JOIN_CHORDS + 2);
    }
    size_t found = plan_lane_find(plan, PLAN_LANE_HARMONY, 0, window, ids, sizeof(ids) / sizeof(ids[0]));
    if (found == 0 || found > sizeof(ids) / sizeof(ids[0])) {
        return 0;
    }
    if (before) {
        /* From the last span, back while each one ends where the one after it starts. */
        for (size_t i = found; i > 0 && count < HARMONY_JOIN_CHORDS; i--) {
            PlanSpan span = plan_lane_span_span(plan, ids[i - 1]);
            PlanTime end;
            PhraseChord chord;
            if (!given(plan, ids[i - 1]) || plan_span_end(span, &end) != PLAN_OK || plan_time_compare(end, position) != 0 ||
                !harmony_read_chord(plan, ids[i - 1], request->mode, request->cell_length, &chord)) {
                break;
            }
            out[count++] = chord;
            position = span.start;
        }
        /* Reversed into time order, and trimmed so the first starts on a barline: the recognizer counts beats from it. */
        for (size_t i = 0; i < count / 2; i++) {
            PhraseChord swap = out[i];
            out[i] = out[count - 1 - i];
            out[count - 1 - i] = swap;
        }
        while (count > 0 && !on_bar(position, request->bar_length)) {
            /* position is where the first chord of the chain starts; drop it and move on to the next. */
            PlanTime next;
            size_t cells = out[0].cells;
            PlanTime cells_length = {request->cell_length.num * (long)cells, request->cell_length.den};
            if (!time_plus(position, cells_length, &next)) {
                return 0;
            }
            memmove(out, out + 1, (count - 1) * sizeof(PhraseChord));
            count--;
            position = next;
        }
        return count;
    }
    for (size_t i = 0; i < found && count < HARMONY_JOIN_CHORDS; i++) {
        PlanSpan span = plan_lane_span_span(plan, ids[i]);
        PlanTime end;
        PhraseChord chord;
        if (plan_time_compare(span.start, position) < 0) {
            continue;
        }
        if (!given(plan, ids[i]) || plan_time_compare(span.start, position) != 0 || plan_span_end(span, &end) != PLAN_OK ||
            !harmony_read_chord(plan, ids[i], request->mode, request->cell_length, &chord)) {
            break;
        }
        out[count++] = chord;
        position = end;
    }
    return count;
}

/* Whether a phrase or a section of the plan starts (or ends) where the target does. */
static bool at_boundary(const PiecePlan *plan, const PhraseRequest *request, bool start) {
    PlanTime target_end;
    if (plan_span_end(request->span, &target_end) != PLAN_OK) {
        return false;
    }
    for (NodeId node = request->owner; node != PLAN_NODE_NONE; node = plan_node_parent(plan, node)) {
        PlanNodeKind kind = plan_node_kind(plan, node);
        PlanSpan span = plan_node_span(plan, node);
        PlanTime end;
        if ((kind == PLAN_NODE_PHRASE || kind == PLAN_NODE_SECTION) && plan_span_end(span, &end) == PLAN_OK &&
            plan_time_compare(start ? span.start : end, start ? request->span.start : target_end) == 0) {
            return true;
        }
    }
    return false;
}

void harmony_given_read(const PiecePlan *plan, PhraseRequest *request) {
    request->has_given = false;
    request->before_count = 0;
    request->after_count = 0;
    request->start_at_boundary = false;
    request->end_at_boundary = false;
    request->at_seam = false;
    for (size_t cell = 0; cell < HARMONY_GIVEN_CELLS; cell++) {
        request->bass_pc[cell] = -1;
        request->melody_pc[cell] = -1;
    }
    if (request->cells <= HARMONY_GIVEN_CELLS) {
        for (size_t cell = 0; cell < request->cells; cell++) {
            PlanSpan at;
            if (harmony_request_cells_span(request, cell, 1, &at) != PLAN_OK) {
                continue;
            }
            request->bass_pc[cell] = given_pitch_class(plan, PLAN_LANE_BASS, request->span, at.start, false);
            if (harmony_metre_cell_on_downbeat(&request->metre, cell)) {
                request->melody_pc[cell] = given_pitch_class(plan, PLAN_LANE_MELODY, request->span, at.start, true);
            }
            request->has_given = request->has_given || request->bass_pc[cell] >= 0 || request->melody_pc[cell] >= 0;
        }
    }
    PlanTime end;
    if (plan_span_end(request->span, &end) != PLAN_OK) {
        return;
    }
    request->before_count = read_chain(plan, request, request->span.start, true, request->before);
    request->after_count = read_chain(plan, request, end, false, request->after);
    request->start_at_boundary = at_boundary(plan, request, true);
    request->end_at_boundary = at_boundary(plan, request, false);
    request->at_seam = request->after_count > 0 && seam_section_at(plan, end) != PLAN_NODE_NONE;
    request->has_given = request->has_given || request->before_count > 0 || request->after_count > 0;
}

bool harmony_given_fits(const PhraseRequest *request, const PhraseChord *chords, size_t count, const char **why) {
    size_t cell = 0;
    if (why) {
        *why = NULL;
    }
    if (!request->around_given || request->cells > HARMONY_GIVEN_CELLS) {
        return true;
    }
    for (size_t i = 0; i < count; i++) {
        int pc;
        if (cell < request->cells && request->bass_pc[cell] >= 0 &&
            (!harmony_chord_bass_pc(&request->key, &chords[i], &pc) || pc != request->bass_pc[cell])) {
            if (why) {
                *why = "chord does not stand on the given bass";
            }
            return false;
        }
        for (size_t c = cell; c < cell + chords[i].cells && c < request->cells; c++) {
            unsigned mask;
            unsigned seventh;
            if (request->melody_pc[c] >= 0 &&
                (!melody_chord_mask(&request->key, &chords[i], &mask, &seventh) ||
                 !((mask >> request->melody_pc[c]) & 1u))) {
                if (why) {
                    *why = "strong-beat note of the given melody is not a tone of the chord";
                }
                return false;
            }
        }
        cell += chords[i].cells;
    }
    return true;
}

/* The metre of Given chords before the target and the target, in a row from the first of them. False when it is not measurable. */
static bool chain_metre(const PhraseRequest *request, size_t cells, PhraseMetre *out) {
    *out = request->metre;
    out->lead = (PlanTime){0, 1};
    out->tail = (PlanTime){0, 1};
    out->cells = cells;
    if (request->before_count > 0) {
        /* The chords before are trimmed to start on a barline. */
        size_t before_cells = 0;
        for (size_t i = 0; i < request->before_count; i++) {
            before_cells += request->before[i].cells;
        }
        PlanTime before_length;
        if (plan_time_make(-request->cell_length.num * (long)before_cells, request->cell_length.den, &before_length) !=
                PLAN_OK ||
            plan_time_add(request->span.start, before_length, &out->start) != PLAN_OK) {
            return false;
        }
        out->offset = (PlanTime){0, 1};
    }
    return true;
}

bool harmony_joined_chain(const PhraseRequest *request, const PhraseChord *chords, PhraseChord *sequence,
                          PhraseMetre *metre) {
    size_t cells = 0;
    if (request->before_count == 0 || request->before_count > HARMONY_JOIN_CHORDS) {
        return false;
    }
    memcpy(sequence, request->before, request->before_count * sizeof(PhraseChord));
    memcpy(sequence + request->before_count, chords, sizeof(PhraseChord));
    for (size_t i = 0; i <= request->before_count; i++) {
        cells += sequence[i].cells;
    }
    return chain_metre(request, cells, metre);
}

bool harmony_given_join(const PhraseRequest *request, const PhraseChord *chords, size_t count, bool cadence_planned,
                        const char **why) {
    PhraseChord *sequence = NULL;
    size_t total = request->before_count + count + request->after_count;
    if (why) {
        *why = NULL;
    }
    if (!request->around_given || (request->before_count == 0 && request->after_count == 0)) {
        return true;
    }
    sequence = calloc(total, sizeof(PhraseChord));
    if (!sequence) {
        return false;
    }
    memcpy(sequence, request->before, request->before_count * sizeof(PhraseChord));
    memcpy(sequence + request->before_count, chords, count * sizeof(PhraseChord));
    memcpy(sequence + request->before_count + count, request->after, request->after_count * sizeof(PhraseChord));
    /* The cells of the Given chords before, the target and the Given chords after, in a row from the first of them. */
    PhraseMetre joined = request->metre;
    size_t joined_cells = 0;
    for (size_t i = 0; i < total; i++) {
        joined_cells += sequence[i].cells;
    }
    joined.lead = (PlanTime){0, 1};
    joined.tail = (PlanTime){0, 1};
    joined.cells = joined_cells;
    if (request->before_count > 0) {
        /* The chords before are trimmed to start on a barline. */
        size_t before_cells = 0;
        for (size_t i = 0; i < request->before_count; i++) {
            before_cells += sequence[i].cells;
        }
        PlanTime before_length;
        PlanTime start;
        if (plan_time_make(-request->cell_length.num * (long)before_cells, request->cell_length.den, &before_length) !=
                PLAN_OK ||
            plan_time_add(request->span.start, before_length, &start) != PLAN_OK) {
            free(sequence);
            return false;
        }
        joined.start = start;
        joined.offset = (PlanTime){0, 1};
    }
    HeardPhrase phrase = {sequence, total, request->mode, PLAN_CADENCE_HC, request->cells_per_bar, request->cell_length,
                          true, NULL, true, false, NULL, NULL, false, &joined};
    SegmentSet *set = harmony_segments_create(&phrase);
    bool ok = set != NULL;
    HarmonyCadences found = {NULL, 0};
    if (ok) {
        size_t segment_count;
        const HarmonySegment *segments = harmony_segments_get(set, &segment_count);
        ok = harmony_analysis_find_cadences(segments, segment_count, NULL, &found) == HARMONY_CADENCE_ANALYSIS_OK;
    }
    size_t first = request->before_count;
    size_t last = request->before_count + count - 1;
    for (size_t i = 0; ok && i < found.count && why && !*why; i++) {
        const HarmonyCadenceResult *cadence = &found.items[i];
        /* A cadence that arrives before the target does is the Given chords' own (a run of the same chord spans the join). */
        if (cadence->arrival_first < first) {
            continue;
        }
        if (cadence->arrival_last >= first && cadence->arrival_last <= last) {
            bool planned = cadence_planned && cadence->arrival_last == last;
            bool opening = cadence->arrival_last == first && request->start_at_boundary;
            if (!planned && !opening) {
                *why = "cadence heard across the join with the given chords where none is planned";
            }
        } else if (cadence->arrival_last > last && cadence->dominant_first <= last && !cadence_planned &&
                   !request->end_at_boundary) {
            *why = "cadence heard across the join with the given chords where none is planned";
        }
    }
    harmony_cadences_destroy(&found);
    if (set) {
        harmony_segments_destroy(set);
    }
    free(sequence);
    return ok;
}
