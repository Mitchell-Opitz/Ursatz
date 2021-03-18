#include "seventh_identification_rules.h"

#include <stddef.h>

#include "major_scale_rules.h"
#include "rule_pair.h"
#include "seventh_spelling_rules.h"
#include "tonal_pitch.h"

#define NAME_COUNT 4

static const char *const NAMES[STANDARD_TONALITY_SEVENTH_IDENTIFICATION_RULE_COUNT] = {
    "seventh-identification-root-and-quality", "seventh-identification-octave-equivalence",
    "seventh-identification-closed-position"};

static StandardTonalityRulePair g_pairs[STANDARD_TONALITY_SEVENTH_IDENTIFICATION_RULE_COUNT];
static bool g_built[STANDARD_TONALITY_SEVENTH_IDENTIFICATION_RULE_COUNT];

static bool valid_rule(StandardTonalitySeventhIdentificationRule which) {
    return which >= 0 && which < STANDARD_TONALITY_SEVENTH_IDENTIFICATION_RULE_COUNT;
}

static void ensure_built(StandardTonalitySeventhIdentificationRule which) {
    if (!valid_rule(which) || g_built[which]) {
        return;
    }
    g_built[which] = true;
    standard_tonality_build_rule_pair(NAMES[which], &g_pairs[which]);
}

const Rule *standard_tonality_seventh_identification_rule(StandardTonalitySeventhIdentificationRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].rule : NULL;
}

const Constraint *standard_tonality_seventh_identification_constraint(
    StandardTonalitySeventhIdentificationRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].constraint : NULL;
}

static int letter_index(PitchLetter letter) {
    int index = 0;
    PitchLetter walk = PITCH_LETTER_C;
    while (walk != letter) {
        walk = standard_tonality_next_letter(walk);
        index++;
    }
    return index;
}

typedef struct {
    PitchLetter letter;
    int accidental;
    int lowest_octave;
} Name;

/* Collects the distinct note names (letter and accidental, octave aside)
   of the notes. Returns the number found, or -1 when more than 4. */
static int distinct_names(const PitchSpelling *const *notes, size_t count, Name names[NAME_COUNT]) {
    int found = 0;
    for (size_t i = 0; i < count; i++) {
        PitchLetter letter = pitch_spelling_letter(notes[i]);
        int accidental = pitch_spelling_accidental(notes[i]);
        int octave = pitch_spelling_octave(notes[i]);

        int match = -1;
        for (int j = 0; j < found; j++) {
            if (names[j].letter == letter && names[j].accidental == accidental) {
                match = j;
                break;
            }
        }
        if (match >= 0) {
            if (octave < names[match].lowest_octave) {
                names[match].lowest_octave = octave;
            }
            continue;
        }
        if (found == NAME_COUNT) {
            return -1;
        }
        names[found].letter = letter;
        names[found].accidental = accidental;
        names[found].lowest_octave = octave;
        found++;
    }
    return found;
}

/* The root, third, fifth, and seventh among four names: the one whose
   letters two, four, and six above are the other three. */
static bool find_stack(const Name names[NAME_COUNT], int *root, int *third, int *fifth, int *seventh) {
    for (int r = 0; r < NAME_COUNT; r++) {
        int third_index = -1;
        int fifth_index = -1;
        int seventh_index = -1;
        for (int j = 0; j < NAME_COUNT; j++) {
            if (j == r) {
                continue;
            }
            int step = (letter_index(names[j].letter) - letter_index(names[r].letter) + 7) % 7;
            if (step == 2) {
                third_index = j;
            } else if (step == 4) {
                fifth_index = j;
            } else if (step == 6) {
                seventh_index = j;
            }
        }
        if (third_index >= 0 && fifth_index >= 0 && seventh_index >= 0) {
            *root = r;
            *third = third_index;
            *fifth = fifth_index;
            *seventh = seventh_index;
            return true;
        }
    }
    return false;
}

static bool analyze(const PitchSpelling *const *notes, size_t count, Name names[NAME_COUNT], int *root, int *third,
                    int *fifth, int *seventh) {
    if (notes == NULL || count < NAME_COUNT) {
        return false;
    }
    for (size_t i = 0; i < count; i++) {
        if (notes[i] == NULL) {
            return false;
        }
    }
    if (distinct_names(notes, count, names) != NAME_COUNT) {
        return false;
    }
    return find_stack(names, root, third, fifth, seventh);
}

/* Spells the name above the root in closed position: the octave number
   follows the letter, so it is the root's octave plus the octaves the
   letters wrap. */
static bool spell_in_closed_position(const Name *name, const Name *root, int root_octave, int letters_above,
                                     PitchSpelling **out) {
    int octave = root_octave + (letter_index(root->letter) + letters_above) / 7;
    return pitch_spelling_create(name->letter, name->accidental, octave, out) == PITCH_SPELLING_OK;
}

