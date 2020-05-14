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
- **Ledger lines**: notes beyond the staff extend it with ledger lines,
  and a note takes exactly the ledger lines between it and the staff --
  never an extra one.
- **Stem direction**: stems point down above the middle line, up below
  it, and either way on it.
- **Seconds**: a harmonic second is drawn with the lower note on the
  left, whichever way the stem points.

Each rule has a backing `Rule`/`Constraint` pair referencing this
Framework, plus a pure check function. `constraint_evaluate` is never
called here.

## Not covered

Engraving craft (clef drawing steps, notehead shape/size/tilt, stem
length, beam thickness), rhythm and beaming, accidentals, key and time
signatures, octave-transposing clefs.

Value types (`Clef`, `StaffPosition`) live in `src/pitch/`; this module
holds only the rules built on them.
