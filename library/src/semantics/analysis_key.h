#ifndef MUSIC_SEMANTICS_ANALYSIS_KEY_H
#define MUSIC_SEMANTICS_ANALYSIS_KEY_H

#include <stdbool.h>

#include "interpretation/key.h"
#include "pitch/pitch_spelling.h"
#include "time/musical_time.h"

/*
 * AnalysisKey: the neutral key framework adapters accept (R-242): a tonic spelled as letter and
 * accidental, and a mode. KeyStructureMode (interpretation/key.h) is major or minor; the minor forms
 * (natural, harmonic, melodic) stay framework theory. id is borrowed and may be NULL.
 *
 * AnalysisKeyEvent places an AnalysisKey at a time (borrowed) for key-change input.
 */
typedef struct {
    const char *id;
    PitchLetter tonic_letter;
    int tonic_accidental;
    KeyStructureMode mode;
} AnalysisKey;

typedef struct {
    const MusicalTime *time;
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

#endif
