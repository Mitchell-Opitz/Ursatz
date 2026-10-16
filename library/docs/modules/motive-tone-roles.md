# Motive tone roles (analysis, L8)

Files: `src/analysis/motive_tone_roles.h/.c`, internal `src/analysis/motive_chord_internal.h` (chord lookup shared with the harmony relations). Test: `tests/analysis/test_motive_tone_roles.c`. Part of `analysis` (see `docs/modules/analysis.md`); includes `analysis/motive_sketch.h` and `semantics/non_chord_tone.h`.

Labels every note of a `MotiveSketch` (`docs/modules/motive-relations.md`) against the sketch's chords: a chord tone, a non-chord tone of one of the ten `NonChordTone` types, or unclassified. It is for a melody on its own; the polyphonic classifier in common-practice-minimal is unchanged.

`motive_tone_roles(sketch, overrides, override_count, out)` writes one `MotiveToneLabel` per note (`role`, `type`, `resolves_to`, `unresolved`, `overridden`). Results: `OK`, `NO_SCALE`, `INVALID` (no notes, too many notes, a NULL), `BAD_OVERRIDE`.

## Chords

A note's chord is the last chord whose onset is at or before the note's onset. A sketch without chords is labelled against the tonic triad of its scale. A note is a chord tone when its pitch class is in its chord. A note with an unknown pitch is unclassified, never unresolved, and breaks every rule that needs a neighbouring pitch.

## Rules

For a note t that is not a chord tone, in this order (a the note before, d the note after; a step is 1 or 2 semitones, a leap more; two chords differ when their pitch-class sets differ):

| Label | Rule |
|---|---|
| suspension | t repeats a's pitch, t's chord differs from a's, d is a chord tone a step down |
| retardation | the same, d a step up |
| anticipation | d repeats t's pitch, that pitch class is in d's chord, and d's chord differs from t's |
| double neighbor, changing tone | c t d c with c a chord tone, t and d both non-chord on opposite sides of c, steps in and out; both notes are labelled. A third (3 or 4 semitones) between t and d makes them changing tones, a smaller gap double neighbors |
| passing | step in, step out, same direction |
| neighbor | step in, step out, opposite directions |
| appoggiatura | leap in, step out, opposite direction |
| escape tone | step in, leap out, opposite direction |
| pedal | the same pitch repeated or held across two or more chord changes |
| unclassified | none fits |

The first and the last note take no step rule.

## Resolution

`resolves_to` is the 0-based index of the chord tone the figure moves to: the next note, except for the pair figures (the next note after the pair) and the pedal (the first note after the run of its pitch). A non-chord or unclassified note with no such chord tone has `resolves_to` -1 and `unresolved` true: the motive ends, or the next note is not a chord tone. A pedal that lasts to the end is unresolved.

## Overrides

An override (`MotiveToneOverride`: note, role, type) replaces the role and type of its note and sets `overridden`; the later of two on one note wins. Its resolution is computed again for the new label from the pitch-class chord tones, so an override never changes how another note is labelled. A note index out of range, a role that does not exist, or a non-chord type that does not exist is `BAD_OVERRIDE`.

## Limits

- The chord tones are decided by pitch class alone; a chord's inversion and the octave do not matter.
- The rules look one note either way (and one more for the pair figures); a longer figure, such as a run of three passing tones, leaves all but the last unresolved.
- Metric position is not used: a suspension is found by its repeated pitch and its changed chord, not by the beat.
