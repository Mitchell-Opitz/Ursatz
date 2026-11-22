#include "analysis/harmony/analysis_resolution.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "analysis/harmony/analysis_runs.h"
#include "model/core/value.h"
#include "theory/harmony/harmony_steps.h"
#include "model/identity/entity_id.h"
#include "model/pitch/midi_pitch.h"
#include "theory/tonality/tonal_key.h"

HarmonyResolutionConfig harmony_resolution_config_default(void) {
    HarmonyResolutionConfig config;
    config.emit_satisfied = false;
    return config;
}

/* The semitone of a present cell, or false (*out untouched) for a cell with no usable pitch (no pitch and no spelling, or a
   position that is not a whole semitone). */
static bool harmony_voice_cell_semitone(const HarmonyVoiceCell *cell, long *out) {
    if (cell == NULL || out == NULL) {
        return false;
    }
    if (cell->pitch != NULL) {
        if (pitch_numeric_value_position_denominator(cell->pitch) != 1) {
            return false;
        }
        *out = pitch_numeric_value_position_numerator(cell->pitch);
        return true;
    }
    if (cell->spelling != NULL) {
        *out = pitch_spelling_midi(cell->spelling);
        return true;
    }
    return false;
}

static int pitch_class(long semitone) {
    return pitch_class_of_midi((int)semitone);
}

/* A move of `delta` semitones as diatonic steps: 1 or 2 semitones a step, 3 or 4 a third, and so on. */
static int steps_of(long delta) {
    long magnitude = delta < 0 ? -delta : delta;
    int degree = 1;
    int alteration = 0;
    standard_tonality_natural_degree_of_semitone((int)(magnitude % 12), &degree, &alteration);
    int steps = (int)(magnitude / 12) * 7 + degree - 1;
    return delta < 0 ? -steps : steps;
}

void harmony_resolutions_destroy(HarmonyResolutions *resolutions) {
    if (resolutions == NULL) {
        return;
    }
    free(resolutions->items);
    resolutions->items = NULL;
    resolutions->count = 0;
}

/* The member of the chord (index into its interval set) a pitch class is, or -1. */
static int member_of(const HarmonySegmentInput *input, int root_pc, int pc) {
    for (size_t m = 0; m < input->interval_count; m++) {
        if (pitch_class(root_pc + input->intervals[m]) == pc) {
            return (int)m;
        }
    }
    return -1;
}

static bool validate_grid(const HarmonyVoiceGrid *grid, size_t count) {
    if (grid->segment_count != count || grid->voice_count == 0 || (count > 0 && grid->cells == NULL)) {
        return false;
    }
    for (size_t i = 0; i < count * grid->voice_count; i++) {
        const HarmonyVoiceCell *cell = &grid->cells[i];
        long semitone;
        if (cell->present && (cell->note_id == NULL || cell->note_id[0] == '\0' ||
                              !(cell->confidence >= 0.0 && cell->confidence <= 1.0) ||
                              !harmony_voice_cell_semitone(cell, &semitone))) {
            return false;
        }
    }
    return true;
}

typedef struct {
    const HarmonySegment *segments;
    const HarmonyVoiceGrid *grid;
    const HarmonyResolutionConfig *config;
    HarmonyResolutions *found;
} ResolutionContext;

static const HarmonyVoiceCell *cell_at(const HarmonyVoiceGrid *grid, size_t segment, size_t voice) {
    const HarmonyVoiceCell *cell = &grid->cells[segment * grid->voice_count + voice];
    return cell->present ? cell : NULL;
}

static bool emit(const ResolutionContext *ctx, HarmonyResolutionRule rule, size_t voice, size_t from, size_t to, bool satisfied,
                 const HarmonyVoiceCell *a, const HarmonyVoiceCell *b) {
    HarmonyResolutionResult *result = &ctx->found->items[ctx->found->count];
    result->rule = rule;
    result->voice = voice;
    result->segment_from = from;
    result->segment_to = to;
    result->satisfied = satisfied;
    double least = a->confidence < b->confidence ? a->confidence : b->confidence;
    double chord = ctx->segments[from].input->uncertainty > ctx->segments[to].input->uncertainty
                       ? ctx->segments[from].input->uncertainty
                       : ctx->segments[to].input->uncertainty;
    result->uncertainty = 1.0 - (1.0 - (1.0 - least)) * (1.0 - chord);
    ctx->found->count++;
    return true;
}

