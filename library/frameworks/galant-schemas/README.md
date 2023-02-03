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
that resembles it), or a theory (an explanation of how the exemplar functions).
This framework holds the prototypes and decides whether an exemplar resembles one.

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

- **Prototypes** (`galant_prototypes.h`), 26 schemas. Meyer, Prinner, and the
  modulating Prinner are as the Galant schemas chapter of Open Music Theory sets
  them out; the rest are from the Galant Schemas - Summary chapter, taken from
  its itemized stage tables. Each has stages with a beat strength, melody and
  bass degrees (a stage of the Monte has two melody notes), figures, and Roman
  numerals, a function, and two flags: whether the bass is defining (every schema
  but the Meyer family) and whether the melody moves in parallel tenths (the
  Prinner and modulating Prinner).
  - *Opening gambits:* Meyer, Romanesca, Do-Re-Mi (three stages, and a derived
    four-stage form), Sol-Fa-Mi, Aprile, Jupiter, Pastorella. The bass and
    harmony of the Meyer, Aprile, Jupiter, and Pastorella are less fixed than
    their melody.
  - *Continuation* (the answer, process, and transition schemas): Prinner,
    modulating Prinner, each with an added dominant (five stages), Fonte, Monte,
    Ponte.
  - *Pre-cadential:* Fenaroli, Indugio, Deceptive Cadence, Evaded Cadence, Passo
    Indietro, Comma, Converging Cadence.
  - *Cadence:* Cadenza Semplice, Cadenza Composta, Cadenza Doppia.
  - *Post-cadential:* Quiescenza.
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
- **Other schemas.** Schemas of the Galant style beyond the 26 above.
- **Parts of a schema.** Many schemas divide into question and answer parts (the
  Do-Re and Re-Mi parts, the Sol-Fa and Fa-Mi parts, the Meyer's opening and
  closing parts, the Fonte and Monte "modular" halves); the parts are not
  prototypes of their own, though the Comma and the Passo Indietro are listed as
  schemas.
- **Harmonic variants.** The Sol-Fa-Mi with I vii V I in place of I ii V I
  differs only in harmony, which is not checked.
- **Sources.** The itemized stage tables are followed where the summary table
  disagrees (the Cadenza Composta has five stages in the itemized table, and the
  Passo Indietro and the Converging Cadence have the meters the itemized tables
  give). The Prinner's figures row ends in "I", a numeral, in the chapter; it is
  corrected to 5, as the other schemas end. The four-stage Do-Re-Mi is derived
  from the chapter's description (the Do-Re and Re-Mi parts put back together),
  not from a table. The Cadenza Composta's third numeral, "Cad.", is kept as
  given.
- **Minor-key Meyer.** The Meyer's last melody degree is mi, as the chapter has
  it, so a minor-key Meyer that ends on me is not accepted.
- **Finding the stages.** The caller says which notes are the stages of an
  exemplar and which beats they fall on.
- **Theory.** The third sense of "schema", the explanation of
  how an exemplar functions in a passage, is not modeled beyond the match
  itself.
