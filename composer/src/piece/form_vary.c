#include <stdlib.h>
#include <string.h>

#include "grow/grow_internal.h"
#include "piece/form_return_internal.h"
#include "util/util.h"

#include <standard-counterpoint/melodic_rules.h>

/*
 * The variations of a returning section (R-070), made on the copy form-return moved onto the target: ORNAMENT adds
 * passing and neighbor tones between the notes of the melody, REHARMONIZE reopens the chords (and what is written over
 * them), REREGISTER moves the melody an octave, VARIED does the first two the way Schoenberg's A1 does (a few
 * ornaments, at most one a bar, over a reharmonized harmony). Time inside this file is whole ticks, TICKS to a whole note.
 */

#define TICKS 128L
/* The shortest note the ornament splits in two parts: an eighth. */
#define EIGHTH 16L
/* The placeholder notes an open bass, melody and inner voice span holds until the operators write them. */
#define BASS_PLACEHOLDER 40
#define INNER_PLACEHOLDER 55

static bool to_ticks(Rational time, long *out) {
    long numerator = time.num * TICKS;
    if (numerator % (long)time.den != 0) {
        return false;
    }
    *out = numerator / (long)time.den;
    return true;
}

static bool from_ticks(long ticks, Rational *out) {
    return rational_make(ticks, (long long)TICKS, out) == RATIONAL_OK;
}

static bool is_step(int move) {
    return standard_counterpoint_is_step(move);
}

/* ------------------------------------------------------------- ornament */

/* The ticks of a cell of the meter (a strong beat); 0 for a meter the harmony operators do not place on. */
static long cell_ticks(const PlanMeter *meter) {
    size_t cells;
    Rational bar = grow_bar_length(meter);
    long bar_ticks;
    if (!grow_cells_per_bar(meter, &cells) || cells == 0 || !to_ticks(bar, &bar_ticks) ||
        bar_ticks % (long)cells != 0) {
        return 0;
    }
    return bar_ticks / (long)cells;
}

static bool in_scale(AnalysisKey key, int pitch) {
    static const bool MAJOR[12] = {true, false, true, false, true, true, false, true, false, true, false, true};
    static const bool MINOR[12] = {true, false, true, true, false, true, false, true, true, false, true, false};
    int degree = ((pitch - analysis_key_tonic_pitch_class(&key)) % 12 + 12) % 12;
    return key.mode == KEY_STRUCTURE_MODE_MAJOR ? MAJOR[degree] : MINOR[degree];
}

/*
 * The non-chord tone between a note and the one after it: a passing tone when they are a third apart and the scale has
 * a tone between, a neighbor tone (upper or lower, drawn from rng) when the next note is the same. Approached and left
 * by step either way. False for any other pair.
 */
static bool choose_tone(AnalysisKey key, int from, int to, FillRng *rng, int *out) {
    int difference = to - from;
    if (difference == 0) {
        int up = 0;
        int down = 0;
        for (int step = 1; step <= 2; step++) {
            up = up == 0 && in_scale(key, from + step) ? from + step : up;
            down = down == 0 && in_scale(key, from - step) ? from - step : down;
        }
        if (up == 0 && down == 0) {
            return false;
        }
        *out = up == 0 ? down : (down == 0 ? up : (fill_rng_below(rng, 2) == 0 ? up : down));
        return true;
    }
    if (util_absolute(difference) == 3 || util_absolute(difference) == 4) {
        int direction = util_sign(difference);
        for (int step = 1; step <= 2; step++) {
            int pitch = from + direction * step;
            if (in_scale(key, pitch) && is_step(to - pitch) && util_sign(to - pitch) == direction) {
                *out = pitch;
                return true;
            }
        }
    }
    return false;
}

/* A note that is ornamented: the note of a melody span, the ticks its own pitch keeps, and the tone that follows. */
typedef struct {
    size_t span;
    size_t note;
    long first;
    int pitch;
    long onset; /* ticks from the start of the target section to the tone that is added */
} Ornament;

