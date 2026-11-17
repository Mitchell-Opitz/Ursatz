# standard-markings

Theory Framework instance `standard-markings-v1`: the conventional
markings of Western staff notation other than pitch placement. Where it lives: the rules are in `library/theory/markings/` (tests `library/theory/tests/markings/`). A Framework extension module
physically separate from `theory/style/`, same shape as
`theory/tonality/`.

## Scope

Meaning for the Italian vocabulary carried by the opaque marking types
in `model/events/` (`Dynamics`, `TempoMarking`):

- **Dynamics**: loudness order from ppp to fff with unlimited -issimo
  stacking, mezzo as "moderately", crescendo/decrescendo/diminuendo as
  louder/quieter.
- **Tempo**: textual tempi in groups and a slowest-to-fastest order,
  ritardando/accelerando/a tempo.

Each rule is a pure check function; no rule builds a `Rule` or `Constraint`.
