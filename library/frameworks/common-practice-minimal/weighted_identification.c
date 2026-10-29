#include "weighted_identification.h"

#include <math.h>

#include "core/rational.h"
#include "time/time_rational.h"

CommonPracticeWeightedConfig common_practice_weighted_config_default(void) {
    CommonPracticeWeightedConfig config;
    config.strength_factor[BEAT_STRENGTH_STRONG] = 1.0;
    config.strength_factor[BEAT_STRENGTH_MEDIUM] = 0.7;
    config.strength_factor[BEAT_STRENGTH_WEAK] = 0.4;
    config.held_factor = 0.6;
    config.bass_weight = 1.1;
    config.missing_third_penalty = 0.15;
    config.bass_chord_tone_bonus = 0.05;
    config.min_coverage = 0.5;
    config.classifier = NULL;
    config.classifier_context = NULL;
    return config;
}

static bool usable(double value) {
    return isfinite(value) && value >= 0.0;
}

static bool config_valid(const CommonPracticeWeightedConfig *config) {
    for (int i = 0; i < BEAT_STRENGTH_COUNT; i++) {
        if (!usable(config->strength_factor[i])) {
            return false;
        }
    }
    return usable(config->held_factor) && usable(config->bass_weight) && usable(config->missing_third_penalty) &&
           usable(config->bass_chord_tone_bonus) && usable(config->min_coverage);
}

static int pitch_class_of(int pitch) {
    return ((pitch % 12) + 12) % 12;
}

CommonPracticeWeightedStatus common_practice_weight_segment(const CommonPracticeMember *members, size_t count,
                                                             const CommonPracticeWeightedConfig *config,
                                                             CommonPracticeWeights *out) {
    if (config == NULL || out == NULL || (members == NULL && count > 0)) {
        return COMMON_PRACTICE_WEIGHTED_ERR_NULL_ARGUMENT;
    }
    if (!config_valid(config)) {
        return COMMON_PRACTICE_WEIGHTED_ERR_BAD_CONFIG;
    }

    int bass_pitch = -1;
    bool have_bass = false;
    for (size_t i = 0; i < count; i++) {
        if (!(members[i].duration > 0.0) || !isfinite(members[i].duration) ||
            (!members[i].held && !beat_strength_valid(members[i].strength))) {
            return COMMON_PRACTICE_WEIGHTED_ERR_BAD_MEMBER;
        }
        if (!have_bass || members[i].pitch < bass_pitch) {
            bass_pitch = members[i].pitch;
            have_bass = true;
        }
    }

    CommonPracticeWeights weights = {{0}, 0.0, -1, -1};
    for (size_t i = 0; i < count; i++) {
        double factor = members[i].held ? config->held_factor : config->strength_factor[members[i].strength];
        double weight = members[i].duration * factor;
        if (pitch_class_of(members[i].pitch) == pitch_class_of(bass_pitch)) {
            weight *= config->bass_weight;
        }
        weights.pitch_class[pitch_class_of(members[i].pitch)] += weight;
        weights.total += weight;
    }
    if (have_bass) {
        weights.bass_pitch = bass_pitch;
        weights.bass_pitch_class = pitch_class_of(bass_pitch);
    }
    *out = weights;
    return COMMON_PRACTICE_WEIGHTED_OK;
}

/* --- scoring --- */

#define TOLERANCE 1e-9

/* One chord of a vocabulary: where its root sits above the tonic, and its quality. */
typedef struct {
    int offset;
    int degree;
    bool is_seventh;
    CommonPracticeTriad triad;
    CommonPracticeSeventh seventh;
    int form_rank;
} VocabularyEntry;

#define TRI(offset, degree, triad, rank) {offset, degree, false, triad, COMMON_PRACTICE_SEVENTH_DOMINANT7, rank}
#define SEV(offset, degree, triad, seventh, rank) {offset, degree, true, triad, seventh, rank}

