# standard-form

Theory Framework instance `standard-form-v1`: how Western tonal music is
organized into phrase-level form. Path `frameworks/standard-form/`, tests
in `frameworks/standard-form/tests/`. A Framework extension module
physically separate from `src/theory/`, same shape as
`frameworks/standard-meter/` and `frameworks/standard-harmony/`. The names
it uses are plain value types in `src/semantics/` (`FormLevel`,
`MotiveKind`); this framework gives them their conventions.

## Scope

- **Hierarchy** (`hierarchy_rules.h`): form is a hierarchical grouping of
  units, piece, movement, section, theme, phrase, subphrase, idea, motive
  (`FormLevel`, largest to smallest). A larger level contains a smaller
  one. Two levels may collapse into one (a single phrase may be a whole
  section), so the hierarchy is a guide, not a strict definition.
  Phrase-level form is the phrase, subphrase, idea and motive levels.
- **Motives** (`motive_rules.h`): a motive is a short, regularly recurring
  snippet of music, shorter than the idea that contains it; a whole theme
  or melody is not a motive. A bare "motive" means a pitch motive
  (`MotiveKind`); rhythmic, contour and timbre motives are named for their
  kind. The usual transformations are enlargement, contraction, inversion,
  displacement, retrograde, intervallic manipulation and embellishment,
  each mapped to the `src/transform/` type that carries it out (enlargement
  is `augmentation`, contraction is `diminution`, intervallic manipulation
  is `intervallic_manipulation`, embellishment is `ornamentation`).
- **Phrases** (`phrase_rules.h`): a phrase is a relatively complete
  thought with a beginning, middle and end, trending toward a goal and
  arriving at closure. A cadence signals closure; the lack of a cadence
  does not say a phrase is open, since closure has other signals.
- **Segmentation analysis** (`segmentation_rules.h`): divides a passage
  into grouping units with square brackets. The steps run in order:
  identify points of closure (usually cadences), divide each phrase into
  smaller units, bracket those units, verify and label the cadences. An
  idea is often two measures long but may be longer or shorter, and the
  ideas of a phrase together fill it exactly. Lengths are whole measures.

Related types: `Idea` (`src/containers/idea.h`) is the reference-only
grouping of events at the idea level, built like `Phrase`. Segmentation
is a procedure over `Phrase`, `Idea` and `Cadence`; it is not a stored
entity.

Each rule has a backing `Rule`/`Constraint` pair referencing this
Framework, plus a pure check function. `constraint_evaluate` is never
called here.

## Not covered

Subphrases and the phrase archetypes (a later chapter), other signals of
closure besides the cadence, and a numeric size limit on a motive (only
its relation to the idea is asserted). `analysis/phrase_segmentation` and
`analysis/motif_detection` do not use these rules yet.
