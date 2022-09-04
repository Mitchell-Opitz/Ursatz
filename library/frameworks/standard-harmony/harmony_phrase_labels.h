#ifndef URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_PHRASE_LABELS_H
#define URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_PHRASE_LABELS_H

#include <stdbool.h>
#include <stddef.h>

#include "harmony_phrase_rules.h"
#include "semantics/cadence_type.h"
#include "semantics/phrase_model_label.h"

/*
 * Phrase model labeling for standard-harmony-v1 (Performing Harmonic Analysis Using
 * the Phrase Model). To see the phrase model at work in a phrase, each phrase gets one
 * (and only one) set of the labels Tb, PD, D, Te (semantics/phrase_model_label.h),
 * placed by working from the ending back:
 *
 *   D    the cadential dominant, V or V7. If the phrase ends with a half cadence it
 *        is the last chord; with an authentic cadence it is the chord before the
 *        final tonic. A cadential 6/4 before it is part of the dominant unit and gets
 *        no label of its own (harmony_chord.h)
 *   Te   the tonic of an authentic cadence, the last chord. A half cadence has none
 *   PD   the first strong predominant, reading left to right, in the run of
 *        predominants that comes immediately before the cadential dominant (the
 *        cadential 6/4, when there is one, sits between the run and the V). If that
 *        run has only weak predominants, there is no PD
 *   Tb   the tonic that starts the phrase: the first chord, when it is a tonic. A
 *        phrase may rarely delay this tonic or omit it (more common in Romantic
 *        music), and then there is none; a tonic prolongation labels only the first
 *        chord
 *
 * The cadence is the one the listener heard, given as a CadenceType: a perfect or
 * imperfect authentic cadence (V to I), or a half cadence (the phrase ends on V). A
 * subverted cadence is not a cadence, so it is not labeled. This module does not
 * listen: finding the phrase ending, and guessing the harmony from the bass line, are
 * the analyst's.
 */

#define HARMONY_PHRASE_LABELS_MAX 64

typedef struct {
    size_t chord_count;
    bool has_label[HARMONY_PHRASE_LABELS_MAX];
    PhraseModelLabel label[HARMONY_PHRASE_LABELS_MAX];
} HarmonyPhraseLabels;

/* Labels the phrase's chords by the rules above: for each chord, has_label says whether
   it carries a label and label says which. At most one chord carries each label. False
   (*out untouched) for an invalid phrase, a phrase of more than
   HARMONY_PHRASE_LABELS_MAX chords, a subverted or invalid cadence type, and a phrase
   that does not end as the cadence says (an authentic cadence needs V then a tonic; a
   half cadence needs V last), or NULL. */
bool harmony_phrase_model_label(const HarmonyPhrase *phrase, CadenceType cadence, HarmonyPhraseLabels *out);

/* The index of the chord that carries the label, or -1 when no chord does (including
   an invalid label). */
int harmony_phrase_labels_index_of(const HarmonyPhraseLabels *labels, PhraseModelLabel label);

#endif
