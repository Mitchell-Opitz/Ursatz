#include "ground_variety_rules.h"

#include <stdint.h>

#include "ground_rule_table.h"
#include "standard-counterpoint/counterpoint_pitch.h"

static const char *const NAMES[GROUND_VARIETY_RULE_COUNT] = {"upper-parts-vary", "upper-rhythm-varies",
                                                             "every-iteration-imitates"};

static GroundRulePair g_pairs[GROUND_VARIETY_RULE_COUNT];
static UrsatzOnce g_built[GROUND_VARIETY_RULE_COUNT];
static const GroundRuleTable TABLE = {"variety", NAMES, GROUND_VARIETY_RULE_COUNT, ground_rule_table_never_hard,
                                      g_pairs, g_built};

const Rule *ground_variety_rule(GroundVarietyRule which) {
    return ground_rule_table_rule(&TABLE, (int)which);
}

const Constraint *ground_variety_constraint(GroundVarietyRule which) {
    return ground_rule_table_constraint(&TABLE, (int)which);
}

const char *ground_variety_rule_name(GroundVarietyRule which) {
    return ground_rule_table_name(&TABLE, (int)which);
}

static bool same_slot(const GroundBassPiece *piece, size_t voice, size_t a, size_t b) {
    const StandardCounterpointSlot *x = &piece->voices[voice].slots[a];
    const StandardCounterpointSlot *y = &piece->voices[voice].slots[b];
    if (x->kind != y->kind) {
        return false;
    }
    return x->kind == STANDARD_COUNTERPOINT_SLOT_REST || standard_counterpoint_same_pitch(x->pitch, y->pitch);
}

static bool check_upper_parts_vary(const GroundBassPiece *piece) {
    size_t per = piece->slots_per_iteration;
    size_t iterations = ground_piece_iteration_count(piece);
    for (size_t k = 1; k < iterations; k++) {
        bool differs = false;
        for (size_t v = 0; v < piece->voice_count && !differs; v++) {
            if (v == piece->bass_voice) {
                continue;
            }
            for (size_t s = 0; s < per && !differs; s++) {
                differs = !same_slot(piece, v, (k - 1) * per + s, k * per + s);
            }
        }
        if (!differs) {
            return false;
        }
    }
    return true;
}

static bool check_upper_rhythm_varies(const GroundBassPiece *piece) {
    size_t per = piece->slots_per_iteration;
    size_t iterations = ground_piece_iteration_count(piece);
    size_t shortest[GROUND_MAX_SLOTS];
    for (size_t k = 0; k < iterations; k++) {
        shortest[k] = SIZE_MAX;
    }
    for (size_t v = 0; v < piece->voice_count; v++) {
        GroundNote notes[GROUND_MAX_NOTES];
        size_t count;
        if (v == piece->bass_voice) {
            continue;
        }
        if (!ground_voice_notes(piece, v, notes, GROUND_MAX_NOTES, &count)) {
            return false;
        }
        for (size_t n = 0; n < count; n++) {
            size_t k = notes[n].start / per;
            if (k < iterations && notes[n].length < shortest[k]) {
                shortest[k] = notes[n].length;
            }
        }
    }
    size_t seen = 0;
    bool varies = false;
    size_t first = SIZE_MAX;
    for (size_t k = 0; k < iterations; k++) {
        if (shortest[k] == SIZE_MAX) {
            continue;
        }
        seen++;
        if (first == SIZE_MAX) {
            first = shortest[k];
        } else if (shortest[k] != first) {
            varies = true;
        }
    }
    return seen < 3 || varies;
}

static bool check_every_iteration_imitates(const GroundBassPiece *piece) {
    size_t per = piece->slots_per_iteration;
    size_t iterations = ground_piece_iteration_count(piece);
    for (size_t k = 0; k < iterations; k++) {
        bool found = false;
        for (size_t i = 0; i < piece->statement_count && !found; i++) {
            const GroundStatement *s = &piece->statements[i];
            found = s->role == IMITATIVE_ROLE_DUX && s->start / per == k;
        }
        if (!found) {
            return false;
        }
    }
    return true;
}

bool ground_variety_rule_holds(GroundVarietyRule which, const GroundBassPiece *piece) {
    if (!ground_rule_table_valid(&TABLE, (int)which) || !ground_piece_valid(piece)) {
        return false;
    }
    switch (which) {
        case GROUND_VARIETY_UPPER_PARTS_VARY:
            return check_upper_parts_vary(piece);
        case GROUND_VARIETY_UPPER_RHYTHM_VARIES:
            return check_upper_rhythm_varies(piece);
        case GROUND_VARIETY_EVERY_ITERATION_IMITATES:
            return check_every_iteration_imitates(piece);
        default:
            return false;
    }
}

static bool holds_adapter(int which, const void *subject) {
    return ground_variety_rule_holds((GroundVarietyRule)which, subject);
}

bool ground_variety_check(const GroundBassPiece *piece, unsigned *hard_violations, unsigned *soft_violations) {
    if (hard_violations == NULL || soft_violations == NULL || !ground_piece_valid(piece)) {
        return false;
    }
    ground_rule_table_check(&TABLE, holds_adapter, piece, hard_violations, soft_violations);
    return true;
}
