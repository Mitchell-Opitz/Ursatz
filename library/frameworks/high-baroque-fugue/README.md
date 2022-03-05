# high-baroque-fugue

Theory Framework instance `high-baroque-fugue-v1`: the fugal exposition of the
high Baroque (Bach, Handel, and their contemporaries). Path
`frameworks/high-baroque-fugue/`, tests in
`frameworks/high-baroque-fugue/tests/`. A Framework extension module
physically separate from `src/theory/`, the same shape as
`frameworks/standard-counterpoint/`.

The dependency on `standard-counterpoint` is one way: this module reads its
pitch arithmetic, intervals, and slot types
(`standard-counterpoint/counterpoint_pitch.h`, `counterpoint_interval.h`,
`counterpoint_slots.h`), and that framework knows nothing of this one. File
names carry a `fugue_` prefix so they never collide with the
`standard-counterpoint` headers on the include path.

## Scope

The subject of every rule is `FugueExposition` (`fugue_exposition.h`): the
voices of a fugue, listed from the highest to the lowest (soprano, alto,
tenor, bass in four voices), each a slotted line (attacks, ties, and rests) on
one common grid; the statements the caller identifies in them, each a voice,
a start slot, a `FugueRole` (`src/semantics/fugue_role.h`), and a number of
notes; the number of slots in a bar; the tonic; whether the countersubject is
declared invertible; and the limits behind the soft rules.

The rule groups are added in the sections below.
