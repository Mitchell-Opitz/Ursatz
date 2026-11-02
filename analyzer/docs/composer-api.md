# Composer API

Library: `include/ursatz_analyzer/composer_api.h`, part of the `ursatz_analyzer` target. Calls are in process; there is
no server and no file. The header includes only public Analyzer headers and Ursatz kernel headers.

The Composer hands the Analyzer a fragment it generated and gets back the analysis as an `AnalyzerResult`
([analysis-result.md](analysis-result.md)), the rules the fragment breaks, and its distance from a style.

Two sets of entries exist until the Composer switches (P25). The hearing entry, the style reference and the distance
from a result are the current API. `analyzer_analyze_fragment`, `analyzer_fragment_style_distance` and
`analyzer_fragment_style_distance_without` are deprecated wrappers over them with equal output; `AnalyzerFragmentAnalysis`
gains a `result` member and its `report` member is no longer filled (read the report through
`analyzer_fragment_report`).

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
per event, the streams). `meter` `NULL` is 4/4; the key and `streams` are as for a fragment.

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
as `analyzer_fragment_style_distance` does: categories the result has no data for stay out of the means and are listed
with `in_a` false. A result that did not run every chain stage gives `ANALYZER_ERR_ARGUMENT`.

## Analyze a fragment (deprecated)

Deprecated: use `analyzer_hear`. This entry still copies the events into the `AnalyzerFragmentAnalysis`, so a caller may
free them after it returns.

```c
AnalyzerFragment fragment = {events, count, meter, has_key, tonic_pitch_class, key_minor, streams};
AnalyzerFragmentAnalysis analysis;
if (analyzer_analyze_fragment(&fragment, &analysis) == ANALYZER_FRAGMENT_OK) {
    const AnalysisReport *report = analyzer_fragment_report(&analysis);
    const char *document = analyzer_fragment_document_json(&analysis);
    /* ... */
    analyzer_fragment_analysis_destroy(&analysis);
}
```

| Field | Meaning |
| --- | --- |
| `events`, `count` | The fragment's `NoteEvent`s. The caller keeps them and they are not changed. |
| `meter` | The fragment's `MeterMap`, copied. `NULL` is 4/4. |
| `has_key`, `key_tonic_pitch_class`, `key_minor` | A key signature event at time 0 (tonic 0 to 11, C is 0). |
| `streams` | One stream (voice line or part) per event; `NULL` puts every event in stream 0. |

Every event's pitch must be a MIDI note number made the way the MIDI importer makes it (a pitch identity of the
`midi-note-number` tuning); anything else gives `ANALYZER_FRAGMENT_ERR_PITCH`. Ursatz cannot decode a pitch identity,
so the Analyzer finds the note number whose identity equals the event's. The events are copied into an
`ImportedNotes` (the pipeline reads a copy, never the caller's events) and `run_analysis_pipeline` runs on it, the same
pipeline the command line runs. A fragment with no events gives an empty report.

The document is the analysis document with the input path `<fragment>`. For events taken from a MIDI file it is the
file's document, except that `ticks_per_quarter` is `null` (a fragment has no SMF header).

## Violations

`analyzer_fragment_violations` gives one entry per fault, in this order: voice-leading faults of the report (parallels,
then directs), unresolved sevenths, unresolved leading tones, then counterpoint violations when the check ran.

| `rule` | `hard` | Source |
| --- | --- | --- |
| `parallel-fifths`, `parallel-octaves` | true | the report's voice-leading violations |
| `direct-fifths`, `direct-octaves` | false | the library's `direct-perfects`, by the interval arrived at (5 is fifths; 8 or 1 is octaves) |
| `unresolved-seventh` | false | `seventh-resolves-down` |
| `unresolved-leading-tone` | false | `leading-tone-resolves` |
| `counterpoint:<group>` | the library's | `analyzer_fragment_check_counterpoint` |

`measure` and `beat` place the fault where it arrives (the second chord of the move); a beat is 1 for the first beat and
a fraction between beats. A counterpoint violation is placed at the start of the exercise, because the library's species
check does not say where a rule broke. `note_ids` index the fragment's events. Free the array with
`analyzer_violations_free`.

`analyzer_fragment_check_counterpoint(&analysis, species, cantus_upper)` runs the species check (1 to 5) on a fragment
of two voices. It returns `ANALYZER_FRAGMENT_ERR_ARGUMENT` for any other fragment.

## Style distance (deprecated entries)

Deprecated: use the style reference above.

`analyzer_fragment_style_distance(&analysis, model, &comparison)` counts the fragment's document into a one-piece
profile and compares it with the model ([style-profile.md](style-profile.md)). A fragment is short, so it has no data
for some categories. Those stay out of the dimension and overall means, and stay in `comparison.categories` with
`in_a` false and `in_b` true; a caller lists them as missing. Categories the fragment has and the model lacks are
distance 1, as in any comparison. Free the result with `style_comparison_destroy`.

`analyzer_fragment_style_distance_without(&analysis, model, categories, count, &comparison)` is the same call with the
named categories dropped from the fragment's statistics and from the model's before the comparisons, so they appear
nowhere in the result (not in the means, not in `comparison.categories`). Use it when the model was read without
categories (`style_model_file_read_without`) and pass that same list. With plain `analyzer_fragment_style_distance`, a
category the fragment has and the model lacks, such as the four motive categories in a model read without them, counts
as distance 1 and pulls every mean up. An unknown category name, or `count` above 0 with NULL `categories`, is
`ANALYZER_FRAGMENT_ERR_ARGUMENT`. `analyzer_fragment_style_distance` is this call with none.

## Style labels

A caller that labels its own material to meet a style's counts uses the label builders of `style_labels.h`, the ones the
profile uses ([style-profile.md](style-profile.md#label-builders)): `style_label_tempo`, `style_label_density`,
`style_label_degree`, `style_label_join`, `style_label_motive_intervals` and `style_label_nct_figure`. A caller frees
what `style_label_join` and `style_label_motive_intervals` return.

## Tests

`tests/test_composer_api.c`: Bach 114 imported and passed as a fragment gives the same chords, cadences, phrases and
document as the file; a four-voice fragment with a parallel fifth gives one `parallel-fifths` at its measure and beat;
Mozart 545 is closer to the `sonatinas` model than to the `chorales`; the distance without the four motive categories is the same against a full model as against the model read without them, and the plain call against the latter still lists them as missing; the caller's events are unchanged. The
`test_composer_api_valgrind` run uses `tests/valgrind.supp`, which silences a few bytes that Ursatz's melody and motive
code leaks on every analysis.

`tests/test_hearing.c`: on the five reference pieces, a hearing without `render_document` has no document and with it
has the document the fragment entry renders; the result's notes point at the caller's events; the claim counts equal
those of `analyzer_analyze`; the profile of a result equals the profile of its rendered document
(`style_profile_add_result`); the distance against a reference equals `analyzer_fragment_style_distance` on the same
fragment and model; a cancel callback that refuses stops the hearing; bad arguments are refused. `test_hearing_smoke`
runs Bach 114 only; `test_hearing_valgrind` runs the smoke test under valgrind.

## Switching the Composer (P25)

Replace `analyzer_analyze_fragment` with `analyzer_hear` (pass the MIDI numbers the Composer holds; keep the events alive
for the result), `analyzer_fragment_report` with the claim accessors of `analysis_result.h`, and the two distance calls
with one `AnalyzerStyleReference` per style and `analyzer_result_style_distance`. The deprecated entries stay until
every consumer has switched.
