# Hearing the phrase form

`form_hear` (`<ursatz_composer/form_hear.h>`) hears the phrase-level form of a written plan the way
Ursatz's standard-form analysis adapter hears real music. It is how the Composer checks the form of
what it wrote with the framework's rules, never with rules of its own.

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

## What is read

Only the plan's written lanes over the span. Constraints and nodes are never read, so a cadence
constraint does not make a cadence: the notes and chords must.

| Lane | Read as |
| --- | --- |
| Melody, voice 0 | The soprano, rendered by `plan_render`. |
| Bass | The bass line, rendered by `plan_render`. |
| Meter | The MeterMap the adapter counts measures in. |
| Key | The scale degree and alteration of every note. |
| Harmony | One segment per chord, and the chords the cadences are heard over. |

A note belongs to the span when it starts inside it.

The cadences are what standard-harmony's cadence recognizer hears over the harmony lane chords. The
soprano of each chord is the melody note sounding at the chord's start, as a scale degree (none
over a rest), not the soprano a cadence constraint implies. Each cadence goes to the adapter with
`harmony_cadence_strength`'s rank and uncertainty 0; a plagal cadence directly after a PAC is
`post_cadential`. Then the adapter's three steps run with its default configuration and the bass
included: `standard_form_segment_phrases` (with the kernel's Phrase Boundary Detection as the
boundary detector, `src/form/form_boundary.c`), `standard_form_segment_ideas` and
`standard_form_identify_themes`.

## The result

`FormHeard` holds one `FormHeardTheme` per theme the adapter identified, in its order, and the
number of phrases it segmented. A theme has the form (`PhraseLevelForm`), its beginning and ending
(`PhraseBeginning`, `PhraseEnding`, -1 for none), the commonality of the pairing, its first bar and
length in bars (0-based from the start of the plan), its phrase count, the cadence types of its two
parts (-1 when a part has no cadence of its own, as a presentation), and whether it is archetypal
and has fragmentation. `form_hear_form_name` gives standard-form's own name for a form.

| Result | When |
| --- | --- |
| `FORM_HEAR_ERR_NOT_WRITTEN` | A melody, bass or harmony span over the range is Constrained. |
| `FORM_HEAR_ERR_METER` | The span is not in one meter span, does not start on a barline, or is not whole bars. |
| `FORM_HEAR_ERR_ANALYSIS` | No single key span covers the span, the chords do not tile it in whole strong beats, or an analysis step failed. |

## Limits

These are the limits of the input the adapter is given and of the adapter itself. They are written
here, not worked around.

- One key and one meter over the span, and at most 64 chords.
- standard-harmony hears a half cadence only at the END of a phrase on V: a root-position V that
  the next chord resolves as a cadence's dominant is that cadence's. An antecedent's V followed by a
  root-position tonic is heard as an authentic cadence into the consequent, and a V reached from a
  predominant followed by I6 as a subverted cadence; neither ends the antecedent. The section
  post-pass that fixes this (`harmony_analysis_add_boundary_cadences`) needs section starts, which a
  plan does not hand the adapter. The period fixture reaches its V from the tonic and starts the
  consequent on i6.
- An inverted dominant is never a cadence, so V6/5 inside a presentation keeps it open.
- The adapter cuts ideas at the nominal two bars and compares them by semitone intervals: a
  presentation's repetition is heard when it repeats the basic idea's intervals, not as a diatonic
  sequence. Fragmentation is judged on the nominal ideas, so it is rarely found, and the cadential
  ending is not identified (standard-form's own limits, `analysis_theme.h`).

## Fixtures

`tests/test_form_hear.c` hears two hand-written Realized themes. Both are heard as written.

| Fixture | Written | Heard |
| --- | --- | --- |
| `sentence_c_major.json` | 8 bars, 4/4: presentation I V6/5 I V6/5, continuation IV ii V I, PAC | sentence, bars 0 to 7, presentation + continuation, PAC |
| `period_a_minor.json` | 8 bars, 4/4: antecedent i V6 i V (HC), consequent i6 V6 iv V i (PAC) | period, bars 0 to 7, antecedent + consequent, HC then PAC |

The presentation alone (bars 0 to 3) hears no theme, having no cadence.
