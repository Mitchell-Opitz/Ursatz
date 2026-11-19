# Span hearing: the baselines heard on harmonic spans

Measured 2026-11-09 for P41, which turns the hearing onto spans (R-378, R-379). Nothing here is a test: the hearing still reads one chord per segment (`hearing_analyzer_options` has `harmonic_spans = false`). P61 B6 turned it onto spans: see the last section.

Pins: Ursatz `c06f802` (P40 B6a merge), Ursatz-Analyzer `7525814` (P40 B6b merge). Debug build, full suite, slow tests included.

Method: a scratch change set `options.analysis.harmonic_spans = true` in `hearing_analyzer_options` (`src/check/hearing.c`) and the full suite ran in a separate build directory. The change is not committed.

## Without spans (the committed state)

226 of 227 tests pass. `test_piece_keys_rounded-binary` completes 20 of 20 runs (19 of 20 before the pins), so its `WILL_FAIL` is removed (`docs/piece.md`). Compose rates, `test_middle_out` baselines and the loop seam tests hold.

## On spans

| Measure | On spans | P39 B6 on spans |
| --- | --- | --- |
| `test_middle_out` melody, completed | 10 of 10 | 10 of 10 |
| melody, sections heard / cadences heard as planned | 18 of 30 / 18 of 30 | 18 / 18 of 30 |
| progression, completed | 10 of 10 | 10 of 10 |
| progression, sections heard / cadences | 7 of 30 / 20 of 30 | 6 / 16 of 30 |
| bass, completed | 10 of 10 | 10 of 10 |
| bass, sections heard / cadences | 19 of 20 / 17 of 20 | 19 / 17 of 20 |
| `test_grow_compose_rates` and `_fast` | pass | |
| `test_loop_seam_rules`, `test_loop_seam_sweep` | pass | |
| tests failing | 17 of 227 | 19 of 227 |

`test_middle_out` baselines are 22 / 22 (melody), 5 / 19 (progression), 20 / 17 (bass), so melody and bass fall below their sections-heard baselines (18 of 22, 19 of 20).

## Failing tests on spans

- `test_grow_period_heard_compound_basic_idea_continuation`: not every grown 8-bar theme is heard as the form it was grown as (30 of 40 heard as requested, 8 no theme, 2 another form).
- `test_form_templates_layered_compose`: layered 30 of 60 runs complete (baseline 30), 88 sections heard (baseline 90).
- `test_plan_cli`, `test_explain_choices`, `test_explain_same`, `test_explain_cli`: a theme or piece cannot be grown or composed (D minor period, "no theme could be grown"; `compose_run` not `PIECE_OK`).
- `test_middle_out_melody`, `test_middle_out_bass`: sections heard below the baseline.
- `test_cli_snapshots`: a CLI snapshot differs.
- `test_analyzer_check_candidates`: `tried == 6 && count > 0` fails. `test_analyzer_check_planned`: period C major 0 of 10 complete with the check (baseline 10), rounded binary C major 8 of 10 (baseline 9).
- `test_style_distance`, `test_style_blend_harmony`: styled harmony distances above their recorded ceilings.
- `test_api_theme`, `test_api_piece`, `test_api_plan_workflow`: response JSON differs from the recorded fixture. `test_api_schema` is not run, since its fixture needs the three before it.

## P41: phrases first, the hearing stays off spans

Measured 2026-11-09 at B7. Pins: Ursatz `ceac466` (P41 B5a merge), Ursatz-Analyzer `b4b0cc3` (P41 B6 merge). Debug build, full suite, slow tests included.

Ursatz P41 finds phrases on a 4, 8 or 16 bar grid and claims a cadence only at a phrase point (R-389 to R-391). The hearing stays off spans (R-394 keeps R-378).

### Off spans (the committed state)

227 of 227 pass after the re-records of B7. Compose rates (`test_grow_compose_rates` and its fast companion), the loop seam tests and the grow-period-heard and analyzer-check tests pass unchanged.

| Measure | Off spans | Baseline |
| --- | --- | --- |
| `test_middle_out` melody, sections heard / cadences | 28 of 30 / 22 of 30 | 22 / 22 |
| progression, sections heard / cadences | 7 of 30 / 22 of 30 | 5 / 19 |
| bass, sections heard / cadences | 19 of 20 / 17 of 20 | 20 / 17 (re-recorded to 19 / 17) |

