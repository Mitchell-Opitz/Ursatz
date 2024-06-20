# Plan format

A piece plan is written as JSON by `plan_to_json` / `plan_write_json_file` and read by
`plan_from_json` / `plan_read_json_file` (`<ursatz_composer/plan_json.h>`). The document has the
shape of an [Ursatz-Analyzer analysis document](https://github.com/Mitchell-Opitz/Ursatz-Analyzer/blob/main/docs/analysis-document.md)
(`schema_version` 1), so the Analyzer's schema accepts it and the Composer and the Analyzer share
one vocabulary. Anything the Analyzer has no field for sits in a `composer` object on the node or
claim it belongs to.

The output is pretty-printed with a 2-space indent, in the key order shown below, and ends with a
newline. The same plan always gives the same bytes.

## Conventions

These are the Analyzer's.

- **Time** is an exact rational position in whole notes from the start of the piece:
  `{"num": 3, "den": 2}`, in lowest terms. `den` is at least 1.
- **Span** is `{"start": <time>, "end": <time>}`. In memory a span is a start and a length; `end`
  is start plus length, and a span must be longer than zero.
- **Claim** is a lane span written the way the Analyzer writes a claim: `framework`, `span`,
  `note_ids`, `label`, `uncertainty`.

## Top level

| Key | Content |
| --- | --- |
| `schema_version` | Always `1`. |
| `metadata` | `source_file` `""`, `analyzer_version` (the Composer version), `ursatz_commit` `"unknown"`, `note_count` 0, `ticks_per_quarter` `null`, `key_found` (true when the plan has a key span). |
| `meter` | `null`, or `{"spans": [<claim>, ...]}` for meter spans. |
| `keys` | Key lane claims. |
| `chords` | Harmony lane claims. |
| `form` | The node tree, or `null` for an empty plan. |
| `melody` | Melody lane claims. |
| `texture` | Bass and inner voice lane claims. |
| `cadences`, `phrases`, `motives`, `rhythm`, `voice_leading`, `schemas` | Always `[]`. The plan keeps its own versions of these in the tree and the constraints. |

Reading needs `schema_version` and `form`. A lane section that is absent counts as empty.

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
| `key` | `tonic_pitch_class` (0 to 11, C is 0), `mode` (`major` or `minor`). |
| `harmony` | `roman_numeral` (1 to 15 characters), `function` (`tonic`, `predominant`, `dominant` or `unknown`). |
| `bass`, `melody`, `inner_voice` | `notes`: a non-empty array of `{"midi_pitch": 0 to 127, "offset": <time>, "duration": <time>}`. `offset` is measured from the start of the span, and a note ends inside the span. An `inner_voice` span may have an empty array: the voice is silent over the span. |

## State

Every node and lane span is in one state. New ones are `constrained`.

| State | Meaning |
| --- | --- |
| `given` | Fixed by the user. Never changed: a write, a state change or a constraint change is refused. |
| `constrained` | Not written yet, and has requirements. |
| `realized` | Written by the Composer. Can be regenerated. |

Node state and span state are independent: a Given node does not lock its children or its spans.

## Constraints

A constraint is an object with a `kind` and the fields for that kind. Constraints are data only;
nothing checks them.

| `kind` | Fields |
| --- | --- |
| `length_bars` | `bars`: integer, 1 or more. |
| `cadence` | `type`: `pac`, `iac`, `hc`, `dc` or `pc`. |
| `motive_ref` | `motive`: the id of a motive node; `transform`: `none`, `transposed`, `inverted`, `retrograde`, `augmented`, `diminished`, `fragmented`, `restated`, `interval_changed` or `liquidated`; `semitones`: used by `transposed`. Optional development parameters follow. |
| `key` | `tonic_pitch_class` (0 to 11), `mode` (`major` or `minor`). |
| `archetype` | `type`: `sentence`, `period` or `hybrid`. |
| `embellish` | No fields. Marks a harmony lane span reopened for the embellishment pass. |
| `schema` | `schema`: the name of a galant schema as Ursatz's galant-schemas framework names it (`romanesca`, `do-re-mi`, `fonte` and so on); `stage`: the index of the schema's stage the span sounds, from 0. The span's other voices are read from the framework's prototype. |
| `material` | No fields. On a section node: the section and everything under it is source material, not part of the piece. Rendering skips its lane spans (`RENDER_SKIP_MATERIAL`) and the piece ends where the last section that is not material ends. |
| `harmony_shape` | The shape a harmony span is to take. `shape`: `prolongation`, `phrase_model` or `cadential`; `starts_on` and `ends_on`: `any`, `tonic` or `dominant` (optional, `any`); `max_cells_per_chord`: whole number, 0 for no limit (optional, 0); `split_at_bars`: true when no chord may cross a barline (optional, false). See [grow.md](grow.md). |
| `theme_pairing` | How a theme begins and ends. `beginning`: `presentation`, `antecedent` or `compound_basic_idea`; `ending`: `continuation`, `consequent` or `cadential`. The nine pairings make the sentence, the period and the seven hybrids of standard-form. |
| `accompaniment` | The accompaniment an inner voice span (voice 1) is to be written as. `pattern`: `block`, `arpeggiated`, `alberti`, `oom-pah` or `any`; `texture`: a standard-texture name (`homophony`, `homorhythm`, `melody and accompaniment` and the others the library names), absent for none; `low` and `high`: MIDI pitches, absent for 48 and 67. `high` must be above `low`. See [accompaniment.md](accompaniment.md). |
| `voicing` | The four-part voicing an inner voice span (voice 1, the alto) is to be written as. `write_soprano`: true when the voicing writes the melody span over the same time too (optional, false). See [voicing.md](voicing.md). |

### Motive development parameters

A `motive_ref` on a motive node says how the node's motive is made from another motive node: the
motivic development operators (`<ursatz_composer/motive_develop.h>`) read it and write the node's
melody. Every member below is optional and is written only when set; a constraint without them leaves
the choice to the operator, which draws it from the run's seed.

| Member | Used by | Meaning |
| --- | --- | --- |
| `tonal` | `transposed`, `inverted` | `true`: move in steps of the key's scale instead of semitones. Needs a key. |
| `steps` | `transposed`, tonal | Diatonic steps up; negative is down. |
| `axis` | `inverted` | The MIDI pitch mirrored about; the source's first note when absent. |
| `interval`, `amount`, `keep_contour` | `interval_changed` | The 1-based interval to widen by `amount` semitones (negative narrows); every interval when `interval` is absent. `keep_contour` refuses a result that changes a direction. |
| `position`, `length` | `fragmented` | `leading`, `trailing` or `middle`; and how many notes. |
| `drop`, `keep_rhythm` | `liquidated` | How many notes to drop, the weakest first. With `keep_rhythm` the notes left keep their onsets. |
| `ratio_num`, `ratio_den` | `augmented`, `diminished` | The factor durations and onsets are multiplied by; both or neither. 2:1 and 1:2 when absent. |

`restated` asks for the exact repetition or a transposition of it. The operators write only the
Realized melody lane span of the node, and only when the result fits the node's span.

## Reading

- Unknown fields are ignored, as the Analyzer's versioning rule says. Unknown values in a field
  the Composer owns (`kind`, `state`, `lane`, a constraint `kind` or its enums) are an error.
- A failure returns `PLAN_ERR_PARSE` for text that is not JSON, `PLAN_ERR_FORMAT` for JSON that is
  not a valid plan, `PLAN_ERR_IO` for a file that cannot be read, or `PLAN_ERR_ALLOC`. The
  `PlanJsonError` gives a `path` such as `form.children[1].span.end.den`, a `message`, and the byte
  `offset` for a syntax error.
- A lane claim without a `composer` object is an error. An analysis document is loaded with
  `plan_from_analysis_document`, below.

## Loading an analysis document

`plan_from_analysis_document` (`<ursatz_composer/analysis_load.h>`) reads an Analyzer analysis
document and builds a plan in which every node and lane span is `realized`.

| Analysis document | Plan |
| --- | --- |
| The piece | A form root over the extent of the key and chord claims, named for `metadata.source_file`. The document has no form tree yet. |
| `keys` claim | A key span: `C major` or `Eb minor` gives the tonic and mode. |
| `chords` claim | A harmony span: the label is the Roman numeral and the function is `unknown`. |

Nothing is dropped silently. The report lists:

- a key or chord label that cannot be read, including `"no match"` (skipped)
- a claim that runs past the start of the next in the same section (cut off at that start, or
  skipped when both start together)
- a section with content the plan has no place for yet: `meter`, `cadences`, `phrases`, `form`,
  `motives`, `melody`, `rhythm`, `texture`, `voice_leading`, `schemas`

A document with no key or chord claim that can be read is an error, since there is no timeline to
build on.

## Motive documents

A motive is a short cell of notes the user supplies (`<ursatz_composer/motive.h>`). It is read from MIDI,
MusicXML, a note list or JSON, and attaches to a plan as a Given motive node with a Given melody lane span
(`motive_attach`). The JSON below is the motive on its own and the form `motive_from_json` reads and
`motive_to_json` writes. It uses the plan's vocabulary: the meter and key are payloads of the meter and key
lanes, and `notes` is the note payload of a melody lane, with times as in the conventions above.

| Key | Content |
| --- | --- |
| `meter` | Optional. `beats_per_bar` (1 to 64), `beat_unit` (1, 2, 4, 8 or 16) and optionally `bars_per_hyperbar`. |
| `key` | Optional. `tonic_pitch_class` (0 to 11) and `mode` (`major` or `minor`). |
| `notes` | A non-empty array of at most 64 `{"midi_pitch": 0 to 127, "offset": <time>, "duration": <time>}`. `offset` is measured from the start of the motive, so a gap between notes is a rest and a first note after 0 is a leading rest. Notes are in order and do not overlap. A tie is one note as long as both. |

The motive ends where its last note ends. Unknown fields are ignored; a value that is not allowed is an error
that names the field, for example `notes[2].duration.den`.

```json
{
  "meter": {"beats_per_bar": 4, "beat_unit": 4, "bars_per_hyperbar": 0},
  "key": {"tonic_pitch_class": 2, "mode": "minor"},
  "notes": [
    {"midi_pitch": 62, "offset": {"num": 0, "den": 1}, "duration": {"num": 1, "den": 4}},
    {"midi_pitch": 65, "offset": {"num": 1, "den": 4}, "duration": {"num": 1, "den": 8}},
    {"midi_pitch": 69, "offset": {"num": 3, "den": 8}, "duration": {"num": 1, "den": 8}},
    {"midi_pitch": 74, "offset": {"num": 1, "den": 2}, "duration": {"num": 1, "den": 2}}
  ]
}
```

The meter is the file's own for MIDI and MusicXML (4/4 when a file has none), and a meter given by the caller
that differs from a declared one is an error. A note list has no meter and needs one from the caller. The key is
the caller's, else the input's (a key signature with a major or minor mode, or `key` above); otherwise it is
inferred when the pitches settle on one key, and is left out when they do not. A key that was inferred is not
written, since reading the motive infers it again.

### Motive analysis

`motive_analysis_to_json` writes the motive and its analysis together, as an object with a `motive` member (the
document above) and an `analysis` member:

| Key | Content |
| --- | --- |
| `note_count` | The number of notes. |
| `length`, `bars` | The motive's length in whole notes, and as a number of bars of its meter. Times. |
| `key` | `null` without a key. Otherwise `tonic_pitch_class`, `mode` and `source`: `given` or `inferred`. |
| `intervals` | One per pair of neighboring notes: `semitones` (signed) and `steps` (signed diatonic steps in the key, `null` without a key). |
| `contour` | `shape` (`single note`, `level`, `ascending`, `descending`, `arch`, `valley` or `wave`) and `directions`, one of `up`, `down` and `same` per interval. |
| `rhythm` | One per note: `onset`, `duration`, `bar` (from 0), `beat` (the position in the bar in beats of the beat unit, 0 is the downbeat; a time), `on_beat`, `dotted` (the duration is 3 or 7 over a power of two) and `rest_before` (a time). |
| `range` | `lowest` and `highest` MIDI pitch, and `semitones` between them. |
| `scale_degrees` | `null` without a key. Otherwise `start`, `end` and `all` (one per note), each `{"degree": 1 to 7, "alteration": 0 or 1}`; 1 is a raised degree. |
| `implied_harmony` | `key_known`, and `candidates`, best fit first: `roman_numeral`, `function`, `fit` (the share of the motive that is chord tones, 0 to 1, to four places), `chord_tones` and `non_chord_tones`. Empty without a key. |

For the motive above, `scale_degrees` gives `1 3 5 1`, the contour is `ascending` and the intervals are `+3`, `+4`
and `+5` semitones, `+2`, `+2` and `+3` steps. The best implied chord is `i`, with `fit` 1.

The analysis is deterministic: the same motive gives the same document, byte for byte.

## Analyzer compatibility

Version 1 of the Analyzer's schema lists three values for a key claim's `method`. A plan's key
claims use `"composer_plan"`, which is not among them, so a strict schema check rejects a plan until
the Analyzer adds the value. The Analyzer's versioning rule treats a new `method` value as an
additive change.

## Example

A two-bar plan: a form in 4/4 and C major, one section, one chord. This is the exact output of
`plan_to_json`.

```json
{
  "schema_version": 1,
  "metadata": {
    "source_file": "",
    "analyzer_version": "0.1.0",
    "ursatz_commit": "unknown",
    "note_count": 0,
    "ticks_per_quarter": null,
    "key_found": true
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
          "tonic_pitch_class": 0,
          "mode": "major"
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
  "cadences": [],
  "phrases": [],
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
  "motives": [],
  "melody": [],
  "rhythm": [],
  "texture": [],
  "voice_leading": [],
  "schemas": []
}
```
