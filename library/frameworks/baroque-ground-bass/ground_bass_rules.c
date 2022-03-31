#include "ground_bass_rules.h"

#include "ground_rule_table.h"
#include "standard-counterpoint/counterpoint_pitch.h"

static const char *const NAMES[GROUND_BASS_RULE_COUNT] = {"iterations-whole", "ground-repeats", "bass-lowest",
                                                          "triple-meter", "exact-repeat"};

static bool table_is_hard(int which) {
    return which <= GROUND_BASS_TRIPLE_METER;
}

static GroundRulePair g_pairs[GROUND_BASS_RULE_COUNT];
static bool g_built[GROUND_BASS_RULE_COUNT];
static const GroundRuleTable TABLE = {"ground", NAMES, GROUND_BASS_RULE_COUNT, table_is_hard, g_pairs, g_built};

const Rule *ground_bass_rule(GroundBassRule which) {
    return ground_rule_table_rule(&TABLE, (int)which);
}

const Constraint *ground_bass_constraint(GroundBassRule which) {
    return ground_rule_table_constraint(&TABLE, (int)which);
}

const char *ground_bass_rule_name(GroundBassRule which) {
    return ground_rule_table_name(&TABLE, (int)which);
}

/* The index of the first note of the bass that begins at or after `slot`. */
static size_t first_note_from(const GroundNote *notes, size_t count, size_t slot) {
    size_t i = 0;
    while (i < count && notes[i].start < slot) {
        i++;
    }
    return i;
}

bool ground_bass_altered_notes(const GroundBassPiece *piece, size_t iteration, size_t *out) {
    if (out == NULL || !ground_piece_valid(piece) || iteration >= ground_piece_iteration_count(piece)) {
        return false;
    }
    GroundNote notes[GROUND_MAX_NOTES];
    size_t count;
    if (!ground_voice_notes(piece, piece->bass_voice, notes, GROUND_MAX_NOTES, &count)) {
        return false;
    }
    size_t per = piece->slots_per_iteration;
    size_t ground_begin = first_note_from(notes, count, 0);
    size_t ground_end = first_note_from(notes, count, per);
    size_t begin = first_note_from(notes, count, iteration * per);
    size_t end = first_note_from(notes, count, (iteration + 1) * per);
    size_t ground_count = ground_end - ground_begin;
    size_t this_count = end - begin;
    size_t common = ground_count < this_count ? ground_count : this_count;
    size_t altered = (ground_count > this_count ? ground_count : this_count) - common;
    for (size_t i = 0; i < common; i++) {
        const GroundNote *g = &notes[ground_begin + i];
        const GroundNote *n = &notes[begin + i];
        if (n->start - iteration * per != g->start || n->length != g->length ||
            !standard_counterpoint_same_pitch(g->pitch, n->pitch)) {
            altered++;
        }
    }
    *out = altered;
    return true;
}

static bool check_repeats(const GroundBassPiece *piece, size_t limit) {
    size_t iterations = ground_piece_iteration_count(piece);
    for (size_t i = 1; i < iterations; i++) {
        size_t altered;
        if (!ground_bass_altered_notes(piece, i, &altered) || altered > limit) {
            return false;
        }
    }
    return true;
}

static bool check_bass_lowest(const GroundBassPiece *piece) {
    size_t slots = ground_piece_slot_count(piece);
    for (size_t s = 0; s < slots; s++) {
        const PitchSpelling *bass = ground_pitch_at(piece, piece->bass_voice, s);
        if (bass == NULL) {
            continue;
        }
        for (size_t v = 0; v < piece->voice_count; v++) {
            const PitchSpelling *other = ground_pitch_at(piece, v, s);
            if (v != piece->bass_voice && other != NULL &&
                standard_counterpoint_semitone_number(other) < standard_counterpoint_semitone_number(bass)) {
                return false;
            }
        }
    }
    return true;
}

bool ground_bass_rule_holds(GroundBassRule which, const GroundBassPiece *piece) {
    if (!ground_rule_table_valid(&TABLE, (int)which) || !ground_piece_valid(piece)) {
        return false;
    }
    switch (which) {
        case GROUND_BASS_ITERATIONS_WHOLE:
            return ground_piece_slot_count(piece) % piece->slots_per_iteration == 0;
        case GROUND_BASS_GROUND_REPEATS:
            return check_repeats(piece, ground_piece_limits(piece).max_altered_notes);
        case GROUND_BASS_BASS_LOWEST:
            return check_bass_lowest(piece);
        case GROUND_BASS_TRIPLE_METER:
            return !ostinato_type_is_triple_meter_form(piece->type) || piece->beats_per_bar == 3;
        case GROUND_BASS_EXACT_REPEAT:
            return check_repeats(piece, 0);
        default:
            return false;
    }
}

static bool holds_adapter(int which, const void *subject) {
    return ground_bass_rule_holds((GroundBassRule)which, subject);
}

bool ground_bass_check(const GroundBassPiece *piece, unsigned *hard_violations, unsigned *soft_violations) {
    if (hard_violations == NULL || soft_violations == NULL || !ground_piece_valid(piece)) {
        return false;
    }
    ground_rule_table_check(&TABLE, holds_adapter, piece, hard_violations, soft_violations);
    return true;
}
