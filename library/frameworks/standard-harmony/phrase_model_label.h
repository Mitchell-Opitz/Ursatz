#ifndef MUSIC_SEMANTICS_PHRASE_MODEL_LABEL_H
#define MUSIC_SEMANTICS_PHRASE_MODEL_LABEL_H

#include <stdbool.h>

/*
 * PhraseModelLabel (Value): the role a chord plays in the phrase model, where each
 * phrase gets one (and only one) set of the labels Tb, PD, D, Te. A plain enum: it
 * names a role and carries no EntityID, no chord, and no framework claim. It is not
 * HarmonicFunction (harmonic_function.h), which says what kind of chord
 * something is wherever it stands; this says which chord of one phrase fills each
 * place in the model.
 *
 *   TONIC_BEGINNING   Tb: the tonic that starts the phrase
 *   PREDOMINANT       PD: the first strong predominant, reading left to right, that
 *                     comes immediately before the cadential dominant
 *   DOMINANT          D: the cadential dominant, V or V7
 *   TONIC_ENDING      Te: the tonic of an authentic cadence; a half cadence has none
 *
 * The values are in the order the labels occur in a phrase. Where each label goes is
 * a convention of a Framework (frameworks/standard-harmony), not of this type.
 */

typedef enum {
    PHRASE_MODEL_LABEL_TONIC_BEGINNING = 0,
    PHRASE_MODEL_LABEL_PREDOMINANT,
    PHRASE_MODEL_LABEL_DOMINANT,
    PHRASE_MODEL_LABEL_TONIC_ENDING,
    PHRASE_MODEL_LABEL_COUNT
} PhraseModelLabel;

/* True for the four labels above; false for any other value. */
bool phrase_model_label_valid(PhraseModelLabel label);

/* "tonic-beginning" "predominant" "dominant" "tonic-ending"; NULL for an invalid
   label. */
const char *phrase_model_label_name(PhraseModelLabel label);

/* "Tb" "PD" "D" "Te"; NULL for an invalid label. */
const char *phrase_model_label_abbreviation(PhraseModelLabel label);

#endif
