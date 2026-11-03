#include <ursatz_composer/harmony_bass.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <standard-counterpoint/counterpoint_limits.h>
#include <standard-counterpoint/counterpoint_line.h>
#include <standard-counterpoint/melodic_rules.h>
#include <pitch/pitch_spelling.h>

#include <ursatz_composer/plan_state.h>

#include "harmony/harmony_internal.h"
#include "melody/melody_internal.h"
#include "util/util.h"

/* The pitch class of each letter's natural note, C D E F G A B. */
static const int NATURAL_PC[7] = {0, 2, 4, 5, 7, 9, 11};
static const PitchLetter LETTERS[7] = {PITCH_LETTER_C, PITCH_LETTER_D, PITCH_LETTER_E, PITCH_LETTER_F,
                                       PITCH_LETTER_G, PITCH_LETTER_A, PITCH_LETTER_B};

/* The place of a letter in the order C D E F G A B. */
static int letter_index(PitchLetter letter) {
    for (int i = 0; i < 7; i++) {
        if (LETTERS[i] == letter) {
            return i;
        }
    }
    return 0;
}

/* The scale degree of the bass member of the chord, which names the letter of its note. */
int harmony_member_degree(KeyStructureMode mode, const PhraseChord *chord) {
    if (chord->applied) {
        return (harmony_applied_root_degree(harmony_mode_of(mode), &chord->applied_chord) - 1 +
                2 * chord->applied_inversion) % 7 + 1;
    }
    return (chord->chord.degree - 1 + 2 * chord->chord.inversion) % 7 + 1;
}

/* Spells a MIDI pitch as the given scale degree of the key; NULL when it cannot be spelled. */
PitchSpelling *harmony_spell_pitch(const AnalysisKey *key, int degree, int pitch) {
    if (degree < 1 || degree > 7) {
        return NULL;
    }
    int letter = (letter_index(key->tonic_letter) + degree - 1) % 7;
    int pc = ((pitch % 12) + 12) % 12;
    int accidental = ((pc - NATURAL_PC[letter] + 6) % 12 + 12) % 12 - 6;
    int octave = (pitch - accidental) / 12 - 1;
    PitchSpelling *spelling = NULL;
    if (pitch_spelling_create(LETTERS[letter], accidental, octave, &spelling) != PITCH_SPELLING_OK) {
        return NULL;
    }
    return spelling;
}

PitchSpelling *harmony_spell_tonic(const AnalysisKey *key, int octave) {
    PitchSpelling *spelling = NULL;
    if (pitch_spelling_create(key->tonic_letter, key->tonic_accidental, octave, &spelling) != PITCH_SPELLING_OK) {
        return NULL;
    }
    return spelling;
}

bool harmony_bass_judge(const BassCandidate *line, PhraseVerdict *verdict) {
    memset(verdict, 0, sizeof(*verdict));
    verdict->soft_total = melody_melodic_soft_rules();
    if (line->note_count == 0 || line->note_count > BASS_MAX_NOTES) {
        verdict->hard = "no bass notes";
        return true;
    }
    const BassContext *context = &line->context;
    PitchSpelling *spellings[BASS_MAX_NOTES];
    size_t made = 0;
    bool ok = true;
    for (size_t i = 0; ok && !verdict->hard && i < line->note_count; i++) {
        const BassNote *note = &line->notes[i];
        int pc;
        if (note->chord >= context->count || !harmony_chord_bass_pc(&context->key, &context->chords[note->chord], &pc)) {
            verdict->hard = "bass note without a chord";
        } else if (note->pitch < line->params.range_low || note->pitch > line->params.range_high) {
            verdict->hard = "outside the bass range";
        } else if (((note->pitch % 12) + 12) % 12 != pc) {
            verdict->hard = "not the chord's bass member";
        } else {
            spellings[made] = harmony_spell_pitch(&context->key, harmony_member_degree(context->key.mode, &context->chords[note->chord]),
                                    note->pitch);
            if (!spellings[made]) {
                ok = false;
            } else {
                made++;
            }
        }
    }
    if (ok && !verdict->hard) {
        StandardCounterpointLine melody = {(const PitchSpelling *const *)spellings, made};
        StandardCounterpointLimits limits = standard_counterpoint_default_limits();
        unsigned hard = 0;
        unsigned soft = 0;
        if (!standard_counterpoint_melodic_check(&melody, &limits, &hard, &soft)) {
            ok = false;
        } else {
            /* The last note of a bass that outlines a cadence is reached by a leap (V to I), which the
               rule for the last note of a strict counterpoint line forbids. */
            hard &= ~(1u << STANDARD_COUNTERPOINT_MELODIC_RULE_CLAUSULA_VERA);
            for (int rule = 0; rule < STANDARD_COUNTERPOINT_MELODIC_RULE_COUNT; rule++) {
                if (hard & (1u << rule)) {
                    verdict->hard = standard_counterpoint_melodic_rule_name((StandardCounterpointMelodicRule)rule);
                    break;
                }
            }
            verdict->soft_held = verdict->soft_total - util_bits_set(soft);
        }
    }
    for (size_t i = 0; i < made; i++) {
        pitch_spelling_destroy(spellings[i]);
    }
    return ok;
}

