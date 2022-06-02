# scale-harmonization

Theory Framework instance `scale-harmonization-v1`: harmonizing a bass scale in
the schema and partimento tradition (Open Music Theory, Galant schemas: the
Rule of the Octave and Harmonizing the Scale with Sequences). Path
`frameworks/scale-harmonization/`, tests in
`frameworks/scale-harmonization/tests/`. A Framework extension module physically
separate from `src/theory/`, the same shape as `frameworks/standard-counterpoint/`.

The dependencies on other frameworks are one way: this module reads the pitch
arithmetic and intervals of `standard-counterpoint` and the scale degrees of
`galant-schemas` (`galant_degree.h`), and neither knows anything of this one.
File names carry a `scale_` prefix so they never collide with the other
frameworks' headers on the include path.

## Scope

The subject of every rule is `ScaleHarmonization` (`scale_harmonization.h`): a
run of stages, one for each bass note (or for each harmony, where two sit over one
bass note), each with the bass and the upper voices, and the direction the bass
runs (`ScaleDirection`, `src/semantics/scale_direction.h`) and the tonic. Rhythm
is not modeled.

The Rule of the Octave is a cheat sheet for harmonizing bass lines, one chord for
each scale degree, in an ascending and a descending form. A sequence
(`SequenceKind`, `src/semantics/sequence_kind.h`) is a pattern of one or more
harmonies that repeats in the direction of the scale.

The rule groups are added in the sections below.
