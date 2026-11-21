# Theory

The theory group (`library/theory/`, target `ursatz_theory`): the style model, the rule table,
the shared vocabulary enums and the rule libraries of the 11 Theory Frameworks. A framework is no longer a CMake target
or a folder of its own: its rules, vocabularies and predicates are an area here, its analysis adapter is an area of
[analysis/](../analysis/README.md), and its identity stays (`standard-harmony-v1` and the others are in Analyzer
documents). Code that needs the whole stack lives in the analysis chain (`chain/`, see `chain/README.md`).

## Areas and the framework each holds

| Area | Holds | Former framework |
|---|---|---|
| `style/` | Style models from corpus statistics, the labels and the profile counting (see Style below) | kernel `theory` |
| `rules/` | `RuleTable`: the names, the soft rules and the check of a rule group | kernel `rules` |
| `knowledge/` | The knowledge snapshot: the entries of `knowledge/` embedded in the build, and the pitch material read from them | none (new, R-536) |
| `vocabulary/` | Shared theory enums (`CadenceType`, `VoicePart`, ...) | kernel `semantics` |
| `tonality/` | Roman numeral grammar, keys, triad and seventh rules, chord and key vocabulary (the chord-quality intervals and the chord symbol suffixes have one owner each), `standard_tonality_roman_numeral_letters`, the interval size helpers and `standard_tonality_mask_step`, the common-practice materials, and regions named by relations and brightness (see Tonality below) | standard-tonality, common-practice-minimal (vocabulary) |
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

## Style

`theory/style/` holds everything about a style that does not read a document or a score (R-617). The Analyzer's job is to
turn a document into a piece; counting, comparing and the model file are here.

