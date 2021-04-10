#include "chord_symbol_rules.h"

#include <stdio.h>
#include <string.h>

#include "core/framework_reference.h"
#include "core/provenance_reference.h"
#include "core/value.h"
#include "framework.h"
#include "interpretation/uncertainty.h"
#include "rule_pair.h"
#include "seventh_identification_rules.h"
#include "triad_identification_rules.h"

static const char *const NAMES[STANDARD_TONALITY_CHORD_SYMBOL_RULE_COUNT] = {
    "chord-symbol-root", "chord-symbol-quality", "chord-symbol-slash-bass", "chord-symbol-seventh-quality"};

static StandardTonalityRulePair g_pairs[STANDARD_TONALITY_CHORD_SYMBOL_RULE_COUNT];
static bool g_built[STANDARD_TONALITY_CHORD_SYMBOL_RULE_COUNT];

static bool valid_rule(StandardTonalityChordSymbolRule which) {
    return which >= 0 && which < STANDARD_TONALITY_CHORD_SYMBOL_RULE_COUNT;
}

static void ensure_built(StandardTonalityChordSymbolRule which) {
    if (!valid_rule(which) || g_built[which]) {
        return;
    }
    g_built[which] = true;
    standard_tonality_build_rule_pair(NAMES[which], &g_pairs[which]);
}

const Rule *standard_tonality_chord_symbol_rule(StandardTonalityChordSymbolRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].rule : NULL;
}

const Constraint *standard_tonality_chord_symbol_constraint(StandardTonalityChordSymbolRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].constraint : NULL;
}

const char *standard_tonality_triad_quality_symbol(StandardTonalityTriadQuality quality) {
    switch (quality) {
        case STANDARD_TONALITY_TRIAD_MAJOR:
            return "";
        case STANDARD_TONALITY_TRIAD_MINOR:
            return "mi";
        case STANDARD_TONALITY_TRIAD_DIMINISHED:
            return "\xC2\xB0";
        case STANDARD_TONALITY_TRIAD_AUGMENTED:
            return "+";
        default:
            return NULL;
    }
}

static char letter_char(PitchLetter letter) {
    switch (letter) {
        case PITCH_LETTER_A:
            return 'A';
        case PITCH_LETTER_B:
            return 'B';
        case PITCH_LETTER_C:
            return 'C';
        case PITCH_LETTER_D:
            return 'D';
        case PITCH_LETTER_E:
            return 'E';
        case PITCH_LETTER_F:
            return 'F';
        case PITCH_LETTER_G:
        default:
            return 'G';
    }
}

static const char *accidental_text(int accidental) {
    switch (accidental) {
        case -2:
            return "\xF0\x9D\x84\xAB"; /* double flat */
        case -1:
            return "\xE2\x99\xAD"; /* flat */
        case 0:
            return "";
        case 1:
            return "\xE2\x99\xAF"; /* sharp */
        case 2:
            return "\xF0\x9D\x84\xAA"; /* double sharp */
        default:
            return NULL;
    }
}

/* Writes root, quality mark, and the optional slash bass. */
static bool write_symbol(PitchLetter root_letter, int root_accidental, const char *quality_mark, bool has_bass,
                         PitchLetter bass_letter, int bass_accidental, char *buffer, size_t size) {
    const char *root_mark = accidental_text(root_accidental);
    const char *bass_mark = has_bass ? accidental_text(bass_accidental) : "";
    if (buffer == NULL || size == 0 || root_mark == NULL || quality_mark == NULL || bass_mark == NULL) {
        return false;
    }

    char text[64];
    int length;
    if (has_bass) {
        length = snprintf(text, sizeof(text), "%c%s%s/%c%s", letter_char(root_letter), root_mark, quality_mark,
                          letter_char(bass_letter), bass_mark);
    } else {
        length = snprintf(text, sizeof(text), "%c%s%s", letter_char(root_letter), root_mark, quality_mark);
    }
    if (length < 0 || (size_t)length >= sizeof(text) || (size_t)length >= size) {
        return false;
    }
    memcpy(buffer, text, (size_t)length + 1);
    return true;
}

bool standard_tonality_chord_symbol(PitchLetter root_letter, int root_accidental,
                                    StandardTonalityTriadQuality quality, bool has_bass, PitchLetter bass_letter,
                                    int bass_accidental, char *buffer, size_t size) {
    return write_symbol(root_letter, root_accidental, standard_tonality_triad_quality_symbol(quality), has_bass,
                        bass_letter, bass_accidental, buffer, size);
}

