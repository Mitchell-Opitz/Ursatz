# Rhythm, texture and voice leading analyses

What the Analyzer reads for rhythm, meter, texture, voice leading, schemas and sequences, and how. The fields of these claims are in [rhythm and texture](../analysis-document/rhythm-and-texture.md) and [voice leading](../analysis-document/voice-leading.md). Back to the [docs index](../README.md).

## Rhythm and meter

The analyzer builds the neutral input of the rhythm adapter of Ursatz's standard-meter
(`src/pipeline/rhythm_pipeline.c`) and reports the library's results.
The input holds every note (its line: the outer voice soprano, the bass or any other), a chord change wherever the base
numeral or key region differs from the chord before, and the phrases with the arrival of their cadence and their length
change. The document's `rhythm` is one flat list of claims labelled by kind:

- **Meter fit** (first in the list): the meter the analysis used (`declared`: meter, scale, pickup and a score from 0 to 1)
  against the best other reading (`alternative`, `null` unless it scores at least 0.15 higher). The score is how well the
  reading's barlines carry the harmony changes (on a downbeat, weight 2; on any beat, 1) and the cadence arrivals (on a
  downbeat, 3). It exists for a file whose meter does not fit its music, such as a 3/4 piece written in 4/4; the
  alternative is the `--meter`, `--value-scale` and `--pickup` that would read it. It is offered and never applied. A reading
  applied with those options is the one checked, so a corrected piece offers nothing. The Markdown report prints one line
  of it under `## Rhythm and meter`.
- **Density** per phrase, for all its notes and for each line: notes, attacks, attacks per beat, the mean distance
  between attacks, the share of attacks on the beat and a histogram of the notes' lengths. The recurring rhythm patterns
  are the motive families' rhythms (see [Motives](form-and-motive.md#motives)), not a second detector.
- **Syncopation**: a note attacked at a weaker metric level and held through a stronger one that no note of its line
  attacks, with the level it is attacked at, the one it is held through and the strength of the difference.
- **Hemiola**: two measures of a simple triple meter, or one of 6/8, whose six pulses read as three duple units, with the
  three kinds of evidence (the durations, the chord changes, the accents) and how many agree.
- **Hypermeter**: for each phrase the hypermeasure length (2 or 4 measures) that fills it and holds the cadence in the
  last hypermeasure, the hyperdownbeats and the hypermeasures (regular, short, expanded, extension or contracted).
- **Tuplets**: runs of triplets (sextuplets included) and duplets, named borrowed or not and placed at a level by the
  library's rules. The importers carry no notated tuplets, so they are read from the notes' times alone, rounded to a grid
  of 96 to the whole note.

The Markdown report has a `## Rhythm and meter` section with a subsection for each.

Measured on the reference pieces (MIDI):

| Piece | Meter read | Phrases | Syncopations (soprano / bass / other) | Hemiolas | Hypermeasures (regular / short / expanded / extension / contracted) | Tuplet runs |
| --- | --- | --- | --- | --- | --- | --- |
| Bach 114 | 4/4 | 2 | 21 (4 / 11 / 6) | 0 | 8 (6 / 1 / 1 / 0 / 0) | 0 |
| Clementi 36 | 4/4 | 10 | 0 | 0 | 14 (9 / 2 / 0 / 0 / 3) | 0 |
| Mozart 545 | 4/4 | 52 | 31 (29 / 0 / 2) | 0 | 113 (79 / 19 / 6 / 0 / 9) | 4 (triplets, borrowed) |
| Field 1 | 12/8 | 10 | 146 (114 / 1 / 31) | 0 | 19 (11 / 3 / 2 / 1 / 2) | 83 (triplets, not borrowed) |
| Field 5 | 12/8 | 9 | 11 (5 / 0 / 6) | 0 | 15 (6 / 6 / 2 / 1 / 0) | 2 (triplets, not borrowed) |

