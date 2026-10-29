# standard-texture

Theory Framework instance `standard-texture-v1`: musical texture, the
density of and interaction between a work's voices. Path
`frameworks/standard-texture/`, tests in
`frameworks/standard-texture/tests/`. A Framework extension module
physically separate from `src/theory/`, same shape as
`frameworks/standard-tonality/` and `frameworks/standard-rhythm/`. It does
not depend on any other framework module.

## Scope

Rules about musical texture, the density of and interaction between a
work's voices. The vocabulary names `Texture`
(`src/interpretation/texture.h`), the framework-relative interpretation
of a passage; its target is the passage's `EntityID`.

- **Textures** (`texture_rules.h`): four main categories and six names.
  Monophony is a single unaccompanied line (voices in unison included);
  heterophony is simultaneous variants of one melodic line; homophony is
  voices moving together harmonically at the same pace, with two
  subcategories, homorhythm (similar or unison rhythm in all voices, block
  chords) and melody and accompaniment (a clear melody over supporting
  voices, often in a different rhythm); polyphony is voices with separate
  melodic lines and rhythms. Names read back, categories, and the fewest
  voices each has.
- **Classification** (`texture_classification_rules.h`): a passage's
  textures from features the caller has measured: voice count, unison,
  shared melody, rhythmic unison, a predominant melody, independent lines.
  The source gives no numeric threshold for any of them, so each is a
  yes or no. Two answers: every texture the passage has (a melody standing
  out in a homorhythmic passage is both subcategories), and one by a fixed
  precedence (monophony, heterophony, polyphony, homorhythm, melody and
  accompaniment). The general homophony is the category, never the single
  answer. Contradictory features are not a passage.
- **Mixed textures** (`mixed_texture_rules.h`): most works move between
  textures. A work's textures are an ordered list of passages and their
  textures; the rules find where the texture changes (by texture, or by
  category, so homorhythm to melody and accompaniment is not a change of
  category), count the textures used, say whether a work is mixed, and
  build a `Texture` labeled with the texture's name. No container type is
  added for the list.

Each rule has a backing `Rule`/`Constraint` pair referencing this
Framework, plus a pure check function. `constraint_evaluate` is never
called here.

## Not covered

Reading the features from events (are these voices variants of one line?
is there a predominant melody?) is analysis, in `src/analysis`, and the
source gives no algorithm for it. The listening examples in the source are
recordings, not rules; the tests model them as feature sets. Other
textures and finer distinctions (counterpoint species, orchestration, the
kinds of polyphony such as imitative or free) are not in this framework.

## Analysis of real music

`analysis/` measures texture features per measure from separated voices, classifies each measure with the rules above (no category is added) and merges the measures into passages with change points. The input is a neutral struct the caller fills; standard-texture includes no other framework. Accompaniment patterns (Alberti, arpeggiated, block chords, oom-pah) are not categories or subtypes here, so none is classified; the repeated-pattern score is measured and reported as evidence for them. See `docs/modules/texture-analysis.md`.


## Neutral analysis input

`standard_texture_input_add_analysis_note` (`analysis/texture_analysis_input.h`) takes an `AnalysisNote` (`semantics/analysis_note.h`): id, span, `midi` as the pitch, track, voice and voice confidence carry over. A note with an unknown pitch is rejected.
