# standard-notation

Theory Framework instance `standard-notation-v1`: the conventions of
Western staff notation. Path `frameworks/standard-notation/`, tests in
`frameworks/standard-notation/tests/`. A Framework extension module
physically separate from `src/theory/`, same shape as
`frameworks/common-practice-minimal/`.

## Scope

Rules about where and how notation is placed on a staff:

- **Staff and clefs**: a staff has five lines; a clef assigns pitches to
  its lines and spaces (treble G4 on line 2, bass F3 on line 4, alto and
  tenor C4 on lines 3 and 4); higher pitches are written higher.
- **Letter names**: each clef gives its lines and spaces fixed letter
  names (treble lines E G B D F, spaces F A C E; likewise for bass,
  alto, tenor, and the other C clefs), and ledger lines continue the
  letters as if the staff extended.
- **Octave equivalence**: letters A-G repeat in a loop; pitches an octave
  apart share a letter. This is this framework's assumption, not a
  universal one -- a framework for another system can assert otherwise.
- **Half steps and whole steps**: a half step is one semitone (the
  adjacent key), a whole step is two; of the white-key neighbors only
  E-F and B-C are half steps.
- **Accidentals**: a sharp raises a half step, a flat lowers a half step,
  a natural cancels a previous accidental, double sharp and double flat
  move a whole step. An accidental sets the alteration absolutely rather
  than accumulating. Placement: to the left of the note, directly across
  its line or space, and each accidental applies to its own note.
- **Enharmonic equivalence**: notes spelled differently that sound the
  same (C-sharp and D-flat; B-sharp and C, which crosses an octave
  number). Strict `PitchSpelling` equality still tells them apart.
- **Ledger lines**: notes beyond the staff extend it with ledger lines,
  and a note takes exactly the ledger lines between it and the staff --
  never an extra one.
- **Stem direction**: stems point down above the middle line, up below
  it, and either way on it.
- **Beamed stems**: notes joined by a beam share one direction, set by
  the note farthest from the middle line (either way when the farthest
  above and below are equally far).
- **Flag direction**: a down-stem has an inward flag, an up-stem an
  outward one, and a note on the middle line may be flagged either way.
- **Seconds**: a harmonic second is drawn with the lower note on the
  left, whichever way the stem points.

Each rule has a backing `Rule`/`Constraint` pair referencing this
Framework, plus a pure check function. `constraint_evaluate` is never
called here.

## Not covered

Engraving craft (clef drawing steps, notehead shape/size/tilt, stem
length, beam thickness), which notes are beamed (that is
`frameworks/standard-meter/`), key signatures, single-line (percussion) staves, instrument clef conventions.

Rhythm (note and rest values, dots, ties) is `frameworks/standard-rhythm/`;
meter, time signatures, counting, and beaming are
`frameworks/standard-meter/`.

Neutral (unpitched) clefs assign no letters, so every letter and
reference-pitch rule treats them as not applicable.

Value types (`Clef`, `StaffPosition`, `Accidental`) live in `src/pitch/`; this module
holds only the rules built on them.

## 12-TET assumption

The semitone arithmetic (`semitone.h`) assumes twelve-tone equal
temperament: twelve semitones to the octave, the piano keyboard layout.
`src/pitch/` takes no such assumption, so it lives here. It is isolated
in `semitone.{c,h}` so it can be lifted into a shared module if another
framework needs the same numbering; frameworks may not depend on each
other, so that extraction waits for a second consumer.
