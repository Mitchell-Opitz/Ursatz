# knowledge

[Docs index](../README.md): the Ursatz docs by topic.

Path `library/theory/knowledge/`. The knowledge snapshot: every entry of `knowledge/` as the build embedded it, plus a user's own files, read once into an immutable `KnowledgeSnapshot`. The entry format, the kinds and how to add an entry are in [knowledge/README.md](../../../knowledge/README.md).

## Provides

`KnowledgeSnapshot`, `KnowledgeEntry`, `Relation` (K6), `Instrument`, `EnsembleTemplate` and `EnsembleSlot` (K12). `PitchMaterial` and `ChordType` are `model/pitch`.

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
