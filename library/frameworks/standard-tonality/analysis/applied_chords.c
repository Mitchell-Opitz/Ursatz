#include "applied_chords.h"

#include <math.h>
#include <string.h>

#include "roman_numeral_analysis_rules.h"
#include "triad_in_key_rules.h"

static const int MAJOR_SCALE[7] = {0, 2, 4, 5, 7, 9, 11};
static const int NATURAL_MINOR_SCALE[7] = {0, 2, 3, 5, 7, 8, 10};

/* The targets: scale degree 1..7 of the key. */
static const int MAJOR_TARGETS[5] = {2, 3, 4, 5, 6};
static const int MINOR_TARGETS[5] = {3, 4, 5, 6, 7};

#define SUBMEDIANT_SEMITONES 9

typedef struct {
    int root_offset;              /* the root's semitones above the target's root */
    int intervals[4];
    size_t interval_count;
    AppliedChordKind kind;
    bool seventh;
    int numeral_degree;           /* 5 or 7: the numeral in the tonicized key */
    StandardTonalityTriadQuality triad;
    StandardTonalitySeventhQuality seventh_quality;
} Shape;

/* In the order of tie-breaking: V, V7, vii-diminished, vii-diminished-7. */
static const Shape SHAPES[4] = {
    {7, {0, 4, 7, 0}, 3, APPLIED_CHORD_KIND_SECONDARY_DOMINANT, false, 5, STANDARD_TONALITY_TRIAD_MAJOR,
     STANDARD_TONALITY_SEVENTH_MAJOR_MINOR},
    {7, {0, 4, 7, 10}, 4, APPLIED_CHORD_KIND_SECONDARY_DOMINANT, true, 5, STANDARD_TONALITY_TRIAD_MAJOR,
     STANDARD_TONALITY_SEVENTH_MAJOR_MINOR},
    {11, {0, 3, 6, 0}, 3, APPLIED_CHORD_KIND_SECONDARY_LEADING_TONE, false, 7, STANDARD_TONALITY_TRIAD_DIMINISHED,
     STANDARD_TONALITY_SEVENTH_FULLY_DIMINISHED},
    {11, {0, 3, 6, 9}, 4, APPLIED_CHORD_KIND_SECONDARY_LEADING_TONE, true, 7, STANDARD_TONALITY_TRIAD_DIMINISHED,
     STANDARD_TONALITY_SEVENTH_FULLY_DIMINISHED}};

StandardTonalityAppliedConfig standard_tonality_applied_config_default(void) {
    StandardTonalityAppliedConfig config;
    config.chromatic_min = 0.15;
    config.missing_third_penalty = 0.15;
    config.bass_chord_tone_bonus = 0.05;
    return config;
}

static bool usable(double value) {
    return isfinite(value) && value >= 0.0;
}

bool standard_tonality_applied_config_valid(const StandardTonalityAppliedConfig *config) {
    return config != NULL && usable(config->chromatic_min) && usable(config->missing_third_penalty) &&
           usable(config->bass_chord_tone_bonus);
}

static int wrap12(int value) {
    return ((value % 12) + 12) % 12;
}

/* The L2.5 score of a chord over the weights (see the header). */
static double score_chord(const double weights[12], double total, int root, const int *intervals, size_t count,
                          int bass_pitch_class, const StandardTonalityAppliedConfig *config) {
    double in_chord = 0.0;
    bool bass_is_chord_tone = false;
    for (size_t i = 0; i < count; i++) {
        int pc = wrap12(root + intervals[i]);
        in_chord += weights[pc];
        bass_is_chord_tone = bass_is_chord_tone || pc == bass_pitch_class;
    }
    double score = total > 0.0 ? in_chord / total : 0.0;
    if (count > 1 && weights[wrap12(root + intervals[1])] <= 0.0) {
        score -= config->missing_third_penalty;
    }
    if (bass_is_chord_tone) {
        score += config->bass_chord_tone_bonus;
    }
    return score;
}

static bool in_key(const StandardTonalityChordInput *input, int pitch_class) {
    int offset = wrap12(pitch_class - input->tonic_pitch_class);
    const int *scale = input->mode == STANDARD_TONALITY_ROMAN_KEY_MAJOR ? MAJOR_SCALE : NATURAL_MINOR_SCALE;
    for (int i = 0; i < 7; i++) {
        if (scale[i] == offset) {
            return true;
        }
    }
    return input->mode == STANDARD_TONALITY_ROMAN_KEY_MINOR && offset == 11; /* the raised leading tone */
}

static int member_in_bass(int bass_pitch_class, int root, const int *intervals, size_t count) {
    if (bass_pitch_class < 0) {
        return 0;
    }
    int above_root = wrap12(bass_pitch_class - root);
    for (size_t member = 0; member < count && member < 4; member++) {
        if (intervals[member] == above_root) {
            return (int)member;
        }
    }
    return 0;
}

