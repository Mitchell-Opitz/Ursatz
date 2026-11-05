# standard-counterpoint

Theory Framework instance `standard-counterpoint-v1`: strict two-part
counterpoint in the species tradition (Fux, Gradus ad Parnassum). Path
`frameworks/standard-counterpoint/`, tests in
`frameworks/standard-counterpoint/tests/`. A Framework extension module
physically separate from `src/theory/`, same shape as
`frameworks/standard-voicing/` and `frameworks/standard-texture/`. It does not
depend on any other framework module.

The rules are style-independent training rules for writing lines and
combining them, not a claim about one repertoire. They use the
`MotionType` value type (`motion_type.h`, in this module: only standard-counterpoint uses it, so it moved here from `src/semantics/`).

## Scope

A line is `StandardCounterpointLine` (`counterpoint_line.h`): an ordered run
of sounding `PitchSpelling`s, one per note. A two-part texture is
`StandardCounterpointPair`: two lines of equal length, the note at position
i of the upper line sounding with the note at position i of the lower line.
Rhythm is not modeled. Each group below has a Rule/Constraint pair per rule
referencing this Framework plus a pure check function.
Each rule table (except consonance and motion, which only classify) registers an
evaluation callback on its Constraints, so `constraint_evaluate` runs the same check
(`standard_counterpoint_rule_table_evaluate`) on a candidate `Value` tagged
`standard-counterpoint/<table prefix>` holding the rule's subject; the multi-input subjects are in
`counterpoint_subject.h`. Each soft rule also has a `Preference` (`standard_counterpoint_*_preference`)
whose scoring callback makes `preference_rank` score the same candidate 1/1 when the rule holds
and 0/1 when it does not.

- **Consonance** (`consonance_rules.h`): harmonic intervals are perfect
  consonances (unison, fifth, octave), imperfect consonances (major and minor
  thirds and sixths), or dissonances (seconds, sevenths, augmented and
  diminished intervals, and the perfect fourth, which is dissonant against
  the lowest part and every fourth in two parts involves it). Compound
  intervals count as their simple counterparts. Melodic intervals have their
  own consonant set: perfect intervals and major and minor seconds, thirds,
  and sixths.
- **Motion** (`motion_rules.h`): contrary, similar, parallel, oblique, or
  static. Parallel is the refined case of similar: same direction and same
  distance in letter names, so the generic interval is kept. A part moves when
  its next note is a different pitch.
- **Cantus firmus** (`cantus_firmus_rules.h`): 8 to 16 notes, begins and ends
  on the tonic, final approach by step (re to do, or ti to do by a half step),
  melodic consonances only, range of at most a tenth, leap recovery, limits on
  consecutive leaps (with the consonant triad outline exception), leading tone
  resolution (the seventh degree raised where it steps to the tonic). Soft:
  a single climax, mostly stepwise motion, no repeated four-note motive.
- **Melodic writing** (`melodic_rules.h`): clausula vera, forbidden leaps
  (augmented, diminished, any seventh, any sixth but the ascending minor sixth,
  larger than an octave), and forbidden outlines (a run in one direction whose
  endpoints form an augmented or diminished interval or a seventh), all hard.
  Soft: limits on repeated notes, repeated pitches, consecutive leaps, leap
  density, and repeated generic intervals (`counterpoint_limits.h`, with
  defaults), a unique climax, not ending on the climax, and reaching it by
  leap.
- **Harmonic writing** (`harmonic_rules.h`): first interval perfect, last
  interval a unison or octave, gap of at most a twelfth, no voice crossing, no
  parallel fifths or octaves (unisons count as octaves), all hard. Soft: the
  gap stays within an octave for the most part, no direct fifths or octaves,
  no interior unisons, the two climaxes do not coincide.
- **Melodic tendencies** (`tendency_rules.h`): Huron's pitch proximity, step
  declination, step inertia, melodic regression, and melodic arch, all soft.
- **Aggregate check** (`standard_counterpoint_check`,
  `standard_counterpoint_valid`): runs the cantus firmus, melodic, harmonic,
  and tendency groups over an exercise and reports the groups with a hard rule
  broken and the groups with a soft rule broken as two bit masks indexed by
  `StandardCounterpointGroup`. An exercise is valid when no hard rule is
  broken.

The five species are documented in [standard-counterpoint-species.md](../../docs/modules/standard-counterpoint-species.md).

## Adding a rule module

Every group of rules (a species' strong beats, its line rules, the common
melodic rules) follows one pattern, built on two shared helpers.

