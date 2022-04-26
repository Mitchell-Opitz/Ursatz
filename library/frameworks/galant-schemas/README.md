# galant-schemas

Theory Framework instance `galant-schemas-v1`: the schemas of the Galant style,
the stock musical phrases that composers and improvisers of the 18th century
learned as the melodic, harmonic, and metric skeletons for new music
(Gjerdingen 2007). Path `frameworks/galant-schemas/`, tests in
`frameworks/galant-schemas/tests/`. A Framework extension module physically
separate from `src/theory/`, the same shape as
`frameworks/standard-counterpoint/`.

The dependency on `standard-counterpoint` is one way: this module reads its
pitch arithmetic (`standard-counterpoint/counterpoint_pitch.h`), and that
framework knows nothing of this one. File names carry a `galant_` prefix so
they never collide with the `standard-counterpoint` headers on the include
path.

## Scope

A schema is a prototype (an idealized pattern), an exemplar (a single pattern
that resembles it), or a theory (an explanation of how the exemplar functions):
`SchemaSense`, `src/semantics/schema_sense.h`. This framework holds the
prototypes and decides whether an exemplar resembles one.

- A prototype (`galant_prototype.h`) is two or more stages, each with the
  strength of the beat it falls on (`BeatStrength`, strong or weak), the scale
  degrees of the melody and the bass, the figures, and the Roman numeral, and
  where it typically appears (`SchemaFunction`, `src/semantics/schema_function.h`).
- An exemplar (`galant_exemplar.h`) is the pitches of the melody and the bass
  and the strength of the beat at each stage, and the tonic. The caller says which
  notes are the stages.
- A degree (`galant_degree.h`) is counted from the tonic in letter names, with
  an alteration in semitones against the major scale, so the third and sixth
  degrees can be lowered in a minor key (mi or me, la or le).

The prototypes and the matching rules are added in the sections below.
