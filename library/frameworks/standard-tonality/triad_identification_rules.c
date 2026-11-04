#include "triad_identification_rules.h"

#include <stddef.h>

#include "interval_quality_rules.h"
#include "major_scale_rules.h"
#include "rule_pair.h"
#include "tonal_pitch.h"
#include "triad_spelling_rules.h"
#include "pitch/pitch_arithmetic.h"

static const char *const NAMES[STANDARD_TONALITY_TRIAD_IDENTIFICATION_RULE_COUNT] = {
    "triad-identification-root-and-quality", "triad-identification-octave-equivalence",
    "triad-identification-closed-position"};

static RulePairTable g_table = RULE_PAIR_TABLE_INIT(standard_tonality_framework, "standard-tonality",
    NAMES, STANDARD_TONALITY_TRIAD_IDENTIFICATION_RULE_COUNT);

URSATZ_RULE_ACCESSORS(standard_tonality_triad_identification, StandardTonalityTriadIdentificationRule, STANDARD_TONALITY_TRIAD_IDENTIFICATION_RULE_COUNT, g_table)

typedef struct {
    PitchLetter letter;
    int accidental;
    int lowest_octave;
} Name;

/* Collects the distinct note names (letter and accidental, octave aside)
   of the notes. Returns the number found, or -1 when more than 3. */
static int distinct_names(const PitchSpelling *const *notes, size_t count, Name names[3]) {
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
        if (found == 3) {
            return -1;
        }
        names[found].letter = letter;
        names[found].accidental = accidental;
        names[found].lowest_octave = octave;
        found++;
    }
    return found;
}

/* The root, third, and fifth among three names: the one whose letters two
   and four above are the other two. */
static bool find_stack(const Name names[3], int *root, int *third, int *fifth) {
    for (int r = 0; r < 3; r++) {
        int third_index = -1;
        int fifth_index = -1;
        for (int j = 0; j < 3; j++) {
            if (j == r) {
                continue;
            }
            int step = (pitch_letter_index(names[j].letter) - pitch_letter_index(names[r].letter) + 7) % 7;
            if (step == 2) {
                third_index = j;
            } else if (step == 4) {
                fifth_index = j;
            }
        }
        if (third_index >= 0 && fifth_index >= 0) {
            *root = r;
            *third = third_index;
            *fifth = fifth_index;
            return true;
        }
    }
    return false;
}

static bool analyze(const PitchSpelling *const *notes, size_t count, Name names[3], int *root, int *third,
                    int *fifth) {
    if (notes == NULL || count < 3) {
        return false;
    }
    for (size_t i = 0; i < count; i++) {
        if (notes[i] == NULL) {
            return false;
        }
    }
    if (distinct_names(notes, count, names) != 3) {
        return false;
    }
    return find_stack(names, root, third, fifth);
}

/* Spells the name above the root in closed position: the octave number
   follows the letter, so it is the root's octave plus the octaves the
   letters wrap. */
static bool spell_in_closed_position(const Name *name, const Name *root, int root_octave, int letters_above,
                                     PitchSpelling **out) {
    int octave = root_octave + (pitch_letter_index(root->letter) + letters_above) / 7;
    (void)name;
    return pitch_spelling_create(name->letter, name->accidental, octave, out) == PITCH_SPELLING_OK;
}