/* The ticks of the first part of a note split in two, so that the tone after it falls off the strong beats; 0 for none. */
static long split_at(long onset, long duration, long cell) {
    long candidates[2] = {(duration / EIGHTH / 2) * EIGHTH, duration - EIGHTH};
    for (size_t i = 0; i < 2; i++) {
        if (candidates[i] >= EIGHTH && candidates[i] <= duration - EIGHTH && (onset + candidates[i]) % cell != 0) {
            return candidates[i];
        }
    }
    return 0;
}

/* The ticks from the start of the target section to the start of a note of a span. */
static bool note_onset(const ReturnPayload *payload, const ReturnSpan *span, const PlanNote *note, long *onset) {
    Rational start;
    Rational total;
    long ticks;
    return rational_sub(span->span.start, payload->span.start, &start) == RATIONAL_OK && rational_add(start, note->offset, &total) == RATIONAL_OK &&
           to_ticks(total, &ticks) && (*onset = ticks, true);
}

/* Whether the notes of a span are contiguous from note i to i + 1, as ticks. */
static bool contiguous(const PlanNote *notes, size_t i) {
    long end;
    long next;
    Rational sum;
    return rational_add(notes[i].offset, notes[i].duration, &sum) == RATIONAL_OK && to_ticks(sum, &end) &&
           to_ticks(notes[i + 1].offset, &next) && end == next;
}

/* per_bar: at most this many ornaments are kept in a bar (0: no limit). */
static bool ornament_melody(ReturnPayload *payload, FillRng *rng, size_t per_bar) {
    ReturnCopy *copy = &payload->result;
    long cell = cell_ticks(&payload->meter);
    size_t capacity = 0;
    for (size_t i = 0; i < copy->span_count; i++) {
        capacity += copy->spans[i].lane == PLAN_LANE_MELODY ? copy->spans[i].note_count : 0;
    }
    Ornament *found = capacity ? calloc(capacity, sizeof(Ornament)) : NULL;
    size_t count = 0;
    long bar_ticks = 0;
    if (per_bar > 0 && !to_ticks(grow_bar_length(&payload->meter), &bar_ticks)) {
        bar_ticks = 0;
    }
    if (cell == 0 || !found || (per_bar > 0 && bar_ticks <= 0)) {
        free(found);
        return false;
    }
    /* The notes that can be split: on a strong beat, followed at once by a note a third away or the same, and not the
       last of the span. */
    for (size_t s = 0; s < copy->span_count; s++) {
        const ReturnSpan *span = &copy->spans[s];
        if (span->lane != PLAN_LANE_MELODY || span->voice != 0) {
            continue;
        }
        for (size_t n = 0; n + 1 < span->note_count; n++) {
            long onset;
            long duration;
            int pitch;
            if (!note_onset(payload, span, &span->notes[n], &onset) || !to_ticks(span->notes[n].duration, &duration) ||
                onset % cell != 0 || duration % EIGHTH != 0 || duration < 2 * EIGHTH || !contiguous(span->notes, n)) {
                continue;
            }
            long first = split_at(onset, duration, cell);
            if (first == 0 ||
                !choose_tone(payload->key, span->notes[n].midi, span->notes[n + 1].midi, rng, &pitch)) {
                continue;
            }
            found[count++] = (Ornament){s, n, first, pitch, onset + first};
        }
    }
    /* Each is taken with the chance of one in two (with a limit per bar, the first kept in a bar), and at least one is. */
    size_t kept = 0;
    long bar_of_last = -1;
    size_t in_bar = 0;
    for (size_t i = 0; i < count; i++) {
        if (fill_rng_below(rng, 2) != 0) {
            continue;
        }
        if (per_bar > 0) {
            long bar = found[i].onset / bar_ticks;
            in_bar = bar == bar_of_last ? in_bar : 0;
            bar_of_last = bar;
            if (in_bar >= per_bar) {
                continue;
            }
            in_bar++;
        }
        found[kept++] = found[i];
    }
    if (kept == 0 && count > 0) {
        found[0] = found[fill_rng_below(rng, count)];
        kept = 1;
    }
    bool ok = kept > 0;
    for (size_t s = 0; ok && s < copy->span_count; s++) {
        ReturnSpan *span = &copy->spans[s];
        size_t added = 0;
        for (size_t i = 0; i < kept; i++) {
            added += found[i].span == s;
        }
        if (added == 0) {
            continue;
        }
        PlanNote *notes = calloc(span->note_count + added, sizeof(PlanNote));
        size_t used = 0;
        ok = notes != NULL;
        for (size_t n = 0; ok && n < span->note_count; n++) {
            const Ornament *ornament = NULL;
            for (size_t i = 0; i < kept; i++) {
                if (found[i].span == s && found[i].note == n) {
                    ornament = &found[i];
                }
            }
            notes[used] = span->notes[n];
            if (ornament) {
                long duration;
                Rational first;
                Rational second;
                Rational offset;
                ok = to_ticks(span->notes[n].duration, &duration) && from_ticks(ornament->first, &first) &&
                     from_ticks(duration - ornament->first, &second) &&
                     rational_add(span->notes[n].offset, first, &offset) == RATIONAL_OK;
                notes[used].duration = first;
                used++;
                notes[used].midi = ornament->pitch;
                notes[used].spelled = false;
                notes[used].offset = offset;
                notes[used].duration = second;
            }
            used++;
        }
        if (ok) {
            free(span->notes);
            span->notes = notes;
            span->note_count = used;
        } else {
            free(notes);
        }
    }
    free(found);
    return ok;
}

