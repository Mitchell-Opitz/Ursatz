# Text export

Back to the [docs index](README.md). Header: `<ursatz_analyzer/text_export.h>`. Command: `ursatz-analyzer export`.

## What it is for

The Score and Analysis tabs show notes and claims a person reads off a screen. The text export writes the same thing as
plain text, so it can be pasted into a conversation: "measure 27 is flagged as X, I read it as Y". Every line is
addressed by a measure number as it is written in the score, and every claim sits in the measure where it starts.

The notes are not read from the imported notes directly. The export writes the score MusicXML the Score tab draws
(`analyzer_score_export_musicxml`), reads it back (the reader is private, `src/export/score_text_reader.h`) and prints
what it finds, so the text shows exactly what the Score tab shows, wrong spelling and wrong tuplets included. The claims
come from the analysis document ([analysis-document.md](analysis-document.md)) and nothing is added to it: the export
changes neither `analysis.json`, its schema nor the score MusicXML.

## Format v1

`URSATZ_ANALYZER_TEXT_EXPORT_VERSION` is 1 and is the `v1` of the first line. This is the start of an export of
`tests/musicxml/pickup.musicxml`:

```
# Ursatz export v1: piece
title: pickup.musicxml
key: C major | meter: 4/4 | tempo: not declared | measures: 0-4
analyzer: 0.7.0 ursatz d2b85cedb7bb
notes: <pitch><octave> @<beat> <duration>; r = rest; ~ = tied to next; 3:2 = tuplet; c=<confidence>

m0 4/4
 P1 S1 V1: G4 @4 q
 key C major c=0.37 (m0-m4)
 chord C/G I6/4 @4 c=0.45

m1
 P1 S1 V1: C3 @1 w
 P1 S1 V2: C4+E4+G4 @1 w
 chord C I @1 c=0.45
 phrase 1 m1-m4 c=0.20 | form A
 form A m1-m4 c=0.20
```

### Header

| Line | Content |
| --- | --- |
| 1 | `# Ursatz export v<version>: piece` |
| `title:` | The `title` option, else the file name the document records (`metadata.source_file`), else `untitled`. |
| `key:` | The first key region's label (`unknown` with none), the first measure's time signature, the declared tempo as `q=<quarters per minute>` or `not declared` (from the document's `metadata.input.tempo`), and the measures exported. |
| `analyzer:` | `URSATZ_ANALYZER_VERSION`, then `ursatz` and the first 12 characters of the Ursatz commit. |
| `notes:` | A fixed legend. |

`measures:` is the first and last measure number as written, for example `0-4`. A range export adds the whole piece:
`measures: 2-3 of 0-4`.

### Measures and notes

Each exported measure is a blank line, then its line: `m<number>`, where the number is the `<measure number>` of the
MusicXML as written (a pickup is `m0`), followed by the time signature (`4/4`) on the first measure and wherever it
changes. Then one line per part, staff and voice that has notes or rests in the measure, in that order:
` P<part> S<staff> V<voice>: <event>, <event>, ...`. Parts are numbered from 1 in score order; the voice is the MusicXML
voice.

An event is `<pitches> @<beat> <duration>`:

| Token | Meaning |
| --- | --- |
| `C#4`, `Bb3` | Step, one `#` per sharp or one `b` per flat (a double sharp is `##`), octave (middle C is `C4`). |
| `C4+E4+G4` | A chord: its pitches joined by `+`, at most 16. |
| `r` | A rest. |
| `@2.5`, `@1+2/3` | The onset beat, counted from 1 in the beats of the time signature's denominator (an eighth in 12/8), exact: an integer, a terminating decimal, or whole plus fraction when the value does not terminate. The beat of a pickup is counted so that the pickup ends the bar: a quarter pickup in 4/4 is `@4`. |
| `w h q e s t` | The notated value, whole to 32nd. `b` is a breve, `x` a 64th, `y` a 128th, `?` a value that matches no note. |
| `.` | One dot after the value, per dot (`h..` is a double-dotted half). |
| `3:2` | Before the value, a note in a tuplet of 3 in the time of 2 (`3:2e`). |
| `~` | After the value, a note tied to the next one. |

### Claims

After the notes of a measure come the claims that start in it, one per line, in order of start time, then in the order
below. `c=<confidence>` is one minus the claim's uncertainty, two decimals, and is left out when the claim has no
uncertainty.

| Line | Source | Notes |
| --- | --- | --- |
| ` key G major c=0.82 (m1-m16)` | `keys.regions` | The label and the measures it covers. |
| ` tonicization F major (m9-m11)` | `tonicizations` | The key tonicized. |
| ` modulation G major to D minor (m62-m65)` | `modulations` | |
| ` chord D7 V7 @1 c=0.91` | `chords` | The chord symbol (root, then `m`, `7`, `maj7`, `m7`, `dim`, `dim7`, `m7b5` or `aug`; `/bass` when the bass is not the root), the Roman numeral with its figures when it has one, and the beat. A chord with no root ("no match") is left out. |
| ` cadence PAC m12 @1 c=0.77` | `cadences` | `PAC`, `IAC`, `AC` (authentic, unspecified), `HC`, `PC`, `DC`, or the library's own name (`subverted`). The measure and beat are where the cadence arrives; the line sits in the measure where it starts (its dominant). |
| ` phrase 2 m9-m12 c=0.75 \| form A` | `phrases` | Numbered from 1, with the form section that holds it. |
| ` theme hybrid m1-m15` | `themes` | The theme type and its measures. |
| ` form A m1-m15` | `form.sections` | The section label and its measures. |
| ` motive a m12 @1-@2.5` | `motives[].occurrences` | The motive letter (`a` for the first motive, then `b`, ... `z`, `aa`), the measure and beat where the occurrence starts and the beat where it ends. An occurrence that ends in a later measure names it: `m1 @3-m2@2`. An end on a barline is a beat past the bar (`@5` in 4/4). Occurrences carry no `c=`. |

A measure range such as `(m1-m16)` runs from the measure where the claim starts to the measure that holds its last
instant, so a claim that ends on a barline stops at the measure before it.

## How claims are placed

A claim is placed in a measure by its start time, taken from the document's `span.start` (an exact fraction of a whole
note). The measure start times are the ones the reader takes from the MusicXML: each measure starts where the one before
ends, and a measure's length is its time signature's, or its content's for a short first or implicit measure. So pickup
numbering and meter changes are always the Score tab's, and a document written for the same import and MusicXML never
disagrees with the notes above it.