/* Judges the voices at one chord change. false only on allocation failure. */
static bool judge(const ResolutionContext *ctx, size_t from, size_t to) {
    const HarmonySegment *from_segment = &ctx->segments[from];
    const HarmonySegment *to_segment = &ctx->segments[to];
    const HarmonyChord *chord = &from_segment->chord;
    const HarmonySegmentInput *input = from_segment->input;
    bool dominant = chord->degree == 5 && input->intervals[1] == 4;
    bool leading_tone_chord = chord->degree == 7 && input->interval_count >= 3 && input->intervals[1] == 3 &&
                              input->intervals[2] == 6;
    if (!(chord->seventh || dominant || leading_tone_chord)) {
        return true;
    }
    const HarmonyVoiceGrid *grid = ctx->grid;
    const HarmonyVoiceCell *bass = cell_at(grid, from, grid->voice_count - 1);
    long bass_semitone;
    if (bass == NULL || !harmony_voice_cell_semitone(bass, &bass_semitone) || input->inversion < 0 ||
        (size_t)input->inversion >= input->interval_count) {
        return true; /* no bass to find the root from */
    }
    int root_pc = pitch_class(bass_semitone - input->intervals[input->inversion]);

    bool present[4] = {false, false, false, false};
    for (size_t v = 0; v < grid->voice_count; v++) {
        const HarmonyVoiceCell *cell = cell_at(grid, from, v);
        long semitone;
        if (cell != NULL && harmony_voice_cell_semitone(cell, &semitone)) {
            int member = member_of(input, root_pc, pitch_class(semitone));
            if (member >= 0 && member < 4) {
                present[member] = true;
            }
        }
    }
    bool complete = chord->degree == 5 && chord->seventh && present[0] && present[1] && present[2] && present[3];

    /* the next chord: its own when it is a diatonic chord, else a stand-in that is not a tonic */
    HarmonyChord next = {2, 0, false, false, false, false};
    if (to_segment->valid && !to_segment->applied) {
        next = to_segment->chord;
    }

    for (size_t v = 0; v < grid->voice_count; v++) {
        const HarmonyVoiceCell *a = cell_at(grid, from, v);
        const HarmonyVoiceCell *b = cell_at(grid, to, v);
        long from_semitone, to_semitone;
        if (a == NULL || b == NULL || !harmony_voice_cell_semitone(a, &from_semitone) ||
            !harmony_voice_cell_semitone(b, &to_semitone)) {
            continue;
        }
        int member = member_of(input, root_pc, pitch_class(from_semitone));
        if (member < 0) {
            continue;
        }
        HarmonyVoiceResolution subject;
        subject.mode = input->mode;
        subject.from = *chord;
        subject.to = next;
        subject.outer = v == 0 || v == grid->voice_count - 1;
        subject.complete_dominant_seventh = complete;
        int steps = steps_of(to_semitone - from_semitone);

        /* the seventh: its diatonic step in the chord is the seventh degree */
        if (chord->seventh && member == 3) {
            subject.from_step = 70 + ((chord->degree - 1 + 6) % 7);
            subject.to_step = subject.from_step + steps;
            if (harmony_resolution_rule_applies(HARMONY_RESOLUTION_SEVENTH_RESOLVES_DOWN, &subject)) {
                bool satisfied = harmony_resolution_rule_holds(HARMONY_RESOLUTION_SEVENTH_RESOLVES_DOWN, &subject);
                if ((!satisfied || ctx->config->emit_satisfied) &&
                    !emit(ctx, HARMONY_RESOLUTION_SEVENTH_RESOLVES_DOWN, v, from, to, satisfied, a, b)) {
                    return false;
                }
            }
        }
        /* the leading tone: the third of a V, the root of a vii-diminished; its step is the seventh degree of the key */
        if ((dominant && member == 1) || (leading_tone_chord && member == 0)) {
            subject.from_step = 70 + 6;
            subject.to_step = subject.from_step + steps;
            if (harmony_resolution_rule_applies(HARMONY_RESOLUTION_LEADING_TONE_RESOLVES, &subject)) {
                bool satisfied = harmony_resolution_rule_holds(HARMONY_RESOLUTION_LEADING_TONE_RESOLVES, &subject);
                if ((!satisfied || ctx->config->emit_satisfied) &&
                    !emit(ctx, HARMONY_RESOLUTION_LEADING_TONE_RESOLVES, v, from, to, satisfied, a, b)) {
                    return false;
                }
            }
        }
    }
    return true;
}

HarmonyResolutionStatus harmony_analysis_check_resolutions(const HarmonySegment *segments, size_t count,
                                                           const HarmonyVoiceGrid *grid,
                                                           const HarmonyResolutionConfig *config,
                                                           HarmonyResolutions *out) {
    if (out == NULL || grid == NULL || (segments == NULL && count > 0)) {
        return HARMONY_RESOLUTION_ERR_NULL_ARGUMENT;
    }
    if (!validate_grid(grid, count)) {
        return HARMONY_RESOLUTION_ERR_GRID_MISMATCH;
    }
    HarmonyResolutionConfig defaults = harmony_resolution_config_default();
    if (config == NULL) {
        config = &defaults;
    }
    HarmonyRun *runs = (HarmonyRun *)calloc(count > 0 ? count : 1, sizeof(HarmonyRun));
    /* at most two results (the seventh and the leading tone) per voice per chord change */
    size_t capacity = count > 1 ? (count - 1) * grid->voice_count * 2 : 0;
    HarmonyResolutions found = {NULL, 0};
    if (capacity > 0) {
        found.items = (HarmonyResolutionResult *)calloc(capacity, sizeof(HarmonyResolutionResult));
    }
    if (runs == NULL || (capacity > 0 && found.items == NULL)) {
        free(runs);
        free(found.items);
        return HARMONY_RESOLUTION_ERR_ALLOC;
    }
    size_t run_count = harmony_analysis_build_runs(segments, count, runs);
    ResolutionContext ctx = {segments, grid, config, &found};
    for (size_t r = 0; r + 1 < run_count; r++) {
        size_t from = runs[r].last;
        size_t to = runs[r + 1].first;
        if (!segments[from].valid || segments[from].applied) {
            continue;
        }
        if (!judge(&ctx, from, to)) {
            harmony_resolutions_destroy(&found);
            free(runs);
            return HARMONY_RESOLUTION_ERR_ALLOC;
        }
    }
    free(runs);
    if (found.count == 0) {
        free(found.items);
        found.items = NULL;
    }
    *out = found;
    return HARMONY_RESOLUTION_OK;
}