/* ------------------------------------------------------------- reharmonize */

/* Gives a span the one placeholder note an open span holds, as long as the span. */
static bool open_notes(ReturnSpan *span, int pitch) {
    PlanNote *note = calloc(1, sizeof(PlanNote));
    if (!note) {
        return false;
    }
    free(span->notes);
    note->midi = pitch;
    note->spelled = false;
    note->offset = (Rational){0, 1};
    note->duration = span->span.length;
    span->notes = note;
    span->note_count = 1;
    span->state = PLAN_STATE_CONSTRAINED;
    return true;
}

/* A harmony target the source was written from: the spans of one owner from one with a shape to the next. */
typedef struct {
    size_t owner;
    PlanSpan span;
    size_t first;       /* the index of its first harmony span */
    bool bass_emitted;
} Target;

static bool has_kind(const ReturnSpan *span, PlanConstraintKind kind) {
    for (size_t i = 0; i < span->constraint_count; i++) {
        if (span->constraints[i].kind == kind) {
            return true;
        }
    }
    return false;
}

static bool span_end(PlanSpan span, Rational *end) {
    return plan_span_end(span, end) == PLAN_OK;
}

/* Whether a bass span starts inside the target. */
static bool inside(const Target *target, const ReturnSpan *span) {
    Rational end;
    return span->owner == target->owner && span_end(target->span, &end) &&
           rational_cmp(span->span.start, target->span.start) >= 0 && rational_cmp(span->span.start, end) < 0;
}