bool standard_tonality_identify_triad(const PitchSpelling *const *notes, size_t count, PitchLetter *root_letter,
                                      int *root_accidental, StandardTonalityTriadQuality *quality) {
    if (root_letter == NULL || root_accidental == NULL || quality == NULL) {
        return false;
    }
    Name names[3];
    int root;
    int third;
    int fifth;
    if (!analyze(notes, count, names, &root, &third, &fifth)) {
        return false;
    }

    /* Stack the three above the root (octave 4) and read the intervals. */
    PitchSpelling *root_note = NULL;
    PitchSpelling *third_note = NULL;
    PitchSpelling *fifth_note = NULL;
    bool ok = pitch_spelling_create(names[root].letter, names[root].accidental, 4, &root_note) == PITCH_SPELLING_OK &&
              spell_in_closed_position(&names[third], &names[root], 4, 2, &third_note) &&
              spell_in_closed_position(&names[fifth], &names[root], 4, 4, &fifth_note);

    StandardTonalityTriadQuality found = STANDARD_TONALITY_TRIAD_MAJOR;
    if (ok) {
        ok = standard_tonality_triad_stack_quality(root_note, third_note, fifth_note, &found);
    }

    if (root_note != NULL) {
        pitch_spelling_destroy(root_note);
    }
    if (third_note != NULL) {
        pitch_spelling_destroy(third_note);
    }
    if (fifth_note != NULL) {
        pitch_spelling_destroy(fifth_note);
    }
    if (!ok) {
        return false;
    }

    *root_letter = names[root].letter;
    *root_accidental = names[root].accidental;
    *quality = found;
    return true;
}

bool standard_tonality_identify_triad_by_key_signature(const PitchSpelling *const *notes, size_t count,
                                                       PitchLetter *root_letter, int *root_accidental,
                                                       StandardTonalityTriadQuality *quality) {
    if (root_letter == NULL || root_accidental == NULL || quality == NULL) {
        return false;
    }
    Name names[3];
    int root;
    int third;
    int fifth;
    if (!analyze(notes, count, names, &root, &third, &fifth)) {
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

    /* The third and fifth against the key's: how far each is from it. */
    int third_shift = names[third].accidental - pitch_spelling_accidental(scale[2]);
    int fifth_shift = names[fifth].accidental - pitch_spelling_accidental(scale[4]);
    for (size_t i = 0; i < STANDARD_TONALITY_MAJOR_SCALE_NOTE_COUNT; i++) {
        pitch_spelling_destroy(scale[i]);
    }

    for (int q = 0; q < STANDARD_TONALITY_TRIAD_QUALITY_COUNT; q++) {
        int want_third;
        int want_fifth;
        if (standard_tonality_triad_alteration((StandardTonalityTriadQuality)q, &want_third, &want_fifth) &&
            want_third == third_shift && want_fifth == fifth_shift) {
            *root_letter = names[root].letter;
            *root_accidental = names[root].accidental;
            *quality = (StandardTonalityTriadQuality)q;
            return true;
        }
    }
    return false;
}

bool standard_tonality_triad_closed_position(const PitchSpelling *const *notes, size_t count, PitchSpelling *out[3]) {
    if (out == NULL) {
        return false;
    }
    Name names[3];
    int root;
    int third;
    int fifth;
    if (!analyze(notes, count, names, &root, &third, &fifth)) {
        return false;
    }

    PitchSpelling *built[3] = {NULL, NULL, NULL};
    int root_octave = names[root].lowest_octave;
    bool ok = pitch_spelling_create(names[root].letter, names[root].accidental, root_octave, &built[0]) ==
                  PITCH_SPELLING_OK &&
              spell_in_closed_position(&names[third], &names[root], root_octave, 2, &built[1]) &&
              spell_in_closed_position(&names[fifth], &names[root], root_octave, 4, &built[2]);
    if (!ok) {
        for (size_t i = 0; i < 3; i++) {
            if (built[i] != NULL) {
                pitch_spelling_destroy(built[i]);
            }
        }
        return false;
    }
    for (size_t i = 0; i < 3; i++) {
        out[i] = built[i];
    }
    return true;
}

bool standard_tonality_lowest_note(const PitchSpelling *const *notes, size_t count, PitchLetter *letter,
                                   int *accidental) {
    if (notes == NULL || count == 0 || letter == NULL || accidental == NULL) {
        return false;
    }
    size_t lowest = 0;
    for (size_t i = 0; i < count; i++) {
        if (notes[i] == NULL) {
            return false;
        }
        if (standard_tonality_semitone_number(notes[i]) < standard_tonality_semitone_number(notes[lowest])) {
            lowest = i;
        }
    }
    *letter = pitch_spelling_letter(notes[lowest]);
    *accidental = pitch_spelling_accidental(notes[lowest]);
    return true;
}
