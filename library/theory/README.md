# Theory

The theory group (`library/theory/`, target `ursatz_theory`): the `Framework` type, the style model, the rule objects,
the shared vocabulary enums and the rule libraries of the 11 Theory Frameworks. A framework is no longer a CMake target
or a folder of its own: its rules, vocabularies and predicates are an area here, its analysis adapter is an area of
[analysis/](../analysis/README.md), and its identity stays (`standard-harmony-v1` and the others are in Analyzer
documents). Code that needs the whole stack lives in the analysis chain (`chain/`, see `chain/README.md`).

## Areas and the framework each holds

| Area | Holds | Former framework |
|---|---|---|
| `framework/` | The `Framework` type and `framework_singleton_get` | kernel `theory` |
| `style/` | Style models from corpus statistics | kernel `theory` |
| `rules/` | `Rule`, `Constraint`, `Preference`, `Context` | kernel `rules`, `context` |
| `knowledge/` | The knowledge snapshot: the entries of `knowledge/` embedded in the build, and the pitch material read from them | none (new, R-536) |
| `vocabulary/` | Shared theory enums (`CadenceType`, `VoicePart`, ...) | kernel `semantics` |
| `tonality/` | Roman numeral grammar, keys, triad and seventh rules, chord and key vocabulary, and the common-practice materials (see Tonality below) | standard-tonality, common-practice-minimal (vocabulary) |
| `harmony/` | Function, cadence, six-four and prolongation rules | standard-harmony |
| `voicing/` | Range, spacing and voicing rules | standard-voicing |
| `counterpoint/` | Species rules | standard-counterpoint |
| `form/` | Phrase, period, sentence, hybrid and expansion rules | standard-form |
| `meter/` | Meter classification (simple, compound, duple, triple, quadruple) read from the signature's beat groups, which own the beat (`time_signature_beat_length`), beat grid, syncopation, hemiola, hypermeter rules | standard-meter |
| `galant/` | Schema prototypes, rule of the octave, sequences | galant-schemas, scale-harmonization |
| `markings/` | Dynamics and tempo rules | standard-markings |

Each area's `README.md` (`galant/sequences.md` for scale-harmonization) is the former framework's README and names both halves.

Each framework's own `README.md` holds its full scope and exclusions. This page is the
catalog and the shared conventions.

## Catalog

| Framework | Covers | Depends on |
|---|---|---|
| `common-practice-minimal` | 7 diatonic triads, 4 diatonic seventh qualities, major and natural-minor keys. The weighted chord identification and key regions the chain runs | standard-tonality (`tonal_key.h` only, the scale degree owner), model group |
| `standard-markings` | Dynamics order, tempo words | model group |
| `standard-meter` | Simple and compound meter, borrowed divisions, hemiola, hypermeter, syncopation | model group |
| `standard-tonality` | Keys and key signatures, triad and seventh qualities, Roman numerals | model group |
| `standard-texture` | Monophony, heterophony, homophony, polyphony | model group |
| `standard-voicing` | Strict SATB voicing of one chord | model group |
| `standard-counterpoint` | Species counterpoint, first through fourth | model group |
| `galant-schemas` | Gjerdingen's Galant schemas | standard-counterpoint, model group |
| `scale-harmonization` | Rule of the octave, sequences over a bass scale | galant-schemas, standard-counterpoint, model group |
| `standard-harmony` | Harmonic function, phrase model, cadences (two competing readings where the music has two: V to I6 as evaded cadence or prolongation, a dominant before a section start as half or authentic cadence, each with a share in thousandths), six-four chords, prolongation, tonicization, modulation | model group |
| `standard-form` | Phrases, sentence, period, hybrid forms, phrase expansion and contraction, motive analysis | standard-harmony, model group |

Most of the frameworks after `common-practice-minimal` follow Open Music Theory, a free
textbook; each README names the chapters it covers.

## Shared conventions

