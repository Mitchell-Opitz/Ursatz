#include "test_oracles.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "harmony/harmony_internal.h"
#include "melody/melody_internal.h"
#include "piece/form_figure_internal.h"
#include "util/plan_util.h"
#include "util/util.h"

/* The tests' check oracles, moved unchanged from the operators' files (see test_oracles.h). */

bool harmony_check_lane_without_cadence(const PiecePlan *plan, PlanSpan span, HarmonyMode mode,
                                         const char **violation) {
    PhraseMetre metre;
    PhraseChord chords[HARMONY_MAX_CHORDS];
    size_t count;
    PhraseVerdict verdict;
    if (!violation || !harmony_read_lane(plan, span, mode, chords, &count, &metre)) {
        return false;
    }
    HeardPhrase phrase = {chords, count, mode, CADENCE_TYPE_PERFECT_AUTHENTIC, metre.cells_per_bar, metre.cell_length, true, NULL, false, false, NULL, NULL, false, &metre};
    if (!harmony_judge(&phrase, &verdict)) {
        return false;
    }
    *violation = verdict.hard;
    return true;
}

/* Marks the chords of the lane over span that a schema made: they need no reason of their own to be there. */
static void mark_schema_chords(const PiecePlan *plan, PlanSpan span, size_t count, bool *exempt) {
    LaneSpanId ids[HARMONY_MAX_CHORDS];
    size_t found = plan_lane_find(plan, PLAN_LANE_HARMONY, 0, span, ids, HARMONY_MAX_CHORDS);
    for (size_t i = 0; i < count; i++) {
        exempt[i] = i < found && plan_lane_span_find_constraint(plan, ids[i], PLAN_CONSTRAINT_SCHEMA) != NULL;
    }
}

bool harmony_check_lane(const PiecePlan *plan, PlanSpan span, HarmonyMode mode, CadenceType requested,
                        const char **violation) {
    PhraseMetre metre;
    PhraseChord chords[HARMONY_MAX_CHORDS];
    bool exempt[HARMONY_MAX_CHORDS];
    size_t count;
    PhraseVerdict verdict;
    if (!violation || !harmony_read_lane(plan, span, mode, chords, &count, &metre)) {
        return false;
    }
    mark_schema_chords(plan, span, count, exempt);
    HeardPhrase phrase = {chords, count, mode, requested, metre.cells_per_bar, metre.cell_length, false, exempt, false, false, NULL, NULL, false, &metre};
    if (!harmony_judge(&phrase, &verdict)) {
        return false;
    }
    *violation = verdict.hard;
    return true;
}

bool harmony_hear_lane_cadences(const PiecePlan *plan, PlanSpan span, HarmonyMode mode, const int *soprano,
                                size_t count, const Rational *section_starts, size_t section_count,
                                HarmonyCadences *out) {
    PhraseMetre metre;
    PhraseChord chords[HARMONY_MAX_CHORDS];
    HarmonyMode modes[HARMONY_MAX_CHORDS];
    unsigned keys[HARMONY_MAX_CHORDS];
    size_t found;
    /* Each chord is read in the key under it, so a cadence is heard in the key of the chord it arrives on. */
    if (!soprano || !out || !harmony_read_lane_keys(plan, span, mode, chords, &found, &metre, modes, keys) ||
        found != count) {
        return false;
    }
    HeardPhrase phrase = {chords, count, mode, CADENCE_TYPE_PERFECT_AUTHENTIC, metre.cells_per_bar, metre.cell_length, true, NULL, false, false, modes, keys, false, &metre};
    SegmentSet *set = harmony_segments_create_heard(&phrase, soprano);
    if (!set) {
        return false;
    }
    size_t segment_count;
    const HarmonySegment *segments = harmony_segments_get(set, &segment_count);
    bool ok = harmony_analysis_find_cadences(segments, segment_count, NULL, out) == HARMONY_CADENCE_ANALYSIS_OK;
    if (ok && section_count > 0) {
        /* Ursatz counts the starts in whole notes from the start of the range heard. */
        Rational starts[HARMONY_MAX_CHORDS];
        size_t used = 0;
        for (size_t i = 0; i < section_count && used < HARMONY_MAX_CHORDS; i++) {
            if (rational_sub(section_starts[i], span.start, &starts[used]) != RATIONAL_OK) {
                break;
            }
            used++;
        }
        if (used < section_count && used < HARMONY_MAX_CHORDS) {
            harmony_cadences_destroy(out);
            ok = false;
        } else if (harmony_analysis_add_boundary_cadences(segments, segment_count, NULL, starts, used, out) !=
            HARMONY_CADENCE_ANALYSIS_OK) {
            harmony_cadences_destroy(out);
            ok = false;
        }
    }
    harmony_segments_destroy(set);
    return ok;
}