- **`rule_table.h`**: a table for one group. The module defines its rule
  names, a function that says which rules are hard, and static storage for the
  cached Rule/Constraint pairs, then points the table at them. The table
  builds each pair the first time it is asked for, names it
  `<prefix>-<rule>`, and reports a group's failures as hard and soft bit
  masks. The module writes only how one rule is evaluated and a small adapter
  that hands the table the exercise (or the exercise and the tonic).
- **`species_aggregate.h`**: the parts every species aggregate shares: the
  helper that records a group's result in the hard and soft masks, and the
  spacing group (the common gap, crossing, and climax rules run on every
  sounding note against its cantus firmus note).

A new species keeps its own exercise type, its own rule modules, and its own
list of groups, and uses these for the rest.

## Conformance to Fux

The distinct cantus firmi of the two-voice exercises in Gradus ad Parnassum
are tested against the cantus firmus rules and the melodic rules
(`tests/test_fux_cantus_conformance.c`), with the exact hard and soft results
pinned. The rules are the textbook ideal, and three of Fux's melodies depart
from it in a hard rule; the rules stay strict and the departures are recorded in
the test:

- The F melody leaps up a fifth and then down twice (three leaps in a row, the
  first not recovered by a step), and its rising run D3 to C4 outlines a
  seventh.
- The G melody has runs of leaps and a fourth up that is followed by a leap,
  and few steps.
- The A melody without its first D (fig. 42) has a rising run, B3 to F4, that
  outlines a diminished fifth. The full form does not.
- The D, E, full A, and both C melodies break no hard rule.

## Analysis of real music

`analysis/` reads a voice-pitch grid: motion classification between voice pairs and parallel fifths and octaves (all pairs) and direct perfects (outer pair, top voice leaping). See `docs/modules/voice-leading-analysis.md`. Doubling, spacing and voice-crossing checks are not there yet.

The melody adapter (`analysis/analysis_melody*.h`) reads one line: melodic intervals and contour, the climax of each caller-supplied span, leap recovery and consecutive leaps by the cantus firmus rules, and the non-chord-tone result of each note attached by reference. See `docs/modules/melody-analysis.md`.

## Semitone forms

`standard_counterpoint_melodic_semitones_allowed` and `standard_counterpoint_is_step` (`melodic_rules.h`) are the forbidden-leap rule and the step test on a move in semitones, for a caller that holds MIDI numbers. `standard_counterpoint_imperfect_run` (`harmonic_rules.h`) is the soft preference against the last `run` vertical intervals being the same third or sixth; it is a plain function, not a rule table entry.

## Not covered

Dissonance treatment beyond the figures above, three or more parts, and imitation. Note values and
whole-note rhythm of a cantus firmus and counterpoint: the one-against-one
ratio is only that the two lines have the same number of notes. The overall shape of a cantus firmus
beyond the melodic arch tendency. Choosing a counterpoint for a cantus
firmus. The perfect fourth between two upper parts is consonant, but this
framework has two parts only. Three or more parts and imitation in the style of
16th-century choral polyphony are in `frameworks/sixteenth-century-style`.

## Vocabulary enums

Owned here (moved from `src/semantics/`; type names and values unchanged):

- `CounterpointSpecies` is a plain enum value naming one of the five species of strict counterpoint (first through fifth, ordered simplest to most elaborate), with a helper for the counterpoint notes against each cantus firmus note (1, 2, 4, 2, and 0 for the fifth, which mixes ratios). It carries no `EntityID`, rules, or framework claim. What each species requires of a line is a convention of `frameworks/standard-counterpoint`.
- `ImitationAnswer` is a plain enum value naming how a later entry of a melody answers an earlier one in imitative writing: real (every interval kept at a fixed transposition), tonal (adjusted by a step to stay in the key), or free (keeps the rhythm and opening shape, then goes its own way). `imitation_answer_keeps_intervals` is true only for the real answer. It carries no `EntityID`, pitches, or framework claim. Which answer a style expects, and how closely it is kept, is a convention of a framework (`frameworks/sixteenth-century-style`).

## Neutral analysis input

`StandardCounterpointVoiceCell` and `StandardCounterpointVoiceGrid` are the shared `AnalysisVoiceCell` and `AnalysisVoiceGrid` (`semantics/voice_grid.h`), the same grid standard-harmony reads. `slot_note.h` defines `StandardCounterpointSlotNote` and `StandardCounterpointRuleResult`, which the fugue, ground bass and sixteenth-century frameworks use as their note and result types.

`standard_counterpoint_melody_nct_labels_with` takes a `NonChordToneClassifier` (`semantics/tone_line.h`; the owner is `motive_tone_roles_line`) and relabels the notes that carry a `chord_pitch_classes` mask, run by run; `standard_counterpoint_melody_nct_labels` is the same with no classifier, the attached results as given.