static void destroy_all(PitchSpelling *notes[NAME_COUNT]) {
    for (size_t i = 0; i < NAME_COUNT; i++) {
        if (notes[i] != NULL) {
            pitch_spelling_destroy(notes[i]);
        }
    }
}

/* Stacks the four names above the root at `root_octave`. */
static bool stack_names(const Name names[NAME_COUNT], int root, int third, int fifth, int seventh, int root_octave,
                        PitchSpelling *out[NAME_COUNT]) {
    return pitch_spelling_create(names[root].letter, names[root].accidental, root_octave, &out[0]) ==
               PITCH_SPELLING_OK &&
           spell_in_closed_position(&names[third], &names[root], root_octave, 2, &out[1]) &&
           spell_in_closed_position(&names[fifth], &names[root], root_octave, 4, &out[2]) &&
           spell_in_closed_position(&names[seventh], &names[root], root_octave, 6, &out[3]);
}

bool standard_tonality_identify_seventh(const PitchSpelling *const *notes, size_t count, PitchLetter *root_letter,
                                        int *root_accidental, StandardTonalitySeventhQuality *quality) {
    if (root_letter == NULL || root_accidental == NULL || quality == NULL) {
        return false;
    }
    Name names[NAME_COUNT];
    int root;
    int third;
    int fifth;
    int seventh;
    if (!analyze(notes, count, names, &root, &third, &fifth, &seventh)) {
        return false;
    }

    /* Stack the four above the root (octave 4) and read the intervals. */
    PitchSpelling *stacked[NAME_COUNT] = {NULL, NULL, NULL, NULL};
    StandardTonalitySeventhQuality found = STANDARD_TONALITY_SEVENTH_MAJOR_MAJOR;
    bool ok = stack_names(names, root, third, fifth, seventh, 4, stacked) &&
              standard_tonality_seventh_stack_quality(stacked[0], stacked[1], stacked[2], stacked[3], &found);
    destroy_all(stacked);
    if (!ok) {
        return false;
    }

    *root_letter = names[root].letter;
    *root_accidental = names[root].accidental;
    *quality = found;
    return true;
}

bool standard_tonality_identify_seventh_by_key_signature(const PitchSpelling *const *notes, size_t count,
                                                         PitchLetter *root_letter, int *root_accidental,
                                                         StandardTonalitySeventhQuality *quality) {
    if (root_letter == NULL || root_accidental == NULL || quality == NULL) {
        return false;
    }
    Name names[NAME_COUNT];
    int root;
    int third;
    int fifth;
    int seventh;
    if (!analyze(notes, count, names, &root, &third, &fifth, &seventh)) {
        return false;
    }

    PitchSpelling *root_note;
    if (pitch_spelling_create(names[root].letter, names[root].accidental, 4, &root_note) != PITCH_SPELLING_OK) {
        return false;
    }
    PitchSpelling *scale[STANDARD_TONALITY_MAJOR_SCALE_NOTE_COUNT];
    bool built = standard_tonality_build_major_scale(root_note, scale);
    pitch_spelling_destroy(root_note);
    if (!built) {
        return false;
    }

    /* The third, fifth, and seventh against the key's: how far each is
       from it. */
    int third_shift = names[third].accidental - pitch_spelling_accidental(scale[2]);
    int fifth_shift = names[fifth].accidental - pitch_spelling_accidental(scale[4]);
    int seventh_shift = names[seventh].accidental - pitch_spelling_accidental(scale[6]);
    for (size_t i = 0; i < STANDARD_TONALITY_MAJOR_SCALE_NOTE_COUNT; i++) {
        pitch_spelling_destroy(scale[i]);
    }

    for (int q = 0; q < STANDARD_TONALITY_SEVENTH_QUALITY_COUNT; q++) {
        int want_third;
        int want_fifth;
        int want_seventh;
        if (standard_tonality_seventh_alteration((StandardTonalitySeventhQuality)q, &want_third, &want_fifth,
                                                 &want_seventh) &&
            want_third == third_shift && want_fifth == fifth_shift && want_seventh == seventh_shift) {
            *root_letter = names[root].letter;
            *root_accidental = names[root].accidental;
            *quality = (StandardTonalitySeventhQuality)q;
            return true;
        }
    }
    return false;
}

bool standard_tonality_seventh_closed_position(const PitchSpelling *const *notes, size_t count,
                                               PitchSpelling *out[4]) {
    if (out == NULL) {
        return false;
    }
    Name names[NAME_COUNT];
    int root;
    int third;
    int fifth;
    int seventh;
    if (!analyze(notes, count, names, &root, &third, &fifth, &seventh)) {
        return false;
    }

    PitchSpelling *built[NAME_COUNT] = {NULL, NULL, NULL, NULL};
    if (!stack_names(names, root, third, fifth, seventh, names[root].lowest_octave, built)) {
        destroy_all(built);
        return false;
    }
    for (size_t i = 0; i < NAME_COUNT; i++) {
        out[i] = built[i];
    }
    return true;
}