/* The 14 diatonic chords of a major key, a triad then its seventh on each degree. */
static const VocabularyEntry MAJOR_VOCABULARY[COMMON_PRACTICE_MAJOR_CANDIDATE_COUNT] = {
    TRI(0, 1, COMMON_PRACTICE_TRIAD_I, 0),       SEV(0, 1, COMMON_PRACTICE_TRIAD_I, COMMON_PRACTICE_SEVENTH_MAJOR7, 0),
    TRI(2, 2, COMMON_PRACTICE_TRIAD_II, 0),      SEV(2, 2, COMMON_PRACTICE_TRIAD_II, COMMON_PRACTICE_SEVENTH_MINOR7, 0),
    TRI(4, 3, COMMON_PRACTICE_TRIAD_III, 0),     SEV(4, 3, COMMON_PRACTICE_TRIAD_III, COMMON_PRACTICE_SEVENTH_MINOR7, 0),
    TRI(5, 4, COMMON_PRACTICE_TRIAD_IV, 0),      SEV(5, 4, COMMON_PRACTICE_TRIAD_IV, COMMON_PRACTICE_SEVENTH_MAJOR7, 0),
    TRI(7, 5, COMMON_PRACTICE_TRIAD_V, 0),       SEV(7, 5, COMMON_PRACTICE_TRIAD_V, COMMON_PRACTICE_SEVENTH_DOMINANT7, 0),
    TRI(9, 6, COMMON_PRACTICE_TRIAD_VI, 0),      SEV(9, 6, COMMON_PRACTICE_TRIAD_VI, COMMON_PRACTICE_SEVENTH_MINOR7, 0),
    TRI(11, 7, COMMON_PRACTICE_TRIAD_VII_DIM, 0),
    SEV(11, 7, COMMON_PRACTICE_TRIAD_VII_DIM, COMMON_PRACTICE_SEVENTH_HALF_DIMINISHED7, 0),
};

#define MAJ COMMON_PRACTICE_TRIAD_MAJOR
#define MIN COMMON_PRACTICE_TRIAD_MINOR
#define DIM COMMON_PRACTICE_TRIAD_DIMINISHED
#define AUG COMMON_PRACTICE_TRIAD_AUGMENTED
#define DOM7 COMMON_PRACTICE_SEVENTH_DOMINANT7
#define MAJ7 COMMON_PRACTICE_SEVENTH_MAJOR7
#define MIN7 COMMON_PRACTICE_SEVENTH_MINOR7
#define HDIM7 COMMON_PRACTICE_SEVENTH_HALF_DIMINISHED7
#define DIM7 COMMON_PRACTICE_SEVENTH_DIMINISHED7

/*
 * The 26 chords of minor: the diatonic triads and sevenths of natural (0 2 3 5 7 8 10), harmonic (7th raised to
 * 11) and melodic (6th and 7th raised to 9 and 11) minor, less the minor-major and augmented-major sevenths. A
 * form_rank of 0 is the form that wins a tie against the other form of the same root and quality family:
 * the raised V, V7, vii-dim and vii-dim-7, and the natural form of everything else.
 */
static const VocabularyEntry MINOR_VOCABULARY[COMMON_PRACTICE_MINOR_CANDIDATE_COUNT] = {
    TRI(0, 1, MIN, 0),  SEV(0, 1, MIN, MIN7, 0),
    TRI(2, 2, DIM, 0),  SEV(2, 2, DIM, HDIM7, 0),
    TRI(2, 2, MIN, 1),  SEV(2, 2, MIN, MIN7, 1),
    TRI(3, 3, MAJ, 0),  SEV(3, 3, MAJ, MAJ7, 0),
    TRI(3, 3, AUG, 1),
    TRI(5, 4, MIN, 0),  SEV(5, 4, MIN, MIN7, 0),
    TRI(5, 4, MAJ, 1),  SEV(5, 4, MAJ, DOM7, 1),
    TRI(7, 5, MAJ, 0),  SEV(7, 5, MAJ, DOM7, 0),
    TRI(7, 5, MIN, 1),  SEV(7, 5, MIN, MIN7, 1),
    TRI(8, 6, MAJ, 0),  SEV(8, 6, MAJ, MAJ7, 0),
    TRI(9, 6, DIM, 0),  SEV(9, 6, DIM, HDIM7, 0),
    TRI(10, 7, MAJ, 0), SEV(10, 7, MAJ, DOM7, 0),
    TRI(11, 7, DIM, 0), SEV(11, 7, DIM, DIM7, 0),
    SEV(11, 7, DIM, HDIM7, 1),
};

#undef TRI
#undef SEV
#undef MAJ
#undef MIN
#undef DIM
#undef AUG
#undef DOM7
#undef MAJ7
#undef MIN7
#undef HDIM7
#undef DIM7