bool standard_tonality_triad_chord_symbol(const PitchSpelling *const *notes, size_t count, char *buffer, size_t size) {
    PitchLetter root_letter;
    int root_accidental;
    StandardTonalityTriadQuality quality;
    if (!standard_tonality_identify_triad(notes, count, &root_letter, &root_accidental, &quality)) {
        return false;
    }

    PitchLetter bass_letter;
    int bass_accidental;
    if (!standard_tonality_lowest_note(notes, count, &bass_letter, &bass_accidental)) {
        return false;
    }

    bool has_bass = bass_letter != root_letter || bass_accidental != root_accidental;
    return standard_tonality_chord_symbol(root_letter, root_accidental, quality, has_bass, bass_letter,
                                          bass_accidental, buffer, size);
}

const char *standard_tonality_seventh_quality_symbol(StandardTonalitySeventhQuality quality) {
    switch (quality) {
        case STANDARD_TONALITY_SEVENTH_MAJOR_MAJOR:
            return "ma7";
        case STANDARD_TONALITY_SEVENTH_MAJOR_MINOR:
            return "7";
        case STANDARD_TONALITY_SEVENTH_MINOR_MINOR:
            return "mi7";
        case STANDARD_TONALITY_SEVENTH_HALF_DIMINISHED:
            return "\xE2\x88\x85" "7";
        case STANDARD_TONALITY_SEVENTH_FULLY_DIMINISHED:
            return "\xC2\xB0" "7";
        default:
            return NULL;
    }
}

bool standard_tonality_seventh_chord_symbol_text(PitchLetter root_letter, int root_accidental,
                                                 StandardTonalitySeventhQuality quality, bool has_bass,
                                                 PitchLetter bass_letter, int bass_accidental, char *buffer,
                                                 size_t size) {
    return write_symbol(root_letter, root_accidental, standard_tonality_seventh_quality_symbol(quality), has_bass,
                        bass_letter, bass_accidental, buffer, size);
}

bool standard_tonality_seventh_chord_symbol(const PitchSpelling *const *notes, size_t count, char *buffer,
                                            size_t size) {
    PitchLetter root_letter;
    int root_accidental;
    StandardTonalitySeventhQuality quality;
    if (!standard_tonality_identify_seventh(notes, count, &root_letter, &root_accidental, &quality)) {
        return false;
    }

    PitchLetter bass_letter;
    int bass_accidental;
    if (!standard_tonality_lowest_note(notes, count, &bass_letter, &bass_accidental)) {
        return false;
    }

    bool has_bass = bass_letter != root_letter || bass_accidental != root_accidental;
    return standard_tonality_seventh_chord_symbol_text(root_letter, root_accidental, quality, has_bass, bass_letter,
                                                       bass_accidental, buffer, size);
}

static bool parse_letter(const char *text, PitchLetter *letter) {
    switch (text[0]) {
        case 'A':
            *letter = PITCH_LETTER_A;
            return true;
        case 'B':
            *letter = PITCH_LETTER_B;
            return true;
        case 'C':
            *letter = PITCH_LETTER_C;
            return true;
        case 'D':
            *letter = PITCH_LETTER_D;
            return true;
        case 'E':
            *letter = PITCH_LETTER_E;
            return true;
        case 'F':
            *letter = PITCH_LETTER_F;
            return true;
        case 'G':
            *letter = PITCH_LETTER_G;
            return true;
        default:
            return false;
    }
}

/* Reads one accidental (or none) and returns how many bytes it took. */
static size_t parse_accidental(const char *text, int *accidental) {
    if (strncmp(text, "\xF0\x9D\x84\xAB", 4) == 0) {
        *accidental = -2;
        return 4;
    }
    if (strncmp(text, "\xF0\x9D\x84\xAA", 4) == 0) {
        *accidental = 2;
        return 4;
    }
    if (strncmp(text, "\xE2\x99\xAD", 3) == 0) {
        *accidental = -1;
        return 3;
    }
    if (strncmp(text, "\xE2\x99\xAF", 3) == 0) {
        *accidental = 1;
        return 3;
    }
    if (text[0] == 'b') {
        *accidental = -1;
        return 1;
    }
    if (text[0] == '#') {
        *accidental = 1;
        return 1;
    }
    *accidental = 0;
    return 0;
}

