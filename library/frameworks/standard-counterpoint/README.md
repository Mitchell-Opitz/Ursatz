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

The tonic of the cantus firmus, which part is the cantus firmus, and the
limits for the soft melodic rules are the caller's input. Notes are read as
sounding pitch in 12-tone equal temperament, this module's own copy in
`counterpoint_pitch.h`.

## Not covered

Species beyond the first (rhythm, syncopation, florid counterpoint),
dissonance treatment, three or more parts, and imitation. Note values and
whole-note rhythm of a cantus firmus and counterpoint: the one-against-one
ratio is only that the two lines have the same number of notes. The overall shape of a cantus firmus
beyond the melodic arch tendency. Choosing a counterpoint for a cantus
firmus. The perfect fourth between two upper parts is consonant, but this
framework has two parts only.