bool harmony_shape_keeps(const PlanHarmonyShape *shape, const size_t *cells, size_t count, size_t cells_per_bar,
                         bool exempt_last) {
    size_t start = 0;
    for (size_t i = 0; i < count; i++) {
        bool last = i + 1 == count;
        if (shape->max_cells_per_chord > 0 && cells[i] > (size_t)shape->max_cells_per_chord && !(last && exempt_last)) {
            return false;
        }
        if (shape->split_at_bars && start % cells_per_bar + cells[i] > cells_per_bar) {
            return false;
        }
        start += cells[i];
    }
    return true;
}

bool melody_check_lane(const PiecePlan *plan, PlanSpan span, AnalysisKey key, const MelodyFillParams *params,
                       const char **violation) {
    LaneSpanId ids[HARMONY_MAX_CHORDS];
    MelodyCandidate *melody = calloc(1, sizeof(*melody));
    bool ok = melody && violation;
    size_t found = 0;
    Rational origin = span.start;
    Rational phrase_end;
    if (ok) {
        found = plan_lane_find(plan, PLAN_LANE_MELODY, 0, span, ids, HARMONY_MAX_CHORDS);
        ok = found > 0 && found <= HARMONY_MAX_CHORDS &&
             plan_span_end(span, &phrase_end) == PLAN_OK &&
             melody_phrase_read(plan, plan_lane_span_owner(plan, ids[0]), span, key, &melody->context);
    }
    if (melody) {
        melody->params = params ? *params : melody_fill_params_default();
        melody->magic = MELODY_PAYLOAD_MAGIC;
    }
    /* Every note of the melody lane over the phrase, from the start of the phrase. */
    for (size_t i = 0; ok && i < found; i++) {
        Rational base = plan_lane_span_span(plan, ids[i]).start;
        const PlanNotes *notes = &plan_lane_span_payload(plan, ids[i])->notes;
        Rational lane_end;
        PlanSpan lane_span = plan_lane_span_span(plan, ids[i]);
        /* A melody span may start before the phrase, with a pickup (PLAN_CONSTRAINT_PICKUP), and end where it ends. */
        ok = plan_span_end(lane_span, &lane_end) == PLAN_OK &&
             (plan_span_contains(span, lane_span) ||
              (rational_cmp(lane_span.start, span.start) < 0 && rational_cmp(lane_end, phrase_end) == 0));
        size_t first_note = melody->count;
        for (size_t n = 0; ok && n < notes->note_count; n++) {
            MelodyNote *note = &melody->notes[melody->count];
            Rational onset = position_add(position_sub(base, origin), notes->notes[n].offset);
            ok = melody->count < MELODY_MAX_NOTES;
            note->duration = notes->notes[n].duration;
            if (ok && rational_cmp(onset, POSITION_ZERO) < 0) {
                /* A note before the phrase is the pickup. */
                MelodyNote *pickup = &melody->pickup_notes[melody->pickup_count];
                ok = melody->pickup_count < MELODY_MAX_PICKUP;
                pickup->onset = onset;
                pickup->duration = note->duration;
                pickup->pitch = notes->notes[n].midi;
                pickup->motive_index = -1;
                melody->pickup_count += ok;
            } else if (ok) {
                note->onset = onset;
                note->pitch = notes->notes[n].midi;
                note->motive_index = -1;
                note->strong = melody_on_cell(&melody->context, note->onset);
                note->chord = melody_chord_at(&melody->context, note->onset);
                melody->count++;
            }
        }
        /* The motive the span says (its motive_ref) is what the checker held it to: its first notes are the motive's. */
        for (size_t c = 0; ok && c < plan_lane_span_constraint_count(plan, ids[i]); c++) {
            const PlanConstraint *constraint = plan_lane_span_constraint_at(plan, ids[i], c);
            LaneSpanId motive_span;
            if (constraint->kind != PLAN_CONSTRAINT_MOTIVE_REF || !plan_has_node(plan, constraint->params.motive.motive) ||
                !plan_melody_span_of(plan, constraint->params.motive.motive, &motive_span)) {
                continue;
            }
            const PlanNotes *motive = &plan_lane_span_payload(plan, motive_span)->notes;
            if (motive->note_count == 0 || motive->note_count > MOTIVE_MAX_NOTES || melody->count - first_note < motive->note_count) {
                continue;
            }
            melody->context.motive_count = motive->note_count;
            for (size_t k = 0; k < motive->note_count; k++) {
                melody->context.motive_pitch[k] = motive->notes[k].midi;
                melody->notes[first_note + k].motive_index = (int)k;
            }
            melody->context.motive_waived |= melody_motive_waived(&melody->context);
            /* A motive's own figures are the framework's to name: every type is allowed in a melody that carries one. */
            melody->params.nct_types |= MELODY_NCT_PASSING | MELODY_NCT_NEIGHBOR | MELODY_NCT_APPOGGIATURA |
                                        MELODY_NCT_SUSPENSION | MELODY_NCT_ANTICIPATION | MELODY_NCT_RETARDATION |
                                        MELODY_NCT_ESCAPE | MELODY_NCT_DOUBLE_NEIGHBOR | MELODY_NCT_CHANGING_TONE;
        }
    }
    PhraseVerdict verdict;
    ok = ok && melody_judge(melody, &verdict);
    /* A lane that carries a motive and fails the strict rules passes when the relaxed rules hold (R-338): the melody
       operator composes it that way when no strict attempt does. */
    if (ok && verdict.hard) {
        bool carries = false;
        for (size_t i = 0; i < melody->count; i++) {
            carries = carries || melody->notes[i].motive_index >= 0;
        }
        if (carries) {
            melody->relaxed = true;
            ok = melody_judge(melody, &verdict);
        }
    }
    if (ok) {
        *violation = verdict.hard;
    }
    free(melody);
    return ok;
}

