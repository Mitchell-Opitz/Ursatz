# realization

[Docs index](../README.md): the Ursatz docs by topic.

Path `library/realization/`, target `ursatz_realization`, on `model` and `theory` (the layer table row `uses realization model theory`). Music as the realization holds it (target architecture 4.8 and 4.11, R-534, R-541, R-600): lines that belong to no instrument, performer parts that do, and the scoring that makes the second from the first. Sources include headers by their path from `library/` (`realization/<file>.h`). A header is staged when a file outside the group includes it.

## Provides

| Header | Provides |
|---|---|
| `realization.h` | `RealizationResult`, `realization_id_valid`, `REALIZATION_ID_MAX` |
| `derivation.h` | `NoteDerivation` (sources, operation, run) and its setters |
| `line.h` | `Line`, `LineNote`, `LineRole`, `LineRelation` |
| `performer_part.h` | `PerformerPart`, `PerformerNote`, `PedalMarking` |
| `scoring.h` | `ScoringAssignment`, `ScoringTransform`, `scoring_assignment_check`, `realization_score` |
| `movement.h` | `Movement`, `MovementKeySignature`, `MovementMarker`, `MovementPart` |
| `work.h` | `Work`, `work_movement_ensemble` |
| `piano_reduction.h` | `movement_reduce_to_piano` |
| `realization_json.h` | The codec: `realization_work_to_json`, `realization_work_to_compact_json`, `realization_work_from_json`, `realization_digest` |

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

## Movements and works

A `Movement` owns its `MeterMap` and `TempoMap` (a setter takes ownership and frees the one held before; NULL clears), so a movement in 3/4 and one in 4/4 sit in one work without sharing a map. It holds its lines, scoring assignments and performer parts (ids unique in each; `movement_add_line` and `movement_add_part` take ownership on success only), key signatures and markers in time order, played spans, compound parts (a minuet and its trio), an optional home `Region` and an optional ensemble template id. `movement_score` is `realization_score` over its own lines, assignments and parts. A played span means a repeat or a path through the written music; none means the written order once.

A `Work` holds movements in order and the defaults they inherit: a title, a composer, an ensemble template id and material references (`<kind>:<name>`, as spec 4.13 writes them). `work_movement_ensemble(work, movement)` is the movement's own ensemble, else the work's, else NULL. A project stays the store's: the library has no type for it (R-600).

## The piano reduction

`movement_reduce_to_piano(movement, piano, run)` (the only realization header that includes the K12 `Instrument`) makes the part `piano` and one whole-line assignment `a-<line id>` per line, in line order, then scores the movement. A melody line is on staff 1 and a bass line on staff 2; any other line is on staff 1 when the mean MIDI number of its notes is 60 or more (a line with no notes: staff 1) and on staff 2 below that. The voice is the line's index plus 1. The part has two staves when any line is on staff 2, else one (a motif stays on one staff). It is refused with `REALIZATION_ERR_BAD_VALUE` and nothing changed when the movement already holds parts or assignments or the instrument has fewer than two staves. This is R-599's rule: the Composer exports every plan as one movement whose lines are all assigned to one piano part.

## The codec

`realization_work_to_json` writes a work as `{"format": "ursatz-realization", "format_version": 1, "work": {...}}` (2-space indent, a final newline); `realization_work_from_json` reads it with the group's own checks at the path of the member that fails (`work.movements[0].lines[1].notes[2].id`). `realization_digest` is the FNV-1a 64 of the compact text, 16 lowercase hexadecimal digits: the name of one version of a realization, which a reading names as its target. A work written and read back is equal, and writes the same text.

## The realization of an import

`imported_score_realization` (`io/imported_realization.h`, R-628) makes the work an import holds: work `w1`, movement `m1` with the file's meter and tempo maps and key signatures, one performer part for each part of the import and one performer note for each event, and no lines. It also gives the id of the note made from each import index, which the chain's results use (`note_ids`), so a reading can name the notes. The Analyzer builds it after its chain run (`analyzer_result_realization`).

## Deferred

Export of a movement (P59 B4; a played order is refused until a phase writes repeats as signs), the Composer's use (P59 B7), other instruments (P74), keyboard limits and pedal writing (P75).
