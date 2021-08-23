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

The tonic of the cantus firmus, which part is the cantus firmus, and the
limits for the soft melodic rules are the caller's input. Notes are read as
sounding pitch in 12-tone equal temperament, this module's own copy in
`counterpoint_pitch.h`.

## Not covered

Species beyond the second (four notes to one, syncopation, florid counterpoint),
dissonance treatment, three or more parts, and imitation. Note values and
whole-note rhythm of a cantus firmus and counterpoint: the one-against-one
ratio is only that the two lines have the same number of notes. The overall shape of a cantus firmus
beyond the melodic arch tendency. Choosing a counterpoint for a cantus
firmus. The perfect fourth between two upper parts is consonant, but this
framework has two parts only.