size_t common_practice_ranked_chord_intervals(const CommonPracticeRankedChord *chord, int out[4]) {
    if (chord == NULL || out == NULL) {
        return 0;
    }
    if (chord->is_seventh) {
        return common_practice_seventh_intervals(chord->seventh, out) ? 4 : 0;
    }
    return common_practice_triad_intervals(chord->triad, out) ? 3 : 0;
}

/* True when `a` ranks ahead of `b` for a segment whose bass has pitch class bass_pitch_class. */
static bool ranks_ahead(const CommonPracticeRankedChord *a, const CommonPracticeRankedChord *b,
                        int bass_pitch_class) {
    if (fabs(a->score - b->score) > TOLERANCE) {
        return a->score > b->score;
    }
    bool a_root_position = bass_pitch_class >= 0 && a->root_pitch_class == bass_pitch_class;
    bool b_root_position = bass_pitch_class >= 0 && b->root_pitch_class == bass_pitch_class;
    if (a_root_position != b_root_position) {
        return a_root_position;
    }
    if (a->is_seventh != b->is_seventh) {
        return !a->is_seventh;
    }
    if (a->degree != b->degree) {
        return a->degree < b->degree;
    }
    if (a->tonic_offset != b->tonic_offset) {
        return a->tonic_offset < b->tonic_offset;
    }
    return a->form_rank < b->form_rank;
}

CommonPracticeWeightedStatus common_practice_rank_candidates(const CommonPracticeWeights *weights,
                                                              int tonic_pitch_class, CommonPracticeKeyMode mode,
                                                              const CommonPracticeWeightedConfig *config,
                                                              CommonPracticeRankedChord out[COMMON_PRACTICE_CANDIDATE_COUNT],
                                                              size_t *out_count) {
    if (weights == NULL || config == NULL || out == NULL || out_count == NULL) {
        return COMMON_PRACTICE_WEIGHTED_ERR_NULL_ARGUMENT;
    }
    if (!config_valid(config)) {
        return COMMON_PRACTICE_WEIGHTED_ERR_BAD_CONFIG;
    }
    if (mode != COMMON_PRACTICE_KEY_MAJOR && mode != COMMON_PRACTICE_KEY_MINOR) {
        return COMMON_PRACTICE_WEIGHTED_ERR_UNSUPPORTED_MODE;
    }
    if (tonic_pitch_class < 0 || tonic_pitch_class > 11) {
        return COMMON_PRACTICE_WEIGHTED_ERR_BAD_TONIC;
    }

    const VocabularyEntry *vocabulary = mode == COMMON_PRACTICE_KEY_MAJOR ? MAJOR_VOCABULARY : MINOR_VOCABULARY;
    size_t vocabulary_size = mode == COMMON_PRACTICE_KEY_MAJOR ? COMMON_PRACTICE_MAJOR_CANDIDATE_COUNT
                                                                : COMMON_PRACTICE_MINOR_CANDIDATE_COUNT;

    CommonPracticeRankedChord ranked[COMMON_PRACTICE_CANDIDATE_COUNT];
    size_t count = 0;
    for (size_t k = 0; k < vocabulary_size; k++) {
        const VocabularyEntry *entry = &vocabulary[k];
        int root = (tonic_pitch_class + entry->offset) % 12;
        if (weights->pitch_class[root] <= 0.0) {
            continue; /* the root does not sound: not a reading the notes support */
        }

        CommonPracticeRankedChord chord;
        chord.degree = entry->degree;
        chord.is_seventh = entry->is_seventh;
        chord.triad = entry->triad;
        chord.seventh = entry->seventh;
        chord.root_pitch_class = root;
        chord.score = 0.0;
        chord.tonic_offset = entry->offset;
        chord.form_rank = entry->form_rank;

        int intervals[4];
        size_t interval_count = common_practice_ranked_chord_intervals(&chord, intervals);
        int third = (root + intervals[1]) % 12;
        double in_chord = 0.0;
        bool bass_is_chord_tone = false;
        for (size_t i = 0; i < interval_count; i++) {
            int pitch_class = (root + intervals[i]) % 12;
            in_chord += weights->pitch_class[pitch_class];
            bass_is_chord_tone = bass_is_chord_tone || weights->bass_pitch_class == pitch_class;
        }

        double score = weights->total > 0.0 ? in_chord / weights->total : 0.0;
        if (weights->pitch_class[third] <= 0.0) {
            score -= config->missing_third_penalty;
        }
        if (bass_is_chord_tone) {
            score += config->bass_chord_tone_bonus;
        }
        chord.score = score;
        ranked[count++] = chord;
    }

    /* Insertion sort, best first: at most 26 candidates, and it keeps the order stable and total. */
    for (size_t i = 1; i < count; i++) {
        CommonPracticeRankedChord moving = ranked[i];
        size_t j = i;
        while (j > 0 && ranks_ahead(&moving, &ranked[j - 1], weights->bass_pitch_class)) {
            ranked[j] = ranked[j - 1];
            j--;
        }
        ranked[j] = moving;
    }
    for (size_t i = 0; i < count; i++) {
        out[i] = ranked[i];
    }
    *out_count = count;
    return COMMON_PRACTICE_WEIGHTED_OK;
}

