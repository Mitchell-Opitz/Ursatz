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

Every rule has a Rule/Constraint pair referencing this Framework plus a pure
check function. A rule that states what the schema requires is hard; "less
fixed" and "usually" features are soft. `constraint_evaluate` is never called
here.

- **Prototypes** (`galant_prototypes.h`), as the Galant schemas chapter of Open
  Music Theory sets them out:
  - *Meyer*, an opening gambit of four stages on weak, strong, weak, strong
    beats: melody do ti fa mi, bass do re ti do, harmony I V V I. The first and
    last stages are tonic chords. The bass and the harmony are less fixed than
    the melody.
  - *Prinner*, a continuation (the typical response to an opening schema) of
    four stages on strong, weak, strong, weak beats: melody la/le sol fa mi/me
    over bass fa mi/me re do, in parallel tenths, harmony IV I vii I. The bass
    is defining.
  - *Modulating Prinner*, a continuation: melody mi re do ti over bass do ti la
    sol, in parallel tenths, with Roman numerals in the tonic key (I V vii/V V)
    and in the dominant key (IV I vii I).
- **Matching rules** (`galant_matching_rules.h`), an exemplar against a
  prototype stage by stage (`GalantMatch`). Hard: the same number of stages, the
  same beat strengths, and the melody degrees (an accepted alteration counts;
  la/le and mi/me accept either the major or the lowered degree); and, when the
  prototype's bass is defining, the bass degrees. Soft: the bass degrees, when
  the bass is less fixed (the Meyer), and parallel tenths (a third, or a tenth,
  with the melody above the bass) for the Prinner schemas.
- **Classification** (`galant_classification.h`): `galant_classify` reports, as a
  bit mask indexed by `GalantSchemaId`, the prototypes an exemplar resembles:
  those it breaks no hard rule against. It can resemble more than one, or none.

## Not covered

- **Harmony.** The figures and Roman numerals of a prototype are data. They are
  not checked in an exemplar, which needs chord analysis.
- **Other schemas.** The Pastorella, Jupiter, and Aprile (named as like the
  Meyer), and the continuation, cadence, and post-cadential schemas of the
  summary chapter, are not encoded yet.
- **Parts of a schema.** The Meyer divides into an opening part (I to V) and a
  closing part (V to I); the parts are not prototypes of their own.
- **Minor-key Meyer.** The Meyer's last melody degree is mi, as the chapter has
  it, so a minor-key Meyer that ends on me is not accepted.
- **The Prinner figures.** The figures row as the chapter gives it (5, 6, 7-6,
  I) looks garbled and is kept as given.
- **Finding the stages.** The caller says which notes are the stages of an
  exemplar and which beats they fall on.
- **Theory.** The third sense of "schema" (`SchemaSense`), the explanation of
  how an exemplar functions in a passage, is not modeled beyond the match
  itself.
