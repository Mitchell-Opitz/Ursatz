#ifndef MUSIC_SEMANTICS_TONE_LINE_H
#define MUSIC_SEMANTICS_TONE_LINE_H

#include <stdbool.h>
#include <stddef.h>

#include "core/rational.h"
#include "semantics/non_chord_tone.h"

/*
 * ToneLine (Value, L6): the types of an injected non-chord-tone classifier. A line is the notes of one voice in
 * time order, each with the 12-bit pitch-class mask of the chord it sounds against. A framework that labels
 * non-chord tones takes a NonChordToneClassifier instead of owning the rules (the owner is
 * analysis/motive_tone_roles.h); NULL keeps the framework's own fallback.
 */

/* midi may be ANALYSIS_PITCH_UNKNOWN (-1); chord_pitch_classes has bit pc set for each tone of the note's chord. */
typedef struct {
    int midi;
    Rational onset;
    Rational duration;
    unsigned chord_pitch_classes;
} ToneLineNote;

/*
 * The label of one note. classified false: no rule fits (type and resolves_to are not meaningful). chord_tone
 * true: the note is in its chord. resolves_to is the index in the line of the chord tone a figure moves to, or
 * -1; unresolved is true for a non-chord tone with none.
 */
typedef struct {
    bool chord_tone;
    bool classified;
    NonChordTone type;
    long resolves_to;
    bool unresolved;
} ToneLineLabel;

/*
 * Labels every note of the line: out has count entries, written only when true is returned. False when a note
 * has an invalid time or the classifier cannot label the line.
 */
typedef bool (*NonChordToneClassifier)(const ToneLineNote *notes, size_t count, ToneLineLabel *out,
                                       void *context);

/* Sets a label to unclassified: not a chord tone, no type, resolves_to -1, not unresolved. */
void tone_line_label_clear(ToneLineLabel *label);

#endif
