# knowledge

[Docs index](../README.md): the Ursatz docs by topic.

Path `library/theory/knowledge/`. The knowledge snapshot: every entry of `knowledge/` as the build embedded it, plus a user's own files, read once into an immutable `KnowledgeSnapshot`. The entry format, the kinds and how to add an entry are in [knowledge/README.md](../../../knowledge/README.md).

## Provides

`KnowledgeSnapshot`, `KnowledgeEntry`, `Relation` (K6), `Instrument`, `EnsembleTemplate` and `EnsembleSlot` (K12), `VocabularyEntry` (K4), `FormalFunction` (K7), `ThemeType` (K8), `VoicingEntry` (K3, `voicing_entry.h`) and `RuleEntry` (K13). The loaders of the kinds that have a file of their own (`knowledge_vocabulary.c`, `knowledge_rule.c`) read into the snapshot's slot through `knowledge_internal.h`, which only this folder includes. `PitchMaterial` and `ChordType` are `model/pitch`.

## The instrument kind (K12, R-603)

`theory/knowledge/instrument.h` holds what a performer part needs from an instrument entry. The entry's `content` becomes an `Instrument`:

| Field | Meaning |
|---|---|
| `family` | `keyboard`, `strings`, `woodwinds`, `brass`, `percussion`, `voice` or `plucked` |
| `transposition` | sounding minus written, in semitones (-36 to 36) |
| `clefs`, `staff_count` | the default clef of staff 1, 2 and so on; the staff count is how many there are (1 to `INSTRUMENT_STAVES_MAX`) |
| `lowest`, `highest` | the sounding range, MIDI numbers 0 to 127; `instrument_in_range` tests a note against it |
| `has_comfortable_range`, `comfortable_lowest`, `comfortable_highest` | an optional range inside the full one |
| `general_midi_program` | 0 to 127, numbered from 0 |
| `sustain_pedal` | true when the entry lists the sustain pedal |

An ensemble entry becomes an `EnsembleTemplate`: slots in order, each with the id of an instrument entry, a part name unique in the template and a player count of at least 1.

```c
const Instrument *piano = knowledge_snapshot_instrument(snapshot, "omt.instrument.piano");
const EnsembleTemplate *solo = knowledge_snapshot_ensemble(snapshot, "omt.instrument.solo-piano");
```

Both lookups return NULL for an id that is absent or of another kind or subtype. Every field the loader reads is checked; a bad one refuses the build with `KNOWLEDGE_ERR_FIELD` and `<file>: <id>: content.<field>: <problem>`. An ensemble slot whose instrument is not an instrument entry of the snapshot (embedded or user) refuses it too. The first two entries are the piano and a solo-piano ensemble; other instruments and the General MIDI catalogue come with P74, keyboard hand span and crossing with P75.

## The modal fields, chord constructions and voicings (P69 B1)

A `PitchMaterial` carries `primary_degrees`, `avoided_degrees` (the numerals of `primary_chords` and `chords_to_avoid` read as degrees; the case and the `o` are checked against the stacked triad by `check-knowledge.py` and not kept), `characteristic_degrees` and `cadence` (an id, or NULL), each count 0 when the entry gives none. A numeral with an alteration (`bII`) or a degree above the step count refuses the build with `content.primary_chords` or `content.chords_to_avoid`. A cadence-kind `VocabularyCadence` gains `material` (linked once every entry is read), `approach_count`, `approach_degrees[3]` and `arrival_degree`: the dorian cadence is degree 4 then 1 of `omt.material.dorian`.

A `ChordType` gains `construction` (`ChordConstruction`, tertian when the entry gives none); `knowledge_snapshot_chord_type_count` and `_at` list the chord types in id order.

`voicing_entry.h` holds a `VoicingEntry`: `members` (a role and 0 to 35 semitones, rising from the root at 0) and `fits`, the chord types it sounds (`const ChordType *`, linked once every entry is read; a fit that is no chord type refuses the build with `content.fits`). Members that do not rise refuse it with `content.members`. `knowledge_snapshot_voicing`, `_voicing_count` and `_voicing_at` find them; no application reads them before P69 B9, so they have rows in `tools/ci/reach-allow.txt`.

## The vocabulary kind (K4, R-707)

`theory/knowledge/vocabulary_entry.h` holds a `VocabularyEntry`: the names a C enum stands for, as an entry a person can read and cite. The kind has five subtypes, and every entry has a `code`, the text the C table uses (`"perfect-authentic"`, `"escape-tone"`, `"melody and accompaniment"`, `"tonic"`):

