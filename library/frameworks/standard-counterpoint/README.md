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
`MotionType` value type (`src/semantics/motion_type.h`).

## Scope

A line is `StandardCounterpointLine` (`counterpoint_line.h`): an ordered run
of sounding `PitchSpelling`s, one per note. A two-part texture is
`StandardCounterpointPair`: two lines of equal length, the note at position
i of the upper line sounding with the note at position i of the lower line.
Rhythm is not modeled. Each group below has a Rule/Constraint pair per rule
referencing this Framework plus a pure check function.
`constraint_evaluate` is never called here.

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

### First species

Note against note counterpoint (`CounterpointSpecies` first,
`src/semantics/counterpoint_species.h`) stacks stricter rules on the common
ones above. The caller supplies the tonic and which part is the cantus firmus;
the counterpoint is the other part. Where the common rules treat something as
advice, first species can make it hard.

- **Harmony** (`first_species_harmony_rules.h`): every interval a consonance,
  no two perfect consonances of the same size in a row (compound counted as
  simple, any motion, unisons included), no direct fifths or octaves, no more
  than three of the same imperfect consonance in a row, all hard. Soft:
  imperfect consonances for every interval but the first and last, a perfect
  consonance followed by an imperfect one, at least three different interval
  sizes.
- **Beginning and ending** (`first_species_ends_rules.h`), all hard: the
  counterpoint begins on do or sol above the cantus firmus and on do below it,
  ends on do, reaches the last position by step in contrary motion with a
  third or sixth before it (the clausula vera). Skipped when the tonic is
  NULL.
- **Independence** (`first_species_independence_rules.h`): at most one repeated
  note in the counterpoint (hard). Soft: no repeated note at all, no voice
  overlap (a part leaping past the other's previous note, which is different
  from voice crossing), no more than two intervals larger than a tenth.
- **Motion** (`first_species_motion_rules.h`), all soft: contrary, similar,
  and parallel motion each appear (from four motions up), contrary motion is at
  least as common as every other type, no similar or parallel motion in which a
  part leaps, and none in which a part leaps a fifth or more. Similar motion
  that is not parallel always pairs different distances, so it always includes
  a leap of at least a third.
- **Counterpoint line** (`first_species_line_rules.h`): the cantus firmus rules
  applied to the counterpoint, except that it need not begin on the tonic.
- **Aggregate check** (`standard_counterpoint_first_species_check`,
  `standard_counterpoint_first_species_valid`): the common groups plus the
  counterpoint line, harmony, ends, independence, and motion groups, reported
  as hard and soft bit masks. The first four bits match
  `standard_counterpoint_check`. The groups that need the tonic are skipped
  when it is NULL.

### Second species

Two notes against one (`CounterpointSpecies` second,
`src/semantics/counterpoint_species.h`). There is no rhythm model, so the
counterpoint is a list of slots, two per bar (`second_species_exercise.h`):
the first slot is the strong beat (`BeatStrength`,
`src/semantics/beat_strength.h`), the second the weak beat. A slot is an
attack, a tie to the slot before it (a whole note is an attack tied through the
weak beat), or a rest (a half rest on the first beat only). The cantus firmus
is the same as in first species. The caller supplies the exercise, which says
which part is the cantus firmus, and the tonic.

- **Weak-beat patterns** (`second_species_pattern.h`): from a downbeat, the
  weak beat, and the next downbeat, a bar is a consonant passing tone,
  substitution, skipped passing tone, interval subdivision, change of
  register, delay of melodic progression, consonant neighbor tone, or
  dissonant passing tone. The patterns are exclusive, and every melodic
  interval in them is consonant (a change of register may make a seventh with
  the downbeat).
- **Strong beats** (`second_species_strong_rules.h`): downbeats consonant, no
  repeated perfect interval between downbeats or from a weak beat to the next
  downbeat, no dissonant outline between downbeats (an octave leap and a step
  back is the exception), at most three bars in a row with the same imperfect
  consonance, all hard. Soft: imperfect consonances and no unisons on interior
  downbeats. Hidden fifths and octaves between downbeats are not forbidden.
- **Weak beats** (`second_species_weak_rules.h`): a dissonant weak beat must be
  a passing tone (hard). Soft: a consonant weak beat fits a pattern, a mixture
  of consonant and dissonant weak beats from four weak beats up, at most one
  change of register. Weak-beat unisons are allowed.
- **Beginning and ending** (`second_species_ends_rules.h`), all hard: do or sol
  above the cantus firmus and do below it, a first bar of two half notes or a
  half rest and a half note, a last bar that is a whole note on do, and the
  clausula vera on the note before it. Skipped when the tonic is NULL.
- **Counterpoint line** (`second_species_line_rules.h`): on the line's
  attacks. Hard: the species leap rule (a sixth only as the ascending minor
  sixth or inside a change of register), final step, range, leading tone.
  Soft: single climax, no repeated motive, at least two thirds steps, leaps
  from strong to weak beat at least as many as leaps across the bar line, one
  or two secondary climaxes from eight notes up. Leap recovery and consecutive
  leaps of the cantus firmus rules are not applied, because an interval
  subdivision is two leaps in a row.
