#include "harmony/harmony_internal.h"

#include <string.h>

#include <ursatz_composer/plan_state.h>

/*
 * The shapes of the embellishments. A shape only says which chords go where; whether standard-harmony
 * accepts it in this phrase is for the checker to say (harmony_judge), so a shape need not know the
 * meter, the beat strengths or the neighbors it sits between.
 */

void harmony_options_add(OptionList *list, const PhraseContext *phrase, const Edit *edit) {
    size_t added = edit->lead_count + edit->piece_count - 1;
    if (list->count >= OPTIONS_MAX || phrase->count + added > HARMONY_MAX_CHORDS) {
        return;
    }
    list->items[list->count++] = *edit;
}

size_t harmony_options_pick(FillRng *rng, size_t low, size_t high, size_t out[2]) {
    if (high < low) {
        return 0;
    }
    size_t span = high - low + 1;
    if (span <= 2) {
        out[0] = low;
        out[1] = high;
        return span;
    }
    out[0] = low + (size_t)fill_rng_below(rng, span);
    out[1] = low + (size_t)fill_rng_below(rng, span - 1);
    if (out[1] >= out[0]) {
        out[1]++;
    }
    return 2;
}

bool harmony_edit_compose(const PhraseContext *phrase, const Edit *edit, EmbellishCandidate *out) {
    size_t target = phrase->target;
    size_t count = 0;
    memset(out, 0, sizeof(*out));
    for (size_t i = 0; i < phrase->count; i++) {
        if (i == target) {
            for (size_t k = 0; k < edit->piece_count; k++) {
                out->chords[count++] = edit->pieces[k];
            }
        } else {
            out->chords[count] = phrase->chords[i];
            if (i + 1 == target && edit->steal_cells > 0) {
                if (out->chords[count].cells <= edit->steal_cells) {
                    return false;
                }
                out->chords[count].cells -= edit->steal_cells;
                count++;
                for (size_t k = 0; k < edit->lead_count; k++) {
                    out->chords[count++] = edit->lead[k];
                }
            } else {
                count++;
            }
        }
    }
    out->edit = *edit;
    out->target_cells = phrase->chords[target].cells;
    out->mode = phrase->mode;
    out->cadence = phrase->cadence;
    out->cells_per_bar = phrase->metre.cells_per_bar;
    out->cell_length = phrase->metre.cell_length;
    out->count = count;
    return true;
}

/* The marked chord is a plain root-position triad or seventh on this degree. */
static bool is_plain(const PhraseChord *chord, int degree) {
    return !chord->applied && chord->chord.degree == degree && chord->chord.inversion == 0;
}

static void with_cells(const PhraseChord *chord, size_t cells, PhraseChord *out) {
    *out = *chord;
    out->cells = cells;
}

/*
 * Tonic prolongation: the marked tonic is split into three chords, the first and last the tonic (or
 * its first inversion) and the middle one a chord that standard-harmony has a reason for between
 * them. The middle chord takes one cell.
 */
static void tonic_options(const PhraseContext *phrase, FillRng *rng, OptionList *list) {
    const PhraseChord *tonic = &phrase->chords[phrase->target];
    PlanKeyMode mode = phrase->mode;
    size_t n = tonic->cells;
    if (!is_plain(tonic, 1) || tonic->chord.seventh || phrase->target + 1 == phrase->count || n < 3) {
        return;
    }
    /* Middle chord (degree, inversion, seventh) and the inversion of the last tonic. */
    static const struct {
        int degree;
        int inversion;
        bool seventh;
        int last_inversion;
    } SHAPES[] = {
        {5, 1, false, 0}, /* I V6 I        a neighbor bass, do ti do */
        {5, 1, true, 0},  /* I V6/5 I      the same with the seventh */
        {5, 2, true, 1},  /* I V4/3 I6     do re mi */
        {5, 3, true, 1},  /* I V4/2 I6     do fa mi */
        {4, 2, false, 0}, /* I IV6/4 I     the neighbor (pedal) 6/4 */
        {5, 2, false, 1}, /* I V6/4 I6     the passing 6/4 */
    };
    for (size_t s = 0; s < sizeof(SHAPES) / sizeof(SHAPES[0]); s++) {
        size_t firsts[2];
        size_t picked = harmony_options_pick(rng, 1, n - 2, firsts);
        for (size_t k = 0; k < picked; k++) {
            Edit edit;
            memset(&edit, 0, sizeof(edit));
            edit.piece_count = 3;
            with_cells(tonic, firsts[k], &edit.pieces[0]);
            if (!harmony_make_chord(mode, SHAPES[s].degree, SHAPES[s].inversion, SHAPES[s].seventh, false, 1,
                                    &edit.pieces[1]) ||
                !harmony_make_chord(mode, 1, SHAPES[s].last_inversion, false, false, n - firsts[k] - 1,
                                    &edit.pieces[2])) {
                continue;
            }
            harmony_options_add(list, phrase, &edit);
        }
    }
}

/*
 * Dominant prolongation and the cadential 6/4. The marked V (or V7) either is split around a neighbor
 * I6/4 (V I6/4 V), or has the cadential I6/4 put before it, taken from its own first cells or from
 * the end of the chord before it.
 */
static void dominant_options(const PiecePlan *plan, const PhraseContext *phrase, FillRng *rng, OptionList *list) {
    const PhraseChord *dominant = &phrase->chords[phrase->target];
    PlanKeyMode mode = phrase->mode;
    size_t n = dominant->cells;
    size_t t = phrase->target;
    bool last = t + 1 == phrase->count;
    if (!is_plain(dominant, 5)) {
        return;
    }

    if (!last && n >= 3) {
        size_t firsts[2];
        size_t picked = harmony_options_pick(rng, 1, n - 2, firsts);
        for (size_t k = 0; k < picked; k++) {
            Edit edit;
            memset(&edit, 0, sizeof(edit));
            edit.piece_count = 3;
            with_cells(dominant, firsts[k], &edit.pieces[0]);
            with_cells(dominant, n - firsts[k] - 1, &edit.pieces[2]);
            if (harmony_make_chord(mode, 1, 2, false, false, 1, &edit.pieces[1])) {
                harmony_options_add(list, phrase, &edit);
            }
        }
    }

    if (n >= 2) {
        size_t firsts[2];
        size_t picked = harmony_options_pick(rng, 1, n - 1, firsts);
        for (size_t k = 0; k < picked; k++) {
            Edit edit;
            memset(&edit, 0, sizeof(edit));
            edit.piece_count = 2;
            if (harmony_make_chord(mode, 1, 2, false, true, firsts[k], &edit.pieces[0])) {
                with_cells(dominant, n - firsts[k], &edit.pieces[1]);
                harmony_options_add(list, phrase, &edit);
            }
        }
    }

    if (t > 0 && phrase->chords[t - 1].cells >= 2 &&
        plan_lane_span_state(plan, phrase->ids[t - 1]) != PLAN_STATE_GIVEN) {
        Edit edit;
        memset(&edit, 0, sizeof(edit));
        edit.steal_cells = 1;
        edit.lead_count = 1;
        edit.piece_count = 1;
        edit.pieces[0] = *dominant;
        if (harmony_make_chord(mode, 1, 2, false, true, 1, &edit.lead[0])) {
            harmony_options_add(list, phrase, &edit);
        }
    }
}

void harmony_options_prolongation(const PiecePlan *plan, const PhraseContext *phrase, FillRng *rng, OptionList *list) {
    tonic_options(phrase, rng, list);
    dominant_options(plan, phrase, rng, list);
}