The bass falls from 20 to 19: seed 8's first section ends on a half cadence arriving in bar 7 of 8, which phrase first does not claim (R-390).

### On spans (scratch `harmonic_spans = true`, not committed)

| Measure | On spans | P40 on spans |
| --- | --- | --- |
| `test_middle_out` melody, sections heard / cadences | 26 of 30 / 18 of 30 | 18 / 18 of 30 |
| progression, sections heard / cadences | 6 of 30 / 21 of 30 | 7 / 20 of 30 |
| bass, sections heard / cadences | 19 of 20 / 17 of 20 | 19 / 17 of 20 |
| tests failing | 9 of 227 (`test_api_schema` not run) | 17 of 227 |

Failing on spans: `test_api_piece`, `test_api_theme` (response JSON differs), `test_grow_period_heard_compound_basic_idea_continuation` (8 bars: 32 of 40 heard as requested, 8 no theme; 4 bars: 0 of 40), `test_middle_out_melody` (cadences below the baseline 22), `test_cli_snapshots`, `test_explain_rejections`, `test_analyzer_check_planned` (sentence C major 9 of 10, rounded binary C major 8 of 10, baseline 9), `test_style_distance`, `test_style_blend_harmony`.

### Why the hearing stays off spans

The melody's cadences fall (22 to 18 of 30) and the grow-period-heard test fails. A bar-long arrival span's soprano is the highest note of its first beat, while the composed melody reaches the tonic later in the bar; the grid claims the cadence at the span, so the hearing misses it (R-394, R-395). The hearing moves onto spans when the arrival melody is measured (later phase).

## P61 B6: the Composer hears on spans (R-618)

Measured 2026-11-18 at P61 B6. The hearing is the library chain run by `analysis_chain_run_score` with its default configuration (harmonic spans on) and every stage, the settings the Analyzer uses; `hearing_analyzer_options` and `AnalyzerOptions.harmonic_spans` are gone. The refusal of R-394 is lifted: the Analyzer's own settings are the Composer's, so the two hear the same music the same way.

| Measure | Before | After |
| --- | --- | --- |
| `test_middle_out` melody, sections heard / cadences (of 20) | 19 / 18 | 17 / 16 |
| progression, sections heard / cadences (of 30) | 5 / 19 | 6 / 21 |
| bass, sections heard / cadences (of 20) | 19 / 17 | 19 / 17 |
| `test_grow_period_heard_compound_basic_idea_continuation`, 8 bars heard as requested | 40 of 40 | 32 of 40 (8 no theme); now WILL_FAIL |
| `test_grow_period_heard_antecedent_consequent`, 8 bars | 38 of 40 | 32 of 40 |
| `test_grow_sentence_heard`, all runs heard as a sentence | 19 of 80 | 16 of 80 |

`test_explain_rejections` found no run the analysis contradicts in 300 seeds of A minor 3/4; it now composes a period in G major 6/8 (seeds 1 to 10 give 7 runs with a contradiction). The snapshots `theme-fast-minor`, `piece-loop-and-ending` and `fill-given` moved in their `style_distance` (harmony and overall) and `heard`/`cadences` members only. The listen outputs keep their notes (`mid` and `xml`), and `json` moves once (`themes/01.json` heard as a sentence, `themes/02.json` no theme heard). The piece sweep moves four pieces' notes (`large-rondo-2`, `sonatina-1`, `sonatina-2`, `sonatina-tonic-major-1`): listen to them before and after (`notes.md`, For Michelle).

The slow tests that the fast set does not run were re-recorded in P61 B8: the form template shares heard as requested (minuet 30 to 29, trio 45 to 41, scherzo 28 to 26, figured-variations 126 to 123, large-rondo 116 to 110, sonatina-tonic-major 51 to 48), the ternary returns heard (9 to 8), the style distance ceilings (`test_style_distance`: styled harmony and melody 0.8826 and 0.7995 to chorales, 0.7087 and 0.7606 to sonatinas; unstyled 0.8398 and 0.8082, 0.6720 and 0.7687) and `test_style_blend_harmony` (0.8826 and 0.7135 at weight 0).
