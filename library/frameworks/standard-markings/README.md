# standard-markings

Theory Framework instance `standard-markings-v1`: the conventional
markings of Western staff notation other than pitch placement. Path
`frameworks/standard-markings/`, tests in
`frameworks/standard-markings/tests/`. A Framework extension module
physically separate from `src/theory/`, same shape as
`frameworks/common-practice-minimal/` and `frameworks/standard-notation/`.

## Scope

Meaning for the Italian vocabulary and symbols carried by the opaque
marking types in `src/events/` (`Dynamics`, `Articulation`,
`TempoMarking`, `StructuralMarker`):

- **Dynamics**: loudness order from ppp to fff with unlimited -issimo
  stacking, mezzo as "moderately", crescendo/decrescendo/diminuendo as
  louder/quieter.
- **Articulations**: connected, separated, and accented attacks, and the
  stated force relations between accents.
- **Tempo**: textual tempi in groups and a slowest-to-fastest order,
  metronome marking arithmetic, ritardando/accelerando/a tempo.
- **Structural markers**: fermata, caesura, breath mark, repeats and
  numbered endings, D.C./D.S. al coda/fine, and the order in which
  measures are played.

Each rule has a backing `Rule`/`Constraint` pair referencing this
Framework, plus a pure check function. `constraint_evaluate` is never
called here.
