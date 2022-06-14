# scale-harmonization

Theory Framework instance `scale-harmonization-v1`: harmonizing a bass scale in
the schema and partimento tradition (Open Music Theory, Galant schemas: the
Rule of the Octave and Harmonizing the Scale with Sequences). Path
`frameworks/scale-harmonization/`, tests in
`frameworks/scale-harmonization/tests/`. A Framework extension module physically
separate from `src/theory/`, the same shape as `frameworks/standard-counterpoint/`.

The dependencies on other frameworks are one way: this module reads the pitch
arithmetic and intervals of `standard-counterpoint` and the scale degrees of
`galant-schemas` (`galant_degree.h`), and neither knows anything of this one.
File names carry a `scale_` prefix so they never collide with the other
frameworks' headers on the include path.

## Scope

The subject of every rule is `ScaleHarmonization` (`scale_harmonization.h`): a
run of stages, one for each bass note (or for each harmony, where two sit over one
bass note), each with the bass and the upper voices, and the direction the bass
runs (`ScaleDirection`, `src/semantics/scale_direction.h`) and the tonic. Rhythm
is not modeled.

The Rule of the Octave is a cheat sheet for harmonizing bass lines, one chord for
each scale degree, in an ascending and a descending form. A sequence
(`SequenceKind`, `src/semantics/sequence_kind.h`) is a pattern of one or more
harmonies that repeats in the direction of the scale.

Every rule has a Rule/Constraint pair referencing this Framework plus a pure
check function. A rule that states what the pattern requires is hard; "avoid" and
"usually" features are soft. `constraint_evaluate` is never called here.

- **Sequence patterns** (`scale_patterns.h`), as the Harmonizing the Scale with
  Sequences chapter describes them. A pattern is a repeating unit of stages over
  a bass that runs by step in a direction; each stage says whether the bass moves,
  which intervals above the bass (generic sizes, octaves removed) the upper voice
  may make, and whether the upper voice is suspended (held from the stage
  before while the bass moves):
  - *Parallel chords* (both directions): a third, a sixth, or an octave above the
    bass at every stage, the same at each (a first inversion chord on each note).
  - *5-6 ascending*, and *5-6 descending* note by note: over each bass note a
    fifth and then a sixth.
  - *5-6 descending, alternating*: the fifth on one bass note and the sixth on the
    next.
  - *7-6 descending*: a chain of suspensions. At each new bass note the upper voice
    is held (a seventh), then resolves down a step (a sixth); the pattern is the
    parallel chords with the top line delayed.
- **Sequence rules** (`scale_sequence_rules.h`), a harmonization against a pattern
  (`ScaleSequence`), on the bass and the lowest upper voice. Hard: the direction is
  one the pattern allows; the bass moves by a step in the direction where the
  pattern's bass moves, and stays where it does not; the upper voice makes an
  interval the pattern allows at every stage; each stage repeats the interval one
  unit before it (so the sequence repeats); and a suspended upper voice holds its
  pitch. Soft: the harmonization is a whole number of units.
- **Rule of the Octave rules** (`scale_octave_rules.h`), on a four-voice
  realization of eight chords over the bass scale (Fenaroli's version, with four
  voices throughout and no suggestion of parallels). Hard: four voices at every
  stage; the bass is the degrees of a diatonic scale in the direction (1 2 3 4 5
  6 7 1 ascending, 1 7 6 5 4 3 2 1 descending, with the third, sixth, and seventh
  allowed lowered); the first and last chords are tonic 5/3 chords; and no
  parallel fifths or octaves between any two voices. Soft: the chords between are
  first inversion (6/3) chords.

## Not covered

- **The Rule of the Octave's chord table.** The chord on each scale degree, in the
  ascending and descending forms, and the three positions (the top voice
  centered on the tonic, mediant, or dominant), are in the chapter's scores, not
  its text, and are not encoded. Neither are the steps of its construction past the
  parallel chords and the tonic 5/3 chords (the extra figure on the dominant
  chords, the sevenths before the tonic and dominant chords, and the one
  chromatic alteration).
- **Other sequences.** The ascending 7-6 and 2-3 patterns (which need a leap up an
  octave to restart: 7-6-8 and 2-3-1, or 9-8-10 with the bass), the descending 2-3
  pattern, the three descending cycles of fifths (triads; sevenths and
  suspensions; the zigzag with its outer-voice canon), and the ascending cycle
  with 4-3 suspensions.
- **Rhythm.** A harmonization is stages, not durations.
- **Finding the stages.** The caller says which pitches are the bass and the upper
  voices at each stage.
