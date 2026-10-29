#include "interval_quality_rules.h"

#include <stddef.h>

#include "interval_size_rules.h"
#include "major_scale_rules.h"
#include "pitch/generic_interval.h"
#include "rule_pair.h"
#include "tonal_pitch.h"

static const char *const NAMES[STANDARD_TONALITY_INTERVAL_QUALITY_RULE_COUNT] = {
    "interval-quality-classes",     "interval-quality-major-scale-method", "interval-quality-augmented",
    "interval-quality-diminished",  "interval-quality-further-altered",    "interval-quality-compound"};

static RulePairTable g_table = RULE_PAIR_TABLE_INIT(standard_tonality_framework, "standard-tonality",
    NAMES, STANDARD_TONALITY_INTERVAL_QUALITY_RULE_COUNT);

static bool valid_rule(StandardTonalityIntervalQualityRule which) {
    return which >= 0 && which < STANDARD_TONALITY_INTERVAL_QUALITY_RULE_COUNT;
}

const Rule *standard_tonality_interval_quality_rule(StandardTonalityIntervalQualityRule which) {
    const RulePair *pair = valid_rule(which) ? rule_pair_table_get(&g_table, (size_t)which) : NULL;
    return pair != NULL ? pair->rule : NULL;
}

const Constraint *standard_tonality_interval_quality_constraint(StandardTonalityIntervalQualityRule which) {
    const RulePair *pair = valid_rule(which) ? rule_pair_table_get(&g_table, (size_t)which) : NULL;
    return pair != NULL ? pair->constraint : NULL;
}

static bool valid_quality(StandardTonalityIntervalQuality quality) {
    return quality >= 0 && quality < STANDARD_TONALITY_QUALITY_COUNT;
}

static const char *const QUALITY_NAMES[STANDARD_TONALITY_QUALITY_COUNT] = {
    "triply diminished", "doubly diminished", "diminished", "minor",           "perfect",
    "major",             "augmented",         "doubly augmented", "triply augmented"};

static const char *const QUALITY_ABBREVIATIONS[STANDARD_TONALITY_QUALITY_COUNT] = {"ddd", "dd", "d",  "mi", "P",
                                                                                    "ma",  "A",  "AA", "AAA"};

const char *standard_tonality_interval_quality_name(StandardTonalityIntervalQuality quality) {
    return valid_quality(quality) ? QUALITY_NAMES[quality] : NULL;
}

const char *standard_tonality_interval_quality_abbreviation(StandardTonalityIntervalQuality quality) {
    return valid_quality(quality) ? QUALITY_ABBREVIATIONS[quality] : NULL;
}

bool standard_tonality_interval_quality_is_augmented(StandardTonalityIntervalQuality quality) {
    return quality == STANDARD_TONALITY_QUALITY_AUGMENTED || quality == STANDARD_TONALITY_QUALITY_DOUBLY_AUGMENTED ||
           quality == STANDARD_TONALITY_QUALITY_TRIPLY_AUGMENTED;
}

bool standard_tonality_interval_quality_is_diminished(StandardTonalityIntervalQuality quality) {
    return quality == STANDARD_TONALITY_QUALITY_DIMINISHED || quality == STANDARD_TONALITY_QUALITY_DOUBLY_DIMINISHED ||
           quality == STANDARD_TONALITY_QUALITY_TRIPLY_DIMINISHED;
}

bool standard_tonality_interval_size_is_perfect_class(int size) {
    int simple = standard_tonality_interval_simple_size(size);
    return simple == 1 || simple == 4 || simple == 5 || simple == 8;
}

bool standard_tonality_interval_quality_fits_size(int size, StandardTonalityIntervalQuality quality) {
    if (size < 1 || !valid_quality(quality)) {
        return false;
    }
    if (quality == STANDARD_TONALITY_QUALITY_PERFECT) {
        return standard_tonality_interval_size_is_perfect_class(size);
    }
    if (quality == STANDARD_TONALITY_QUALITY_MAJOR || quality == STANDARD_TONALITY_QUALITY_MINOR) {
        return !standard_tonality_interval_size_is_perfect_class(size);
    }
    return true;
}

static const int MAJOR_SEMITONES[8] = {0, 2, 4, 5, 7, 9, 11, 12};

int standard_tonality_major_interval_semitones(int simple_size) {
    if (simple_size < 1 || simple_size > 8) {
        return 0;
    }
    return MAJOR_SEMITONES[simple_size - 1];
}

/* Half steps from the perfect or major interval: perfect and major are 0,
   augmented +1, minor -1 (major/minor sizes) and diminished -1 (perfect
   sizes) or -2 (major/minor sizes), and so on. The caller has checked
   the quality fits the size. */
static int offset_from_reference(int size, StandardTonalityIntervalQuality quality) {
    bool perfect_class = standard_tonality_interval_size_is_perfect_class(size);
    switch (quality) {
        case STANDARD_TONALITY_QUALITY_PERFECT:
        case STANDARD_TONALITY_QUALITY_MAJOR:
            return 0;
        case STANDARD_TONALITY_QUALITY_MINOR:
            return -1;
        case STANDARD_TONALITY_QUALITY_AUGMENTED:
            return 1;
        case STANDARD_TONALITY_QUALITY_DOUBLY_AUGMENTED:
            return 2;
        case STANDARD_TONALITY_QUALITY_TRIPLY_AUGMENTED:
            return 3;
        case STANDARD_TONALITY_QUALITY_DIMINISHED:
            return perfect_class ? -1 : -2;
        case STANDARD_TONALITY_QUALITY_DOUBLY_DIMINISHED:
            return perfect_class ? -2 : -3;
        case STANDARD_TONALITY_QUALITY_TRIPLY_DIMINISHED:
        default:
            return perfect_class ? -3 : -4;
    }
}