| File | Holds |
|---|---|
| `style_labels.h` | The labels a profile counts (numerals, progressions, intervals, durations, tempo and density bands, motive figures) and the category and dimension names. The Composer labels its own material with the same functions. |
| `style_piece.h` | `StylePiece`: the facts about one piece a profile counts (chords, cadences, harmonic rhythm, meter, tempo, densities, phrases, soprano notes, non-chord tones, motive families, melody ranges, theme and section labels, the home key), with the `style_piece_add_*` functions that fill it. Two sources fill it: the Analyzer's document reader (`style_piece_from_document`) and `chain_style_piece` (`chain/chain_style.h`). |
| `style_profile.h` | `StyleProfile` and `style_profile_add_piece`: the counts, sorted by category and label, with `style_profile_statistics`, the piece count and the training corpus label. |
| `style_compare.h` | `style_profile_compare`: the total variation distance per category as an exact `Rational`, the dimension and overall means as `double`. A distance that does not fit in a `Rational` is `STYLE_COMPARE_ERR_OVERFLOW`. |
| `style_model_file.h` | The style model file (`schema/style-model.v1.schema.json`): `style_model_file_write` (the producer's version is an argument), `style_model_file_read`, `style_model_file_read_without` and `style_model_statistics`. |
| `style_reference.h` | `StyleReference`: a model prepared once to measure any number of fragments (`style_reference_distance`). |
| `style_model.h`, `style_model_build.h`, `style_model_blend.h`, `style_features.h` | The `StyleModel`, its construction from statistics, blending and the statistics format. |

`chain_style_distance` (`chain/chain_style.h`) measures the piece a chain heard against a reference: the chain's result becomes a
piece, the piece a profile, the profile a distance. Documents, text exports and composed outputs do not depend on which source
filled the piece: `test_style_piece` checks that both give one set of statistics on the five reference pieces.

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
- **Framework names.** A framework is a name, not an object: its `<name>-v1` string is a constant in `theory/vocabulary/framework_names.h` (R-687). Rules are
  checked with the framework's own `*_rule_holds()` and check functions; a rule area says whether a rule is soft with a `*_rule_soft` query (R-685).
- **Hard vs. soft.** Rules phrased as "must" are hard. Rules phrased as "limit", "avoid" or
  "prefer" are soft.
- **Dependency ceiling.** The model group, `theory/rules/`, and any area listed in its row of
  `cmake/layer-table.txt` (`theory-<area>` rows, test `layer_table`); a new include edge updates its row in the same
  commit. Input from `analysis` is passed in by the caller as plain data, never included.
- **Test-only edges.** Area tests follow the same layer rules (the `tests-theory-<area>` rows of the layer table). Edges the tests have beyond the table are rows there with a reason: harmony tests include tonality (Roman numeral grammar cases), meter tests include the MIDI importer.
- **Vocabulary enums.** Names a framework needs (for example `CadenceType`, `VoicePart`) live
  as plain enums in `theory/vocabulary/` and carry no framework claim. What each name means is the
  framework's convention. See `docs/modules/semantics.md`.

## Using a kernel analysis owner

A framework may not include kernel L8 (`analysis/`), so when a job's owner lives there ([analytic-job-owners.md](../docs/analytic-job-owners.md)) the framework takes it as an injected callback and never re-derives it. The pattern is `StandardFormBoundaryDetector` in `standard-form/analysis/analysis_phrase.h`: the framework declares the function type, the caller (in practice the `chain/` target) passes the L8 owner behind it.

## Neutral analysis input

Framework adapters accept the neutral types in `analysis/notes/` (`AnalysisNote`, `AnalysisSegment`, `SpelledKey`, `AnalysisVoiceGrid`) through mapping functions that copy field for field with no new arithmetic, so a caller no longer writes the converter. Time is exact `Rational` everywhere in analysis; `double` is only for scores and display (Law 19). The mappings are in each framework's `analysis/` input header; `docs/modules/semantics.md` describes the types.

## Claims and rule tables

A framework builds its durable claims through the kernel helpers in `interpretation/claim_interpretation.h`. Each `analysis/*claim.c` keeps its public names and is a one-line call with the framework's id and short name: `claim_interpretation_create` for the Interpretation, `claim_interpretation_note_group` for the note shell, `claim_interpretation_relationship` for the Relationship. standard-harmony and standard-counterpoint use the one rule table and pair builder (`theory/rules/rule_table.h`, `rule_pair.h`), the pairs used only for the Preferences of their soft rules; scale-harmonization and galant-schemas set only the count and the hardness of it. A framework needs no claim code of its own.

## Rule table

A rule group declares its names, a `static const bool SOFT[N] = {[RULE] = true, ...}` for the rules that are soft (every other rule is hard) and `static const RuleTable TABLE = {.name = "...", .names = NAMES, .soft = SOFT, .count = N};` (`theory/rules/rule_table.h`). A static that is built lazily by a framework is a plain value computed from constants; there is no `Framework` object and no once primitive.

## Knowledge

`knowledge/` at the repository root holds music-theory knowledge as JSON entries. The build embeds every file in `ursatz_theory` (`cmake/embed_json.py` writes `knowledge_embedded.c` into the build folder; no binary reads `knowledge/` at run time). `knowledge_snapshot_build` parses the embedded files and any user files the caller gives it into an immutable `KnowledgeSnapshot` (entries in id order, each with a content hash, and a hash of the whole). Whoever calls the library builds the snapshot and passes it on; the library keeps no global copy. `model/pitch/pitch_material.h` (a model value type since P58) is the K1 entry read as steps above a center, with its key word, family, brightness rank, color notes and the material it varies or rotates. The snapshot also loads each K2 chord type (`model/pitch/chord_type.h`: the members above the root) and each K6 relation (`relation.h`: tonal, modal, neo-Riemannian and set relations by their fields; a serial entry by its common fields only), and finds a material, chord type or relation by id or by word (`knowledge_snapshot_material_named`, `_chord_type_of`, `_relation_named`). The snapshot reaches the library from the application that calls it: the Analyzer takes it in `AnalyzerOptions.knowledge` and the chain in `AnalysisChainConfig.knowledge` (a run without one is refused, `ANALYSIS_CHAIN_ERR_KNOWLEDGE`), and a snapshot must outlive every key made from it. `knowledge_snapshot.h` and `relation.h` are staged since P58 B10, because the applications include them (the Analyzer CLI and the GUI backend build one snapshot at start, the Composer CLI one per process, and the Composer takes the snapshot of its call context).

## Building and testing

The root build compiles `ursatz_theory` and adds `library/theory/tests/`, one folder per area, so root `ctest` and CI run
every area's tests.

## Public headers

`ursatz_theory` stages the headers something outside the group includes (`theory/harmony/harmony_chord.h`); the list
is in `library/theory/CMakeLists.txt` (`ursatz_public_headers`) and the layer table holds it
([docs/public-surface.md](../docs/public-surface.md)). Every header is included by its path from `library/`.

## Tonality: materials and the common-practice tables

A key in the library carries the pitch material it was made from (`KeyStructure.material`, `SpelledKey.material`, `MotiveSketch.material`, and the home key's material in the chain's `ChainRelationContext`). The tonal functions of `tonality/tonal_key.h` read materials: `standard_tonality_material_degree_semitone`, `_material_pitch_classes` (the material's mask on a center), `_material_degree_at_or_below_semitone`, `_material_degree_of_semitone` (the lowered readings are semitone 1 always, and 3, 8 and 10 where the material has a major third with its sixth and seventh at 9 and 11, the major scale's rule), and the signature functions `standard_tonality_key_fifths_of`, `_key_signature_of_material`, `_key_of_fifths_material`, `_key_tonic_of_pitch_class_material`. A diatonic material (family "diatonic", seven steps) has the signature of its scale spelled up from the tonic letter, so D dorian has none and B-flat mixolydian has three flats; any other material uses the signature of the material it is a variant of (harmonic and melodic minor use the natural minor's). The sites that read a degree against the major scale with no key use `standard_tonality_natural_degree_semitone` and `_natural_degree_of_semitone`. There is no mode type and no function that takes one: a key is a tonic and a material, and its quality (major or minor) is read from the material's third (`common_practice_key_mode_of`).

`common_practice_materials.h` holds `CommonPracticeMaterials`, the four materials of common practice (`omt.material.major`, `.natural-minor`, `.harmonic-minor`, `.melodic-minor`) resolved once from a snapshot with `common_practice_materials_resolve` (false, with a message naming the missing id, when one is not there). The chain resolves them once per run and sets them in the stage configs that read a scale: `CommonPracticeKeyRegionsConfig.materials`, `CommonPracticeWeightedConfig.materials`, `StandardTonalityChordInput.materials`, and the chain's own motive and melody inputs. A stage given none (a test that does not set them) reads no scale: `key_regions_estimate` refuses the config (`BAD_CONFIG`), the weighted ranking refuses applied chords, and a numeral reading finds no degree. The Composer resolves them from the snapshot of its call context, and no code holds a copy of them as static data.

The tables written for major and minor keys stay as they are: `CommonPracticeKeyMode`, `HarmonyMode`, `StandardTonalityRomanKeyMode`, the triad and seventh quality rows, the applied-chord vocabularies, the Krumhansl-Kessler profiles and the key finder's 24-key candidate index. A key's material reaches them only through `common_practice_key_mode_of` (by the size of the material's third: major or minor, false for a material with no third), and `common_practice_material(materials, mode)` goes the other way for the two modes the tables know.

## Tonality: regions, relations and brightness

A region (`model/pitch/region.h`) is where a stretch of music sits in pitch: a kind (tonal or modal, pitch field, serial, none), a center (0 to 11, or -1 for a pitch field), a pitch material and a strength (established, tonicized, implied, fragile, absent; OMT VII ch. 13). `region_tonal(center, material)` makes an established tonal or modal region and `region_equals` compares kind, center, strength and material. Serial regions hold nothing until serial material exists (P80).

A plan names a region by a relation to an anchor ("the dominant", "two steps brighter", "the relative minor"), and `region_resolve(snapshot, relation, anchor, &out, message, size)` (`tonality/region_relation.h`) resolves a K6 relation entry to a concrete region with an established strength. The anchor is a tonal or modal region with a material; which anchor a plan means, the home region or the previous one, is the caller's choice (`Relation.anchor_default` is the usual one). The rules are the ones `tools/ci/check-knowledge.py` checks every entry's examples against:

- A tonal relation by root puts the center `root_semitones` above the anchor's, by degree on that degree of the anchor's material. Its material is the anchor's (`same`), the parallel one (`parallel`: major gives natural minor, major's minor forms give major), an entry's, or `diatonic`: major or natural minor by the quality of the triad on that degree of the anchor's material, refused when the triad is diminished or augmented. That is the closely related keys of `harmony_related_key_of`: for the degrees the entries cover (2 to 7) the relation and the function agree in major and in minor (degree 1, the tonic, has no entry).
- A modal relation keeps the center (or moves it) and shifts the brightness (below).
- A neo-Riemannian relation applies its moves P, L and R in order to a major or natural minor triad region (P: same center, other mode; L: up a major third from major, down from minor; R: down a minor third from major, up from minor) and is refused on any other material.
- A set relation transposes by n (Tn) or inverts at n (In: every pitch class p becomes n - p, the center too, and the collection must match a material of the snapshot by its mask, else it is refused). No set relation entry exists yet (P80), so only tests build one.

`region_resolve_named` looks the relation up by id, name or alias first. A refusal names the relation and the anchor ("relation 'supertonic' from A minor: the triad on degree 2 is not major or minor, so it gives no key"); a refused call leaves the result untouched.

Brightness (`tonality/brightness.h`) orders the seven diatonic modes by how many degrees are raised (OMT I ch. 14 example 1): locrian 1, phrygian 2, aeolian (natural minor) 3, dorian 4, mixolydian 5, major 6, lydian 7. `brightness_rank_of` reads a rank and `brightness_shift(snapshot, material, steps, ...)` gives the material ranked `steps` away, refused past locrian or lydian. Harmonic and melodic minor are not diatonic and have no rank: asking is refused with "harmonic minor has no brightness: only the diatonic modes rank". Nothing outside the library calls these yet: the Composer's forms resolve through relations from P58 B13, and regions per section in requests come with P60.