static bool reharmonize(ReturnPayload *payload) {
    ReturnCopy *copy = &payload->result;
    Target *targets = calloc(copy->span_count + 1, sizeof(Target));
    size_t target_count = 0;
    bool ok = targets != NULL;
    /* The targets: a harmony span with a shape starts one, the spans after it with none are its chords. */
    for (size_t i = 0; ok && i < copy->span_count; i++) {
        const ReturnSpan *span = &copy->spans[i];
        if (span->lane != PLAN_LANE_HARMONY) {
            continue;
        }
        Target *current = target_count ? &targets[target_count - 1] : NULL;
        Rational end;
        if (!current || current->owner != span->owner || has_kind(span, PLAN_CONSTRAINT_HARMONY_SHAPE)) {
            targets[target_count++] = (Target){span->owner, span->span, i, false};
            continue;
        }
        ok = span_end(span->span, &end) && rational_sub(end, current->span.start, &current->span.length) == RATIONAL_OK;
    }
    ok = ok && target_count > 0;

    ReturnSpan *spans = ok ? calloc(copy->span_count, sizeof(ReturnSpan)) : NULL;
    size_t used = 0;
    ok = ok && spans != NULL;
    for (size_t i = 0; ok && i < copy->span_count; i++) {
        ReturnSpan *span = &copy->spans[i];
        ReturnSpan *out = &spans[used];
        switch (span->lane) {
        case PLAN_LANE_HARMONY: {
            const Target *target = NULL;
            for (size_t t = 0; t < target_count; t++) {
                target = targets[t].first == i ? &targets[t] : target;
            }
            if (!target) {
                free(span->notes);
                free(span->constraints);
                span->notes = NULL;
                span->constraints = NULL;
                continue; /* a chord after the first: the target's span covers it */
            }
            /* The shape the target was written from, and the cadence that was moved to its last chord. */
            *out = *span;
            out->span = target->span;
            out->state = PLAN_STATE_CONSTRAINED;
            memset(&out->harmony, 0, sizeof(out->harmony));
            strcpy(out->harmony.roman_numeral, "I");
            out->harmony.has_function = false;
            out->constraint_count = 0;
            out->constraints = calloc(span->constraint_count + 2, sizeof(PlanConstraint));
            ok = out->constraints != NULL;
            for (size_t c = 0; ok && c < span->constraint_count; c++) {
                if (span->constraints[c].kind != PLAN_CONSTRAINT_CADENCE) {
                    out->constraints[out->constraint_count++] = span->constraints[c];
                }
            }
            for (size_t j = i; ok && j < copy->span_count; j++) {
                const ReturnSpan *chord = &copy->spans[j];
                Rational end;
                Rational target_end;
                if (chord->lane != PLAN_LANE_HARMONY || chord->owner != target->owner ||
                    !span_end(target->span, &target_end) || !span_end(chord->span, &end) ||
                    rational_cmp(end, target_end) > 0) {
                    continue;
                }
                for (size_t c = 0; c < chord->constraint_count; c++) {
                    if (chord->constraints[c].kind == PLAN_CONSTRAINT_CADENCE) {
                        out->constraints[out->constraint_count++] = chord->constraints[c];
                    }
                }
            }
            free(span->constraints);
            free(span->notes);
            span->constraints = NULL;
            span->notes = NULL;
            used++;
            break;
        }
        case PLAN_LANE_BASS: {
            Target *target = NULL;
            for (size_t t = 0; t < target_count; t++) {
                target = !target && inside(&targets[t], span) ? &targets[t] : target;
            }
            if (target && target->bass_emitted) {
                free(span->notes);
                free(span->constraints);
                span->notes = NULL;
                span->constraints = NULL;
                continue;
            }
            *out = *span;
            if (target) {
                out->span = target->span;
                target->bass_emitted = true;
            }
            ok = open_notes(out, BASS_PLACEHOLDER);
            used++;
            break;
        }
        case PLAN_LANE_MELODY:
            *out = *span; /* the melody is the return: it stays as it was written */
            used++;
            break;
        case PLAN_LANE_INNER_VOICE:
            *out = *span;
            ok = open_notes(out, INNER_PLACEHOLDER);
            used++;
            break;
        default:
            break;
        }
    }
    free(targets);
    if (!ok) {
        /* The spans moved into the new list are owned by it; the rest by the old, which is freed whole. */
        free(spans);
        return false;
    }
    free(copy->spans);
    copy->spans = spans;
    copy->span_count = used;
    copy->span_capacity = used;
    return true;
}

/* ------------------------------------------------------------- reregister */

static bool reregister(ReturnPayload *payload) {
    ReturnCopy *copy = &payload->result;
    for (size_t i = 0; i < copy->span_count; i++) {
        ReturnSpan *span = &copy->spans[i];
        if (span->lane != PLAN_LANE_MELODY && span->lane != PLAN_LANE_INNER_VOICE) {
            continue;
        }
        for (size_t n = 0; n < span->note_count; n++) {
            int pitch = span->notes[n].midi + payload->octave;
            if (pitch < 0 || pitch > 127 ||
                (span->lane == PLAN_LANE_MELODY && (pitch < payload->range_low || pitch > payload->range_high))) {
                return false;
            }
            span->notes[n].midi = pitch;
            span->notes[n].spelled = false;
        }
    }
    return true;
}

