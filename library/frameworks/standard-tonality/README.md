# standard-tonality

Theory Framework instance `standard-tonality-v1`: the tonal conventions
of Western music. Path `frameworks/standard-tonality/`, tests in
`frameworks/standard-tonality/tests/`. A Framework extension module
physically separate from `src/theory/`, same shape as
`frameworks/standard-notation/`. It does not depend on any other
framework module: pitch arithmetic it needs is its own.

## Scope

Rules about how pitches relate within a key:

- **Major scale**: W-W-H-W-W-W-H, named for its first note.
- **Scale degrees**: numbers, movable-do solfege, and degree names.
- **Key signatures and keys**: which major key a key signature belongs
  to, and real versus imaginary keys.
- **Circle of fifths**: major keys ordered by number of accidentals.

How accidentals are written and ordered in a key signature is notation
and lives in `frameworks/standard-notation/`. Minor scales and the
subtonic are not covered yet.

Each rule has a backing `Rule`/`Constraint` pair referencing this
Framework, plus a pure check function. `constraint_evaluate` is never
called here.