`test_rhythm` checks the structure of the section on Field 5 and its floors against Ursatz's hand annotation
(`tests/fixtures/annotations/field5_rhythm.txt`, a draft reading of measures 1 to 30 of the 12/8 score): the annotated
syncopation (measure 14, beat 2) is found, no hemiola is annotated (12/8 has none) and 7 of the 8 annotated hyperdownbeats
are the first measure of a hypermeasure, against the phrases the form adapter detects (Ursatz scores the annotated
phrases). `test_rhythm_constructed` runs small pieces whose answer is known: eighth-note triplets in 4/4 (one run of six
attacks, a borrowed division at the beat), a duplet in 6/8 and a hemiola in 3/4. `test_rhythm_field5_target` asserts
Ursatz's target (80% of the syncopations and hyperdownbeats, every hemiola, every reinterpretation) and is marked
`WILL_FAIL` (see [Known limits](../known-limits.md)).

## Texture

The analyzer builds the neutral input of the texture adapter of Ursatz's standard-texture
(`src/pipeline/texture_pipeline.c`) and reports the library's results.
Every imported note carries a stream (`ImportedNotes.streams`: the part, that is the MIDI track and channel read back
from the file, or the MusicXML part), the kernel's voice separation runs per stream (a part is usually a hand) and gives each note a voice and
a confidence, and the library measures each measure, classifies it with its defaults and merges equal consecutive
measures into passages. The document's `texture` has a claim per passage (texture, category, measures, confidence and
the measured features) and one per change. The library names no accompaniment pattern, so the passage reports the
repeated-pattern score only. The Markdown report's `## Texture` prints the passages and the changes.

Measured on the reference pieces (MIDI), passages by texture:

| Piece | Passages | Changes (category changes) | Monophony | Polyphony | Homorhythm | Melody and accompaniment |
| --- | --- | --- | --- | --- | --- | --- |
| Bach 114 | 9 | 8 (4) | 0 | 2 | 2 | 5 |
| Clementi 36 | 7 | 6 (4) | 0 | 2 | 1 | 4 |
| Mozart 545 | 137 | 136 (60) | 15 | 16 | 47 | 59 |
| Field 1 | 10 | 9 (9) | 1 | 4 | 0 | 5 |
| Field 5 | 19 | 18 (14) | 0 | 7 | 2 | 10 |

`test_texture` checks the structure on Field 1 and scores it against Ursatz's hand annotation
(`tests/fixtures/annotations/field1_texture.txt`, a draft reading): 59 of 65 annotated measures have the right category
(90.8%, target 80%; measures 8, 40, 42, 43, 63 and 64 are read as polyphony, the annotation says homophony) and the one
annotated change point (measure 2) is found. A MusicXML file's streams are its parts, so its passages can differ from the
MIDI file's.

## Voice leading

The analyzer builds the voice-pitch grid of the adapters of Ursatz's standard-counterpoint and standard-harmony
(`src/pipeline/voice_leading_pipeline.c`) and reports the library's results. The kernel's voice separation runs over every note of
the piece as one stream (all tracks and parts together, voice 0 the top); the grid has one row per chord segment (a
report entry) and one column per voice, up to six (lower voices are left out), and each cell holds the note of that voice
sounding at the start of the segment. The cell's spelling is the input's own when it is notated and matches the sounding
pitch, otherwise the library derives it from the semitone. The document's `voice_leading` has these claims:

- `motions`, one claim: the library's classification (contrary, similar, parallel, oblique, static) of the motion between
  consecutive chord segments for every pair of voices that sound in both, in total and per pair.
- `violation`: a parallel fifth or octave between any pair of voices, or a direct fifth or octave in the outer pair
  (`check_perfects`), with the voices, the two chords and the four notes.
- `resolution`: a seventh or leading tone left unresolved when its chord changes (`check_resolutions`, against the chords
  the harmony stage prepared), with the voice, the two chords and the two notes.
- `doubling`: for each chord three or more voices sound, the chord member doubled, the voices on notes the chord does not
  have, the semitone gaps between adjacent voices and, when exactly four voices sound, the verdict of standard-voicing's
  spacing rule (`spacing_rules.h`: an octave between soprano and alto and between alto and tenor, a twelfth between tenor
  and bass).