/* The quality at `offset` half steps from the reference, or false if
   beyond triply augmented or diminished. */
static bool quality_from_offset(int size, int offset, StandardTonalityIntervalQuality *out) {
    bool perfect_class = standard_tonality_interval_size_is_perfect_class(size);
    int lowest = perfect_class ? -3 : -4;
    if (offset < lowest || offset > 3) {
        return false;
    }
    if (offset == 0) {
        *out = perfect_class ? STANDARD_TONALITY_QUALITY_PERFECT : STANDARD_TONALITY_QUALITY_MAJOR;
    } else if (offset > 0) {
        *out = (StandardTonalityIntervalQuality)(STANDARD_TONALITY_QUALITY_AUGMENTED + offset - 1);
    } else if (!perfect_class && offset == -1) {
        *out = STANDARD_TONALITY_QUALITY_MINOR;
    } else {
        int below_minor_or_perfect = perfect_class ? -offset : -offset - 1; /* 1 diminished ... 3 triply */
        *out = (StandardTonalityIntervalQuality)(STANDARD_TONALITY_QUALITY_DIMINISHED - (below_minor_or_perfect - 1));
    }
    return true;
}

bool standard_tonality_interval_quality_shift(int size, StandardTonalityIntervalQuality quality, int semitones,
                                              StandardTonalityIntervalQuality *out) {
    if (out == NULL || !standard_tonality_interval_quality_fits_size(size, quality)) {
        return false;
    }
    return quality_from_offset(size, offset_from_reference(size, quality) + semitones, out);
}

bool standard_tonality_interval_semitones(int size, StandardTonalityIntervalQuality quality, int *semitones) {
    if (semitones == NULL || !standard_tonality_interval_quality_fits_size(size, quality)) {
        return false;
    }
    int simple = standard_tonality_interval_simple_size(size);
    int octaves = (size - simple) / 7;
    *semitones = standard_tonality_major_interval_semitones(simple) + STANDARD_TONALITY_SEMITONES_PER_OCTAVE * octaves +
                 offset_from_reference(size, quality);
    return true;
}

bool standard_tonality_interval_quality(const PitchSpelling *lower, const PitchSpelling *upper, int *size,
                                        StandardTonalityIntervalQuality *quality) {
    if (size == NULL || quality == NULL) {
        return false;
    }
    int generic;
    if (!standard_tonality_interval_generic_size(lower, upper, &generic)) {
        return false;
    }
    int semitones = standard_tonality_semitone_number(upper) - standard_tonality_semitone_number(lower);
    if (semitones < 0) {
        return false;
    }

    int simple = standard_tonality_interval_simple_size(generic);
    int octaves = (generic - simple) / 7;
    int offset = semitones - STANDARD_TONALITY_SEMITONES_PER_OCTAVE * octaves - standard_tonality_major_interval_semitones(simple);

    StandardTonalityIntervalQuality found;
    if (!quality_from_offset(generic, offset, &found)) {
        return false;
    }
    *size = generic;
    *quality = found;
    return true;
}

/* Builds the major scale on `lower` and returns the half-step offset of
   `upper` from its note a size above, with the generic size. */
static bool offset_from_major_scale(const PitchSpelling *lower, const PitchSpelling *upper, int *size, int *offset) {
    int generic;
    if (!standard_tonality_interval_generic_size(lower, upper, &generic)) {
        return false;
    }

    PitchSpelling *scale[STANDARD_TONALITY_MAJOR_SCALE_NOTE_COUNT];
    if (!standard_tonality_build_major_scale(lower, scale)) {
        return false;
    }

    int simple = standard_tonality_interval_simple_size(generic);
    int octaves = (generic - simple) / 7;
    int scale_note = standard_tonality_semitone_number(scale[simple - 1]) + STANDARD_TONALITY_SEMITONES_PER_OCTAVE * octaves;
    for (int i = 0; i < STANDARD_TONALITY_MAJOR_SCALE_NOTE_COUNT; i++) {
        pitch_spelling_destroy(scale[i]);
    }

    *size = generic;
    *offset = standard_tonality_semitone_number(upper) - scale_note;
    return true;
}

bool standard_tonality_interval_quality_by_major_scale(const PitchSpelling *lower, const PitchSpelling *upper,
                                                       int *size, StandardTonalityIntervalQuality *quality) {
    if (size == NULL || quality == NULL) {
        return false;
    }
    int generic;
    int offset;
    if (!offset_from_major_scale(lower, upper, &generic, &offset)) {
        return false;
    }
    StandardTonalityIntervalQuality found;
    if (!quality_from_offset(generic, offset, &found)) {
        return false;
    }
    *size = generic;
    *quality = found;
    return true;
}

bool standard_tonality_interval_top_in_major_scale(const PitchSpelling *lower, const PitchSpelling *upper) {
    int generic;
    int offset;
    if (!offset_from_major_scale(lower, upper, &generic, &offset)) {
        return false;
    }
    return offset == 0;
}
