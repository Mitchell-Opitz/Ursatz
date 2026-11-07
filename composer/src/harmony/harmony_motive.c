#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/harmony_motive.h>
#include <standard-tonality/roman_numeral_grammar.h>

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

/*
 * One diatonic chord of the numeral, one cell long. A motive writes its harmony as degrees whatever the mode ("I" for
 * the tonic of a minor key too, R-195), so only the degree, the inversion and the seventh are read, not the case. False for
 * an altered chord or one aimed at another (applied), which the harmony operators write as they do.
 */
static bool tone_chord(KeyStructureMode mode, const char *numeral, PhraseChord *out) {
    StandardTonalityNumeralText parsed;
    if (!standard_tonality_numeral_parse(numeral, &parsed) || parsed.alteration != 0 || parsed.has_target) {
        return false;
    }
    return harmony_make_chord(mode, parsed.degree, parsed.inversion,
                              standard_tonality_numeral_quality_is_seventh(parsed.quality), false, 1, out);
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

/* --- the operator ------------------------------------------------------------------------------------------------- */

/* Combinations of fills proposed for one target. */
#define HARMONY_MOTIVE_CANDIDATES 8
#define MAX_RUNS 16
#define FILLS_PER_RUN 4

typedef struct {
    size_t first;
    size_t count;
} Run;

typedef struct {
    size_t chord_count;
    PhraseChord chords[HARMONY_MAX_CHORDS];
} Fill;

typedef struct {
    Run run;
    size_t fill_count;
    Fill fills[FILLS_PER_RUN];
} RunFills;

static bool motive_read_request(const PiecePlan *plan, LaneSpanId target, PhraseRequest *out) {
    return (harmony_prolong_read_request_given(plan, target, out) || harmony_phrase_read_request_given(plan, target, out)) &&
           out->has_motive && out->lead.num == 0 && out->tail.num == 0;
}

static bool motive_applies(const PiecePlan *plan, FillTarget target, void *context) {
    PhraseRequest *request = malloc(sizeof(*request));
    (void)context;
    bool applies = request && harmony_is_target(target) && motive_read_request(plan, (LaneSpanId)target.id, request);
    free(request);
    return applies;
}

static size_t free_runs(const PhraseRequest *request, Run *runs) {
    size_t count = 0;
    for (size_t cell = 0; cell < request->cells; cell++) {
        if (request->motive_fixed[cell]) {
            continue;
        }
        if (count > 0 && runs[count - 1].first + runs[count - 1].count == cell) {
            runs[count - 1].count++;
        } else if (count < MAX_RUNS) {
            runs[count].first = cell;
            runs[count].count = 1;
            count++;
        } else {
            return MAX_RUNS + 1;
        }
    }
    return count;
}

/* Appends the chord over cells [first, first + count), cut so that no piece passes the shape's cap or a barline it avoids. */
static bool append_chord(const PhraseRequest *request, const PhraseChord *chord, size_t first, size_t count, Fill *fill) {
    size_t cap = request->has_shape && request->shape.max_cells_per_chord > 0 ? (size_t)request->shape.max_cells_per_chord : 0;
    bool split = request->has_shape && request->shape.split_at_bars;
    size_t at = first;
    while (at < first + count) {
        size_t length = 1;
        while (at + length < first + count && (cap == 0 || length < cap) &&
               !(split && harmony_metre_cell_on_downbeat(&request->metre, at + length))) {
            length++;
        }
        if (fill->chord_count >= HARMONY_MAX_CHORDS) {
            return false;
        }
        fill->chords[fill->chord_count] = *chord;
        fill->chords[fill->chord_count].cells = length;
        fill->chord_count++;
        at += length;
    }
    return true;
}

/* A request for a run: the cells [first, first + count) of the target, with the cadence only when the run ends it. */
static bool sub_request(const PhraseRequest *request, Run run, PhraseRequest *out) {
    Rational start;
    Rational length;
    size_t bar;
    Rational in_bar;
    if (!harmony_cells_of_span(&request->metre, run.first, run.count, &start, &length) ||
        !harmony_metre_cell_position(&request->metre, run.first, &bar, &in_bar)) {
        return false;
    }
    *out = *request;
    out->has_motive = false;
    if (rational_add(request->span.start, start, &out->span.start) != RATIONAL_OK) {
        return false;
    }
    out->span.length = length;
    out->cells = run.count;
    out->bars = (in_bar.num > 0 ? 1 : 0) + (run.count + request->cells_per_bar - 1) / request->cells_per_bar;
    out->metre.start = out->span.start;
    out->metre.cells = run.count;
    out->metre.bars = out->bars;
    out->metre.offset = in_bar;
    out->before_count = 0;
    out->after_count = 0;
    out->start_at_boundary = false;
    out->end_at_boundary = false;
    out->at_seam = false;
    out->pivot = false;
    if (run.first > 0) {
        /* The motive chord before the run is its opening, so the run has none of its own. */
        if (out->has_shape) {
            out->shape.kind = PLAN_HARMONY_SHAPE_CADENTIAL;
            out->shape.starts_on = PLAN_HARMONY_ANCHOR_ANY;
        } else {
            out->has_shape = true;
            memset(&out->shape, 0, sizeof(out->shape));
            out->shape.kind = PLAN_HARMONY_SHAPE_CADENTIAL;
        }
    }
    bool last = run.first + run.count == request->cells;
    if (!last) {
        out->cadence = CADENCE_TYPE_HALF;
        if (out->has_shape) {
            out->shape.ends_on = PLAN_HARMONY_ANCHOR_ANY;
        }
    }
    for (size_t cell = 0; cell < run.count; cell++) {
        out->bass_pc[cell] = request->bass_pc[run.first + cell];
        out->melody_pc[cell] = request->melody_pc[run.first + cell];
    }
    return true;
}

/* The chord that holds a run when the phrase model has no room: the anchor it touches, else the chord next to it. */
static bool plain_fill(const PhraseRequest *request, Run run, Fill *fill) {
    PhraseChord chord;
    PlanHarmonyAnchor anchor = PLAN_HARMONY_ANCHOR_ANY;
    if (request->has_shape && run.first == 0) {
        anchor = request->shape.starts_on;
    } else if (request->has_shape && run.first + run.count == request->cells) {
        anchor = request->shape.ends_on;
    }
    if (anchor == PLAN_HARMONY_ANCHOR_TONIC || anchor == PLAN_HARMONY_ANCHOR_DOMINANT) {
        if (!harmony_make_chord(request->mode, anchor == PLAN_HARMONY_ANCHOR_TONIC ? 1 : 5, 0, false, false, 1, &chord)) {
            return false;
        }
    } else if (run.first > 0 && request->motive_fixed[run.first - 1]) {
        chord = request->motive_chord[run.first - 1];
    } else if (run.first + run.count < request->cells && request->motive_fixed[run.first + run.count]) {
        chord = request->motive_chord[run.first + run.count];
    } else if (!harmony_make_chord(request->mode, 1, 0, false, false, 1, &chord)) {
        return false;
    }
    return append_chord(request, &chord, run.first, run.count, fill);
}

/* The ways to fill one run: the phrase model's candidates over it, else the plain chord. */
static bool fill_open_run(const PhraseRequest *request, Run run, FillRng *rng, RunFills *out) {
    PhraseRequest *sub = malloc(sizeof(*sub));
    FillCandidateList list = {NULL, 0, 0};
    out->run = run;
    out->fill_count = 0;
    /* A prolongation has no cadence to make: its open cells are held by the plain chord. */
    bool prolongation = request->has_shape && request->shape.kind == PLAN_HARMONY_SHAPE_PROLONGATION;
    if (sub && !prolongation && sub_request(request, run, sub)) {
        harmony_phrase_propose(sub, rng, &list);
        for (size_t i = 0; i < list.count && out->fill_count < FILLS_PER_RUN; i++) {
            const PhraseProgression *progression = list.items[i].payload;
            Fill *fill = &out->fills[out->fill_count];
            fill->chord_count = 0;
            if (!progression || progression->count > HARMONY_MAX_CHORDS) {
                continue;
            }
            size_t total = 0;
            for (size_t c = 0; c < progression->count; c++) {
                total += progression->chords[c].cells;
            }
            if (total != run.count) {
                continue;
            }
            memcpy(fill->chords, progression->chords, progression->count * sizeof(PhraseChord));
            fill->chord_count = progression->count;
            out->fill_count++;
        }
    }
    fill_candidates_clear(&list);
    free(sub);
    if (out->fill_count == 0) {
        out->fills[0].chord_count = 0;
        if (!plain_fill(request, run, &out->fills[0])) {
            return false;
        }
    }
    out->fill_count = out->fill_count ? out->fill_count : 1;
    return true;
}

/* The chords of the target with each run filled by the fill the pick gives it (a counter over the runs, digit by digit). */
static bool combine(const PhraseRequest *request, const RunFills *runs, size_t run_count, size_t pick,
                    PhraseChord *chords, size_t *count) {
    Fill *whole = malloc(sizeof(*whole));
    if (!whole) {
        return false;
    }
    whole->chord_count = 0;
    size_t cell = 0;
    size_t next_run = 0;
    bool ok = true;
    while (ok && cell < request->cells) {
        if (next_run < run_count && runs[next_run].run.first == cell) {
            const Fill *fill = &runs[next_run].fills[pick % runs[next_run].fill_count];
            for (size_t i = 0; ok && i < fill->chord_count; i++) {
                ok = whole->chord_count < HARMONY_MAX_CHORDS;
                if (ok) {
                    whole->chords[whole->chord_count++] = fill->chords[i];
                }
            }
            cell += runs[next_run].run.count;
            pick /= runs[next_run].fill_count;
            next_run++;
            continue;
        }
        size_t end = cell;
        while (end < request->cells && request->motive_fixed[end] &&
               (end == cell || strcmp(request->motive_chord[end].roman, request->motive_chord[cell].roman) == 0)) {
            end++;
        }
        ok = end > cell && append_chord(request, &request->motive_chord[cell], cell, end - cell, whole);
        cell = end;
    }
    if (ok) {
        memcpy(chords, whole->chords, whole->chord_count * sizeof(PhraseChord));
        *count = whole->chord_count;
    }
    free(whole);
    return ok;
}

/* Whether the chords keep to the shape and the given material, and end in the cadence the target asks for. */
static bool whole_fits(const PhraseRequest *request, const PhraseChord *chords, size_t count) {
    size_t cells[HARMONY_MAX_CHORDS];
    size_t total = 0;
    for (size_t i = 0; i < count; i++) {
        cells[i] = chords[i].cells;
        total += chords[i].cells;
    }
    if (total != request->cells || count == 0) {
        return false;
    }
    bool prolongation = request->has_shape && request->shape.kind == PLAN_HARMONY_SHAPE_PROLONGATION;
    if (request->has_shape && !harmony_shape_keeps_in(&request->shape, cells, count, &request->metre, !prolongation)) {
        return false;
    }
    if (request->has_given && !harmony_given_fits(request, chords, count, NULL)) {
        return false;
    }
    if (!prolongation && count > 1) {
        CadenceType heard;
        bool at_end;
        HeardPhrase phrase = {chords, count, request->mode, request->cadence, request->cells_per_bar, request->cell_length,
                              false, NULL, true, false, NULL, NULL, false, &request->metre};
        if (!harmony_hear_phrase(&phrase, &heard, &at_end) || !at_end || heard != request->cadence) {
            return false;
        }
    }
    return true;
}

static bool motive_generate(const PiecePlan *plan, FillTarget target, FillRng *rng, void *context,
                            FillCandidateList *out) {
    PhraseRequest *request = malloc(sizeof(*request));
    RunFills *runs = malloc(MAX_RUNS * sizeof(*runs));
    PhraseProgression *kept[HARMONY_MOTIVE_CANDIDATES];
    size_t kept_count = 0;
    bool ok = request && runs && harmony_is_target(target) && motive_read_request(plan, (LaneSpanId)target.id, request);
    (void)context;
    Run spans[MAX_RUNS];
    size_t run_count = ok ? free_runs(request, spans) : 0;
    ok = ok && run_count <= MAX_RUNS;
    for (size_t i = 0; ok && i < run_count; i++) {
        ok = fill_open_run(request, spans[i], rng, &runs[i]);
    }
    if (!ok) {
        free(request);
        free(runs);
        return true; /* no candidates: the other operators write this target */
    }
    size_t combinations = 1;
    for (size_t i = 0; i < run_count; i++) {
        combinations *= runs[i].fill_count;
        if (combinations > HARMONY_MOTIVE_CANDIDATES) {
            combinations = HARMONY_MOTIVE_CANDIDATES;
        }
    }
    bool memory = true;
    for (size_t pick = 0; memory && pick < combinations; pick++) {
        PhraseChord *chords = calloc(HARMONY_MAX_CHORDS, sizeof(PhraseChord));
        size_t count = 0;
        if (!chords) {
            memory = false;
            break;
        }
        if (!combine(request, runs, run_count, pick, chords, &count) || !whole_fits(request, chords, count)) {
            free(chords);
            continue;
        }
        PhraseProgression *payload = malloc(sizeof(*payload));
        if (!payload) {
            free(chords);
            memory = false;
            break;
        }
        payload->request = *request;
        payload->chords = chords;
        payload->count = count;
        payload->repeatable = false;
        payload->repeats = false;
        bool repeat = false;
        for (size_t i = 0; i < kept_count; i++) {
            repeat = repeat || harmony_same_progression(kept[i], payload);
        }
        if (repeat) {
            harmony_progression_destroy(payload);
            continue;
        }
        char label[FILL_LABEL_MAX];
        harmony_progression_label(payload, label, sizeof(label));
        if (!fill_candidates_add(out, label, payload, harmony_progression_destroy)) {
            memory = false;
            break;
        }
        kept[kept_count++] = payload;
    }
    free(request);
    free(runs);
    return memory;
}

static PlanResult motive_apply(PiecePlan *plan, FillTarget target, const FillCandidate *candidate, void *context) {
    (void)context;
    return harmony_progression_apply(plan, target, candidate, motive_read_request);
}

FillResult harmony_motive_register(FillOperatorRegistry *registry) {
    FillOperator operator_;
    memset(&operator_, 0, sizeof(operator_));
    operator_.generator.name = HARMONY_MOTIVE_OPERATOR_NAME;
    operator_.generator.supported_inputs = "PiecePlan harmony lane span over developed motives";
    operator_.generator.supported_outputs = "PiecePlan harmony lane spans";
    operator_.generator.configuration_description = "none";
    operator_.generator.constraints_description = "the chords of the ideas' developed motives, the phrase model for the cells left open";
    operator_.generator.deterministic = true;
    operator_.generator.version = "1";
    operator_.kind = FILL_OPERATOR_FILL;
    operator_.applies = motive_applies;
    operator_.generate = motive_generate;
    operator_.apply = motive_apply;
    return fill_registry_register(registry, &operator_);
}