unsigned common_practice_ranked_chord_mask(const CommonPracticeRankedChord *chord, int tonic_pitch_class) {
    if (chord == NULL || chord->degree < 1 || chord->degree > 7 || chord->root_pitch_class < 0 || chord->root_pitch_class > 11 || tonic_pitch_class < 0 ||
        tonic_pitch_class > 11) {
        return 0;
    }
    int intervals[4];
    size_t count = common_practice_ranked_chord_intervals(chord, intervals);
    unsigned mask = 0;
    for (size_t i = 0; i < count; i++) {
        mask |= 1u << ((chord->root_pitch_class + intervals[i]) % 12);
    }
    return mask;
}

bool common_practice_member_from_analysis_note(const AnalysisNote *note, const TimeSpan *segment, BeatStrength strength,
                                               CommonPracticeMember *out) {
    if (note == NULL || segment == NULL || out == NULL || !analysis_note_valid(note) ||
        note->midi == ANALYSIS_PITCH_UNKNOWN) {
        return false;
    }
    Rational onset;
    Rational release;
    Rational from;
    Rational to;
    if (rational_of_musical_time(time_span_start(note->span), &onset) != RATIONAL_OK ||
        rational_of_musical_time(time_span_end(note->span), &release) != RATIONAL_OK ||
        rational_of_musical_time(time_span_start(segment), &from) != RATIONAL_OK ||
        rational_of_musical_time(time_span_end(segment), &to) != RATIONAL_OK) {
        return false;
    }
    Rational start = rational_cmp(onset, from) > 0 ? onset : from;
    Rational stop = rational_cmp(release, to) < 0 ? release : to;
    Rational overlap;
    if (rational_cmp(stop, start) <= 0 || rational_sub(stop, start, &overlap) != RATIONAL_OK) {
        return false;
    }
    out->note_id = note->note_id;
    out->pitch = note->midi;
    out->held = rational_cmp(onset, from) < 0;
    out->duration = rational_to_double(overlap);
    out->strength = strength;
    out->voice_id = note->voice_id;
    out->onset = rational_to_double(onset);
    return true;
}

const char *common_practice_ranked_chord_quality_name(const CommonPracticeRankedChord *chord) {
    int offsets[4];
    size_t count = chord != NULL ? common_practice_ranked_chord_intervals(chord, offsets) : 0;
    if (count == 4) {
        return common_practice_seventh_label(chord->seventh);
    }
    if (count != 3) {
        return NULL;
    }
    if (offsets[1] == 4 && offsets[2] == 7) {
        return "major";
    }
    if (offsets[1] == 3 && offsets[2] == 7) {
        return "minor";
    }
    if (offsets[1] == 3 && offsets[2] == 6) {
        return "diminished";
    }
    if (offsets[1] == 4 && offsets[2] == 8) {
        return "augmented";
    }
    return NULL;
}

int common_practice_ranked_chord_inversion(const CommonPracticeRankedChord *chord, int bass_pitch_class) {
    int offsets[4];
    size_t count = chord != NULL ? common_practice_ranked_chord_intervals(chord, offsets) : 0;
    if (count == 0 || bass_pitch_class < 0 || bass_pitch_class > 11) {
        return -1;
    }
    int bass_offset = ((bass_pitch_class - chord->root_pitch_class) % 12 + 12) % 12;
    for (size_t i = 0; i < count; i++) {
        if (offsets[i] == bass_offset) {
            return (int)i;
        }
    }
    return -1;
}
