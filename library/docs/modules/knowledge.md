# knowledge

[Docs index](../README.md): the Ursatz docs by topic.

Path `library/theory/knowledge/`. The knowledge snapshot: every entry of `knowledge/` as the build embedded it, plus a user's own files, read once into an immutable `KnowledgeSnapshot`. The entry format, the kinds and how to add an entry are in [knowledge/README.md](../../../knowledge/README.md).

## Provides

`KnowledgeSnapshot`, `KnowledgeEntry`, `Relation` (K6), `Instrument`, `EnsembleTemplate` and `EnsembleSlot` (K12), `VocabularyEntry` (K4). The loaders of the kinds that have a file of their own (`knowledge_vocabulary.c`) read into the snapshot's slot through `knowledge_internal.h`, which only this folder includes. `PitchMaterial` and `ChordType` are `model/pitch`.

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