bool harmony_bass_check(const void *payload, FillCheck *out) {
    const BassCandidate *line = payload;
    PhraseVerdict verdict;
    if (!line || !out) {
        return false;
    }
    memset(out, 0, sizeof(*out));
    out->score_denominator = 1;
    if (!harmony_bass_judge(line, &verdict)) {
        return false;
    }
    if (verdict.hard) {
        out->hard_violation = true;
        snprintf(out->reason, sizeof(out->reason), "%s", verdict.hard);
        return true;
    }
    out->score_numerator = verdict.soft_held;
    out->score_denominator = verdict.soft_total ? verdict.soft_total : 1;
    snprintf(out->reason, sizeof(out->reason), "melodic rules");
    return true;
}

bool harmony_check_bass(const PiecePlan *plan, PlanSpan span, AnalysisKey key, const HarmonyBassParams *params,
                        const char **violation) {
    BassCandidate *line = calloc(1, sizeof(*line));
    if (!line || !violation) {
        free(line);
        return false;
    }
    line->params = params ? *params : harmony_bass_params_default();
    BassContext *context = &line->context;
    context->span = span;
    context->key = key;
    bool ok = harmony_read_lane(plan, span, key.mode, context->chords, &context->count, &context->metre);

    /* Each chord has one bass span of its own span, whose notes follow one another to its end. */
    for (size_t i = 0; ok && i < context->count; i++) {
        PlanSpan chord_span;
        LaneSpanId found[2];
        Rational offset = {0, 1};
        size_t cells = 0;
        for (size_t k = 0; ok && k < i; k++) {
            cells += context->chords[k].cells;
        }
        ok = ok && harmony_metre_cells_span(&context->metre, cells, context->chords[i].cells, &chord_span) == PLAN_OK;
        ok = ok && plan_lane_find(plan, PLAN_LANE_BASS, 0, chord_span, found, 2) == 1;
        if (ok) {
            PlanSpan bass_span = plan_lane_span_span(plan, found[0]);
            const PlanNotes *notes = &plan_lane_span_payload(plan, found[0])->notes;
            ok = rational_cmp(bass_span.start, chord_span.start) == 0 &&
                 rational_cmp(bass_span.length, chord_span.length) == 0;
            for (size_t n = 0; ok && n < notes->note_count && line->note_count < BASS_MAX_NOTES; n++) {
                size_t first_cell;
                size_t length_cells;
                PlanSpan note_span;
                /* A note lasts a whole number of cells of the metre, from the cell it starts on. */
                ok = rational_cmp(notes->notes[n].offset, offset) == 0 &&
                     rational_add(chord_span.start, offset, &note_span.start) == RATIONAL_OK;
                note_span.length = notes->notes[n].duration;
                ok = ok && harmony_metre_find_cells(&context->metre, note_span, &first_cell, &length_cells) &&
                     rational_add(offset, notes->notes[n].duration, &offset) == RATIONAL_OK;
                if (ok) {
                    line->notes[line->note_count].pitch = notes->notes[n].midi_pitch;
                    line->notes[line->note_count].cells = length_cells;
                    line->notes[line->note_count].chord = i;
                    line->note_count++;
                }
            }
            ok = ok && rational_cmp(offset, bass_span.length) == 0;
        }
    }
    PhraseVerdict verdict;
    ok = ok && harmony_bass_judge(line, &verdict);
    if (ok) {
        *violation = verdict.hard;
    }
    free(line);
    return ok;
}