/* ------------------------------------------------------------- the check */

static bool fault(char *reason, size_t size, size_t cell, const char *what) {
    if (reason && size > 0) {
        snprintf(reason, size, "cell %zu: %s", cell, what);
    }
    return false;
}

/* Whether a step from one pitch to the next is a step of the figure: 1 or 2 semitones, 3 in minor (the augmented second). */
static bool is_scale_step(const AnalysisKey *key, int from, int to) {
    int move = util_absolute(to - from);
    return (move >= 1 && move <= 2) || (move == 3 && composer_key_is_minor(key));
}

bool form_figure_check(const PiecePlan *plan, NodeId section, char *reason, size_t size) {
    PlanFigure figure;
    FigureReading *reading = plan && form_figure_find(plan, section, &figure) ? form_figure_read_section(plan, section) : NULL;
    size_t own_count = 0;
    FigureSourceNote *own = reading ? form_figure_read_melody(plan, section, 0, false, 0, NULL, &own_count) : NULL;
    if (!reading || !own) {
        free(own);
        form_figure_reading_free(reading);
        return fault(reason, size, 0, "the section has no skeleton or no melody");
    }
    const FormSkeleton *skeleton = &reading->skeleton;
    Rational length;
    size_t per = form_figure_notes_per_cell(reading, figure.rhythm, &length);
    FigureNote *expected = calloc(reading->note_count + 2, sizeof(FigureNote));
    bool ok = per > 0 && expected != NULL;
    int octave = 0;
    /* A raised climax: the first note of the first cell that is not kept is the skeleton tone an octave up. */
    for (size_t k = 0; ok && figure.climax && k < skeleton->count; k++) {
        if (skeleton->kept[k]) {
            continue;
        }
        for (size_t n = 0; n < own_count; n++) {
            if (rational_cmp(own[n].onset, skeleton->onset[k]) == 0) {
                octave = own[n].pitch == skeleton->pitch[k] + 12 ? 12 : 0;
            }
        }
        break;
    }
    ok = ok || fault(reason, size, 0, "the figure's note length does not divide the cell");
    for (size_t k = 0; ok && k < skeleton->count; k++) {
        Rational start;
        Rational end;
        const FigureSourceNote *cell_notes[8 + FORM_SKELETON_MAX];
        size_t found = 0;
        ok = form_figure_cell_bounds(reading, k, &start, &end);
        for (size_t n = 0; ok && n < own_count; n++) {
            if (rational_cmp(own[n].onset, start) >= 0 && rational_cmp(own[n].onset, end) < 0 && found < 8 + FORM_SKELETON_MAX) {
                cell_notes[found++] = &own[n];
            }
        }
        if (skeleton->kept[k]) {
            size_t held = 0;
            ok = form_figure_kept_notes(reading, k, octave, expected, &held);
            if (ok && held != found) {
                ok = fault(reason, size, k, "a kept cell does not hold the returned notes");
            }
            for (size_t n = 0; ok && n < held; n++) {
                Rational duration;
                ok = rational_sub(cell_notes[n]->end, cell_notes[n]->onset, &duration) == RATIONAL_OK &&
                     cell_notes[n]->pitch == expected[n].pitch && rational_cmp(cell_notes[n]->onset, expected[n].onset) == 0 &&
                     rational_cmp(duration, expected[n].duration) == 0;
                ok = ok || fault(reason, size, k, "a kept cell does not hold the returned notes");
            }
            continue;
        }
        int tone = skeleton->pitch[k] + octave;
        if (found != per) {
            ok = fault(reason, size, k, "the cell does not hold the notes of the figure");
            break;
        }
        for (size_t i = 0; ok && i < per; i++) {
            Rational at;
            Rational duration;
            ok = rational_mul_int(length, (long long)i, &at) == RATIONAL_OK && rational_add(start, at, &at) == RATIONAL_OK &&
                 rational_cmp(cell_notes[i]->onset, at) == 0 &&
                 rational_sub(cell_notes[i]->end, cell_notes[i]->onset, &duration) == RATIONAL_OK &&
                 rational_cmp(duration, length) == 0;
            ok = ok || fault(reason, size, k, "the notes of the cell are not equal");
            if (!ok) {
                break;
            }
            int pitch = cell_notes[i]->pitch;
            if (i == 0 && pitch != tone) {
                ok = fault(reason, size, k, "the cell does not start on the skeleton tone");
            } else if (figure.kind == PLAN_FIGURE_NEIGHBOR && i % 2 == 0 && pitch != tone) {
                ok = fault(reason, size, k, "a neighbor figure leaves the skeleton tone on an even note");
            } else if (figure.kind == PLAN_FIGURE_NEIGHBOR && i % 2 == 1 &&
                       (util_absolute(pitch - tone) < 1 || util_absolute(pitch - tone) > 2)) {
                ok = fault(reason, size, k, "a neighbor is not a step from the skeleton tone");
            } else if (figure.kind == PLAN_FIGURE_SCALE && i > 0 && !is_scale_step(&reading->key, cell_notes[i - 1]->pitch, pitch)) {
                ok = fault(reason, size, k, "a scale figure moves by more than a step");
            } else if (figure.kind == PLAN_FIGURE_BROKEN_CHORD && !form_figure_has_pitch_class(reading->mask[k], pitch)) {
                ok = fault(reason, size, k, "a broken chord holds a note that is not a tone of the chord");
            }
        }
    }
    free(expected);
    free(own);
    form_figure_reading_free(reading);
    return ok;
}