- **Aggregate check** (`standard_counterpoint_second_species_check`,
  `standard_counterpoint_second_species_valid`): the cantus firmus, spacing
  (the common gap, crossing, and climax rules on every note against its
  cantus firmus note), strong beat, weak beat, ends, and counterpoint line
  groups, as hard and soft bit masks.

### Third species

Four notes against one (`CounterpointSpecies` third,
`src/semantics/counterpoint_species.h`). The counterpoint is a list of slots
(`counterpoint_slots.h`), four per bar, with the strengths strong, weak, medium,
weak (`BeatStrength`, `src/semantics/beat_strength.h`). A bar is four quarter
notes (the first may be a quarter rest and three notes) or a whole note (an
attack and three ties); `third_species_exercise.h`. The caller supplies the
exercise, which says which part is the cantus firmus, and the tonic.

- **Dissonance figures** (`third_species_figure.h`): a dissonance falls on beat
  2, 3, or 4 and only as part of a figure: a dissonant passing tone, two
  dissonant passing tones in a row, a dissonant neighbor tone, a double
  neighbor, or a nota cambiata (the form that fills in a step down and the form
  that fills in a step up). Each maps to a `NonChordTone`
  (`src/semantics/non_chord_tone.h`).
- **Strong beats** (`third_species_strong_rules.h`): downbeats consonant, no
  three bars in a row beginning with the same perfect interval, at most three
  with the same imperfect consonance, no dissonant outline between downbeats,
  no fifth on beat 3 or 4 (or octave on beat 2, 3, or 4) before a downbeat of
  the same kind, and the first-species rules from beat 4 into the downbeat
  (no repeated perfect interval, no direct perfect interval), all hard. Soft:
  imperfect consonances and no unisons on interior downbeats. Direct fifths and
  octaves between downbeats are allowed.
- **Other beats** (`third_species_beat_rules.h`): every dissonant new note is
  part of a figure (hard). Soft: a mixture of consonant and dissonant beats
  from four bars up, and a leap of a fourth or more followed by a step in the
  opposite direction. Unisons are allowed on every beat but the downbeat.
- **Beginning and ending** (`third_species_ends_rules.h`), all hard: do or sol
  above the cantus firmus and do below it, a first bar of four quarter notes or
  a quarter rest and three, a last bar that is a whole note on do, and the
  clausula vera on the last note of the penultimate bar. Skipped when the tonic
  is NULL.
- **Counterpoint line** (`third_species_line_rules.h`): on the line's attacks.
  Hard: every leap a consonant melodic interval (a sixth only as the ascending
  minor sixth), final step, range, leading tone. Soft: single climax, no
  repeated motive, at least three quarters steps, leaps inside a bar at least as
  many as across the bar line, and one or two secondary climaxes among the
  downbeats from eight bars up.
- **Aggregate check** (`standard_counterpoint_third_species_check`,
  `standard_counterpoint_third_species_valid`): the cantus firmus, spacing (the
  common gap, crossing, and climax rules on every note against its cantus
  firmus note, so the counterpoint's climax stays apart from the cantus
  firmus climax), strong beat, other beat, ends, and counterpoint line groups,
  as hard and soft bit masks.

### Fourth species

Syncopated counterpoint (`CounterpointSpecies` fourth,
`src/semantics/counterpoint_species.h`). The counterpoint is a list of slots
(`counterpoint_slots.h`), two per bar, strong then weak. The weak-beat note is
tied across the bar line to the next strong beat. The first bar is a half rest
and a note, a middle bar is a new note on the weak beat and a tie (or a new
note) on the strong beat, and the last bar is a whole note (an attack and a
tie); `fourth_species_exercise.h`. The caller supplies the exercise, which says
which part is the cantus firmus, and the tonic.

- **Suspensions** (`fourth_species_suspension.h`): a tied downbeat is a
  consonant preparation, a tie into the cantus firmus, and a consonant
  resolution one step down. Above the cantus firmus the dissonant types are
  7-6, 4-3, and 9-8, below it 2-3; a consonant tie that resolves down by step
  is a consonant suspension. The dissonant types map to
  `NON_CHORD_TONE_SUSPENSION` (`src/semantics/non_chord_tone.h`).
- **Suspension rules** (`fourth_species_suspension_rules.h`): every dissonant
  tied downbeat is a proper suspension (hard). Soft: at least half of the tied
  bars hold a dissonant suspension, and no more than half of them are 9-8.
- **Weak beats** (`fourth_species_weak_rules.h`): every weak beat is a
  consonance except in a broken bar, no two weak beats in a row make the same
  perfect interval, no more than three in a row make the same imperfect
  consonance, and no weak beat is a perfect interval reached by similar motion,
  all hard. Soft: no interior unisons.
- **Species breaks** (`species_break_rules.h`): a middle bar whose strong beat
  is a new note follows second-species principles. Hard: the downbeat is
  consonant, and a dissonant weak beat is a passing tone between two consonant
  downbeats. Soft: one stretch of broken bars at most, of two bars at most.
