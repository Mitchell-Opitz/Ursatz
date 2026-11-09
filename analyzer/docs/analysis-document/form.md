# Form claims

Claims about phrases, themes and the form plan in the analysis document. Back to the [docs index](../README.md) and the [analysis document](../analysis-document.md).

## Phrases

`phrases` holds one claim per phrase the library's standard-form analysis finds (`analysis/analysis_phrase.h`,
`analysis_ideas.h`), in time order. The analyzer builds the library's neutral input from the chain (the outer-voice
soprano as notes, with consecutive segments of one note merged, the chord segments, the meter and the cadences of the
cadence step, each with its arrival span) and injects the kernel's Phrase Boundary Detection as the boundary detector
(the chain's `analysis_chain_phrases`, read out by `src/pipeline/form_pipeline.c`). Phrases, ideas and subphrases
are the library's; the analyzer only reads them out. A phrase runs from the measure after the one before it to the end
of its cadence's arrival chord, snapped to the next beat, so the phrases tile the measures of the analysed range and
their spans may leave the rest of a phrase's last measure out.

The ends of all but the last phrase are the boundaries of the second key pass (R-039): the library chain reads the
phrases and runs again with those ends as phrase boundaries and its chords as evidence for the key regions, so
everything in the document comes from the second pass. There is no further iteration.

| Field | Type | Meaning |
| --- | --- | --- |
| `framework` | string | `"standard-form-v1"`, the library's framework name. |
| `span`, `location`, `note_ids` | | The phrase's span, an extension after its cadence included; the notes sounding in the chords that start in it. |
| `label` | string | `"phrase"`. |
| `uncertainty` | number | The library's: the cadence's, 0.25 for a boundary made by a break in the melody, 0.5 for a final phrase with neither (higher is less certain). |
| `index` | integer | The phrase's position, from 0. |
| `measures` | object | `{"first", "last"}`: the library's measure numbers (a pickup is 0) from the phrase's first measure to the measure its cadence's arrival chord ends in; the phrase proper. |
| `length` | integer | The phrase proper in measures, a pickup not counted. |
| `extension_measures` | integer | The measures a post-cadential extension or codetta adds after `measures.last`; 0 with none. |
| `cadence` | integer or null | Index into `cadences` of the cadence that ends the phrase; `null` for a phrase no cadence ended. |
| `signal` | string | What ended the phrase: `"cadence"`, `"discontinuity"` (a break in the melody after the longest run without a cadence) or `"none"` (the end of the input). |
| `role` | string | `"beginning"` for the first phrase, `"ending"` for the last, `"middle"` between. The library names no role for a phrase, so this is position alone (a single phrase is a `"beginning"`). |
| `phrase_model` | array | The chords the library labelled for the phrase model, in order: `{"chord": index into chords, "label": "Tb" \| "PD" \| "D" \| "Te"}`. Empty when the library labels none: a phrase with no cadence, or one its rules cannot label. A phrase spans the chords that start in it up to its cadence's arrival; an extension takes no label. |
| `subphrases` | array | Optional. A phrase of at least three ideas has two: its first two ideas and the rest. Each is a claim (`label` `"subphrase"`) with `index` (0 or 1), `first_idea` and `idea_count` (into this phrase's `ideas`), `measures` and `length`. |
| `ideas` | array | Optional. The phrase's ideas, in order, cut by the library's nominal two-measure length. Each is a claim whose `label` is the library's idea label (`"basic-idea"`, `"contrasting-idea"`, `"fragment"` or `"unit"`) with `index`, `measures`, `length` and `similarity` (the share of matching intervals against the phrase's first idea; `null` for the first idea). Spans are clipped to the phrase's span. |

| `expansions` | array | Optional, absent with none. The expansions and contractions that concern the phrase (see "Themes"): each is a claim whose `label` is `"internal"`, `"external"` or `"contraction"` (the library's `ExpansionPlacement` and `PhraseLengthChange` names; its `StandardFormExpansionKind` has none of its own) with `index`, `change` (`"expansion"` or `"contraction"`), `theme` (index into `themes`, `null` for an external expansion of a phrase in no theme), `expected_length` and `actual_length` in measures, `measures_changed` (at least 1), `measures`, and `techniques` (the library's `"repetition"`, `"stretching"`, `"one-more-time"` and `"alternative-path"` it finds evidence for; the library detects only repetition and one-more-time, and none when it finds no evidence). An external one adds `external_kind` (`"post-cadential-extension"`, `"codetta"`, `"coda"` or `"prefix"`) and the booleans `suffix_length_typical` and `suffix_agrees_with_location`. |

`subphrases`, `ideas`, `expansions` and the chord claims' `phrase_model` are additive. The Markdown report renders
`## Phrases` from this array: a row per phrase (with an Expansion column), then a line per phrase with its ideas,
subphrases and phrase model.

## Themes

`themes` holds one claim per theme the library's standard-form analysis finds (`analysis/analysis_theme.h`) over the
phrases, ideas and subphrases above, in the library's order: a compound period comes after its two sentences, so a
theme may start before the one before it. The analyzer runs `standard_form_identify_themes` and
`standard_form_detect_expansions` with the default config (`src/pipeline/form_pipeline.c`); every form, part name and
judgement is the library's. The form's bass is not part of the input: the library reads the bass only for motives,
and the analyzer finds motives in the soprano only (see Motives), so no bass notes are added and the cadential ending
is never identified (README, Known limits).

| Field | Type | Meaning |
| --- | --- | --- |
| `framework` | string | `"standard-form-v1"`. |
| `span`, `location`, `note_ids` | | From the first measure to the end of the last phrase proper (an extension after its cadence is not part of the theme); the soprano notes starting in it. |
| `label` | string | The library's form name: `"sentence"`, `"period"`, `"hybrid"`, `"compound-period"` or `"repeated-phrase"`. |
| `uncertainty` | number | The library's: the greatest phrase uncertainty combined with the uncertainty of the similarity that decided the form. |
| `index` | integer | The theme's position in this array, from 0. |
| `beginning`, `ending` | string or null | The library's names for the two parts (`"presentation"`, `"compound-basic-idea"`, `"antecedent"`; `"continuation"`, `"consequent"`, `"cadential"`); `null` for a repeated phrase and a compound period. |
| `phrases` | object | `{"first", "count"}`: the phrases it spans (index into `phrases`, 1 or 2). |
| `measures`, `length` | | As for a phrase: the library's measure numbers from the first measure to the last of the last phrase proper, and the length in measures. |
| `parts_are_phrases` | boolean | The two parts are phrases (a two-phrase theme) and not subphrases (a one-phrase theme). |
| `commonality` | string or null | How common the pairing of the two parts is: `"very-common"`, `"slightly-less-common"`, `"occasional"` or `"rare"`; `null` when the library gives none. |
| `part_of_compound` | boolean | A sentence that is a half of a compound period. |
| `parts` | array | The two parts, in order: `{"name", "measures", "length", "cadence"}`. `name` is `beginning` or `ending`; `cadence` is the index into `cadences` of the cadence that ends the part, `null` for a presentation or compound basic idea and for the first half of a theme whose first phrase has no cadence. |
| `traits` | array | What the library's claim finds in the continuation: `"fragmentation"` or none. The claim gives no acceleration or sequence, so the analyzer reports none. |
| `archetypal`, `cadences_usual`, `lengths_usual` | boolean | The claim's own judgements: a sentence of 4 or 8 measures with a balanced continuation that has fragmentation; the usual cadences (a period's half cadence then perfect authentic cadence; a sentence's goal cadence); the usual lengths. |
| `halves` | array | Optional, compound periods only: the indices into `themes` of its two sentences. |

The expansion and contraction claims are in the phrase they concern, in `expansions` (see "Phrases"): an ending longer
or shorter than its beginning is an internal expansion or a contraction by the measures of the difference, and the
extension after a phrase's cadence is an external expansion. The Markdown report renders `## Themes` from this array (a
row per theme with its parts, each with the cadence that ends it, and its traits) and lists the compound periods.

