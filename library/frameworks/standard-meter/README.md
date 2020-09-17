# standard-meter

Theory Framework instance `standard-meter-v1`: how Western staff
notation organizes rhythm into meter. Path `frameworks/standard-meter/`,
tests in `frameworks/standard-meter/tests/`. A Framework extension module
physically separate from `src/theory/`, same shape as
`frameworks/standard-rhythm/` and `frameworks/standard-notation/`. It
depends on `time/` types (`NoteValue`, `Duration`, `TimeSignature`), not
on standard-rhythm.

## Scope

Simple meter, built on `TimeSignature` (`src/time/time_signature.h`: a
beat count and an undotted beat unit):

- **Beats and grouping** (`grouping_rules.h`): the beat divides into
  two, then four; 2, 3, 4 beats per group are duple, triple, quadruple;
  beat 1 is the downbeat and the last beat the upbeat.
- **Time signatures** (`time_signature_rules.h`): a simple meter's top
  number is 2, 3, or 4; the bottom number is usually 2, 4, or 8 (1 and 16
  also occur); common time is 4/4 and cut time is 2/2; a time signature
  is not a fraction (2/4 and 4/8 hold the same length but differ).
- **Measures** (`measure_rules.h`): a measure is one beat group; a
  multi-measure rest of n measures lasts n times the measure.
- **Anacrusis** (`anacrusis_rules.h`): a pickup is shorter than a
  measure, is counted as the end of an imaginary measure, and shortens
  the final measure by its length.
- **Counting** (`counting_rules.h`): American traditional counting by
  position in the beat -- "1 e + a", with "ta" one level finer, and
  parentheses for beats not said aloud.
- **Beaming** (`beaming_rules.h`): only eighth notes and shorter are
  beamed; how many of a value make a beat depends on the time signature;
  a beam group stays inside one beat.
- **Beat ties** (`beat_tie_rules.h`): a note starting off the beat that
  runs past the next beat is split and tied there to show the beat.

Stem and flag direction for beamed and flagged notes are in
`frameworks/standard-notation/`.

Each rule has a backing `Rule`/`Constraint` pair referencing this
Framework, plus a pure check function. `constraint_evaluate` is never
called here.

## Not covered

Compound meter, meter changes, tuplets, `MeterMap`, partial beams for
mixed groupings, the vocal-music beaming convention, conducting patterns,
and engraving craft. That simple quadruple can feel like two groups of two
is a perceptual ambiguity and is not asserted as a rule. Counting is
American only; the syllables are a table so another system can be added.