- **Beginning and ending** (`fourth_species_ends_rules.h`), all hard: do or sol
  above the cantus firmus and do below it, a cantus firmus that ends re to do,
  a penultimate bar of a tied do (a 7-6 above the cantus firmus, a 2-3 below
  it) and then ti, and a last bar that is a whole note on do. Skipped when the
  tonic is NULL.
- **Counterpoint line** (`fourth_species_line_rules.h`): on the line's attacks.
  Hard: every leap a consonant melodic interval (a sixth only as the ascending
  minor sixth), final step, range, leading tone. Soft: at least half of the
  progressions are steps down, and at least one upward leap. The single climax
  and secondary climax rules of the other species are not used.
- **Aggregate check** (`standard_counterpoint_fourth_species_check`,
  `standard_counterpoint_fourth_species_valid`): the cantus firmus, spacing (the
  common gap, crossing, and climax rules on every sounding note against its
  cantus firmus note), suspensions, weak beats, species breaks, ends, and
  counterpoint line groups, as hard and soft bit masks.

### Fifth species

Florid counterpoint (`CounterpointSpecies` fifth,
`src/semantics/counterpoint_species.h`). The counterpoint mixes the note values
of the first four species, and adds eighth notes. It is a list of slots
(`counterpoint_slots.h`) on an eighth-note grid, eight per bar, with the
strengths strong, medium (the third beat), and weak. A note is an attack and
its ties and lasts an eighth, quarter, half, dotted half, or whole note; a
half or longer note may be tied across the bar line from the third beat, and a
quarter from the fourth beat into a half or dotted half. Eighths come in pairs
on the second or fourth beat. The first bar may begin with a rest of one to
three quarters, and the last bar is a whole note; `fifth_species_exercise.h`.
The caller supplies the exercise, which says which part is the cantus firmus,
and the tonic.

- **Note values and bar patterns** (`fifth_species_pattern.h`): the value of
  each note, and the pattern of each bar (whole, halves, half and quarters,
  quarters and half, quarters, eighths, syncopated, rest start, other).
- **Dissonance figures** (`fifth_species_figure.h`): a dissonance is never a
  new note on the downbeat and is part of a figure: a passing tone (a quarter
  or the second eighth of a pair), a double passing tone, a neighbor tone, a
  double neighbor, a nota cambiata (down or up), a suspension (7-6, 4-3, or
  9-8 above the cantus firmus, 2-3 below it, the held note a quarter, half, or
  dotted half), an anticipation (a quarter on the fourth beat), or an
  anticipation with a pair of eighths. Each maps to a `NonChordTone`
  (`src/semantics/non_chord_tone.h`).
- **Suspension rules** (`fifth_species_suspension_rules.h`): every dissonant
  tied downbeat is a proper suspension (hard). Soft: an exercise of five bars
  or more has one, and no more than half of them are 9-8.
- **Rhythm** (`fifth_species_rhythm_rules.h`): an eighth is approached and left
  by step or repetition (hard). Soft: three of the five note values from six
  bars up, no more than three bars in a row with the same pattern (a run of
  syncopated bars is free), and no pair of eighths in two bars in a row.
- **Strong beats and harmony** (`fifth_species_harmony_rules.h`): a new note
  on a downbeat is consonant, every dissonance is part of a figure, no two
  downbeats make the same perfect interval, no downbeat perfect interval is
  reached by similar or parallel motion, and no more than three downbeats in a
  row make the same imperfect consonance, all hard. Soft: no interior unison
  on a downbeat.
- **Beginning and ending** (`fifth_species_ends_rules.h`), all hard: a perfect
  consonance first, a clausula vera into the last bar (the note before it
  steps contrary to the cantus firmus, a third or sixth over its note), and a
  last bar that is a whole note on do. Skipped when the tonic is NULL.
- **Counterpoint line** (`fifth_species_line_rules.h`): on the line's attacks.
  Hard: every leap a consonant melodic interval (a sixth only as the ascending
  minor sixth), final step, range, leading tone. Soft: single climax, leap
  recovery, mostly stepwise, and no repeated motive.
- **Aggregate check** (`standard_counterpoint_fifth_species_check`,
  `standard_counterpoint_fifth_species_valid`): the cantus firmus, spacing (the
  common gap, crossing, and climax rules on every sounding slot against its
  cantus firmus note), rhythm, suspensions, harmony, ends, and counterpoint
  line groups, as hard and soft bit masks.

The tonic of the cantus firmus, which part is the cantus firmus, and the
limits for the soft melodic rules are the caller's input. Notes are read as
sounding pitch in 12-tone equal temperament, this module's own copy in
`counterpoint_pitch.h`.

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

## Not covered

Dissonance treatment beyond the figures above, three or more parts, and imitation. Note values and
whole-note rhythm of a cantus firmus and counterpoint: the one-against-one
ratio is only that the two lines have the same number of notes. The overall shape of a cantus firmus
beyond the melodic arch tendency. Choosing a counterpoint for a cantus
firmus. The perfect fourth between two upper parts is consonant, but this
framework has two parts only.