/* ------------------------------------------------------------- the variations */

bool return_vary(ReturnPayload *payload, FillRng *rng) {
    switch (payload->variation) {
    case PLAN_RETURN_EXACT:
        return true;
    case PLAN_RETURN_ORNAMENT:
        return ornament_melody(payload, rng, 0);
    case PLAN_RETURN_VARIED:
        return ornament_melody(payload, rng, 1) && reharmonize(payload);
    case PLAN_RETURN_REHARMONIZE:
        return reharmonize(payload);
    case PLAN_RETURN_REREGISTER:
        return payload->octave != 0 && reregister(payload);
    }
    return false;
}

bool return_same_melody(const ReturnPayload *a, const ReturnPayload *b) {
    if (a->result.span_count != b->result.span_count) {
        return false;
    }
    for (size_t i = 0; i < a->result.span_count; i++) {
        const ReturnSpan *x = &a->result.spans[i];
        const ReturnSpan *y = &b->result.spans[i];
        if (x->lane != PLAN_LANE_MELODY) {
            continue;
        }
        if (x->note_count != y->note_count) {
            return false;
        }
        for (size_t n = 0; n < x->note_count; n++) {
            if (x->notes[n].midi != y->notes[n].midi ||
                rational_cmp(x->notes[n].offset, y->notes[n].offset) != 0) {
                return false;
            }
        }
    }
    return true;
}

/* ------------------------------------------------------------- the checks */

static void reject(FillCheck *out, const char *reason) {
    out->hard_violation = true;
    strncpy(out->reason, reason, sizeof(out->reason) - 1);
}

/* A note of the returned section, moved, at the same onset in the span: its index in the result's notes, else -1. */
static long find_onset(const ReturnSpan *span, Rational offset) {
    for (size_t n = 0; n < span->note_count; n++) {
        if (rational_cmp(span->notes[n].offset, offset) == 0) {
            return (long)n;
        }
    }
    return -1;
}

/*
 * The test of one note a variation adds: off the strong beats, and passing or neighbor. NULL when it passes, else the
 * reason; onset gets the ticks from the start of the target section to the note.
 */
static const char *added_note_fault(const ReturnPayload *payload, const ReturnSpan *span, size_t n, long cell,
                                    long *onset) {
    if (!note_onset(payload, span, &span->notes[n], onset) || *onset % cell == 0) {
        return "ornament adds a note on a strong beat";
    }
    int from = n > 0 ? span->notes[n].midi - span->notes[n - 1].midi : 0;
    int to = n + 1 < span->note_count ? span->notes[n + 1].midi - span->notes[n].midi : 0;
    bool passing = is_step(from) && is_step(to) && util_sign(from) == util_sign(to);
    bool neighbor = is_step(from) && is_step(to) && util_sign(from) == -util_sign(to) && n > 0 &&
                    n + 1 < span->note_count && span->notes[n + 1].midi == span->notes[n - 1].midi;
    return passing || neighbor ? NULL : "ornament adds a note that is neither passing nor neighbor";
}

/* Whether a note of the result is one the source has (the same offset in its span). */
static bool is_original(const ReturnSpan *kept, const ReturnSpan *span, size_t n) {
    for (size_t k = 0; k < kept->note_count; k++) {
        if (rational_cmp(kept->notes[k].offset, span->notes[n].offset) == 0) {
            return true;
        }
    }
    return false;
}

