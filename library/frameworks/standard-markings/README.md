# standard-markings

Theory Framework instance `standard-markings-v1`: the conventional
markings of Western staff notation other than pitch placement. Path
`frameworks/standard-markings/`, tests in
`frameworks/standard-markings/tests/`. A Framework extension module
physically separate from `src/theory/`, same shape as
`frameworks/common-practice-minimal/`.

## Scope

Meaning for the Italian vocabulary carried by the opaque marking types
in `src/events/` (`Dynamics`, `TempoMarking`):

- **Dynamics**: loudness order from ppp to fff with unlimited -issimo
  stacking, mezzo as "moderately", crescendo/decrescendo/diminuendo as
  louder/quieter.
- **Tempo**: textual tempi in groups and a slowest-to-fastest order,
  ritardando/accelerando/a tempo.

Each rule is a pure check function; no rule builds a `Rule` or `Constraint`.
