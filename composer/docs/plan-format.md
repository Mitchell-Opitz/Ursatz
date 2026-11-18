# Plan format

A piece plan is written as JSON text by `plan_to_json` and read from text by `plan_from_json`
(`<ursatz_composer/plan_json.h>`): text in, text out, and the caller reads and writes the file. The format is the Composer's own: it has a
name, a version and a JSON Schema ([`schema/plan.v3.schema.json`](../schema/plan.v3.schema.json), JSON Schema 2020-12),
and it is written on library values: a key's tonic is a spelled note name, a chord's function is the library's function
name, a note may carry its spelling. Writing always gives format version 3 (`URSATZ_COMPOSER_PLAN_FORMAT_VERSION`).
Reading takes versions 2 and 1, the formats of every plan saved before it, and maps them to materials ([Format versions](#format-versions)). Back to the [docs index](README.md).

The output is pretty-printed with a 2-space indent, in the key order shown below, and ends with a
newline. The same plan always gives the same bytes.

The constraints and the motive relation steps are in [plan-constraints.md](plan-constraints.md); the motive documents and their analysis are in [motive-format.md](motive-format.md).

## Conventions

These are the library's.

- **Time** is an exact rational position in whole notes from the start of the piece:
  `{"num": 3, "den": 2}`, in lowest terms. `den` is at least 1.
- **Span** is `{"start": <time>, "end": <time>}`. In memory a span is a start and a length; `end`
  is start plus length, and a span must be longer than zero.
- **Claim** is a lane span written as an entry of its lane's section: `framework`, `span`, `note_ids`, `label`,
  `uncertainty`, and the `composer` object that holds the plan's own data.
- **Tonic** is a note name without an octave as the library spells it (`pitch_spelling_name`): a letter and `bb`, `b`,
  nothing, `#` or `x` (`C`, `F#`, `Bb`, `Cx`). A key with a tonic outside these is not written.

## In memory

The file does not change with the types behind it. In memory a time is a library `Rational` (`<core/rational.h>`) in
whole notes, a span a `Rational` start and length, a cadence a `CadenceType` (the file names PAC, IAC, HC, DC and PC as
`pac`, `iac`, `hc`, `dc`, `pc`), a harmonic function a `HarmonicFunction` with a `has_function` flag (the file leaves
`function` out when it is false), a key an `AnalysisKey` (the file writes `tonic`, spelled, and `material`, the id of a K1 material), and a note a MIDI
number with an optional spelling (the file writes `spelling` for a spelled note and nothing for an unspelled one).

## Top level

| Key | Content |
| --- | --- |
| `format` | Always `"ursatz-composer-plan"`. |
| `format_version` | Always `2`. |
| `metadata` | `composer_version` (the Composer that wrote the file), `ursatz_commit` (the Analyzer's producer key, `ursatz_analyzer_producer_key()`; the member keeps its name until plan format 3) and, only for a plan loaded from an Analyzer result, `analyzer_version` (`ursatz_analyzer_version()`). |
| `meter` | `null`, or `{"spans": [<claim>, ...]}` for meter spans. |
| `keys` | Key lane claims. |
| `chords` | Harmony lane claims. |
| `form` | The node tree, or `null` for an empty plan. |
| `melody` | Melody lane claims. |
| `texture` | Bass and inner voice lane claims. |

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

### Motive refs: the segment and the steps

A `motive_ref` constraint may carry two members that record how a developed idea was made (P34, ch. VIII), both optional: `"segment": "b"`, the label of the segment of the basic motif its motive node is made from (a continuation's fragments), and `"steps": 2`, how many liquidation steps by segments made it (a cadential idea). A plan without them reads as no segment and no steps, and writing it again leaves them out. Regenerating a section or locking a lane keeps them with the rest of the constraint. They name what happened; the motive operators do not read them. (The `steps` of an older file's `transform` is that transform's own.) See [plan-constraints.md](plan-constraints.md#motive-relation-steps).

### Pedal

A `construction` constraint (on a section node, R-468) gives the bars of each idea of a theme: `{"kind": "construction", "ideas": {"basic idea": 3, "contrasting idea": 2, ...}, "extensions": [], "reason": "given"}`. `ideas` names the ideas the theme's pairing has, in `GrowIdea` order, each with 1 to 16 bars and only those with bars (the names are those of the plan's idea nodes). `extensions` holds up to 3 `{"kind": "repeat"|"stretch"|"addition"|"evaded", "idea": "<idea name>", "bars": 0 to 4, "varied": bool}`: `repeat` a new idea after `idea` (internal repetition), `stretch` that idea longer, `addition` a tonic phrase after the cadence and `evaded` a deceptive cadence before the last idea is restated (the last two ignore `idea`); `bars` 0 is the idea's own bars (1 for a stretch and an addition), and the theme's `bars` count them. `reason` is `given`, `motive` or `length`. A plan without it reads as before and writing it again gives the same bytes; the schema version is the same. See [grow-construction.md](grow-construction.md).

A `pedal` constraint (`{"kind": "pedal", "degree": 5}`, on a section node) and the `pedal` member of a `harmony_shape` (written `"pedal": true` after `split_at_bars`, only when set) ask for a dominant pedal under a presentation (R-446). The degree is required and only 5 is valid. A plan without either reads as before and writing it again gives the same bytes; the schema version is the same.

## Lane claims

Each lane span is a claim in the section for its lane:

| Lane | Section |
| --- | --- |
| `meter` | `meter.spans` |
| `key` | `keys` |
| `harmony` | `chords` |
| `melody` | `melody` |
| `bass`, `inner_voice` | `texture` |

Claim fields the Composer has no data for are fixed: `framework` is `"ursatz-composer"`,
`note_ids` is `[]` and `uncertainty` is `null`. `label` is for people: the meter (`4/4`), the key
(`C major`), the Roman numeral, or the lane name. A key claim also has `method`, which is
`"composer_plan"`, and a chord claim has `key_label`, the label of the key span at its start or
`"no match"`.

`composer` on a claim:

| Field | Type | Meaning |
| --- | --- | --- |
| `id` | integer | The span's id. Ids run from 1 to the span count across all sections, with no gaps or repeats, and are the order spans were added in. |
| `owner` | integer | The id of the node that owns the span. The span lies inside the node's span. |
| `lane` | string | `meter`, `key`, `harmony`, `bass`, `melody` or `inner_voice`. It must match the section. |
| `voice` | integer | 0, except for inner voices, where it numbers the voice. |
| `state` | string | `given`, `constrained` or `realized`. |
| `constraints` | array | See Constraints. |
| `payload` | object | By lane, below. |

Spans in the same lane and voice do not overlap; a span may start where another ends.

### Payloads

| Lane | Payload |
| --- | --- |
| `meter` | `beats_per_bar`, `beat_unit` (4 is a quarter), `bars_per_hyperbar` (0 for none). |
| `key` | `tonic` (spelled) and `material` (the id of a K1 material, `omt.material.major` or `omt.material.natural-minor`: the Composer writes keys in those two), or `relation` (a K6 id such as `omt.relation.dominant`) and `anchor` (`home` or `previous`), see [Key spans named by a relation](#key-spans-named-by-a-relation). A key may also carry `relation` and `anchor` beside its `tonic` and `material`: the span was reached by that relation, and the key is what it resolved to. |
| `harmony` | `roman_numeral` (1 to 15 characters), `function` (`tonic`, `predominant` or `dominant`; left out when the chord has none). |
| `bass`, `melody`, `inner_voice` | `notes`: a non-empty array of `{"midi_pitch": 0 to 127, "spelling": <tonic>, "offset": <time>, "duration": <time>}`; `spelling` is only there for a spelled note (read from MusicXML). `offset` is measured from the start of the span, and a note ends inside the span. An `inner_voice` span may have an empty array: the voice is silent over the span. |

## State

Every node and lane span is in one state. New ones are `constrained`.

| State | Meaning |
| --- | --- |
| `given` | Fixed by the user. Never changed: a write, a state change or a constraint change is refused. |
| `constrained` | Not written yet, and has requirements. |
| `realized` | Written by the Composer. Can be regenerated. |

Node state and span state are independent: a Given node does not lock its children or its spans. `plan_lock`
(`ursatz-composer lock`) sets both for a node, every node under it and the spans they own, so a lock reads in a file as
`given` on all of them; see [regenerate.md](regenerate.md). A plan file numbers its nodes and lane spans from 1 without
gaps, so the ids of a plan written after `regenerate` can differ from the ids of the plan read.

## Reading

- Unknown fields are ignored, as the Analyzer's versioning rule says. Unknown values in a field
  the Composer owns (`kind`, `state`, `lane`, a constraint `kind` or its enums) are an error.
- A failure returns `PLAN_ERR_PARSE` for text that is not JSON, `PLAN_ERR_FORMAT` for JSON that is
  not a valid plan, or `PLAN_ERR_ALLOC`. The
  `PlanJsonError` gives a `path` such as `form.children[1].span.end.den`, a `message`, and the byte
  `offset` for a syntax error.
- A lane claim without a `composer` object is an error.

## Key spans named by a relation

A key span is a key (`tonic` and `material`) or only the way to a key: `{"relation": "omt.relation.dominant", "anchor": "home"}` names the key
that relation reaches from the plan's home key (`home`, the first key span) or from the span before it (`previous`). The reader keeps such a
span unresolved; a plan that is composed from (`fill`, `lock`, `regenerate`, `variations`, `harmonize` and the piece entries read their plans with
`plan_from_json_resolved`) is resolved first by `plan_resolve_key_spans`, which asks the knowledge snapshot's relation (`region_resolve`) for the
key and fills the span's `tonic` and `material`, leaving `relation` and `anchor` beside them. A span that cannot be resolved (an unknown relation, a
relation with no key from that anchor, a material the Composer does not compose in) is refused with the relation's message, at
`keys[<n>].composer.payload.relation`, and nothing is composed. The piece planner writes `relation` and `anchor` `home` on a span whose key came from a
form region (`parallel`, `supertonic`, `mediant`, `subdominant`, `dominant`, `submediant`, `subtonic`), so a plan says why a span is in its key. The
section's `key` constraint is always a key: `tonic` and `material`.

## Format versions

`format_version` 3 is the format this document describes. Version 2 named a key's material by `mode` (`major` or `minor`) in a key span and in the
`key` constraint; the reader maps `major` to `omt.material.major` and `minor` to `omt.material.natural-minor`. `schema_version` 1 is the format the Composer wrote
before version 2, in the shape of an Ursatz-Analyzer analysis document; `plan_from_json` still reads it (a file with `format` and `format_version` is
version 2 or 3, a file with `schema_version` is version 1) and nothing writes it. A plan of any version reads to the same plan, writes as version 3
and renders the same MIDI. What version 1 had that version 3 does not:

| Version 1 | Read as |
| --- | --- |
| `schema_version` 1 | `format_version` 3 once written. |
| `metadata`: `source_file`, `analyzer_version`, `ursatz_commit` `"unknown"`, `note_count`, `ticks_per_quarter`, `key_found` | Ignored. `analyzer_version` was the Composer's own version, not an Analyzer's (B-A-10), so the plan has no Analyzer version after reading it. |
| `cadences`, `phrases`, `motives`, `rhythm`, `voice_leading`, `schemas`, always `[]` | Ignored; version 2 does not write them. |
| A key's `tonic_pitch_class` (0 to 11) and `mode` | The tonic the library spells for that pitch class and mode (`standard_tonality_key_tonic_of_pitch_class`: fewest accidentals in the signature, sharps on a tie): 3 minor is `D#`, 6 major is `F#`; the mode is read as in version 2. |
| A chord's `function` of `unknown` | No function (the member is left out in version 2). |
| A motive ref's `transform` and its members | The relation step ([Motive relation steps](plan-constraints.md#motive-relation-steps)). |

Writing the plan again gives version 3: `plan_from_json` then `plan_to_json` turns any version 1 or 2 plan into its version 3
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
  "format": "ursatz-composer-plan",
  "format_version": 2,
  "metadata": {
    "composer_version": "0.1.0",
    "ursatz_commit": "528d5ccae6f72661"
  },
  "meter": {
    "spans": [{
        "framework": "ursatz-composer",
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
        "note_ids": [],
        "label": "4/4",
        "uncertainty": null,
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
  "keys": [{
      "framework": "ursatz-composer",
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
      "note_ids": [],
      "label": "C major",
      "uncertainty": null,
      "method": "composer_plan",
      "composer": {
        "id": 2,
        "owner": 2,
        "lane": "key",
        "voice": 0,
        "state": "realized",
        "constraints": [],
        "payload": {
          "tonic": "C",
          "material": "omt.material.major"
        }
      }
    }],
  "chords": [{
      "framework": "ursatz-composer",
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
      "note_ids": [],
      "label": "I",
      "uncertainty": null,
      "key_label": "C major",
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