/* The ornamented melody keeps every note of the returned one and adds only passing and neighbor tones off the strong beats. */
static void check_ornament(const ReturnPayload *payload, FillCheck *out) {
    ReturnAllow allow = {1u << PLAN_LANE_MELODY, 0, 0};
    long cell = cell_ticks(&payload->meter);
    size_t added_total = 0;
    if (!return_result_matches(payload, &allow) || cell == 0) {
        reject(out, "ornament changes more than the melody");
        return;
    }
    for (size_t i = 0; i < payload->result.span_count; i++) {
        const ReturnSpan *kept = &payload->source.spans[i];
        const ReturnSpan *span = &payload->result.spans[i];
        if (span->lane != PLAN_LANE_MELODY) {
            continue;
        }
        /* The returned section's notes sit at the offsets they had in their own span: only the pitches move. */
        for (size_t n = 0; n < kept->note_count; n++) {
            long at = find_onset(span, kept->notes[n].offset);
            if (at < 0 || span->notes[at].midi != kept->notes[n].midi + payload->transpose) {
                reject(out, "ornament drops a note of the melody");
                return;
            }
        }
        long last_beat = -1;
        for (size_t n = 0; n < span->note_count; n++) {
            long onset;
            if (is_original(kept, span, n)) {
                continue;
            }
            added_total++;
            const char *fault = added_note_fault(payload, span, n, cell, &onset);
            if (fault) {
                reject(out, fault);
                return;
            }
            /* One added note a beat. */
            long beat = onset / (TICKS / payload->meter.beat_unit);
            if (beat == last_beat) {
                reject(out, "ornament adds two notes in a beat");
                return;
            }
            last_beat = beat;
        }
    }
    out->score_numerator = added_total > 0 ? 1 : 0;
}

/* Whether the notes of two spans are the same, the second's pitches shift semitones from the first's. */
static bool same_notes(const ReturnSpan *a, const ReturnSpan *b, int shift) {
    if (a->note_count != b->note_count) {
        return false;
    }
    for (size_t n = 0; n < a->note_count; n++) {
        if (a->notes[n].midi + shift != b->notes[n].midi ||
            rational_cmp(a->notes[n].offset, b->notes[n].offset) != 0 ||
            rational_cmp(a->notes[n].duration, b->notes[n].duration) != 0) {
            return false;
        }
    }
    return true;
}

/*
 * The harmony, bass and inner voices are open (the harmony spans with the shape they were written from), the melody
 * is written, and the open harmony covers the returned section's. A violation is recorded in out.
 */
static void check_reopened(const ReturnPayload *payload, FillCheck *out) {
    const ReturnCopy *copy = &payload->result;
    Rational harmony = {0, 1};
    Rational source_harmony = {0, 1};
    size_t reopened = 0;
    for (size_t i = 0; i < copy->span_count; i++) {
        const ReturnSpan *span = &copy->spans[i];
        bool written = span->lane == PLAN_LANE_MELODY;
        if ((span->state != PLAN_STATE_CONSTRAINED) == !written) {
            reject(out, written ? "reharmonization reopens the melody" : "reharmonization leaves a lane written");
            return;
        }
        if (span->lane == PLAN_LANE_HARMONY) {
            reopened++;
            if (!has_kind(span, PLAN_CONSTRAINT_HARMONY_SHAPE) ||
                rational_add(harmony, span->span.length, &harmony) != RATIONAL_OK) {
                reject(out, "a reopened harmony span has no shape");
                return;
            }
        }
    }
    for (size_t i = 0; i < payload->source.span_count; i++) {
        const ReturnSpan *span = &payload->source.spans[i];
        if (span->lane == PLAN_LANE_HARMONY && rational_add(source_harmony, span->span.length, &source_harmony) != RATIONAL_OK) {
            reject(out, "the returned harmony cannot be read");
            return;
        }
    }
    if (reopened == 0 || rational_cmp(harmony, source_harmony) != 0) {
        reject(out, "reharmonization does not cover the returned harmony");
    }
}

/* The next melody span of the result at or after index *next, which moves past it; NULL when there is none. */
static const ReturnSpan *next_melody_span(const ReturnCopy *copy, size_t *next) {
    while (*next < copy->span_count && copy->spans[*next].lane != PLAN_LANE_MELODY) {
        (*next)++;
    }
    return *next < copy->span_count ? &copy->spans[(*next)++] : NULL;
}

