# realization

[Docs index](../README.md): the Ursatz docs by topic.

Path `library/realization/`, target `ursatz_realization`, on `model` and `theory` (the layer table row `uses realization model theory`). Music as the realization holds it (target architecture 4.8 and 4.11, R-534, R-541, R-600): lines that belong to no instrument, performer parts that do, and the scoring that makes the second from the first. Sources include headers by their path from `library/` (`realization/<file>.h`). No header is staged yet: one is staged when a file outside the group first includes it.

## Provides

| Header | Provides |
|---|---|
| `realization.h` | `RealizationResult`, `realization_id_valid`, `REALIZATION_ID_MAX` |
| `derivation.h` | `NoteDerivation` (sources, operation, run) and its setters |
| `line.h` | `Line`, `LineNote`, `LineRole`, `LineRelation` |
| `performer_part.h` | `PerformerPart`, `PerformerNote`, `PedalMarking` |
| `scoring.h` | `ScoringAssignment`, `ScoringTransform`, `scoring_assignment_check`, `realization_score` |

## Ids

Every id is `^[A-Za-z][A-Za-z0-9_.-]{0,62}$`, so a note id can be written unchanged as a MusicXML `xml:id`. A function that gets a bad id returns `REALIZATION_ERR_BAD_ID`; one that would repeat an id in its owner returns `REALIZATION_ERR_DUPLICATE_ID`. Nothing changes when a function fails.

## Derivations

A `NoteDerivation` is a value with no heap: up to `NOTE_DERIVATION_SOURCES_MAX` source note ids, an operation name (lowercase words joined by hyphens) and a run text. A zeroed one means the note was made from nothing. `score` is the operation of a performer note made from a line note; `copy`, `motive` and `return` are the Composer's plan links.

## Lines

A line has an id, a role (`melody`, `counter-melody`, `harmony-line`, `bass`, `chords`, `pad`, `accompaniment`, `drums`, `pedal`, `subject`, `answer`, `countersubject`), an optional name, its notes in insertion order and relations to other lines. A note has an id unique in the line, an onset of at least 0, a duration above 0, a MIDI number and, when spelled, a letter and an accidental from -2 to 2. `line_add_note` copies what it is given.

## Performer parts

`performer_part_create` takes plain values (the id, the instrument entry's id, a name, the number among parts of that instrument, the players, the staff count and the General MIDI program), so the caller copies them from the K12 entry and the header needs no theory include. A note has a staff (1 to the staff count) and a voice (from 1), a derivation and three flags: `edited`, `detached` (a person changed or cut it, so scoring keeps it) and `review` (scoring found it differs from what its source now gives). Sustain pedal markings are kept in start order and may not overlap. A part may own a local meter map.

## Scoring

`realization_score(lines, line_count, assignments, assignment_count, parts, part_count, run)` makes the performer notes. All checks come first and nothing changes on a failure. Per part, notes that are edited, detached or have no source stay and every other note goes; then each assignment to the part, in order, gives one performer note per line note whose onset is in its span (the whole line when `whole`). The note's id is `<part id>-<line note id>` (`-2`, `-3` and on when the id is taken), its onset and duration are the line note's (a long note is not clipped at the span's end), its pitch moves by 12 times the octaves of the transform, and its derivation names the line note, `score` and the run. A kept note whose id is generated again is not duplicated; it gets `review` when its pitch, onset or duration differ. Notes end ordered by onset, ties by assignment order then line note order, kept notes after generated ones at an equal onset.

One line to several parts is doubling, several lines to one part is a reduction, spans are hand-offs, and `divisi` is notation only. An `INTERVAL` transform needs spelling in a key and is refused with `REALIZATION_ERR_NOT_BUILT`.

## Deferred

Movements and works (P59 B3), export (P59 B4), the realization codec (P62), other instruments (P74) and keyboard limits and pedal writing (P75).