/* The numeral of the target degree as the key gives it (ii, iv, V, VI ...). */
static bool target_numeral(StandardTonalityRomanKeyMode mode, int degree, StandardTonalityRomanNumeral *out) {
    StandardTonalityTriadKey key = STANDARD_TONALITY_TRIAD_KEY_MAJOR;
    if (mode == STANDARD_TONALITY_ROMAN_KEY_MINOR) {
        key = degree == 5 ? STANDARD_TONALITY_TRIAD_KEY_MINOR_HARMONIC : STANDARD_TONALITY_TRIAD_KEY_MINOR_NATURAL;
    }
    StandardTonalityTriadQuality quality;
    return standard_tonality_triad_quality_in_key(key, degree, &quality) &&
           standard_tonality_roman_numeral_triad(degree, quality, out);
}

/* The index of the segment the chord resolves to: the first after `index` whose chord is not a repeat of this
   one (same root). SIZE_MAX if there is none. */
static size_t next_distinct(const StandardTonalityChordInput *inputs, size_t count, size_t index) {
    for (size_t next = index + 1; next < count; next++) {
        if (!inputs[next].has_numeral || inputs[next].root_pitch_class != inputs[index].root_pitch_class) {
            return next;
        }
    }
    return (size_t)-1;
}

bool standard_tonality_recheck_applied_chord(const StandardTonalityChordInput *inputs, size_t count, size_t index,
                                             const StandardTonalityAppliedConfig *config,
                                             StandardTonalityAppliedResult *out) {
    StandardTonalityAppliedConfig defaults = standard_tonality_applied_config_default();
    if (config == NULL) {
        config = &defaults;
    }
    if (inputs == NULL || out == NULL || index >= count || !standard_tonality_applied_config_valid(config) ||
        !inputs[index].has_numeral || !inputs[index].has_weights) {
        return false;
    }
    const StandardTonalityChordInput *input = &inputs[index];

    double total = 0.0;
    double chromatic = 0.0;
    for (int pc = 0; pc < 12; pc++) {
        if (!(input->weights[pc] >= 0.0)) {
            return false;
        }
        total += input->weights[pc];
        if (!in_key(input, pc)) {
            chromatic += input->weights[pc];
        }
    }
    if (!(total > 0.0) || chromatic / total < config->chromatic_min) {
        return false;
    }

    size_t resolution = next_distinct(inputs, count, index);
    if (resolution == (size_t)-1 || !inputs[resolution].has_numeral) {
        return false;
    }

    double diatonic_score = score_chord(input->weights, total, input->root_pitch_class, input->intervals,
                                        input->interval_count, input->bass_pitch_class, config);

    const int *targets = input->mode == STANDARD_TONALITY_ROMAN_KEY_MAJOR ? MAJOR_TARGETS : MINOR_TARGETS;
    const int *scale = input->mode == STANDARD_TONALITY_ROMAN_KEY_MAJOR ? MAJOR_SCALE : NATURAL_MINOR_SCALE;
    bool found = false;
    double best_score = 0.0;
    int best_target = 0;
    const Shape *best_shape = NULL;
    for (int t = 0; t < 5; t++) {
        int target_root = wrap12(input->tonic_pitch_class + scale[targets[t] - 1]);
        /* The resolution must be the target or (deceptively) its submediant, and it must be a different
           chord from the one being relabelled. */
        int resolution_root = inputs[resolution].root_pitch_class;
        if (resolution_root != target_root && resolution_root != wrap12(target_root + SUBMEDIANT_SEMITONES)) {
            continue;
        }
        for (int s = 0; s < 4; s++) {
            const Shape *shape = &SHAPES[s];
            int root = wrap12(target_root + shape->root_offset);
            if (input->weights[root] <= 0.0) {
                continue; /* the root does not sound */
            }
            double score = score_chord(input->weights, total, root, shape->intervals, shape->interval_count,
                                       input->bass_pitch_class, config);
            if (score > diatonic_score && (!found || score > best_score)) {
                found = true;
                best_score = score;
                best_target = targets[t];
                best_shape = shape;
            }
        }
    }
    if (!found) {
        return false;
    }

    int target_root = wrap12(input->tonic_pitch_class + scale[best_target - 1]);
    int root = wrap12(target_root + best_shape->root_offset);
    StandardTonalityRomanReading reading;
    memset(&reading, 0, sizeof(reading));
    bool built = best_shape->seventh
                     ? standard_tonality_roman_numeral_seventh_chord(best_shape->numeral_degree,
                                                                     best_shape->seventh_quality, &reading.numeral)
                     : standard_tonality_roman_numeral_triad(best_shape->numeral_degree, best_shape->triad,
                                                             &reading.numeral);
    if (!built || !target_numeral(input->mode, best_target, &reading.applied_target)) {
        return false;
    }
    reading.inversion = member_in_bass(input->bass_pitch_class, root, best_shape->intervals,
                                       best_shape->interval_count);
    reading.applied = true;
    reading.romantext_spelling = input->mode == STANDARD_TONALITY_ROMAN_KEY_MINOR;
    reading.applied_kind = best_shape->kind;

    out->reading = reading;
    out->resolution_index = resolution;
    out->deceptive = inputs[resolution].root_pitch_class != target_root;
    return true;
}