## Form plan

`form` is the plan of the piece: its sections, the phrases, subphrases and ideas inside them, and the thematic returns
between its themes. The sections are the kernel's form structure detection (`analysis/form_structure_detection.h`)
over one phrase occurrence per phrase with its cadence type, and the returns are the kernel's thematic return
detection (`analysis/thematic_return_detection.h`, default config) over the soprano notes of each theme, both in
`src/pipeline/form_plan_pipeline.c`. The analyzer adds only bookkeeping: the phrases after the last closing cadence,
which the kernel leaves out, become a last section; returns between themes that share a phrase are dropped; and the
section letters, which follow from the returns alone. The sections are reported only and do not feed back into the
cadences (R-054).

The tree's nodes carry a `level` field with the exact names `form`, `section`, `phrase`, `subphrase` and `idea`, the
levels of the Composer's plan, so the Composer can load it (R-019). A phrase, subphrase or idea node points at its full
claim in `phrases` and repeats only what reading the outline needs; each idea appears once in the tree, in its
subphrase or, for a phrase of fewer than three ideas (which has no subphrases), in the phrase.

| Field | Type | Meaning |
| --- | --- | --- |
| `level` | `"form"` | |
| `measures` | object | `{"first", "last"}`: from the first section's first measure to the last section's last. |
| `sections` | array | The sections in time order, at least one (see below). |
| `returns` | array | The thematic returns (see below); empty when the library found none. |

A section is a claim (`framework` `"standard-form-v1"`, the framework of the phrases it groups):

