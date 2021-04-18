# standard-texture

Theory Framework instance `standard-texture-v1`: musical texture, the
density of and interaction between a work's voices. Path
`frameworks/standard-texture/`, tests in
`frameworks/standard-texture/tests/`. A Framework extension module
physically separate from `src/theory/`, same shape as
`frameworks/standard-tonality/` and `frameworks/standard-rhythm/`. It does
not depend on any other framework module.

## Scope

Rules about the four main textures (monophony, heterophony, homophony,
polyphony), homophony's two subcategories (homorhythm, melody and
accompaniment), classifying a passage from features, and works that move
between textures. The vocabulary names `Texture` (`src/interpretation/`),
the framework-relative interpretation of a passage.

Each rule has a backing `Rule`/`Constraint` pair referencing this
Framework, plus a pure check function. `constraint_evaluate` is never
called here.
