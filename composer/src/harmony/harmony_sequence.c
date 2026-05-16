#include <ursatz_composer/harmony_sequence.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "harmony/harmony_internal.h"

#define DEFAULT_MIN_REPEATS 2
#define DEFAULT_MAX_REPEATS 4
#define DEFAULT_PREFERENCE 250
#define PREFERENCE_LIMIT 1000000
/* The most stages a sequence is written with, and bass lines tried for each. */
#define MAX_STAGES 48
#define BASS_TRIES 4

HarmonySequenceParams harmony_sequence_params_default(void) {
    HarmonySequenceParams params = {HARMONY_SEQUENCE_ALL, DEFAULT_MIN_REPEATS, DEFAULT_MAX_REPEATS,
                                    DEFAULT_PREFERENCE, {0, 0, 0, 0}};
    params.bass = harmony_bass_params_default();
    return params;
}

static const HarmonySequenceParams *params_of(void *context) {
    static HarmonySequenceParams defaults;
    static bool made = false;
    if (!made) {
        defaults = harmony_sequence_params_default();
        made = true;
    }
    return context ? context : &defaults;
}

bool harmony_sequence_find(const char *name, ScalePatternId *id) {
    if (!name || !id) {
        return false;
    }
    for (int candidate = 0; candidate < SCALE_PATTERN_COUNT; candidate++) {
        if (strcmp(scale_pattern_name((ScalePatternId)candidate), name) == 0) {
            *id = (ScalePatternId)candidate;
            return true;
        }
    }
    return false;
}

/* The chord the upper voice's intervals make with the bass degree: a sixth or a third, a first inversion. */
static bool stage_chord(PlanKeyMode mode, int bass, unsigned intervals, PhraseChord *out) {
    bool first_inversion = (intervals & ((1u << 6) | (1u << 3))) != 0;
    bool root_position = (intervals & (1u << 5)) != 0 && !first_inversion;
    if (!first_inversion && !root_position) {
        return false;
    }
    /* In first inversion the bass is the third, so the root is a third below it. */
    int root = first_inversion ? ((bass - 1 - 2) % 7 + 7) % 7 + 1 : bass;
    return harmony_make_chord(mode, root, first_inversion ? 1 : 0, false, false, 1, out);
}

/* A diminished triad is not written in root position. */
static bool bad_root_position(PlanKeyMode mode, const PhraseChord *chord) {
    int intervals[4];
    size_t count;
    return chord->chord.inversion == 0 && harmony_diatonic_intervals(mode, &chord->chord, intervals, &count) &&
           count == 3 && intervals[1] == 3 && intervals[2] == 6;
}

/*
 * The chords of a pattern played for `repeats` units from the bass degree `start` in `direction` (+1 or
 * -1): at a stage where the bass moves it steps first, then the stage's chord sounds for a cell. A stage
 * that is the same bass and chord as the one before it (a suspension and its resolution) is one chord.
 */
static bool build_chords(PlanKeyMode mode, const ScalePattern *pattern, int direction, int start, size_t repeats,
                         PhraseChord *chords, size_t *count) {
    PhraseChord stages[MAX_STAGES];
    int basses[MAX_STAGES];
    size_t n = pattern->unit_length * repeats;
    int bass = start;
    if (n == 0 || n > MAX_STAGES) {
        return false;
    }
    for (size_t s = 0; s < n; s++) {
        const ScalePatternStage *stage = &pattern->unit[s % pattern->unit_length];
        if (s > 0 && stage->bass_moves) {
            bass = (bass - 1 + direction + 7) % 7 + 1;
        }
        basses[s] = bass;
        /* A suspended stage sounds the chord it resolves to. */
        const ScalePatternStage *chosen = stage->suspended ? &pattern->unit[(s + 1) % pattern->unit_length] : stage;
        if (!stage_chord(mode, bass, chosen->intervals, &stages[s]) || bad_root_position(mode, &stages[s])) {
            return false;
        }
    }
    size_t used = 0;
    for (size_t s = 0; s < n; s++) {
        if (s > 0 && basses[s] == basses[s - 1] && strcmp(stages[s].roman, stages[s - 1].roman) == 0) {
            chords[used - 1].cells++;
        } else {
            chords[used++] = stages[s];
        }
    }
    *count = used;
    return true;
}

static bool sequence_applies(const PiecePlan *plan, FillTarget target, void *context) {
    SkeletonContext read;
    (void)context;
    return target.kind == FILL_TARGET_LANE_SPAN && harmony_skeleton_read(plan, (LaneSpanId)target.id, &read) &&
           !read.at_start;
}

static void sequence_label(const ScalePattern *pattern, int direction, char *label, size_t size) {
    if (pattern->both_directions) {
        snprintf(label, size, "sequence:%s-%s", pattern->name, direction > 0 ? "ascending" : "descending");
    } else {
        snprintf(label, size, "sequence:%s", pattern->name);
    }
}

