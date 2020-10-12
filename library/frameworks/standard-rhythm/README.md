# standard-rhythm

Theory Framework instance `standard-rhythm-v1`: how Western staff
notation writes rhythm. Path `frameworks/standard-rhythm/`, tests in
`frameworks/standard-rhythm/tests/`. A Framework extension module
physically separate from `src/theory/`, same shape as
`frameworks/standard-notation/` and `frameworks/standard-markings/`.

## Scope

Rules about note and rest values, built on `NoteValue`
(`src/time/note_value.h`: a power-of-two exponent plus a dot count):

- **Names** (`name_rules.h`): American and British names, double whole
  note / breve through sixty-fourth note / hemidemisemiquaver. Names stop
  at the sixty-fourth; a dot is not part of a name.
- **Hierarchy** (`hierarchy_rules.h`): each value divides into two of the
  next smaller, so a whole note is two half notes, four quarter notes,
  eight eighth notes. Stated for undotted values.
- **Noteheads, stems, flags** (`notehead_rules.h`): quarter and shorter
  are filled; a filled notehead always has a stem, an unfilled one may;
  flags go only on the stem of a filled notehead, one for the eighth and
  one more per halving; the three ways to halve a value are add a stem,
  fill the notehead, add a flag.
- **Rests** (`rest_rules.h`): every note value has a rest, named for its
  value (the double-length rest is the breve rest); flags halve a rest;
  rests share the hierarchy, so a breve rest equals two whole rests.
- **Dots** (`dot_rules.h`): a dot adds half the value, each further dot
  half the previous; n dots give (2^(n+1) - 1) / 2^n of the base (7/4 for
  two). NoteValue caps dots at four.
- **Ties** (`tie_rules.h`): a tie joins notes of the same pitch, never
  rests; a tied-to note is not rearticulated; tied durations add (a half
  tied to a quarter is a dotted half).
- **Tuplets** (`tuplet_rules.h`): built on `Tuplet` (`src/time/tuplet.h`,
  a ratio of notes played to notes replaced). Three in the time of two is
  a triplet, two in the time of three a duplet, anything else is an
  unnamed tuplet; the number written over the group is the number of
  notes played; the group lasts as long as the notes it replaces (a
  triplet of eighths is a quarter, of quarters a half).

Each rule has a backing `Rule`/`Constraint` pair referencing this
Framework, plus a pure check function. `constraint_evaluate` is never
called here.

## Not covered

Engraving craft (where a rest sits on the staff, notehead shape, stem
length), beams, meter and time signatures (`frameworks/standard-meter/`),
and slurs. The tie
rules take pitches and values as plain inputs because the `Tie` entity
holds only two NoteEvent ids; resolving them is the caller's job. Pitch is
compared as written (`PitchSpelling` equality), so enharmonic respellings
are different pitches here.
