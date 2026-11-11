# Hearing the phrase form

`form_hear` (`<ursatz_composer/form_hear.h>`) hears the phrase-level form of a written plan through the Ursatz Analyzer.
There is one analysis path in the Composer (R-257): the plan is rendered once, `analyzer_hear` (`<ursatz_analyzer/composer_api.h>`)
analyzes the notes that start inside the span, and `form_hear` reads the themes, phrases and cadences of the Analyzer's
result (`analysis_chain_themes`, `analysis_chain_phrases`, `analysis_chain_cadences` of `analyzer_result_chain`). It is how
the Composer checks the form of what it wrote with the framework's rules, never with rules of its own. Back to the [docs index](README.md).

```c
FormHeard heard;
if (form_hear(plan, span, &heard) == FORM_HEAR_OK) {
    for (size_t i = 0; i < heard.count; i++) {
        printf("%s, bars %zu to %zu\n", form_hear_form_name(heard.themes[i].form),
               heard.themes[i].first_bar, heard.themes[i].first_bar + heard.themes[i].bars - 1);
    }
}
form_heard_destroy(&heard);
```

## One hearing a run

`src/check/hearing.h` (private) `ComposerHearing` renders the plan once (`plan_render`, every written lane) and keeps the
render with the MIDI number of each note. `hearing_section(hearing, span, &result)` runs `analyzer_hear` over the events
that start inside the span (borrowed, not copied), with the meter map of the render, the key lane's key under the span's
start, and a stream for each lane, and keeps the result for the life of the hearing, so a second check of the same span
reads the same result. `compose_run` makes one hearing after the texture stage, and `form_hear`, `return_hear`,
`piece_check_keys`, `analyzer_check_section`, `analyzer_check_piece` and `composer_style_measure` read it; each public call
made on its own makes a hearing of its own. `scripts/check-hearing.sh` (run in CI) fails when `src/` or `cli/` calls the
library's form or texture detectors, an `analyzer_analyze_fragment` entry, or `plan_render` anywhere but `hearing.c`,
`export.c` and `variations.c`.

The Composer hears one chord per segment: `hearing_analyzer_options` (`src/check/hearing_result.h`) sets `harmonic_spans` false
for every `analyzer_hear` call, while the Analyzer's own output reads harmonic spans. Compose rates and baselines were recorded
on the one-chord-per-segment reading, and on spans the Analyzer misreads the two-voice texture the Composer renders (melody and
bass, no inner voices) at cadences (R-378, R-379). P41 keeps the hearing off spans (R-394; `testing/span-hearing.md`). Cadences are heard only at phrase points (Ursatz P41).
Keys change only at phrases with a cadence in the new key (Ursatz P42), so a key change inside a phrase is heard as a tonicization, not a modulation.

## What the Analyzer hears

Only the plan's written lanes over the span: every lane (melody, bass, inner voices) as a stream of notes, with the meter
map and the key under the span's start. Constraints, nodes and the planned chords are never read: a cadence constraint does
not make a cadence, and the Analyzer identifies the chords from the notes. A note belongs to the span when it starts inside
it. Bars are counted from the start of the plan.

`form_hear` still checks that the lanes over the span are written (`FORM_HEAR_ERR_NOT_WRITTEN`) and that the span is in one
meter (`FORM_HEAR_ERR_METER`).

## Where a phrase starts

The Analyzer finds the phrases itself. `form_hear_with_sections(plan, span, starts, count, &heard)` keeps its signature, but
the Analyzer's hearing takes no given boundaries (`AnalyzerHearingOptions` holds the stages only, not
`refine_with_given_boundaries`), so the starts are not passed on and it equals `form_hear`. A V followed by I6 is heard as
an evaded (subverted) cadence, and a period whose consequent opens on I6 is heard as one phrase, a hybrid.

## Several keys

The Analyzer is given the key under the span's start and finds its key regions from the notes. `FormHeard.cadences` lists
the cadences heard, each with where its arrival chord starts (from the Analyzer's report) and its `CadenceType`.

`form_hear_keys(plan, span, &keys, &count)` hears the same span once and returns the key regions of the Analyzer's chain (`analysis_chain_key_regions`): a `FormHeardKey` is the region's start in whole notes from the start of the plan and its `AnalysisKey`. The caller frees `keys`; with none heard `keys` is NULL and `count` 0. The same errors as `form_hear`. The piece rows' `key_heard` is the region in force at a section's start in the keys of the whole piece.

## The result

`FormHeard` holds one `FormHeardTheme` per theme the Analyzer's result holds, in its order, and the
number of phrases it segmented. A theme has the form (`PhraseLevelForm`), its beginning and ending
(`PhraseBeginning`, `PhraseEnding`, -1 for none), the commonality of the pairing, its first bar and
length in bars (0-based from the start of the plan), its phrase count, the cadence types of its two
parts (-1 when a part has no cadence of its own, as a presentation), and whether it is archetypal
and has fragmentation. `form_hear_form_name` gives standard-form's own name for a form.

| Result | When |
| --- | --- |
| `FORM_HEAR_ERR_NOT_WRITTEN` | A melody, bass or harmony span over the range is Constrained. |
| `FORM_HEAR_ERR_METER` | The span is not in one meter span, does not start on a barline, is not whole bars, or the plan has no key under its start. |
| `FORM_HEAR_ERR_ANALYSIS` | The Analyzer refused the notes or failed. |

## Limits

These are the limits of the Analyzer's phrase and theme stages (standard-form's own, `analysis_theme.h`). They are written
here, not worked around.

- One meter over the span.
- The Analyzer cuts ideas at the nominal two bars and compares them by semitone intervals: a presentation's repetition is
  heard when it repeats the basic idea's intervals, not as a diatonic sequence. Fragmentation is judged on the nominal
  ideas, so it is rarely found, and the cadential ending is not identified.
- The Analyzer reads every written lane and identifies the chords from the notes, so a theme that was planned as a period
  or a presentation may be heard as a hybrid or not at all: the shares are in [grow.md](grow.md) and [piece.md](piece.md).

## Fixtures

`tests/test_form_hear.c` hears three hand-written Realized themes and their key regions: `sentence_c_major.json` is one region, C major from 0, and `sentence_c_to_g.json` is C major from 0 and G major from bar 4.

| Fixture | Written | Heard |
| --- | --- | --- |
| `sentence_c_major.json` | 8 bars, 4/4: presentation I V6/5 I V6/5, continuation IV ii V I, PAC | sentence, bars 0 to 7, presentation + continuation, PAC |
| `period_a_minor.json` | 8 bars, 4/4: antecedent i V6 i V (HC), consequent i6 V6 iv V i (PAC) | one phrase, a hybrid: compound basic idea + consequent, a subverted cadence at bar 5 and the PAC of bar 8 |
| `sentence_c_to_g.json` | 8 bars, 4/4: the C major sentence with its continuation moved a fifth up, in G major from bar 5: presentation in C, then IV (the pivot) ii V I in G | sentence, bars 0 to 7, presentation + continuation, PAC (arriving at bar 8) |

The presentation alone (bars 0 to 3) hears no theme, having no cadence.

The baselines heard on harmonic spans, measured for P41, are in [testing/span-hearing.md](testing/span-hearing.md).