bool standard_tonality_parse_chord_symbol(const char *symbol, PitchLetter *root_letter, int *root_accidental,
                                          StandardTonalityTriadQuality *quality, bool *has_bass,
                                          PitchLetter *bass_letter, int *bass_accidental) {
    if (symbol == NULL || root_letter == NULL || root_accidental == NULL || quality == NULL || has_bass == NULL ||
        bass_letter == NULL || bass_accidental == NULL) {
        return false;
    }

    const char *cursor = symbol;
    PitchLetter letter;
    if (!parse_letter(cursor, &letter)) {
        return false;
    }
    cursor++;

    int accidental;
    cursor += parse_accidental(cursor, &accidental);

    StandardTonalityTriadQuality parsed_quality = STANDARD_TONALITY_TRIAD_MAJOR;
    if (strncmp(cursor, "mi", 2) == 0) {
        parsed_quality = STANDARD_TONALITY_TRIAD_MINOR;
        cursor += 2;
    } else if (strncmp(cursor, "\xC2\xB0", 2) == 0) {
        parsed_quality = STANDARD_TONALITY_TRIAD_DIMINISHED;
        cursor += 2;
    } else if (cursor[0] == 'o') {
        parsed_quality = STANDARD_TONALITY_TRIAD_DIMINISHED;
        cursor += 1;
    } else if (cursor[0] == '+') {
        parsed_quality = STANDARD_TONALITY_TRIAD_AUGMENTED;
        cursor += 1;
    }

    bool bass = false;
    PitchLetter parsed_bass_letter = PITCH_LETTER_C;
    int parsed_bass_accidental = 0;
    if (cursor[0] == '/') {
        cursor++;
        if (!parse_letter(cursor, &parsed_bass_letter)) {
            return false;
        }
        cursor++;
        cursor += parse_accidental(cursor, &parsed_bass_accidental);
        bass = true;
    }
    if (cursor[0] != '\0') {
        return false;
    }

    *root_letter = letter;
    *root_accidental = accidental;
    *quality = parsed_quality;
    *has_bass = bass;
    if (bass) {
        *bass_letter = parsed_bass_letter;
        *bass_accidental = parsed_bass_accidental;
    }
    return true;
}

bool standard_tonality_parse_seventh_chord_symbol(const char *symbol, PitchLetter *root_letter,
                                                  int *root_accidental, StandardTonalitySeventhQuality *quality,
                                                  bool *has_bass, PitchLetter *bass_letter, int *bass_accidental) {
    if (symbol == NULL || root_letter == NULL || root_accidental == NULL || quality == NULL || has_bass == NULL ||
        bass_letter == NULL || bass_accidental == NULL) {
        return false;
    }

    const char *cursor = symbol;
    PitchLetter letter;
    if (!parse_letter(cursor, &letter)) {
        return false;
    }
    cursor++;

    int accidental;
    cursor += parse_accidental(cursor, &accidental);

    StandardTonalitySeventhQuality parsed_quality;
    if (strncmp(cursor, "ma7", 3) == 0) {
        parsed_quality = STANDARD_TONALITY_SEVENTH_MAJOR_MAJOR;
        cursor += 3;
    } else if (strncmp(cursor, "mi7", 3) == 0) {
        parsed_quality = STANDARD_TONALITY_SEVENTH_MINOR_MINOR;
        cursor += 3;
    } else if (strncmp(cursor, "\xE2\x88\x85" "7", 4) == 0) {
        parsed_quality = STANDARD_TONALITY_SEVENTH_HALF_DIMINISHED;
        cursor += 4;
    } else if (strncmp(cursor, "\xC3\xB8" "7", 3) == 0) {
        parsed_quality = STANDARD_TONALITY_SEVENTH_HALF_DIMINISHED;
        cursor += 3;
    } else if (strncmp(cursor, "\xC2\xB0" "7", 3) == 0) {
        parsed_quality = STANDARD_TONALITY_SEVENTH_FULLY_DIMINISHED;
        cursor += 3;
    } else if (strncmp(cursor, "o7", 2) == 0) {
        parsed_quality = STANDARD_TONALITY_SEVENTH_FULLY_DIMINISHED;
        cursor += 2;
    } else if (cursor[0] == '7') {
        parsed_quality = STANDARD_TONALITY_SEVENTH_MAJOR_MINOR;
        cursor += 1;
    } else {
        return false;
    }

    bool bass = false;
    PitchLetter parsed_bass_letter = PITCH_LETTER_C;
    int parsed_bass_accidental = 0;
    if (cursor[0] == '/') {
        cursor++;
        if (!parse_letter(cursor, &parsed_bass_letter)) {
            return false;
        }
        cursor++;
        cursor += parse_accidental(cursor, &parsed_bass_accidental);
        bass = true;
    }
    if (cursor[0] != '\0') {
        return false;
    }

    *root_letter = letter;
    *root_accidental = accidental;
    *quality = parsed_quality;
    *has_bass = bass;
    if (bass) {
        *bass_letter = parsed_bass_letter;
        *bass_accidental = parsed_bass_accidental;
    }
    return true;
}

