# Growing a theme: hearing the result

How `grow_run` hears the grown theme, with periods and hybrids; the hearing itself is in [form-hear.md](form-hear.md). Back to the [docs index](README.md).

## Hearing the result

After a complete run `grow_run` calls `form_hear` on the theme section (see [form-hear.md](form-hear.md)) and keeps
the themes in `GrowOutcome.heard`. A theme heard as the form the section asks for adds to `GrowOutcome.score`
(`structure share + heard`, out of `structure total + 1`); a mismatch is reported in `heard_form` and
`heard_as_requested`, never rejected. A hybrid is heard as requested only with the beginning and the ending asked
for (`requested_beginning`, `requested_ending`).

Measured by `tests/test_grow_sentence_heard.c`, over four keys and meters and seeds 1 to 10 (`ctest` runs it as
WILL_FAIL, since it asserts that every grown sentence is heard as a sentence). The theme is heard through the Analyzer; the figures measured with the adapter reading the planned chords (the motive-forms) were 28 of 40 (70%) at 8
bars, 12 as a hybrid, none unheard:

| Bars | Heard as a sentence | Heard as another form | No theme |
| --- | --- | --- | --- |
| 8 | 30 of 40 (75%) | 9 | 1 |
| 4 | 0 of 40 (0%) | 0 | 40 |
| all | 30 of 80 (37%) | 9 | 41 |

The causes below were found with the adapter reading the planned chords; the Analyzer identifies the chords from the notes
and hears every written lane, which moved the shares as above. They are not analysed again here. They are limits of the
phrase and theme stages, not tuned away:

- **Statement-response is heard as a hybrid.** The adapter compares ideas by the semitone interval and the onset
  rhythm of each pair of notes (75% must match). A tonal step up changes some intervals by a semitone, so the
  repetition reads as a contrasting idea. The exact repetitions are heard as a sentence, the statement-responses (a
  transposition or a dominant form) mostly as a hybrid.
- **A sentence of 4 bars is not heard at all.** The adapter cuts a phrase into ideas of two measures, so 4 bars are
  two ideas, which it reads as one part, not as a presentation and a continuation: no theme.
- **Fragmentation is not seen.** The nominal cut of 2 measures finds no 1 + 1 + 2 continuation, so `has_fragmentation`
  is false for every grown sentence.

### Periods and hybrids

Measured by `tests/test_grow_period_heard.c`, one `ctest` test for each pairing but the sentence, over four keys and
meters and seeds 1 to 10 (`ctest` runs each as WILL_FAIL, since each asserts that every theme of 8 bars is heard as the
form it was grown as: a period for an antecedent and a consequent, a hybrid with the requested beginning and ending for
the rest). The compound basic idea and a continuation was heard in every run with the motive-forms and is heard in 38 of
40 through the Analyzer. The "before" column is the figure measured with the adapter reading the planned chords:

| Pairing | Before | 8 bars: heard as requested | Heard as another form or with other parts | No theme |
| --- | --- | --- | --- | --- |
| Presentation + consequent | 9 of 40 (22%) | 5 of 40 (12%) | 33 (19 as another form, 14 as a hybrid with other parts) | 2 |
| Presentation + cadential | 0 of 40 (0%) | 0 of 40 (0%) | 39 (32 as another form, 7 as a hybrid with other parts) | 1 |
| Antecedent + continuation | 30 of 40 (75%) | 14 of 40 (35%) | 2 (hybrids with other parts) | 24 |
| Antecedent + consequent (period) | 0 of 40 (0%) | 3 of 40 (7%) | 16 (as another form) | 21 |
| Antecedent + cadential | 0 of 40 (0%) | 0 of 40 (0%) | 37 (hybrids with other parts) | 3 |
| Compound basic idea + continuation | 40 of 40 (100%) | 38 of 40 (95%) | 0 | 2 |
| Compound basic idea + consequent | 19 of 40 (47%) | 3 of 40 (7%) | 34 (hybrids with other parts) | 3 |
| Compound basic idea + cadential | 0 of 40 (0%) | 0 of 40 (0%) | 34 (hybrids with other parts) | 6 |

Every theme of 4 bars is the same as a sentence of 4 bars: 0 of 40 for every pairing, all no theme. Only the themes of 8
bars are held to the assertion for that reason.

The causes below were found with the adapter reading the planned chords; the Analyzer identifies the chords from the notes
and hears every written lane, which moved the shares as above. They are not analysed again here:

- **The antecedent's half cadence is never heard.** The V that ends the antecedent is followed by the root-position
  tonic that begins the consequent, which the adapter hears as an authentic cadence, at the consequent's first bar. It
  cuts the first phrase one bar late, bars 1 to 5 and 6 to 8 of 8, in all 40 periods and all 40 antecedents with a
  continuation. The second phrase is then three bars, one idea, and a consequent needs two, so a period cannot be
  heard as one; at best the theme is heard as an antecedent and a continuation, which needs the second cadence
  stronger than the first. An antecedent and a continuation: 30 of 40 have an imperfect authentic cadence at the
  cut and a perfect one at the end, and are heard; the other 10, all in D minor, have a perfect authentic cadence at
  the cut too, as strong as the last, and have no theme. A period: 27 of 40 have an imperfect authentic cadence at
  the end, since the cadence's tone comes late in the last bar and not with the chord (the consequent's contrasting idea
  is a one-bar motive that fills the bar before the last chord, the melody fills the last bar with the motive's rhythm,
  and the adapter reads the soprano at the chord's start): equal to the first, no theme; 3 have two perfect ones: no
  theme; and 10 are heard as an antecedent and a continuation. This is the limit `docs/form-hear.md` describes: a V
  reached from the tonic and a consequent that starts on I6 are heard as written. The harmony operators write a
  root-position tonic first and a predominant before the V, and a harmony shape cannot ask for anything else, so a
  period cannot be heard here until it can (written in `notes.md`).
- **The cadential ending is never identified.** The adapter's theme identification passes the bass pattern of the
  cadence as false (`analysis_theme.c`), so a second part that is neither a consequent nor fragmented is a continuation
  whatever its bass: 0 of 120 cadential endings.
- **A consequent is heard when its basic idea is heard as the basic idea again.** The adapter labels the ideas of a
  phrase by their interval and rhythm similarity to its first idea, and it needs the first idea of the second part to
  be a match and the second to differ. The consequent's restated basic idea is written over a phrase model, not over the
  prolongation the first half has, so the melody cannot always repeat its intervals (similarity 0.11 to 0.33 in the
  samples, where it needs a match): 2 of 40 after a presentation, 19 of 40 after a compound basic idea.
- **A compound basic idea and a continuation is heard 40 times in 40** since the motive-forms; before them 39, the other theme's contrasting idea read as a
  repetition of its basic idea, which makes it a presentation, so a sentence.
