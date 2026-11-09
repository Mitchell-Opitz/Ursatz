# Span hearing: the baselines heard on harmonic spans

Measured 2026-11-09 for P41, which turns the hearing onto spans (R-378, R-379). Nothing here is a test: the hearing still reads one chord per segment (`hearing_analyzer_options` has `harmonic_spans = false`).

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
