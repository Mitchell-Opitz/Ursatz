#ifndef MUSIC_SEMANTICS_ANALYSIS_KEY_H
#define MUSIC_SEMANTICS_ANALYSIS_KEY_H

#include <stdbool.h>

#include "model/interpretation/key.h"
#include "model/pitch/pitch_material.h"
#include "model/pitch/pitch_spelling.h"
#include "model/time/musical_time.h"

/*
 * AnalysisKey: the neutral key framework adapters accept (R-242): a tonic spelled as letter and
 * accidental, a mode and the pitch material. KeyStructureMode (interpretation/key.h) is major or minor; the minor forms
 * (natural, harmonic, melodic) stay framework theory. `material` is the key's pitch material, a pointer into the knowledge snapshot
 * (or, until P58 B13, the builtin common-practice materials) that outlives the key; every maker sets it and `mode` is read from its
 * third (key_structure_mode_of). A key written as a literal by a test or the Composer's mode-based code may leave it NULL. id is
 * borrowed and may be NULL.
 *
 * AnalysisKeyEvent places an AnalysisKey at a time (borrowed) for key-change input.
 */
typedef struct {
    const char *id;
    PitchLetter tonic_letter;
    int tonic_accidental;
    KeyStructureMode mode;
    const PitchMaterial *material;
} AnalysisKey;

typedef struct {
    MusicalTime time;
    AnalysisKey key;
} AnalysisKeyEvent;

/* 0 = C to 11 = B through pitch_class_of_letter; -1 for NULL. */
int analysis_key_tonic_pitch_class(const AnalysisKey *key);

/* Same tonic spelling and mode; ids are ignored. False for NULL. */
bool analysis_key_equals(const AnalysisKey *a, const AnalysisKey *b);

/*
 * The key a signature of `fifths` sharps (negative: flats) names: the major tonic is `fifths` steps along
 * the line of fifths from C, the minor tonic three steps further (the relative minor). False beyond
 * plus or minus 7, for a mode that is neither major nor minor, or for NULL out; *out is then untouched.
 * id is set to NULL.
 */
bool analysis_key_from_fifths(int fifths, KeyStructureMode mode, AnalysisKey *out);

/*
 * As analysis_key_from_fifths for the key of `material`: major for a major third, minor for a minor third (key_structure_mode_of), the
 * minor tonic three steps further along the line of fifths, and `material` in the key. False beyond plus or minus 7, for a NULL or other
 * material (no major or minor third, or no signature), or for NULL out. analysis_key_from_fifths (the mode-based form the Composer
 * keeps until P58 B13 deletes it) leaves `material` NULL.
 */
bool analysis_key_from_fifths_material(int fifths, const PitchMaterial *material, AnalysisKey *out);

#endif