- **Rule libraries, not analyzers.** Every framework except `common-practice-minimal` takes
  caller-supplied plain data (scale degrees, spellings, inversions, flags) and returns `bool`
  results and hard/soft violation bitmasks. Eight frameworks (`standard-tonality`,
  `standard-harmony`, `standard-counterpoint`, `standard-form`, `standard-meter`,
  `standard-texture`, `galant-schemas`, `scale-harmonization`) also have an `analysis/` adapter
  that builds `Interpretation`s from neutral input. The `chain/` target builds that input from
  imported notes (`chain/README.md`).
- **Framework singleton.** Each exposes one lazily built `Framework` instance. Rules are
  checked with the framework's own `*_rule_holds()` and check functions. standard-harmony and
  standard-counterpoint also build a `Preference` per soft rule, scored by the
  `Preference`'s callback; no other framework builds a `Rule`, `Constraint` or `Preference`.
- **Hard vs. soft.** Rules phrased as "must" are hard. Rules phrased as "limit", "avoid" or
  "prefer" are soft.
- **Dependency ceiling.** The model group, `theory/framework/` and `theory/rules/`, and any area listed in its row of
  `cmake/layer-table.txt` (`theory-<area>` rows, test `layer_table`); a new include edge updates its row in the same
  commit. Input from `analysis` is passed in by the caller as plain data, never included.
- **Test-only edges.** Area tests follow the same layer rules (the `tests-theory-<area>` rows of the layer table). Edges the tests have beyond the table are rows there with a reason: harmony tests include tonality (Roman numeral grammar cases), meter tests include the MIDI importer.
- **Vocabulary enums.** Names a framework needs (for example `CadenceType`, `VoicePart`) live
  as plain enums in `theory/vocabulary/` and carry no framework claim. What each name means is the
  framework's convention. See `docs/modules/semantics.md`.

## Using a kernel analysis owner

A framework may not include kernel L8 (`analysis/`), so when a job's owner lives there ([analytic-job-owners.md](../docs/analytic-job-owners.md)) the framework takes it as an injected callback and never re-derives it. The pattern is `StandardFormBoundaryDetector` in `standard-form/analysis/analysis_phrase.h`: the framework declares the function type, the caller (in practice the `chain/` target) passes the L8 owner behind it.

## Neutral analysis input

Framework adapters accept the neutral types in `analysis/notes/` (`AnalysisNote`, `AnalysisSegment`, `AnalysisKey`, `AnalysisVoiceGrid`) through mapping functions that copy field for field with no new arithmetic, so a caller no longer writes the converter. Time is exact `Rational` everywhere in analysis; `double` is only for scores and display (Law 19). The mappings are in each framework's `analysis/` input header; `docs/modules/semantics.md` describes the types.

## Claims and rule tables

A framework builds its durable claims through the kernel helpers in `interpretation/claim_interpretation.h`. Each `analysis/*claim.c` keeps its public names and is a one-line call with the framework's id and short name: `claim_interpretation_create` for the Interpretation, `claim_interpretation_note_group` for the note shell, `claim_interpretation_relationship` for the Relationship. standard-harmony and standard-counterpoint use the one rule table and pair builder (`theory/rules/rule_table.h`, `rule_pair.h`), the pairs used only for the Preferences of their soft rules; scale-harmonization and galant-schemas set only the count and the hardness of it. A framework needs no claim code of its own.

## Singleton and rule table

A framework declares its `Framework` with `static FrameworkSingleton g_singleton = FRAMEWORK_SINGLETON_INIT("<framework>-v1");` and an accessor that returns `framework_singleton_get(&g_singleton)` (`theory/framework_singleton.h`). A rule table with scoring callbacks (standard-harmony and standard-counterpoint) is declared with `static UrsatzOnce g_built[N];` and its pairs are built once under `ursatz_once` (`core/once.h`), so the first call may come from any thread. Any other lazily built static in a framework uses `ursatz_once` the same way; never a bare `static bool` check-then-set.

## Knowledge

