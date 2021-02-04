#ifndef URSATZ_FRAMEWORKS_STANDARD_TONALITY_CHORD_SYMBOL_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_TONALITY_CHORD_SYMBOL_RULES_H

#include <stdbool.h>
#include <stddef.h>

#include "identity/entity_id.h"
#include "interpretation/chord.h"
#include "pitch/pitch_spelling.h"
#include "rules/constraint.h"
#include "rules/rule.h"
#include "triad_rules.h"

/*
 * Chord symbol rules for standard-tonality-v1. A chord symbol for a
 * triad is the letter name of the root, the accidental if it has one,
 * an indication of the quality, and sometimes the bass:
 *
 *   ROOT           begins with a capital letter and, if needed, an
 *                  accidental: B-flat, F-sharp
 *   QUALITY        major: no symbol. minor: lowercase "mi". diminished:
 *                  a superscript circle (the degree sign, o). augmented:
 *                  a plus sign. C, Cmi, Co, C+ are C major, minor,
 *                  diminished, augmented; B-flat minor is "B♭mi"; F-sharp
 *                  diminished is "F♯°"
 *   SLASH_BASS     when a note other than the root is the lowest, a slash
 *                  and the capital letter (with accidental) of that
 *                  bass note follow: Cmi/E♭, Cmi/G
 *
 * Open Music Theory's abbreviations; other contexts use others (a larger
 * list is a separate chapter's). Symbols are UTF-8: the flat and sharp
 * signs (♭ ♯), double flat and double sharp (𝄫 𝄪), and the degree sign
 * (°). Parsing also takes the plain letters "b" and "#" for the
 * accidentals and "o" for the circle.
 *
 * Inversion beyond naming the bass is a later chapter's.
 */

typedef enum {
    STANDARD_TONALITY_CHORD_SYMBOL_RULE_ROOT = 0,
    STANDARD_TONALITY_CHORD_SYMBOL_RULE_QUALITY,
    STANDARD_TONALITY_CHORD_SYMBOL_RULE_SLASH_BASS,
    STANDARD_TONALITY_CHORD_SYMBOL_RULE_COUNT
} StandardTonalityChordSymbolRule;

const Rule *standard_tonality_chord_symbol_rule(StandardTonalityChordSymbolRule which);
const Constraint *standard_tonality_chord_symbol_constraint(StandardTonalityChordSymbolRule which);

/* QUALITY: "" for major, "mi" minor, "\xC2\xB0" (the degree sign)
   diminished, "+" augmented. NULL for an invalid quality. */
const char *standard_tonality_triad_quality_symbol(StandardTonalityTriadQuality quality);

/*
 * ROOT / QUALITY / SLASH_BASS: writes the chord symbol into buffer (size
 * bytes, NUL-terminated). With has_bass the symbol ends in a slash and
 * the bass note's name. False for an invalid quality or accidental
 * (outside -2..+2), a NULL buffer, or a buffer too small.
 */
bool standard_tonality_chord_symbol(PitchLetter root_letter, int root_accidental,
                                    StandardTonalityTriadQuality quality, bool has_bass, PitchLetter bass_letter,
                                    int bass_accidental, char *buffer, size_t size);

/*
 * The symbol of the triad the notes make: the root and quality from
 * standard_tonality_identify_triad, and a slash bass only when the lowest
 * note is not the root. False for the same reasons, or a buffer too
 * small.
 */
bool standard_tonality_triad_chord_symbol(const PitchSpelling *const *notes, size_t count, char *buffer, size_t size);

/*
 * Reads a chord symbol. On true the outputs hold the root, quality, and
 * the slash bass if any (*has_bass false and the bass outputs untouched
 * when there is none). False (outputs untouched) for NULL, an empty or
 * malformed symbol, an unknown quality mark, or trailing text.
 */
bool standard_tonality_parse_chord_symbol(const char *symbol, PitchLetter *root_letter, int *root_accidental,
                                          StandardTonalityTriadQuality *quality, bool *has_bass,
                                          PitchLetter *bass_letter, int *bass_accidental);

/*
 * Builds a real, populated Chord labeled with the symbol of the triad
 * (root, quality, optional bass), claiming that the thing named by
 * target_id (a Sonority, say) is that chord under standard-tonality-v1.
 * Takes ownership of target_id on success (destroyed with the Chord);
 * on failure the caller still owns it. ChordResult errors as in
 * interpretation/chord.h; an invalid quality or accidental is
 * CHORD_ERR_NULL_LABEL.
 */
ChordResult standard_tonality_build_triad_chord(PitchLetter root_letter, int root_accidental,
                                                StandardTonalityTriadQuality quality, bool has_bass,
                                                PitchLetter bass_letter, int bass_accidental, EntityID *target_id,
                                                Chord **out);

#endif
