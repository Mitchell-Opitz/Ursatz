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
    out->metre = phrase->metre;
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
    KeyStructureMode mode = phrase->mode;
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
    KeyStructureMode mode = phrase->mode;
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

/*
 * Predominants. The marked chord is a strong predominant (IV or ii) in root position. It is replaced
 * by another way of writing a strong predominant (ii6, a predominant seventh, IV or ii), or extended
 * into IV followed by ii in one of those forms (ii never comes before IV).
 */
typedef struct {
    int degree;
    int inversion;
    bool seventh;
} Form;

/* In a minor key ii is diminished and is not written in root position. */
static const Form MAJOR_FORMS[] = {{2, 1, false}, {2, 1, true}, {2, 0, true}, {2, 2, true},
                                   {4, 0, true},  {2, 0, false}, {4, 0, false}};
static const Form MINOR_FORMS[] = {{2, 1, false}, {2, 1, true}, {4, 0, true}, {4, 0, false}};
/* The forms of ii that may follow IV. */
static const Form MAJOR_AFTER_IV[] = {{2, 1, false}, {2, 1, true}, {2, 0, true}, {2, 2, true}};
static const Form MINOR_AFTER_IV[] = {{2, 1, false}, {2, 1, true}};

static bool same_numeral(const PhraseChord *a, const PhraseChord *b) {
    return strcmp(a->roman, b->roman) == 0;
}

void harmony_options_predominant(const PhraseContext *phrase, FillRng *rng, OptionList *list) {
    size_t t = phrase->target;
    const PhraseChord *chord = &phrase->chords[t];
    KeyStructureMode mode = phrase->mode;
    size_t n = chord->cells;
    if (chord->applied || chord->chord.seventh || chord->chord.inversion != 0 ||
        (chord->chord.degree != 4 && chord->chord.degree != 2) || t + 1 == phrase->count) {
        return;
    }
    bool minor = mode == KEY_STRUCTURE_MODE_MINOR;
    const Form *forms = minor ? MINOR_FORMS : MAJOR_FORMS;
    size_t form_count = minor ? sizeof(MINOR_FORMS) / sizeof(MINOR_FORMS[0]) : sizeof(MAJOR_FORMS) / sizeof(MAJOR_FORMS[0]);
    const PhraseChord *before = t > 0 ? &phrase->chords[t - 1] : NULL;
    const PhraseChord *after = &phrase->chords[t + 1];

    for (size_t f = 0; f < form_count; f++) {
        Edit edit;
        memset(&edit, 0, sizeof(edit));
        edit.piece_count = 1;
        if (harmony_make_chord(mode, forms[f].degree, forms[f].inversion, forms[f].seventh, false, n,
                               &edit.pieces[0]) &&
            !same_numeral(&edit.pieces[0], chord) && !(before && same_numeral(&edit.pieces[0], before)) &&
            !same_numeral(&edit.pieces[0], after)) {
            harmony_options_add(list, phrase, &edit);
        }
    }

    if (n < 2) {
        return;
    }
    const Form *seconds = minor ? MINOR_AFTER_IV : MAJOR_AFTER_IV;
    size_t second_count = minor ? sizeof(MINOR_AFTER_IV) / sizeof(MINOR_AFTER_IV[0])
                                : sizeof(MAJOR_AFTER_IV) / sizeof(MAJOR_AFTER_IV[0]);
    for (size_t f = 0; f < second_count; f++) {
        size_t firsts[2];
        size_t picked = harmony_options_pick(rng, 1, n - 1, firsts);
        for (size_t k = 0; k < picked; k++) {
            Edit edit;
            memset(&edit, 0, sizeof(edit));
            edit.piece_count = 2;
            if (harmony_make_chord(mode, 4, 0, false, false, firsts[k], &edit.pieces[0]) &&
                harmony_make_chord(mode, seconds[f].degree, seconds[f].inversion, seconds[f].seventh, false,
                                   n - firsts[k], &edit.pieces[1]) &&
                !(before && same_numeral(&edit.pieces[0], before)) && !same_numeral(&edit.pieces[1], after)) {
                harmony_options_add(list, phrase, &edit);
            }
        }
    }
}

/*
 * Applied chords. An applied dominant or leading-tone chord is put before the marked chord, taking
 * one cell from the end of the chord before it, and leads into it as its target (the marked chord
 * must be one standard-harmony can tonicize in the mode: never the tonic).
 */
static const HarmonyAppliedQuality APPLIED_QUALITIES[] = {
    HARMONY_APPLIED_MAJOR_TRIAD, HARMONY_APPLIED_DOMINANT_SEVENTH, HARMONY_APPLIED_DIMINISHED_TRIAD,
    HARMONY_APPLIED_FULLY_DIMINISHED_SEVENTH};

void harmony_options_applied(const PiecePlan *plan, const PhraseContext *phrase, OptionList *list) {
    size_t t = phrase->target;
    const PhraseChord *target = &phrase->chords[t];
    KeyStructureMode mode = phrase->mode;
    if (t == 0 || t + 1 == phrase->count || target->applied || target->chord.inversion != 0 ||
        target->chord.cadential || !harmony_applied_target_valid(harmony_mode_of(mode), target->chord.degree)) {
        return;
    }
    const PhraseChord *before = &phrase->chords[t - 1];
    if (before->applied || before->cells < 2 || plan_lane_span_state(plan, phrase->ids[t - 1]) == PLAN_STATE_GIVEN) {
        return;
    }
    for (size_t q = 0; q < sizeof(APPLIED_QUALITIES) / sizeof(APPLIED_QUALITIES[0]); q++) {
        Edit edit;
        HarmonyAppliedChord applied;
        memset(&edit, 0, sizeof(edit));
        memset(&applied, 0, sizeof(applied));
        applied.quality = APPLIED_QUALITIES[q];
        applied.target = target->chord.degree;
        if (!harmony_applied_quality_kind(applied.quality, &applied.kind) ||
            !harmony_applied_valid(harmony_mode_of(mode), &applied)) {
            continue;
        }
        PhraseChord *lead = &edit.lead[0];
        if (!harmony_roman_format_applied(mode, &applied, lead->roman)) {
            continue;
        }
        lead->applied = true;
        lead->applied_chord = applied;
        lead->function = HARMONIC_FUNCTION_DOMINANT;
        lead->has_function = true;
        lead->cells = 1;
        edit.steal_cells = 1;
        edit.lead_count = 1;
        edit.piece_count = 1;
        edit.pieces[0] = *target;
        harmony_options_add(list, phrase, &edit);
    }
}
