# Composer API

Library: `include/ursatz_analyzer/composer_api.h`, part of the `ursatz_analyzer` target. Calls are in process; there is
no server and no file. The header includes only public Analyzer headers and Ursatz kernel headers.

The Composer hands the Analyzer a fragment it generated and gets back the analysis as an `AnalyzerResult`
([analysis-result.md](analysis-result.md)), the rules the fragment breaks, and its distance from a style.

The hearing entry, the style reference and the distance from a result are the whole API. The fragment entries
(`analyzer_analyze_fragment`, `analyzer_fragment_*`, `AnalyzerFragmentAnalysis`) were removed in version 0.6.0. Counterpoint
is not checked by the Analyzer: use standard-counterpoint.

## Hear events

```c
AnalyzerHearing hearing = {events, count, midi, meter, has_key, tonic_pitch_class, key_minor, streams};
AnalyzerHearingOptions options = analyzer_hearing_options_default(); /* all stages, no document */
AnalyzerResult *result = NULL;
if (analyzer_hear(&hearing, &options, &result) == ANALYZER_OK) {
    /* read the claims: analyzer_result_claim_count / analyzer_result_claim */
    analyzer_result_destroy(result);
}
```

Everything in the hearing is borrowed and must outlive the result: no `NoteEvent` is copied, and the result's chain
notes point at the caller's events. Only arrays of the hearing's own are built (pointers to the events, a numeric pitch
per event, the streams). `meter` `NULL` is 4/4. `has_key` gives a key signature event at time 0 (tonic 0 to 11, C is 0). `streams` has one stream
(voice line or part) per event; `NULL` puts every event in stream 0.

`midi` holds one MIDI note number (0 to 127) per event. Ursatz has no call that reads a MIDI number from an event's
pitch (a pitch identity is opaque), so `midi` is required when `count` is above 0; `NULL` gives `ANALYZER_ERR_ARGUMENT`.
There is no 128-entry pitch table in this entry. The Composer already holds the number it generated each event from.

`options.analysis` is the `AnalyzerOptions` of `analyzer_analyze`, including `cancel` (asked before the chain run and
between Analyzer stages; `ANALYZER_ERR_CANCELLED`, nothing written). `options.render_document` renders the analysis
document (input path `<fragment>`) once into the result; read it with `analyzer_result_document_json`, which is `NULL`
when it was not asked for. A document needs every stage: `render_document` with fewer gives `ANALYZER_ERR_ARGUMENT`.

## Style reference

```c
AnalyzerStyleReference *reference = NULL;
analyzer_style_reference_create(model, without, without_count, &reference); /* once */
StyleComparison comparison;
analyzer_result_style_distance(result, reference, &comparison);             /* per result */
style_comparison_destroy(&comparison);
analyzer_style_reference_destroy(reference);
```

The reference copies the model's statistics, without the named categories (the list the model was read without), and
keeps them prepared; it serves any number of results and does not need the model afterwards. The distance counts the
result into a one-piece profile from the result's document view, with no JSON text rendered or parsed, and compares it
with the reference: categories the result has no data for stay out of the means and are listed
with `in_a` false. A result that did not run every chain stage gives `ANALYZER_ERR_ARGUMENT`.

## Violations

`analyzer_result_violations(result, &list, &count)` gives one entry per fault of a result from `analyzer_hear`, in this
order: voice-leading faults of the report (parallels,
then directs), unresolved sevenths, unresolved leading tones.

| `rule` | `hard` | Source |
| --- | --- | --- |
| `parallel-fifths`, `parallel-octaves` | true | the report's voice-leading violations |
| `direct-fifths`, `direct-octaves` | false | the library's `direct-perfects`, by the interval arrived at (5 is fifths; 8 or 1 is octaves) |
| `unresolved-seventh` | false | `seventh-resolves-down` |
| `unresolved-leading-tone` | false | `leading-tone-resolves` |

`measure` and `beat` place the fault where it arrives (the second chord of the move); a beat is 1 for the first beat and
a fraction between beats. `note_ids` index the events the result was heard from. Free the array with
`analyzer_violations_free`. It returns `ANALYZER_ERR_ARGUMENT` for a NULL argument.

Counterpoint is checked with standard-counterpoint, not here. `ANALYZER_STAGE_COUNTERPOINT` stays a reserved stage bit.

## Style labels

A caller that labels its own material to meet a style's counts uses the label builders of `style_labels.h`, the ones the
profile uses ([style-profile.md](style-profile.md#label-builders)): `style_label_tempo`, `style_label_density`,
`style_label_degree`, `style_label_join`, `style_label_motive_intervals` and `style_label_nct_figure`. A caller frees
what `style_label_join` and `style_label_motive_intervals` return.

## Tests

`tests/test_composer_api.c`: a four-voice hearing with a parallel fifth gives one `parallel-fifths` at its measure and
beat; an empty hearing has no chords and no violations; Mozart 545 is closer to the `sonatinas` model than to the
`chorales`; the distance without the four motive categories is the same against a full model as against the model read
without them, and a reference that drops nothing against the latter still lists them as missing; the caller's events are
unchanged. The `test_composer_api_valgrind` run uses `tests/valgrind.supp`, which silences a few bytes that Ursatz's
melody and motive code leaks on every analysis.

`tests/test_hearing.c`: on the five reference pieces, a hearing without `render_document` has no document and with it
has one; the result's notes point at the caller's events; the claim counts equal those of `analyzer_analyze`; the profile
of a result equals the profile of its rendered document (`style_profile_add_result`); the distance against a reference
equals that of a second hearing of the same events; the violations equal those of `analyzer_analyze` on the same notes;
a cancel callback that refuses stops the hearing; bad arguments are refused. `test_hearing_smoke` runs Bach 114 only;
`test_hearing_valgrind` runs the smoke test under valgrind.