`knowledge/` at the repository root holds music-theory knowledge as JSON entries. The build embeds every file in `ursatz_theory` (`cmake/embed_knowledge.py` writes `knowledge_embedded.c` into the build folder; no binary reads `knowledge/` at run time). `knowledge_snapshot_build` parses the embedded files and any user files the caller gives it into an immutable `KnowledgeSnapshot` (entries in id order, each with a content hash, and a hash of the whole). Whoever calls the library builds the snapshot and passes it on; the library keeps no global copy. `model/pitch/pitch_material.h` (a model value type since P58) is the K1 entry read as steps above a center, with its key word, family, brightness rank, color notes and the material it varies or rotates. The snapshot also loads each K2 chord type (`model/pitch/chord_type.h`: the members above the root) and each K6 relation (`relation.h`: tonal, modal, neo-Riemannian and set relations by their fields; a serial entry by its common fields only), and finds a material, chord type or relation by id or by word (`knowledge_snapshot_material_named`, `_chord_type_of`, `_relation_named`). The snapshot reaches the library from the application that calls it: the Analyzer takes it in `AnalyzerOptions.knowledge` and the chain in `AnalysisChainConfig.knowledge` (a run without one is refused, `ANALYSIS_CHAIN_ERR_KNOWLEDGE`), and a snapshot must outlive every key made from it. `knowledge_snapshot.h` and `relation.h` are staged since P58 B10, because the applications include them (the Analyzer CLI and the GUI backend build one snapshot at start; the Composer's hearing builds one per hearing until P58 B13 gives it its own).

## Building and testing

The root build compiles `ursatz_theory` and adds `library/theory/tests/`, one folder per area, so root `ctest` and CI run
every area's tests.

## Public headers

`ursatz_theory` stages the headers something outside the group includes (`theory/harmony/harmony_chord.h`); the list
is in `library/theory/CMakeLists.txt` (`ursatz_public_headers`) and the layer table holds it
([docs/public-surface.md](../docs/public-surface.md)). Every header is included by its path from `library/`.

## Tonality: materials and the common-practice tables

A key in the library carries the pitch material it was made from (`KeyStructure.material`, `AnalysisKey.material`, `MotiveSketch.material`, and the home key's material in the chain's `ChainRelationContext`). The tonal functions of `tonality/tonal_key.h` read materials: `standard_tonality_material_degree_semitone`, `_material_pitch_classes` (the material's mask on a center), `_material_degree_at_or_below_semitone`, `_material_degree_of_semitone` (the lowered readings are semitone 1 always, and 3, 8 and 10 where the material has a major third with its sixth and seventh at 9 and 11, the major scale's rule), and the signature functions `standard_tonality_key_fifths_of`, `_key_signature_of_material`, `_key_of_fifths_material`, `_key_tonic_of_pitch_class_material`. A diatonic material (family "diatonic", seven steps) has the signature of its scale spelled up from the tonic letter, so D dorian has none and B-flat mixolydian has three flats; any other material uses the signature of the material it is a variant of (harmonic and melodic minor use the natural minor's). The sites that read a degree against the major scale with no key use `standard_tonality_natural_degree_semitone` and `_natural_degree_of_semitone`. The mode-based functions (`StandardTonalityMode`) stay until P58 B13 and call the material ones through the generated `knowledge_tonality_mode_steps` rows.

`common_practice_materials.h` holds `CommonPracticeMaterials`, the four materials of common practice (`omt.material.major`, `.natural-minor`, `.harmonic-minor`, `.melodic-minor`) resolved once from a snapshot with `common_practice_materials_resolve` (false, with a message naming the missing id, when one is not there). The chain resolves them once per run and sets them in the stage configs that read a scale: `CommonPracticeKeyRegionsConfig.materials`, `CommonPracticeWeightedConfig.materials`, `StandardTonalityChordInput.materials`, and the chain's own motive and melody inputs. A stage built without them (a test, the Composer) reads `common_practice_materials_builtin()`, the same four materials as static data; those sites go when P58 B13 gives the Composer the snapshot.

The tables written for major and minor keys stay as they are: `CommonPracticeKeyMode`, `HarmonyMode`, `StandardTonalityRomanKeyMode`, the triad and seventh quality rows, the applied-chord vocabularies, the Krumhansl-Kessler profiles and the key finder's 24-key candidate index. A key's material reaches them only through `common_practice_key_mode_of` (by the size of the material's third: major or minor, false for a material with no third), and `common_practice_material(materials, mode)` goes the other way for the two modes the tables know.
