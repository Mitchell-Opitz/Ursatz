#ifndef MUSIC_ANALYSIS_MOTIVE_TONE_ROLES_H
#define MUSIC_ANALYSIS_MOTIVE_TONE_ROLES_H

/*
 * Owner of non-chord-tone classification (motive_tone_roles) (docs/analytic-job-owners.md, Analytic job
 * owners).
 */

#include <stdbool.h>
#include <stddef.h>

#include "analysis/motive_sketch.h"
#include "semantics/non_chord_tone.h"
#include "semantics/tone_line.h"

/*
 * MotiveToneRoles (L8 analysis): the harmonic role of every note of a motive sketch against the sketch's
 * chords, for a melody on its own (the polyphonic classifier in common-practice-minimal is not used). A note
 * is a chord tone, a non-chord tone of one of the ten NonChordTone types, or unclassified.
 *
 * A note's chord is the last chord whose onset is at or before the note's onset; a sketch without chords is
 * labelled against the tonic triad of its scale. The sketch needs a scale. A note with an unknown pitch is
 * UNCLASSIFIED and never unresolved; it also breaks every rule that needs the pitch of a neighbouring note.
 *
 * Rules, applied to a note t that is not a chord tone, in this order (a the note before, d the note after,
 * both known; a step is 1 or 2 semitones, a leap more; "the chord differs" means the pitch-class sets differ):
 *
 *   suspension       t repeats a's pitch, t's chord differs from a's, and d is a step down to a chord tone
 *   retardation      the same, but d is a step up to a chord tone
 *   anticipation     d has t's pitch, that pitch class is a tone of d's chord, and d's chord differs from t's
 *   double neighbor  c t d c: t and d both non-chord, c a chord tone before t and the same pitch after d, steps
 *                    in and out, t and d on opposite sides of c. Both notes get the label; it is
 *   changing tone    when the two notes are a third apart (3 or 4 semitones), else a double neighbor
 *   passing          a step in, a step out, in the same direction
 *   neighbor         a step in, a step out, in opposite directions
 *   appoggiatura     a leap in, a step out, in the opposite direction
 *   escape tone      a step in, a leap out, in the opposite direction
 *   pedal            the same pitch repeated or held across two or more chord changes (the chord tones
 *                    differ between two successive notes of the run at least twice)
 *   none fits        UNCLASSIFIED
 *
 * An override replaces the role (and the type, for a non-chord tone) of its note.
 *
 * A chord tone is a note whose pitch class is in its chord. The first and the last note take no step rule.
 *
 * Resolution: resolves_to is the index of the chord tone the figure moves to: the next note for every type but
 * the double neighbor and the changing tone (the next note after the pair, when the pair is not yet finished)
 * and the pedal (the note after the run of the pedal's pitch). A non-chord or unclassified note with no such
 * chord tone (the motive ends, or the note it moves to is not a chord tone) has resolves_to -1 and
 * unresolved true.
 */

/* MotiveToneRole and MotiveToneOverride are defined in analysis/motive_sketch.h. */

typedef struct {
    MotiveToneRole role;
    NonChordTone type; /* meaningful for MOTIVE_TONE_NON_CHORD; NON_CHORD_TONE_PASSING otherwise */
    long resolves_to;  /* 0-based note index of the chord tone it moves to, or -1 */
    bool unresolved;
    bool overridden;   /* the role and type came from an override */
} MotiveToneLabel;

typedef enum {
    MOTIVE_TONE_OK = 0,
    MOTIVE_TONE_NO_SCALE,
    MOTIVE_TONE_INVALID, /* no notes, or more than MOTIVE_SKETCH_MAX_NOTES */
    MOTIVE_TONE_BAD_OVERRIDE /* an override names a note out of range, or a role or type that does not exist */
} MotiveToneStatus;

/*
 * Labels every note: out has sketch->note_count entries, written only on MOTIVE_TONE_OK. An override sets the
 * role and type of its note and its overridden flag; the later of two overrides on one note wins. The note's
 * resolution is computed for the overridden label, from the pitch-class chord tones, so an override on one
 * note never changes how another is labelled.
 */
MotiveToneStatus motive_tone_roles(const MotiveSketch *sketch, const MotiveToneOverride *overrides,
                                   size_t override_count, MotiveToneLabel *out);

/*
 * The same labelling with the overrides the sketch carries (tone_overrides, tone_override_count), so a motive
 * value brings its own corrections: equal to motive_tone_roles(sketch, sketch->tone_overrides,
 * sketch->tone_override_count, out). MOTIVE_TONE_BAD_OVERRIDE when the count is above MOTIVE_SKETCH_MAX_NOTES.
 */
MotiveToneStatus motive_tone_roles_of(const MotiveSketch *sketch, MotiveToneLabel *out);

/*
 * The same rules over a tone line of any length (a NonChordToneClassifier, semantics/tone_line.h): the notes of
 * one voice in time order, each with the pitch-class mask of its chord. Equal to motive_tone_roles for a
 * sketch's notes and chord masks, without a note limit and without overrides. context is unused. False for
 * out NULL, or notes NULL with a count above 0.
 */
bool motive_tone_roles_line(const ToneLineNote *notes, size_t count, ToneLineLabel *out, void *context);

#endif