ChordResult standard_tonality_build_chord_from_label(const char *label, const char *vocabulary, EntityID *target_id,
                                                     Chord **out) {
    const Framework *framework = standard_tonality_framework();
    if (framework == NULL) {
        return CHORD_ERR_ALLOC;
    }

    char interpretation_id_value[128];
    snprintf(interpretation_id_value, sizeof(interpretation_id_value), "standard-tonality-chord-%s", label);

    EntityID *interpretation_id;
    if (entity_id_create(interpretation_id_value, &interpretation_id) != ENTITY_ID_OK) {
        return CHORD_ERR_ALLOC;
    }

    EntityID *framework_target_id;
    if (entity_id_create(entity_id_value(framework_id(framework)), &framework_target_id) != ENTITY_ID_OK) {
        entity_id_destroy(interpretation_id);
        return CHORD_ERR_ALLOC;
    }
    FrameworkReference *framework_reference;
    if (framework_reference_create(framework_target_id, &framework_reference) != FRAMEWORK_REFERENCE_OK) {
        entity_id_destroy(framework_target_id);
        entity_id_destroy(interpretation_id);
        return CHORD_ERR_ALLOC;
    }

    Value *claim;
    if (value_create("standard-tonality/chord-symbol", label, strlen(label) + 1, &claim) != VALUE_OK) {
        framework_reference_destroy(framework_reference);
        entity_id_destroy(interpretation_id);
        return CHORD_ERR_ALLOC;
    }

    Uncertainty *uncertainty;
    if (uncertainty_create_scalar_confidence(1, 1, &uncertainty) != UNCERTAINTY_OK) {
        value_destroy(claim);
        framework_reference_destroy(framework_reference);
        entity_id_destroy(interpretation_id);
        return CHORD_ERR_ALLOC;
    }

    ProvenanceReference *provenance;
    if (provenance_reference_create_absent(&provenance) != PROVENANCE_REFERENCE_OK) {
        uncertainty_destroy(uncertainty);
        value_destroy(claim);
        framework_reference_destroy(framework_reference);
        entity_id_destroy(interpretation_id);
        return CHORD_ERR_ALLOC;
    }

    Interpretation *interpretation;
    InterpretationResult interpretation_result =
        interpretation_create(interpretation_id, target_id, claim, NULL, NULL, framework_reference, uncertainty,
                               "standard-tonality-v1", vocabulary,
                               INTERPRETATION_STATUS_MACHINE_GENERATED, provenance, &interpretation);
    if (interpretation_result != INTERPRETATION_OK) {
        provenance_reference_destroy(provenance);
        uncertainty_destroy(uncertainty);
        value_destroy(claim);
        framework_reference_destroy(framework_reference);
        entity_id_destroy(interpretation_id);
        return CHORD_ERR_ALLOC;
    }

    ChordResult chord_result = chord_create(interpretation, label, out);
    if (chord_result != CHORD_OK) {
        interpretation_destroy(interpretation);
        return chord_result;
    }
    return CHORD_OK;
}

ChordResult standard_tonality_build_triad_chord(PitchLetter root_letter, int root_accidental,
                                                StandardTonalityTriadQuality quality, bool has_bass,
                                                PitchLetter bass_letter, int bass_accidental, EntityID *target_id,
                                                Chord **out) {
    char label[64];
    if (!standard_tonality_chord_symbol(root_letter, root_accidental, quality, has_bass, bass_letter, bass_accidental,
                                        label, sizeof(label))) {
        return CHORD_ERR_NULL_LABEL;
    }
    return standard_tonality_build_chord_from_label(label, "triad-chord-symbol-vocabulary", target_id, out);
}

ChordResult standard_tonality_build_seventh_chord(PitchLetter root_letter, int root_accidental,
                                                  StandardTonalitySeventhQuality quality, bool has_bass,
                                                  PitchLetter bass_letter, int bass_accidental, EntityID *target_id,
                                                  Chord **out) {
    char label[64];
    if (!standard_tonality_seventh_chord_symbol_text(root_letter, root_accidental, quality, has_bass, bass_letter,
                                                     bass_accidental, label, sizeof(label))) {
        return CHORD_ERR_NULL_LABEL;
    }
    return standard_tonality_build_chord_from_label(label, "seventh-chord-symbol-vocabulary", target_id, out);
}
