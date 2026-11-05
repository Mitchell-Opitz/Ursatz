# Module status

The implementation status and doc of each kernel module. [Docs index](README.md)

| Module | Layer | Status | Doc |
|---|---|---|---|
| identity | L0 | Implemented | [identity](modules/identity.md) |
| core | L1 | Implemented | [core](modules/core.md) |
| time | L2 | Implemented | [time](modules/time.md) |
| pitch | L2 | Implemented (partial, see doc) | [pitch](modules/pitch.md) |
| events | L3 | Implemented | [events](modules/events.md) |
| containers | L4 | Implemented (partial, see doc) | [containers](modules/containers.md) |
| structure | L5 | Implemented (partial, see doc) | [structure](modules/structure.md) |
| context | L6 | Implemented | [context](modules/context.md) |
| semantics | L6 | Implemented | [semantics](modules/semantics.md) |
| relationship | L6 | Implemented | [relationship](modules/relationship.md) |
| interpretation | L6 | Implemented (partial, see doc) | [interpretation](modules/interpretation.md) |
| theory | L7 | Implemented (partial, see doc) | [theory](modules/theory.md) |
| rules | L7 | Implemented (partial, see doc) | [rules](modules/rules.md) |
| analysis | L8 | Implemented | [analysis](modules/analysis.md), [motive-relations](modules/motive-relations.md), [motive-variant-recognition](modules/motive-variant-recognition.md), [motive-tone-roles](modules/motive-tone-roles.md) |
| generation | L8 | Implemented (partial, see doc) | [generation](modules/generation.md) |
| io | L9 | Implemented (partial, see doc) | [io](modules/io.md) |
| export | L9 | Implemented (MusicXML from a MeterMap: parts, staves, key, tempo, dynamics; no MXL, repeats or articulations) | `src/export/musicxml_export.h` |
| persistence | L9 | Implemented (partial, see doc) | [persistence](modules/persistence.md) |
| corpus | L9 | Implemented | [corpus](modules/corpus.md) |
| corpus_stats | L9 | Implemented (partial, see doc) | [corpus_stats](modules/corpus_stats.md) |
| query | L9 | Implemented (partial, see doc) | [query](modules/query.md) |

"Partial" means some types from the module's Registry row are implemented and some are explicitly deferred; the module's own doc lists which.
