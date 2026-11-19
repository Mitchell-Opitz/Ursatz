# reading

[Docs index](../README.md): the Ursatz docs by topic.

Path `library/reading/`, target `ursatz_reading`, on `model`, `theory`, `realization`, `plan`, `analysis`, `chain` and cJSON (the layer table row `uses reading ...`). A reading (R-629, spec 4.7) is what one reader says about one realization, as claims over it. The reader is the Analyzer, the Composer (its own hearing) or a person. Sources include headers by their path from `library/` (`reading/<file>.h`); `reading.h`, `reading_build.h` and `reading_json.h` are staged (the Analyzer includes them).

## Provides

| Header | Provides |
|---|---|
| `reading.h` | `Reading`, `ReadingSource`, `ReadingTarget`, the eight claim types, `reading_create`, `reading_destroy`, the `reading_add_*` writers and the `reading_*_count` and `reading_*_at` readers |
| `reading_build.h` | `reading_build`: the reading of a chain run |
| `reading_json.h` | `reading_to_json`, `reading_from_json`, `ReadingJsonError` |

## The reading

A reading has a source (kind `analyzer`, `composer` or `person`; a name; the producer key of the program, the stages of the chain that ran and the knowledge hash, all empty or 0 for a person) and a target (the work, the movement and the digest of the realization, `realization_digest`). It holds claims of eight kinds: regions (a key: center, material, strength, label), chords (the chord reading of the sonority, the Roman numeral, the function), cadences (type, arrival, up to four alternative types), groups (a section, theme, phrase, subphrase or idea, with its parent and its form members), motive occurrences, meters, lines (the soprano and the bass) and voice faults. Every claim has an id unique in the reading, a span in whole notes, a confidence from 0 to 1 (absent when the reader gives none) and its evidence: the performer note ids of the realization it rests on (none for a person who names none). A claim made from a chain run keeps the index of the result element it came from (`detail`), so the analysis document can be printed from the reading (P62 B10); a reading read from a file has none.

A reading keeps a copy of every claim and its evidence; a pointer read back stays valid until `reading_destroy`. Materials and chord types are borrowed from the knowledge snapshot, which has to outlive the reading.

## The builder

`reading_build(chain, source, target, note_ids, note_count, &out)` makes the claims of the chain's collected result (`analysis_chain_result`) in the order of the result's arrays: regions from the key regions, chords from the entries that have a chord, cadences, groups (the sections, then the themes, then the phrases, subphrases and ideas under the group that holds them), motive occurrences, meters (one for each region of the score's meter map), the two outer lines and the voice faults. `note_ids[i]` is the performer note id of the chain's note index i (`imported_score_realization` gives them). A chain whose perception stages did not all run gives the meters and the voice faults only. Claim ids are `region-<n>`, `chord-<n>`, `cadence-<n>`, `group-<n>`, `motive-<n>`, `meter-<n>`, `line-<n>` and `fault-<n>`, n from 1.

## The codec

`reading_to_json` writes `{"format": "ursatz-reading", "format_version": 1, "source", "target", "regions", "chords", "cadences", "groups", "motive_occurrences", "meters", "lines", "voice_faults"}`, described by `schema/reading.v1.schema.json`; times are rationals `{"num", "den"}`. A reading whose digest is empty is refused (it names no realization version). `reading_from_json` refuses an unknown format or version, an unknown material, chord type or cadence type name, a group parent that is not an earlier group, and what `reading_add_*` refuses, at the path of the member. The detail is never written.

## Deferred

Use by the Composer's critic (P62 B12), saving it with the plan (P62 B13), the readings a reader keeps in a store (P63).