Doubling is counted, not judged: the chord members are found from the chord's interval set and the bass, and
standard-voicing's doubling rules need chord spellings the analyzer does not build (R-018). The Markdown report's
`## Voice leading` prints the motion table, the faults and a summary of the doubling.

Measured on the reference pieces (MIDI):

| Piece | Voices | Chord segments | Motions | Parallel fifths | Parallel octaves | Direct | Unresolved sevenths | Unresolved leading tones | Chords with doubling | Four-voice chords, too wide |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Bach 114 | 4 | 155 | 74 | 0 | 0 | 0 | 6 | 1 | 6 | 0 of 6 |
| Clementi 36 | 3 | 205 | 55 | 0 | 0 | 0 | 9 | 0 | 3 | none |
| Mozart 545 | 4 | 1095 | 1025 | 2 | 11 | 0 | 57 | 12 | 289 | 22 of 88 |
| Field 1 | 4 | 268 | 128 | 0 | 1 | 1 | 19 | 2 | 43 | 2 of 4 |
| Field 5 | 4 | 169 | 531 | 0 | 0 | 2 | 24 | 2 | 127 | 39 of 83 |

The counts follow the voice separation: a piece whose voices rest often has fewer motions than segments. `test_voice_leading`
runs a constructed chorale (`tests/musicxml/chorale-voice-leading.musicxml`) with one parallel fifth and one unresolved
leading tone and asserts that both are found at their measures and nothing else.

## Schemas and sequences

The analyzer builds the neutral input of Ursatz's galant-schemas and scale-harmonization adapters
(`src/pipeline/schema_pipeline.c`) and reports the library's matches. Each
chord segment becomes one input segment: the outer voices' pitch classes read as degrees of its local key (the Roman
numeral input's tonic and mode) by the offset above the tonic, that key as a degree of the home key, the library's
numeral (an applied chord's root from the tonicized degree), and the strength and length of its beat. Consecutive chords
with the same key, outer voice degrees and numeral are one segment, with the uncertainty of the least certain; a chord
outside every key region is left out, and a segment's uncertainty is its numeral's (0.5 with none). The library's defaults
run unchanged. The document's `schemas` is one flat array with two kinds of claim:

- `schema`: a galant schema with its function, the stages (each with the measure it starts in and the soprano and bass
  degrees matched; a stage may match on the bass alone, and a two-note stage has a second soprano degree), the frame key
  it was found in, and the library's confidence parts.
- `sequence`: a model of one or more chords repeated, transposed by a constant diatonic interval, at least three times,
  with its kind, units, direction, transposition in diatonic steps and stages (bass, soprano and root degrees in the home
  key). The cycle of fifths has no rule in scale-harmonization yet, so the library reports it from the Roman numerals
  alone (`rule_backed` false).

The Markdown report's `## Schemas and sequences` prints a table of each.

Measured on the reference pieces (MIDI, the library's defaults): no piece has more than one schema.

| Piece | Schemas | Sequences |
| --- | --- | --- |
| Bach 114 | 0 | 2 (parallel chords) |
| Clementi 36 | 0 | 7 (parallel chords) |
| Mozart 545 | 1 (a Jupiter) | 5 (4 cycles of fifths, 1 parallel chords) |
| Field 1 | 0 | 0 |
| Field 5 | 0 | 0 |

`test_schemas` writes the hand-written galant fixture of Ursatz's `test_schema_sequence.c` as a MIDI file and runs the whole
chain on it, as Ursatz does for its chain: 5 of the 9 planted items are found (the Romanesca twice, the Prinner, the
Do-Re-Mi and the ascending 5-6; the Meyer, Fonte, Monte and the cycle of fifths are missed), above the floor of 4 that
Ursatz asserts. Ursatz also scores the adapters on the fixture's own segments (all 9 found); the analyzer has no such
entry point. The reference pieces' counts above are pinned.
