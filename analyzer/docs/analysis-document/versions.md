# Analysis document version history

Every `schema_version` of the analysis document and what changed. Back to the [docs index](../README.md) and the [analysis document](../analysis-document.md).

- **10**: `outer_voices` has one entry per voice segment (one per melody note) instead of one per chord segment, so several
  entries share a chord on a harmonic span and `chord_index` is many to one; each entry's window is its voice segment's.
  The shape of an entry is unchanged, but its meaning is not, and a reader that indexed the lines by chord must read
  `chord_index`. A change of meaning is a breaking change under the rule above. The chord claims' `outer` is unchanged:
  the lines' entry at the chord's start.
- **9**: Motive claims gain the optional `features` object and motive occurrences the optional `degrees` array, both
  additive. Adds the required placeholders `progressions` (`[]`), `harmonic_rhythm`, `markings` and
  `counterpoint_check` (`null`) after `schemas`, so the P3 sections fill them without another version. Required
  sections are a breaking change under the rule above. Chord claims gain the optional `context`, which did not
  need a version of its own. `cadences` is filled (its claims are new), and tonicization and modulation claims
  gain the optional `cadence_confirmed`; both are additive. Chord claims gain the optional `function` and
  `six_four`, and the optional top-level `prolongations` follows `chord_vocabulary`; all additive. `progressions` and `harmonic_rhythm` are filled (their
  entries are new), which is additive. `phrases` is filled (its claims are new, with the optional `subphrases`,
  `ideas` and `phrase_model`), and chord claims gain the optional `phrase_model`; all additive. The optional
  top-level `themes` follows `phrases`, and phrase claims gain the optional `expansions`; both are additive. `form` is
  filled (its object is new), which is additive. `motives` is filled (its claims are new), which is additive. `melody` is filled
  (its claims are new), which is additive. `rhythm` is filled (its claims are new, as one flat array labelled by kind),
  which is additive. `texture` is filled (its claims are new, as one flat array labelled by kind), which is additive.
  `voice_leading` is filled (its claims are new, as one flat array labelled by kind), which is additive. `schemas` is
  filled (its claims are new, as one flat array labelled by kind), which is additive. `markings` is filled (its object is
  new), which is additive.
- **8**: Adds the required `chord_vocabulary` section and the required `roman_numeral.base_numeral` field. A required
  section and a required field are breaking changes under the rule above.
- **7**: Chord claims gain the required `roman_numeral` object (the library's Roman numeral in the local key,
  the key region it is relative to, and the reason when there is none). A required field is a breaking change
  under the rule above. `roman_numeral` also gains the optional `applied` member for applied chords, which did not
  need a version of its own.
- **6**: `metadata` gains the required `input` object (format, source type, whether meter, key and spelling are notated, embedded or inferred, parts, tempo), and `meter.source` gains `"notated"`. A required field is a breaking change under the rule above.
- **5**: Adds the required `outer_voices` section (the soprano and bass lines) and the required `outer`
  field on every chord claim. A required section and a required field are breaking changes under the
  rule above.
- **4**: `keys` is an object of key regions with a derived home key (it was an array holding one
  whole-piece claim with a `method`), `metadata` gains the required `key_signatures`, and the top
  level gains the required `notes`. The `method` field is gone, with the key-signature, bass-fallback
  and pitch-class-scan steps it named. Changing a section's type and removing a field are breaking
  changes under the rule above.
- **3**: Adds the required `non_chord_tones` section. Chord claims gain required `root`, `quality`, `bass`, `inversion`, `duration`, `score` and
  `runner_up`, and are cut by the library's sonority segmentation, so `span` and `note_ids` cover the
  segment's window and every note sounding in it. A required field is a breaking change under the
  rule above.
- **2**: `meter` is filled (it was `null`). Every claim has a required `location`. v1 documents had
  `"meter": null` and no `location`.
- **1**: First version.
