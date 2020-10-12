# standard-meter

Theory Framework instance `standard-meter-v1`: how Western staff
notation organizes rhythm into meter. Path `frameworks/standard-meter/`,
tests in `frameworks/standard-meter/tests/`. A Framework extension module
physically separate from `src/theory/`, same shape as
`frameworks/standard-rhythm/` and `frameworks/standard-notation/`. It
depends on `time/` types (`NoteValue`, `Duration`, `TimeSignature`), not
on standard-rhythm.

## Scope

Simple and compound meter, built on `TimeSignature`
(`src/time/time_signature.h`: a top number and an undotted bottom unit --
the two numbers and nothing more; this framework reads them):

- **Meter class** (`meter_class_rules.h`): top 2, 3, 4 is simple and 6, 9,
  12 is compound, each duple, triple, or quadruple (six categories). Any
  other top number is unclassified, not invalid. A simple top counts
  beats; a compound top counts divisions, three to a beat, so the beat is
  a dotted note (a dotted quarter over 8, a dotted half over 4, a dotted
  eighth over 16). A simple beat divides into 2 then 4, a compound beat
  into 3 then 6. The usual compound bottom numbers are 4, 8, 16.
- **Beat grid** (`beat_grid.h`): where beats fall in a measure, from the
  meter class, so beaming, ties, and counting work for both.

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
  position in the beat -- simple "1 e + a" with "ta" one level finer,
  compound "1 ta la ta li ta" -- and parentheses for beats not said
  aloud. A dot parenthesizes a beat in simple meter but not in compound,
  where the dotted note is the beat.
- **Beaming** (`beaming_rules.h`): only eighth notes and shorter are
  beamed; how many of a value make a beat depends on the time signature;
  a beam group stays inside one beat, so sixteenths beam in sixes in 6/8
  and in threes in 6/16.
- **Borrowed divisions** (`borrowed_rules.h`): a triplet in a simple
  meter and a duplet in a compound meter divide the beat the other way,
  "borrowed" from the other class. A tuplet's metric level (beat,
  subdivision, multi-beat) comes from its span against the beat; triplets
  are typical at any level and duplets at the beat or a subdivision, as a
  description, not a prohibition. Counting borrows the other class's
  syllables: a triplet is "1 la li", a duplet "1 +".
- **Hypermeter** (`hypermeter_rules.h`): groups of measures counted like
  beats (duple, triple, quadruple hypermeter); the first is accented, and
  in quadruple the third also is. Pure counting on a measure index.
- **Syncopation** (`syncopation_rules.h`): off-beat accents, made by a
  note sustained from off the beat across the next beat line (ties and
  dots), a rest on the beat with notes only off it, or an accent on an
  off-beat note. Inputs are plain positions, lengths, and an accent flag.
- **Beat ties** (`beat_tie_rules.h`): a note starting off the beat that
  runs past the next beat is split and tied there to show the beat.

Stem and flag direction for beamed and flagged notes are in
`frameworks/standard-notation/`.

Each rule has a backing `Rule`/`Constraint` pair referencing this
Framework, plus a pure check function. `constraint_evaluate` is never
called here.

## Not covered

Irregular meters (5/8, 7/8), meter changes, `MeterMap`, partial beams for
mixed groupings (including the common partial-beam variations), the
vocal-music beaming convention, conducting patterns (the same for simple
and compound),
and engraving craft. That simple quadruple can feel like two groups of two
is a perceptual ambiguity and is not asserted as a rule. Counting is
American only; the syllables are a table so another system can be added.