| Field | Type | Meaning |
| --- | --- | --- |
| `span`, `location`, `note_ids` | | From the first phrase's start to the last phrase's end; the notes sounding in the chords that start in it. |
| `label` | string | The letter: `"A"`, `"B"`; a section whose opening theme returns an earlier section's material takes that section's letter with one prime more than the sections of that letter already have (`"A'"`, `"A''"`). |
| `uncertainty` | number | The greatest uncertainty of its phrases. |
| `level` | `"section"` | |
| `index` | integer | The section's position in `sections`, from 0. |
| `measures` | object | `{"first", "last"}`: the library's measure numbers, the last including the last phrase's extension. |
| `key` | string or null | The label of the key region the section's first chord lies in (not the key it ends in); `null` outside every region. |
| `region` | integer or null | The index of that region in `keys.regions`. |
| `closed` | boolean | A section-closing cadence (perfect or imperfect authentic) of the library's ended it; `false` only for the last section when phrases follow the last such cadence or there is none. |
| `cadence` | integer or null | The index into `cadences` of the cadence that ends its last phrase. |
| `returns_from` | integer or null | The index into `sections` of the earlier section whose material its opening theme returns; `null` for a section with a new letter. |
| `return` | integer or null | The index into the form's `returns` of the return that set the letter. |
| `phrases` | array | The phrase nodes, at least one. |

A phrase node is `{"level": "phrase", "phrase", "measures", "length", "cadence", "subphrases", "ideas"}`: `phrase` is
the index into `phrases`, `measures`, `length` and `cadence` are that claim's, `subphrases` are nodes
`{"level": "subphrase", "phrase", "index", "measures", "ideas"}` and `ideas` are nodes
`{"level": "idea", "phrase", "index", "label", "measures"}` (the library's `IdeaLabel` names), `index` being the
position within the phrase.

A return is a claim over the later theme, labelled `"thematic-return"`, in the order of the later theme and then the
earlier:

| Field | Type | Meaning |
| --- | --- | --- |
| `span`, `location`, `note_ids` | | The later theme's. |
| `uncertainty` | number | 1 minus the similarity. |
| `earlier_theme`, `later_theme` | integer | Indices into `themes`; the earlier theme's phrases come before the later's. |
| `earlier_section`, `later_section` | integer | Indices into the form's `sections`: the section holding each theme's first phrase. |
| `earlier`, `later` | object | `{"first", "last"}`: the measures of the two themes. |
| `similarity` | object | The kernel's evidence for the best stretch pair: `value` (the lower of `contour`, the share of equal up, down and same directions, and `rhythm`, the share of equal durations; at least 0.8), `window` (the notes compared), and `earlier_offset` and `later_offset` (where the stretches begin inside each theme, in notes). |

The Markdown report renders `## Form plan` from this object: a row per section (letter, measures, key, phrases, closing
cadence, the return that lettered it), the returns, and the tree as an indented outline.

## Repeats and returns

`repeats` (top level, right after `form`) lists the library's repeats and returns (`analysis_chain_repeats`, from
`analysis/analysis_repeats.h`, R-400): a later run of at least four measures that restates an earlier run. In a repeat
every measure has the same notes (onset, pitch and duration) as its earlier measure; in a return the soprano has the same
onsets and every pitch moves by one constant number of semitones, and the other voices need not match. The library finds
them from the notes alone and lists them by the later run's first measure, then the earlier's. The
form plan's thematic returns above are a separate, theme-level reading and stay as they are (R-359); the two lists are
independent and may name the same music. The analyzer adds only bookkeeping (`src/pipeline/form_pipeline.c`).

Each entry is a claim over the later run, in the library's order (`framework` `"standard-form-v1"`):

| Field | Type | Meaning |
| --- | --- | --- |
| `span`, `location` | | From the start of the later run's first measure to the end of its last. |
| `note_ids` | array | The soprano notes that start in the later run. |
| `label`, `kind` | `"repeat"` or `"return"` | Both the same. |
| `uncertainty` | number | Always `0`: the library finds them by exact match. |
| `earlier`, `later` | object | `{"first", "last"}`: the library's measure numbers of the two runs; the earlier run ends before the later begins. |
| `transposition` | integer | Semitones, later minus earlier; `0` for a repeat. A return can also read `0` when only the soprano matches. |

The report holds the same list as `AnalysisReport.repeats` (`AnalysisRepeat`). Measured on the reference pieces:
Mozart 545 has the repeats m1-m28 to m29-m56 and m57-m101 to m102-m146 and the return of m1-m8 at m70-m77 (+5); Clementi
36 has the returns of m1-m4 at m24-m27 (-12) and m8-m12 at m31-m35 (-7). The text export shows each at the later run's
first measure ([text-export.md](../text-export.md)).
