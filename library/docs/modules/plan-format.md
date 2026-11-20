# Plan format

A piece plan is written as JSON text by `plan_to_json` and read from text by `plan_from_json`
(`<plan/plan_json.h>`): text in, text out, and the caller reads and writes the file. The format is the plan's own: it has a
name, a version and a JSON Schema ([`plan.v6.schema.json`](../../plan/schema/plan.v6.schema.json), JSON Schema 2020-12),
and it is written on library values: a key's tonic is a spelled note name, a chord's function is the library's function
name, a note may carry its spelling and always has an id. Writing always gives format version 6 (`URSATZ_PLAN_FORMAT_VERSION`).
Reading takes versions 6 to 1, every plan saved before, and maps them to materials, note ids and section descriptions ([Format versions](#format-versions)). Back to the [docs index](../README.md).

The output has a 2-space indent, the key order shown below and a final newline. The same plan always gives the same bytes.

The constraints and the motive relation steps are in [plan-constraints.md](../../../composer/docs/plan-constraints.md); the motive documents and their analysis are in [motive-format.md](../../../composer/docs/motive-format.md).

## Conventions

These are the library's.

- **Time** is an exact rational position in whole notes from the start of the piece:
  `{"num": 3, "den": 2}`, in lowest terms. `den` is at least 1.
- **Span** is `{"start": <time>, "end": <time>}`. In memory a span is a start and a length; `end`
  is start plus length, and a span must be longer than zero.
- **Lane span** is written as an entry of its lane's section: `span`, `label` and the `composer` object that holds the plan's data.
- **Tonic** is a note name without an octave as the library spells it (`pitch_spelling_name`): a letter and `bb`, `b`,
  nothing, `#` or `x` (`C`, `F#`, `Bb`, `Cx`). A key with a tonic outside these is not written.

## In memory

The file does not change with the types behind it. In memory a time is a library `Rational` (`<core/rational.h>`) in
whole notes, a span a `Rational` start and length, a cadence a `CadenceType` (the file names PAC, IAC, HC, DC and PC as
`pac`, `iac`, `hc`, `dc`, `pc`), a harmonic function a `HarmonicFunction` with a `has_function` flag (the file leaves
`function` out when it is false), a key an `SpelledKey` (the file writes `tonic`, spelled, and `material`, the id of a K1 material), and a note a MIDI
number with an optional spelling (the file writes `spelling` for a spelled note and nothing for an unspelled one), an id, and an optional source and operation (`PlanNote.id`, `.source`, `.operation`: see [Note ids](#note-ids)).

## Top level

| Key | Content |
| --- | --- |
| `format` | Always `"ursatz-plan"`. |
| `format_version` | Always `6`. |
| `next_note_id` | The id the next new note gets: an integer from 1, above every note id of the file. |
| `metadata` | The caller's stamp (`PlanProducer`): `producer`, `producer_version`, `producer_key` (the library chain's key, R-620), `composition_key` (the composing code's key, R-626) and `knowledge_hash`, each "" when unknown; `analyzer_version` for a plan that has one, the only member read. |
| `derived_from` | For a plan made from a reading: `{"reading": <name>, "realization": <16 hex digits>}` (R-635); kept. |
| `meter` | `null`, or `{"spans": [<span>, ...]}` for meter spans. |
| `regions` | Region lane spans. |
| `chords` | Harmony lane spans. |
| `form` | The node tree, or `null` for an empty plan. |
| `melody` | Melody lane spans. |
| `texture` | Bass and inner voice lane spans. |

Reading needs `format` and `format_version` (or `schema_version` for version 1) and `form`. A lane section that is absent counts as empty.

## Nodes

`form` is one node object; its `children` hold the rest of the tree, in order.

| Field | Type | Meaning |
| --- | --- | --- |
| `kind` | string | `form`, `section`, `phrase`, `subphrase`, `idea` or `motive`. |
| `label` | string or null | The node's name. |
| `span` | span | Where the node sits on the timeline. |
| `children` | array of nodes | In the order they were added. |
| `composer` | object | See below. |

A form is the root. A section goes under a form, a phrase under a section, a subphrase under a
phrase, an idea under a phrase or subphrase, and a motive under a subphrase or idea. A child's span
lies inside its parent's.

`composer` on a node:

| Field | Type | Meaning |
| --- | --- | --- |
| `id` | integer | The node's id. Ids run from 1 to the node count, with no gaps or repeats, and a parent's id is lower than its children's. A constraint refers to a motive by this id. |
| `state` | string | `given`, `constrained` or `realized`. |
| `constraints` | array | See Constraints. |
| `section` | object | On a section node only: the section's description ([Section descriptions](#section-descriptions)); `{}` when nothing is set. |

### Section descriptions

A section node holds one description of the section (R-607): the same object a form document and a piece request hold, with the members the plan use of the section description allows (`plan_section` in [`library/plan/schema/section.schema.json`](../../plan/schema/section.schema.json), which the plan schema references; [plan module](plan.md)). The members are written in table order, only when set: `id`, `letter`, `role`, `beginning` and `ending` (the theme pairing), `cadence` and `first_cadence`, `closed`, `keys`, `returns` (the name of the section it returns), `variation`, `setting`, `mode`, `layers`, `pedal`, `leads_in`, `connective` (and `connective_written`), `construction`, `figure`, `function` (with `independent` and `lyric`) and `free_phrase`. A game loop part is the `role` `intro`, `body`, `turnaround` or `ending` and the request's `id`; source material, the seam and a liquidation are the roles `material`, `seam` and `liquidation`; a held section has `leads_in`. In memory it is `SectionDescription` (`plan_node_section`, `plan_node_set_section`); a section node refuses a `cadence` constraint.

### Motive refs: the segment and the steps

A `motive_ref` constraint may carry two members that record how a developed idea was made (P34, ch. VIII), both optional: `"segment": "b"`, the label of the segment of the basic motif its motive node is made from (a continuation's fragments), and `"steps": 2`, how many liquidation steps by segments made it (a cadential idea). A plan without them reads as no segment and no steps, and writing it again leaves them out. Regenerating a section or locking a lane keeps them with the rest of the constraint. They name what happened; the motive operators do not read them. (The `steps` of an older file's `transform` is that transform's own.) See [plan-constraints.md](../../../composer/docs/plan-constraints.md#motive-relation-steps).

### Form record

The form node holds `"form"` in its `composer` object (R-608): `{"name", "version", "hash", "checks", "closing"}`, the form the plan was built from. `hash` is the FNV-1a 64 hash (16 hex digits) of the built-in document it was built from, empty for a form that is not built in; `checks` lists the checks the form runs (`sections`, `returns`, `keys`, `cadences`, `seam`); `closing` says it ends with a cadence. A plan composes with its recorded `checks` and checks its cadences from `closing` and each section's `closed` (`plan_form_record`); a plan with no record runs every check and has no cadences to check. The reader gives a file of a format before 5 the record and `closed` flags of the built-in form its form node is named for, once.

## Lane spans

Each lane span is an entry in the section for its lane:

| Lane | Section |
| --- | --- |
| `meter` | `meter.spans` |
| `key` (the region lane) | `regions` |
| `harmony` | `chords` |
| `melody` | `melody` |
| `bass`, `inner_voice` | `texture` |

`label` is for people: the meter (`4/4`), the region (`C major`), the Roman numeral, or the lane name.

`composer` on a lane span:

| Field | Type | Meaning |
| --- | --- | --- |
| `id` | integer | The span's id. Ids run from 1 to the span count across all sections, with no gaps or repeats, and are the order spans were added in. |
| `owner` | integer | The id of the node that owns the span. The span lies inside the node's span. |
| `lane` | string | `meter`, `key`, `harmony`, `bass`, `melody` or `inner_voice`. It must match the section. |
| `voice` | integer | 0, except for inner voices, where it numbers the voice. |
| `state` | string | `given`, `constrained` or `realized`. |
| `constraints` | array | See Constraints. |
| `section` | object | On a section node only: the section's description ([Section descriptions](#section-descriptions)); `{}` when nothing is set. |
| `payload` | object | By lane, below. |

Spans in the same lane and voice do not overlap; a span may start where another ends.

### Payloads

| Lane | Payload |
| --- | --- |
| `meter` | `beats_per_bar`, `beat_unit` (4 is a quarter), `bars_per_hyperbar` (0 for none). |
| `key` | `kind` (`"tonal"`, the only kind built: a region of another kind is refused with "region kind '<k>' is not built until P69"), `tonic` (spelled) and `material` (the id of a K1 material, `omt.material.major` or `omt.material.natural-minor`: the Composer writes keys in those two), or `relation` (a K6 id such as `omt.relation.dominant`) and `anchor` (`home` or `previous`), see [Key spans named by a relation](#key-spans-named-by-a-relation). A key may also carry `relation` and `anchor` beside its `tonic` and `material`: the span was reached by that relation, and the key is what it resolved to. |
| `harmony` | `roman_numeral` (1 to 15 characters), `function` (`tonic`, `predominant` or `dominant`; left out when the chord has none). |
| `bass`, `melody`, `inner_voice` | `notes`: a non-empty array of `{"midi_pitch": 0 to 127, "spelling": <tonic>, "offset": <time>, "duration": <time>, "id": <integer from 1>, "source": <integer from 1>, "operation": "copy" \| "motive" \| "return"}`; `spelling` is only there for a spelled note (read from MusicXML), `source` and `operation` only for a note made from another (see [Note ids](#note-ids)). `offset` is measured from the start of the span, and a note ends inside the span. An `inner_voice` span may have an empty array: the voice is silent over the span. |

### Note ids

A note's `id` is its identity (R-601): unique in the plan, from 1, never reused (`next_note_id` only rises, so the id of a removed note stays gone), and kept through every
save and edit. The plan gives an id when a note enters a lane span: a note with none gets the next id, a note that passes through a rewrite of its span keeps its id, and
a note whose id another live note holds (a note copied within the plan) gets a fresh id, with the old one as its `source` and `operation` `copy`. A note made from another
also names it where the Composer knows: `motive` for a melody note that carries a note of its motive (the note of the motive node's span, or of the idea before it that an
exact restatement repeats; a developed motive node's own notes have no link), and `return` for a note of a returned section (its source is the note it was copied from).
A theme given to a piece (`plan_give_section`) is made from nothing in the piece: its notes get fresh ids and no source. `source` is the id of a note that may no longer be in the
plan (the motive of a material section, a removed section). Reading refuses a repeated note id, an id below 1, a `next_note_id` that is not above every id or is missing, with the
path of the member (`PLAN_ERR_FORMAT`). The Composer's render keeps its own event ids (`composer-span-<s>-note-<i>`) until P62.

## State

Every node and lane span is in one state. New ones are `constrained`.

| State | Meaning |
| --- | --- |
| `given` | Fixed by the user. Never changed: a write, a state change or a constraint change is refused. |
| `constrained` | Not written yet, and has requirements. |
| `realized` | Written by the Composer. Can be regenerated. |

Node state and span state are independent: a Given node does not lock its children or its spans. `plan_lock`
(`ursatz lock`) sets both for a node, every node under it and the spans they own, so a lock reads in a file as
`given` on all of them; see [regenerate.md](../../../composer/docs/regenerate.md). A plan file numbers its nodes and lane spans from 1 without
gaps, so the ids of a plan written after `regenerate` can differ from the ids of the plan read.

## Reading

- Unknown fields are ignored, as the Analyzer's versioning rule says. Unknown values in a field
  the Composer owns (`kind`, `state`, `lane`, a constraint `kind` or its enums) are an error.
- A failure returns `PLAN_ERR_PARSE` for text that is not JSON, `PLAN_ERR_FORMAT` for JSON that is
  not a valid plan, or `PLAN_ERR_ALLOC`. The
  `PlanJsonError` gives a `path` such as `form.children[1].span.end.den`, a `message`, and the byte
  `offset` for a syntax error.
- A lane span without a `composer` object is an error.

## Key spans named by a relation

A key span is a key (`tonic` and `material`) or only the way to a key: `{"relation": "omt.relation.dominant", "anchor": "home"}` names the key
that relation reaches from the plan's home key (`home`, the first key span) or from the span before it (`previous`). The reader keeps such a
span unresolved; a plan that is composed from (`fill`, `lock`, `regenerate`, `variations`, `harmonize` and the piece entries read their plans with
`plan_from_json_resolved`) is resolved first by `plan_resolve_key_spans`, which asks the knowledge snapshot's relation (`region_resolve`) for the
key and fills the span's `tonic` and `material`, leaving `relation` and `anchor` beside them. A span that cannot be resolved (an unknown relation, a
relation with no key from that anchor, a material the Composer does not compose in) is refused with the relation's message, at
`regions[<n>].composer.payload.relation`, and nothing is composed. The piece planner writes `relation` and `anchor` `home` on a span whose key came from a
form region (`parallel`, `supertonic`, `mediant`, `subdominant`, `dominant`, `submediant`, `subtonic`), so a plan says why a span is in its key. The
section's `region` constraint is always a key: `tonic` and `material`.

## Format versions

`format_version` 6 is the format this document describes. Version 5 (`format` `"ursatz-composer-plan"`) wrote `keys`, the constraint kind `key`, lane span members `framework`, `note_ids`, `uncertainty`, `method` and `key_label` (ignored), and `composer_version` and `ursatz_commit` as metadata. Version 4 wrote a section's attributes as the constraint kinds `archetype`, `material`, `theme_pairing`, `return`, `layers`, `section_letter`, `piece_part`, `free_phrase`, `hold`, `seam`, `pedal`, `construction`, `connective`, `liquidation`, `figure` and `section_function` and `cadence` (two on a section whose first half has its own). A version 1 to 4 file reads each into the section's description (an `archetype` is the pairing's and is dropped, the second `cadence` is `first_cadence`, `piece_part` is the role and `id`, `hold` is `leads_in`, a `return` names its section by name) and writes version 5; version 5 refuses one of those kinds. Version 3 had no note ids and no `next_note_id`: a version 3 file reads with the notes' ids from 1 in file order (the
sections in the order of their lane span ids), no source and no operation, and `next_note_id` one above the last. Version 2 named a key's material by `mode` (`major` or `minor`) in a key span and in the
`key` constraint; the reader maps `major` to `omt.material.major` and `minor` to `omt.material.natural-minor`. `schema_version` 1 is the format the Composer wrote
before version 2, in the shape of an Ursatz-Analyzer analysis document; `plan_from_json` still reads it (a file with `format` and `format_version` is
version 2 to 6, a file with `schema_version` is version 1) and nothing writes it. A plan of any version reads to the same plan, writes as version 6
and renders the same MIDI. The schema's lane enum named the inner voice `inner-voice` while the writer wrote `inner_voice`; the schema now says what the writer writes. What version 1 had that version 6 does not:

| Version 1 | Read as |
| --- | --- |
| `schema_version` 1 | `format_version` 6 once written. |
| `metadata`: `source_file`, `analyzer_version`, `ursatz_commit` `"unknown"`, `note_count`, `ticks_per_quarter`, `key_found` | Ignored. `analyzer_version` was the Composer's own version, not an Analyzer's (B-A-10), so the plan has no Analyzer version after reading it. |
| `cadences`, `phrases`, `motives`, `rhythm`, `voice_leading`, `schemas`, always `[]` | Ignored; version 2 does not write them. |
| A key's `tonic_pitch_class` (0 to 11) and `mode` | The tonic the library spells for that pitch class and mode (`standard_tonality_key_tonic_of_pitch_class`: fewest accidentals in the signature, sharps on a tie): 3 minor is `D#`, 6 major is `F#`; the mode is read as in version 2. |
| A chord's `function` of `unknown` | No function (the member is left out in version 2). |
| A motive ref's `transform` and its members | The relation step ([Motive relation steps](../../../composer/docs/plan-constraints.md#motive-relation-steps)). |

Writing the plan again gives version 6: `plan_from_json` then `plan_to_json` turns any version 1 to 5 plan into its version 6
file, and the two render the same MIDI.

## Analyzer version

A plan's `metadata.analyzer_version` is kept through a read and a write: a plan saved with one comes back with it. A plan
the Composer builds has none. The Composer does not turn an analysis document into a plan: `fill` refuses one, and says it
is an analysis document.

## Example

A two-bar plan: a form in 4/4 and C major, one section, one chord. This is the exact output of
`plan_to_json`.

```json
{
  "format": "ursatz-plan",
  "format_version": 6,
  "next_note_id": 1,
  "metadata": {
    "producer": "ursatz-composer",
    "producer_version": "0.4.1",
    "producer_key": "528d5ccae6f72661",
    "composition_key": "0f1e2d3c4b5a6978",
    "knowledge_hash": "4e5764f2fc6e2592"
  },
  "meter": {
    "spans": [{
        "span": {
          "start": {
            "num": 0,
            "den": 1
          },
          "end": {
            "num": 2,
            "den": 1
          }
        },
        "label": "4/4",
        "composer": {
          "id": 1,
          "owner": 1,
          "lane": "meter",
          "voice": 0,
          "state": "given",
          "constraints": [],
          "payload": {
            "beats_per_bar": 4,
            "beat_unit": 4,
            "bars_per_hyperbar": 0
          }
        }
      }]
  },
  "regions": [{
      "span": {
        "start": {
          "num": 0,
          "den": 1
        },
        "end": {
          "num": 2,
          "den": 1
        }
      },
      "label": "C major",
      "composer": {
        "id": 2,
        "owner": 2,
        "lane": "key",
        "voice": 0,
        "state": "realized",
        "constraints": [],
        "payload": {
          "kind": "tonal",
          "tonic": "C",
          "material": "omt.material.major"
        }
      }
    }],
  "chords": [{
      "span": {
        "start": {
          "num": 0,
          "den": 1
        },
        "end": {
          "num": 1,
          "den": 1
        }
      },
      "label": "I",
      "composer": {
        "id": 3,
        "owner": 2,
        "lane": "harmony",
        "voice": 0,
        "state": "constrained",
        "constraints": [{
            "kind": "length_bars",
            "bars": 1
          }],
        "payload": {
          "roman_numeral": "I",
          "function": "tonic"
        }
      }
    }],
  "form": {
    "kind": "form",
    "label": "example",
    "span": {
      "start": {
        "num": 0,
        "den": 1
      },
      "end": {
        "num": 2,
        "den": 1
      }
    },
    "children": [{
        "kind": "section",
        "label": "A",
        "span": {
          "start": {
            "num": 0,
            "den": 1
          },
          "end": {
            "num": 2,
            "den": 1
          }
        },
        "children": [],
        "composer": {
          "id": 2,
          "state": "constrained",
          "constraints": [{
              "kind": "cadence",
              "type": "pac"
            }]
        }
      }],
    "composer": {
      "id": 1,
      "state": "given",
      "constraints": []
    }
  },
  "melody": [],
  "texture": []
}
```