| Subtype | Entries | Fields besides `code` |
|---|---|---|
| `cadence-kind` | the rows of `harmony_cadence_kinds.c` but `authentic-unspecified` (9) | `label`, `abbreviation`, `core`, `strength`, `conclusive` (the row's own) |
| `embellishment` | the ten `NonChordTone` names | `approach`, `leave`, `direction`, `metric` |
| `texture` | the six `StandardTexture` names | `category`, the code of a texture entry (`standard_texture_category`) |
| `progression-type` | prolongational, cadential, phrase model, sequential (4) | none; examples are chord lists for readers and the check |
| `function` | the three `HarmonicFunction` names | none |

```c
const VocabularyEntry *tone = knowledge_snapshot_vocabulary(snapshot, "omt.vocabulary.suspension");
```

The lookup returns NULL for an id that is absent or of another kind. An unknown subtype, a code that is not lowercase words, a missing or bad field of the subtype, or a texture whose category is no texture entry's code refuses the build with `KNOWLEDGE_ERR_FIELD` and `<file>: <id>: <field>: <problem>`. The enums stay the identity at run time: the test `vocabulary_entries` pins each C table to its entries both ways, so the two cannot drift. Readers of the entries in grow (the progression types through the formal functions) come with P67 B6.

## The formal function and theme type kinds (K7 and K8, R-708)

`theory/knowledge/form_entry.h` holds what a theme is made of. A `FormalFunction` (kind `function`, subtype `formal-function`) says what an idea, a subphrase or a phrase does in a theme and what its harmony and surface follow from that:

| Field | Meaning |
|---|---|
| `core`, `level` | initiating, medial, concluding and so on; `idea`, `subphrase` or `phrase` |
| `harmony` | one to four progression-type entries, in order of preference |
| `harmonic_rhythm`, `rhythmic_activity` | `any`, `faster` or `slower` against the idea before |
| `keeps_motive_harmony` | `motive_harmony` is `keep` (the harmony holds the motive's chords) or `free` |
| `cadence` | `none`, `weak`, `strong` or `any` (any when the entry says nothing) |
| `contrast_against`, `aspects` | for a contrasting idea: the function it contrasts against and, per dimension of R-709 (rhythm, contour, register, density), the least distance |

A `ThemeType` (kind `theme-type`) is a beginning and an ending (functions of subphrase or phrase level) and 2 to 6 ideas, each with its idea-level function, the slot its motive is drawn for, its half, its units (eighths of the theme) and its cadence (`none`, `first` on an antecedent's contrasting idea, `final` on the last idea). The entries are the nine pairings of the Composer's theme shape table: the sentence, the period and seven hybrids.

```c
const ThemeType *type = knowledge_snapshot_theme_type_of(snapshot, "presentation", "continuation"); /* the sentence */
const FormalFunction *fragment = type->ideas[2].function;
```

A harmony id that is no progression-type entry, a contrast target that is no formal function, a beginning or ending of idea level or an idea of any other level refuses the build with `KNOWLEDGE_ERR_FIELD` and `<file>: <id>: <field>: <problem>`. Grow does not read the entries until P67 B5 and B6; the test `test_theme_shape_entries` pins the Composer's table to them.

## The rule kind (K13, R-717)

`theory/knowledge/rule_entry.h` holds a `RuleEntry`: a judgment of a candidate, or a limit on it, as an entry a person can read, cite and change. The kind has three subtypes:

- `constraint`: a declared limit (R-539). It has a `stage` (motive, harmony, melody, texture or counterpoint), `waivable` and the `setting` that waives it. The repository holds the seven melody rules of Fux (`knowledge/rule/fux.json`, setting `melody.waive`).
- `evaluator`: a weighted judgment in her words (`knowledge/rule/michelle.json`, source `michelle`). It has a `default_weight` from 0 to 20 and up to four `parameters`, each a `Rational` (an integer, or a string `n/d` with a positive denominator).
- `observation`: a plain-word note on what was heard. It is never scored and never refuses.

Every rule has a `code`, a `scope` (note, line, line-pair, sonority, progression, phrase, section, movement, performer-part or set), a `message` and a `function`. The message is a template: a placeholder is `{name}` with a name from `bar`, `beat`, `first`, `last`, `count`, `value`, `limit`, `planned`, `heard`, `idea`, `against`, `aspect` and `amount`. The function is the name of the C function that checks the rule. It is data here and a table in the application that owns the function, and a test of that application pins the two together both ways (the Composer's `test_critic_rules`).

```c
const RuleEntry *rule = knowledge_snapshot_rule(snapshot, "michelle.rule.rhythmic-monotony");
/* rule->subtype == RULE_EVALUATOR, rule->default_weight == 1, parameter "run" == 3 */
```

The lookup returns NULL for an id that is absent or of another kind. An unknown subtype, a bad code, scope or function, a message longer than 200 characters or with a placeholder outside the list, a weight outside 0 to 20, a parameter that is neither an integer nor `n/d`, or a constraint without its stage, `waivable` or setting refuses the build with `KNOWLEDGE_ERR_FIELD` and `<file>: <id>: <field>: <problem>`. The test `rule_entries` pins the 19 entries and the refusals; `tools/ci/check-knowledge.py` repeats the code, placeholder and parameter rules over the files.