/* Offers the pattern in one direction; false only when allocation or a check fails. */
static bool offer(const SkeletonContext *read, const HarmonySequenceParams *params, const ScalePattern *pattern,
                  int direction, FillRng *rng, SkeletonCandidate *skeleton, FillCandidateList *out) {
    unsigned low = params->min_repeats > 0 ? params->min_repeats : 1;
    unsigned high = params->max_repeats > low ? params->max_repeats : low;
    unsigned drawn = low + (unsigned)fill_rng_below(rng, high - low + 1);
    int start = 1 + (int)fill_rng_below(rng, 7);
    size_t cpb = read->metre.cells_per_bar;
    PlanKeyMode mode = read->key.mode;
    memset(skeleton, 0, sizeof(*skeleton));
    skeleton->schema = -1;
    skeleton->context = *read;

    /* The most repeats from the draw down to the least that leave room for the cadence. */
    for (unsigned repeats = drawn; repeats >= low; repeats--) {
        size_t count;
        size_t total = 0;
        if (!build_chords(mode, pattern, direction, start, repeats, skeleton->chords, &count)) {
            return true;
        }
        for (size_t i = 0; i < count; i++) {
            total += skeleton->chords[i].cells;
        }
        /* The last chord runs to the end of its bar, so that what is left starts on a bar line. */
        size_t end = (read->metre.first_cell + total) % cpb;
        if (end != 0) {
            skeleton->chords[count - 1].cells += cpb - end;
            total += cpb - end;
        }
        size_t room = read->has_cadence ? harmony_phrase_room_cells(read->cadence, cpb) : 0;
        if (total + room <= read->metre.cells) {
            skeleton->count = count;
            skeleton->remainder_cells = read->metre.cells - total;
            skeleton->preference = params->preference < PREFERENCE_LIMIT ? params->preference : PREFERENCE_LIMIT;
            for (int attempt = 0; attempt < BASS_TRIES; attempt++) {
                PhraseVerdict verdict;
                if (!harmony_skeleton_bass(skeleton, &params->bass, rng)) {
                    return true;
                }
                if (!harmony_skeleton_judge(skeleton, &verdict)) {
                    return false;
                }
                if (!verdict.hard) {
                    SkeletonCandidate *payload = malloc(sizeof(*payload));
                    char label[FILL_LABEL_MAX];
                    if (!payload) {
                        return false;
                    }
                    *payload = *skeleton;
                    sequence_label(pattern, direction, label, sizeof(label));
                    return fill_candidates_add(out, label, payload, free);
                }
            }
            return true;
        }
        if (repeats == low) {
            break;
        }
    }
    return true;
}

static bool sequence_generate(const PiecePlan *plan, FillTarget target, FillRng *rng, void *context,
                              FillCandidateList *out) {
    const HarmonySequenceParams *params = params_of(context);
    SkeletonContext read;
    if (target.kind != FILL_TARGET_LANE_SPAN || !harmony_skeleton_read(plan, (LaneSpanId)target.id, &read) ||
        read.at_start) {
        return false;
    }
    SkeletonCandidate *skeleton = malloc(sizeof(*skeleton));
    if (!skeleton) {
        return false;
    }
    bool ok = true;
    for (int id = 0; ok && id < SCALE_PATTERN_COUNT; id++) {
        const ScalePattern *pattern = scale_pattern((ScalePatternId)id);
        if (!pattern || !(params->patterns & HARMONY_SEQUENCE_BIT(id))) {
            continue;
        }
        for (int direction = -1; ok && direction <= 1; direction += 2) {
            ScaleDirection scale = direction > 0 ? SCALE_DIRECTION_ASCENDING : SCALE_DIRECTION_DESCENDING;
            if (scale_pattern_allows_direction(pattern, scale)) {
                ok = offer(&read, params, pattern, direction, rng, skeleton, out);
            }
        }
    }
    free(skeleton);
    return ok;
}

static PlanResult sequence_apply(PiecePlan *plan, FillTarget target, const FillCandidate *candidate, void *context) {
    const SkeletonCandidate *skeleton = candidate ? candidate->payload : NULL;
    (void)context;
    if (target.kind != FILL_TARGET_LANE_SPAN || !skeleton || skeleton->context.target != (LaneSpanId)target.id) {
        return PLAN_ERR_NULL;
    }
    return harmony_skeleton_apply(plan, skeleton);
}

FillResult harmony_sequence_register(FillOperatorRegistry *registry, const HarmonySequenceParams *params) {
    FillOperator operator_;
    memset(&operator_, 0, sizeof(operator_));
    operator_.generator.name = HARMONY_SEQUENCE_OPERATOR_NAME;
    operator_.generator.supported_inputs = "PiecePlan harmony lane span with the chords before it";
    operator_.generator.supported_outputs = "PiecePlan harmony and bass lane spans";
    operator_.generator.configuration_description = "enabled patterns, repeats, preference, bass parameters";
    operator_.generator.constraints_description = "scale-harmonization patterns, standard-harmony, standard-counterpoint";
    operator_.generator.deterministic = true;
    operator_.generator.version = "1";
    operator_.kind = FILL_OPERATOR_FILL;
    operator_.applies = sequence_applies;
    operator_.generate = sequence_generate;
    operator_.apply = sequence_apply;
    operator_.context = (void *)params;
    return fill_registry_register(registry, &operator_);
}