/* The melody of the result is the returned one's, in the same spans, moved. */
static void check_reharmonize(const ReturnPayload *payload, FillCheck *out) {
    size_t next_melody = 0;
    check_reopened(payload, out);
    if (out->hard_violation) {
        return;
    }
    for (size_t i = 0; i < payload->source.span_count; i++) {
        const ReturnSpan *span = &payload->source.spans[i];
        if (span->lane != PLAN_LANE_MELODY) {
            continue;
        }
        const ReturnSpan *result = next_melody_span(&payload->result, &next_melody);
        if (!result || !same_notes(span, result, payload->transpose)) {
            reject(out, "reharmonization changes the melody");
            return;
        }
    }
    out->score_numerator = 1;
}

/*
 * A varied return: the melody keeps every note of the returned one at its offset, moved, and adds at most one
 * passing or neighbor tone a bar off the strong beats, at least one in all; the other lanes are open as for a
 * reharmonization.
 */
static void check_varied(const ReturnPayload *payload, FillCheck *out) {
    long cell = cell_ticks(&payload->meter);
    long bar_ticks = 0;
    long last_bar = -1;
    size_t added_total = 0;
    size_t next_melody = 0;
    if (cell == 0 || !to_ticks(grow_bar_length(&payload->meter), &bar_ticks) || bar_ticks <= 0) {
        reject(out, "varied return has a meter it cannot place notes in");
        return;
    }
    check_reopened(payload, out);
    if (out->hard_violation) {
        return;
    }
    for (size_t i = 0; i < payload->source.span_count; i++) {
        const ReturnSpan *kept = &payload->source.spans[i];
        if (kept->lane != PLAN_LANE_MELODY) {
            continue;
        }
        const ReturnSpan *span = next_melody_span(&payload->result, &next_melody);
        if (!span) {
            reject(out, "varied return changes the melody");
            return;
        }
        for (size_t n = 0; n < kept->note_count; n++) {
            long at = find_onset(span, kept->notes[n].offset);
            if (at < 0 || span->notes[at].midi != kept->notes[n].midi + payload->transpose) {
                reject(out, "varied return drops a note of the melody");
                return;
            }
        }
        for (size_t n = 0; n < span->note_count; n++) {
            long onset;
            if (is_original(kept, span, n)) {
                continue;
            }
            const char *fault = added_note_fault(payload, span, n, cell, &onset);
            if (fault) {
                reject(out, fault);
                return;
            }
            if (onset / bar_ticks == last_bar) {
                reject(out, "varied return adds two notes in a bar");
                return;
            }
            last_bar = onset / bar_ticks;
            added_total++;
        }
    }
    if (added_total == 0) {
        reject(out, "varied return adds no note");
        return;
    }
    out->score_numerator = 1;
}

static void check_reregister(const ReturnPayload *payload, FillCheck *out) {
    ReturnAllow allow = {0, (1u << PLAN_LANE_MELODY) | (1u << PLAN_LANE_INNER_VOICE), payload->octave};
    if (payload->octave != 12 && payload->octave != -12) {
        reject(out, "re-registration is not an octave");
        return;
    }
    if (!return_result_matches(payload, &allow)) {
        reject(out, "re-registration changes more than the octave of the melody and inner voices");
        return;
    }
    for (size_t i = 0; i < payload->result.span_count; i++) {
        const ReturnSpan *span = &payload->result.spans[i];
        for (size_t n = 0; span->lane == PLAN_LANE_MELODY && n < span->note_count; n++) {
            if (span->notes[n].midi < payload->range_low || span->notes[n].midi > payload->range_high) {
                reject(out, "re-registered melody leaves the range");
                return;
            }
        }
    }
    out->score_numerator = 1;
}

void return_vary_check(const ReturnPayload *payload, FillCheck *out) {
    switch (payload->variation) {
    case PLAN_RETURN_ORNAMENT:
        check_ornament(payload, out);
        break;
    case PLAN_RETURN_REHARMONIZE:
        check_reharmonize(payload, out);
        break;
    case PLAN_RETURN_VARIED:
        check_varied(payload, out);
        break;
    case PLAN_RETURN_REREGISTER:
        check_reregister(payload, out);
        break;
    case PLAN_RETURN_EXACT:
        break;
    }
}
