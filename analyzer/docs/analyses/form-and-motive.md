# Form and motive analyses

What the Analyzer reads for phrases, themes, the form plan, motives and melody, and how. The fields of these claims are in [form](../analysis-document/form.md) and [motive and melody](../analysis-document/motive-and-melody.md). Back to the [docs index](../README.md).

## Phrases

Over the cadences (`src/pipeline/form_pipeline.c`), the analyzer builds the library's standard-form input
from the outer-voice soprano (consecutive segments of one note are one note), the chord segments, the meter and the
cadences with their arrival spans, injects the kernel's Phrase Boundary Detection, and reads back the phrases, then
the ideas and subphrases inside each. The library's phrase model labels (`Tb`, `PD`, `D`, `Te`) go on the chords of
each phrase that ends in a cadence. The ends of all but the last phrase are then the boundaries of the second key
pass (see [Pipeline](../library.md#pipeline)). The Markdown report has a `## Phrases` table (measures, length, cadence, what ended it, role)
and a line per phrase with its ideas, subphrases and phrase model; the document's `phrases` has the claims. A
phrase's role is its position in the piece (the first is a beginning, the last an ending), since the library names
none.

Measured on the reference pieces (MIDI; `test_phrases`, which asserts these counts so a change in the chain shows):

| Piece | Phrases | Ideas |
| --- | --- | --- |
| Bach 114 | 2 | 10 |
| Clementi 36 | 10 | 18 |
| Mozart 545 | 52 | 166 |
| Field 1 | 10 | 30 |
| Field 5 | 9 | 22 |

These are the library's phrases, not hand readings: a phrase without a cadence ends on a break in the melody or the
end of the piece, and the cadences still miss much of the harmony (Mozart K. 545 has 52 phrases, 15 of them without a
cadence, and Field 1 has one).

## Themes and expansions

On the phrases, ideas and subphrases above the library identifies themes (`standard_form_identify_themes`) and
detects expansions and contractions (`standard_form_detect_expansions`), both with the default config. The document's
`themes` has a claim per theme with the form, the names of its two parts, their measures and cadences, the commonality
of the pairing and the traits of its continuation; each phrase claim lists the expansions and contractions that concern
it. The Markdown report has a `## Themes` table and an Expansion column in `## Phrases`. A constructed sentence,
period and compound period (`tests/musicxml/sentence.musicxml`, `period.musicxml`, `compound-period.musicxml`) are
heard as such by `test_themes`.

Measured on the reference pieces (MIDI; `test_themes` asserts structure, not these counts):

| Piece | Themes | Forms | Expansions | Contractions |
| --- | --- | --- | --- | --- |
| Bach 114 | 2 | 2 hybrid | 2 (1 internal, 1 external) | 1 |
| Clementi 36 | 3 | 3 hybrid | 0 | 3 |
| Mozart 545 | 18 | 17 hybrid, 1 repeated phrase | 6 (internal) | 9 |
| Field 1 | 5 | 5 hybrid | 3 (2 internal, 1 external) | 2 |
| Field 5 | 4 | 4 hybrid | 3 (2 internal, 1 external) | 1 |

The constructed pieces give a sentence, a period and a compound period; the reference pieces give hybrids and one
repeated phrase, and no sentence, period or compound period. Continuation traits are the claim's fragmentation only
(Field 5 has one), and the cadential ending is never identified (see [Known limits](../known-limits.md)).

## Form plan

Over the phrases and themes (`src/pipeline/form_plan_pipeline.c`), the analyzer gives the
kernel's form structure detection one phrase occurrence per phrase with its cadence type, and reads back the sections:
a section closes at each phrase that ends on a perfect or imperfect authentic cadence. The kernel leaves out the
phrases after the last such cadence, so the analyzer takes them as a last section (`closed: false`) and the sections
tile the phrases; a piece with no such cadence is one section. Thematic returns come from the kernel's thematic return
detection (default config) over each theme's soprano notes: a later theme recalls an earlier one when a stretch of
six notes of both has the same contour and rhythm to a similarity of at least 0.8. Themes that share a phrase (a
compound period and its sentences) are not returns of one another. The sections are reported only (R-054): they do not
feed back into the cadences, which use the key-return section starts.

The letters are the analyzer's bookkeeping over the returns. Sections are lettered by first appearance. A section
whose opening theme (a theme that starts at the section's first phrase) returns a theme of an earlier section takes
that section's letter with a prime, one more prime for each section of that letter already (`M`, `M'`, `M''`);
where several returns qualify, the most similar one decides, then the earliest section. The document's `form` is the
tree (`form`, `section`, `phrase`, `subphrase`, `idea`) and the returns; the Markdown report has a `## Form plan`
table, a `### Thematic returns` table and an indented `### Outline`.

Measured on the reference pieces (MIDI; `test_form_plan` asserts these counts and letters, and the structure):

| Piece | Sections | Letters | Returns |
| --- | --- | --- | --- |
| Bach 114 | 2 | A A' | 1 |
| Clementi 36 | 5 | A B C D E | 3 |
| Mozart 545 | 24 | A to L, M M' N O M'' P M''' Q to U | 77 |
| Field 1 | 5 | A B C D E | 0 |
| Field 5 | 4 | A B A' C | 4 |

The constructed sentence is one section, the period two (the antecedent's imperfect authentic cadence closes the
first), and the compound period two (A A'). The last section of every reference piece is open: the final phrase is
never a section boundary in the kernel's rule. Mozart 545's 77 returns are almost all between hybrids and repeated
phrases whose six-note stretches share a contour and rhythm (similarity 0.80 to 1.00), so they say that stock figures
recur, not that a theme returns; only the opening themes decide the letters.

## Motives

Over a note-level soprano (`src/pipeline/motive_pipeline.c`), the analyzer runs the library's
motive analysis with the default config: motives of 3 to 8 notes that recur, in the soprano only. The soprano is the
highest sounding note at each attack, because the outer voices keep one note per voice segment and would thin a run of
sixteenths; each note carries its scale degree in its key region, so a diatonic sequence matches. The motive analysis
needs phrases, ideas and themes on the same notes, so the stage reruns them on its own input; its occurrences are then
placed in the report's phrases and ideas by span (an occurrence that crosses a phrase boundary lies in none).

A family has a prototype and occurrences, each with the library's kind against the prototype (exact, real-transposed,
tonal-transposed, inverted, retrograde, augmented, diminished, fragmented, varied) and its score. The development of a
family is its occurrences in time order. The prototype is not always the earliest occurrence, because the library
searches continuations for fragments first. The document's `motives` holds all of it; the Markdown report has a
`## Motives` table (with the prototype's degrees) and a `### Development` list.

Each family also carries `features`, read from its prototype: the notes with exact onsets and durations, the key
region's tonic and mode, the meter, the degrees against the tonic's major scale (`1 b2 2 b3 3 4 #4 5 b6 6 b7 7`, so a
minor-key `1 b6 5` reads the same as one in major), the intervals in semitones and scale steps, the rhythm, the
non-chord-tone type of each note's segment, and a fingerprint of the prototype as a library `MotiveSketch`
(`src/pipeline/motive_sketch_map.c` is the one mapping from features to the sketch, shared by the report and the
motive links). Each occurrence carries its notes' degrees. See
[docs/analysis-document/motive-and-melody.md](../analysis-document/motive-and-melody.md#motives) and [docs/style-profile.md](../style-profile.md).

Measured on the reference pieces (MIDI):

| Piece | Families | Occurrences | Kinds |
| --- | --- | --- | --- |
| Bach 114 | 32 | 92 | exact 78, fragmented 14 |
| Clementi 36 | 0 | 0 | |
| Mozart 545 | 214 | 1610 | exact 556, fragmented 695, tonal-transposed 163, real-transposed 101, varied 38, inverted 35, retrograde 20, augmented 2 |
| Field 1 | 1 | 4 | exact 1, real-transposed 2, fragmented 1 |
| Field 5 | 10 | 48 | exact 32, inverted 8, fragmented 6, tonal-transposed 2 |

`test_motives` checks Mozart 545 against Ursatz's hand annotation of the exposition
(`tests/fixtures/annotations/mozart545_motives.txt`, a draft reading): all 7 annotated families are found, and 20 of
the 21 annotated occurrences are found with the right kind (95.2%; the miss is the tonal-transposed occurrence at
measure 21 beat 2), against the library test's 75% target. The Analyzer finds the motives over the whole piece with
its own cadences, not over the exposition with annotated phrase ends.

## Melody

For each phrase, the analyzer builds the soprano as the neutral line of the melody adapter of Ursatz's
standard-counterpoint (`src/pipeline/melody_pipeline.c`) and reports the library's results: the range, the climax (the first note at the highest pitch),
the contour (the library's direction of each interval, counted, no shape named), every leap with its recovery status
(the cantus firmus rule LEAP_RECOVERY), the pairs of leaps in a row the library's rule faults, the non-chord tones by
type (the chord identification's result attached to each note), and the tendencies. The line is the outer voice soprano
of the phrase, one note per melody note (consecutive voice segments with the same note are one note, a note held in from
the previous phrase is left out, a gap is a rest); each note carries its scale degree in its key region and its beat
strength. The library has no table of scale-degree tendencies, so the report gives the five line tendencies of
`tendency_rules.h` (pitch proximity, step declination, step inertia, melodic regression, melodic arch) as held or not, and
the seventh degree judged by the cantus firmus rule LEADING_TONE: how many sevenths a note follows that follow the rule,
and where the broken ones stand. The document's `melody` holds all of it; the Markdown report has a `## Melody` table and
the subsections `### Leaps`, `### Non-chord tones` and `### Tendencies`.

Measured on the reference pieces (MIDI), summed over the phrases:

| Piece | Phrases | Notes | Leaps (recovered / unrecovered / third / not judged) | Faulted pairs | Non-chord tones | Seventh followed / broken |
| --- | --- | --- | --- | --- | --- | --- |
| Bach 114 | 2 | 149 | 9 / 27 / 18 / 0 | 2 | 11 | 16 / 4 |
| Clementi 36 | 10 | 151 | 0 / 19 / 37 / 6 | 21 | 3 | 8 / 5 |
| Mozart 545 | 52 | 910 | 30 / 217 / 232 / 27 | 189 | 94 | 30 / 45 |
| Field 1 | 10 | 164 | 11 / 26 / 24 / 4 | 22 | 15 | 4 / 8 |
| Field 5 | 9 | 113 | 6 / 20 / 11 / 1 | 4 | 4 | 5 / 4 |

`test_melody` checks the structure of the section on Bach 114 (the range, climax, contour, leaps and non-chord tones agree
with the notes), and its floors against Ursatz's hand annotation of the first eight measures
(`tests/fixtures/annotations/bach114_melody.txt`, a draft reading with placeholder phrase spans): both climaxes are right,
none of the ranges and 4 of the 11 annotated leaps have the annotated status, the same as Ursatz's own whole-chain run.
`test_melody_bach_114_target` asserts the library test's target (every climax and range, at least 90% of the leaps) and is
marked `WILL_FAIL` (see [Known limits](../known-limits.md)).