## Range

`first_measure` and `last_measure` (`--measures <first>-<last>`) are inclusive and match measure numbers as written.
`0` and `0` export the whole piece. A pickup (measure `0`) is included when the first measure is 0 or 1. A range that
starts after a claim leaves that claim out, even when it reaches into the range. A last measure past the end stops at
the last measure. A first measure greater than the last, a negative number, or a range with no measure in it is
`ANALYZER_ERR_ARGUMENT`.

## Entries

```c
AnalyzerTextExportOptions analyzer_text_export_options_default(void);
AnalyzerStatus analyzer_text_export_piece(const ImportedNotes *notes, const char *document_json,
                                          const AnalyzerTextExportOptions *options, char **text);
```

`notes` is the import the analysis ran on and `document_json` its analysis document (`analysis_document_render_result`
or the stored `analysis.json`). On `ANALYZER_OK` the caller frees `*text` with `free`. `ANALYZER_ERR_ARGUMENT` for a NULL
argument, a document that is not JSON or whose `schema_version` is not `URSATZ_ANALYZER_DOCUMENT_SCHEMA_VERSION`, or an
empty range; `ANALYZER_ERR_LIBRARY` when the score cannot be written or read back; `ANALYZER_ERR_ALLOC`.

```sh
ursatz-analyzer export [--input-format midi|musicxml] [--measures <first>-<last>] [--title <text>] <input-file-path>
```

The command analyzes the file as `ursatz-analyzer <file>` does and prints the export to standard output; it writes no
file. Exit 1 with a message on a bad flag, bad input or a range with no measure.

## Versioning

The first line carries the format version. A change to the layout of any line bumps `URSATZ_ANALYZER_TEXT_EXPORT_VERSION`.
Adding a claim line or a token a reader can skip does not. The export has its own snapshots
(`tests/snapshots/<slug>.export.txt`, the `analyzer:` line removed) so a change shows in review.

## Size

An export is at most 700 bytes per measure plus 2 KB; `test_text_export_snapshots` asserts it for the five reference
pieces. The notes are most of it: Field 5, with two staves, 12/8 and four voices, writes about 650 bytes per measure.

## Limits

- The reader reads only what the library's MusicXML writer emits: `<part>` order, `<measure number>` and `implicit`,
  `<attributes>` (`divisions`, `time`), `<note>` with `<rest/>`, `<chord/>`, `<pitch>`, `<duration>`, `<voice>`,
  `<staff>`, `<tie type="start">`, `<time-modification>`, `<type>`, `<dot/>` and `xml:id`, and `<backup>` and
  `<forward>`. Every other element is skipped: grace notes, unpitched notes, directions, dynamics, slurs, beams and
  repeats are not in the text.
- A chord holds 16 pitches; more are dropped.
- Entities in text are not decoded; the fields read are numbers and names.
- A note with no `<type>` takes its value from its duration, and `?` when the duration matches no plain or dotted value
  (up to three dots).
- Claims are those of the analysis document. Sections the export does not print (melody, rhythm, texture, voice leading,
  schemas, progressions, harmonic rhythm, markings, counterpoint check) are in `analysis.json` only.
