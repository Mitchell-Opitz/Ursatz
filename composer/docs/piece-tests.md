# Pieces: tests and measured results

The tests of the piece compose run and what they measured. Back to [piece.md](piece.md).

## Tests and measured results

Four tests for each built-in form (`tests/test_form_templates.c`; the plans themselves are pinned by `test_form_plans_templates` and the documents checked by `test_form_spec`, [forms.md](forms.md#tests)):

| Test | What it checks | Label |
| --- | --- | --- |
| `structure` | The sections tile the form with the letters, bars, pairings and cadences of a table written in the test. | |
| `smoke` | The smallest size in C major 4/4 with an invented motive composes with one of seeds 1 to 5, and every section is heard. Under a second. | |
| `compose` | Seeds 1 to 5, C major 4/4 and A minor 3/4, with and without a given motive, every size (variations: 1, 2 and 5 variations, 16, 24 and 48 bars): the runs that complete and the sections heard are not below the baseline. | `slow` |
| `heard` | The same sweep: the sections heard as the form they ask for are not below the baseline. | `slow` |

CI runs `ctest -LE slow`. The slow sweeps run in the full `ctest` before a push. The sweeps are deterministic, so a
baseline is an exact measurement; a test fails when a count falls below it, passes when it holds or rises, and the
baseline is raised in `BASELINES` when a change lifts it. P44 B6 raised them for runs that no longer stick (R-418): a
half-cadence continuation or free phrase in a minor key gets chords that fill it, and a key change inside a section is
heard in its new key.

Baselines, with grow's measured shares for the same sections for comparison:

| Template | Runs complete | Heard as requested | Share | Expected from grow's shares |
| --- | --- | --- | --- | --- |
| binary | 30 of 40 | 20 of 80 | 25% | 26 |
| rounded binary | 40 of 40 | 58 of 120 | 48% | 58 |
| ternary | 39 of 40 | 51 of 120 | 43% | 13 |
| loop | 40 of 40 | 32 of 60 | 53% | 39 |
| layered | 60 of 60 | 74 of 180 | 41% | 117 |
| variations | 60 of 60 | 184 of 220 | 84% | 0 |

Binary 8 and 16 in A minor 3/4 with a given motive stay 0 of 5 (stuck at the structure stage:
B's inverted motive gets no structure, because the cadential liquidation fails in 36 of its draws, R-345; theme
development, left alone by R-405).

### Keys

`tests/test_piece_keys.c`: every template, C major and A minor 4/4, an invented motive, seeds 1 to 10, with `modulate`, a stuck seed tried again from the next ones up to 16 times as the piece entry does.

| Test | What it checks | Label |
| --- | --- | --- |
| `plan` | The key lane tiles the piece, every key is closely related, a section's KEY constraint is the key it ends in, the cadences pass `piece_check_cadences`, loop and layered keep one key. 64 of the 120 plans change key (8 of 10 for each of the four templates that can). | |
| `smoke` | Binary in C major: the first of seeds 1 to 5 composes, changes key and passes the checks. | |
| `<template>` | The composed runs: every key change opens with a valid pivot, every planned modulation is a modulation with an authentic cadence heard in the new key, the last cadence is the strongest. Prints the share that completes. | `slow` |

Every completed run passes every check, and every template completes 20 of 20 runs, with 16 that change key for the four
that can (binary, rounded binary, ternary, variations). Until P44 B6 five of the six tests were `WILL_FAIL`: a minor
half-cadence continuation got no chords (its cap was fewer than its cells needed), and a section that turns to a new key
halfway was heard whole in the old key, so no authentic cadence fell in the new one and the change was a tonicization
(R-418). The key check now also hears the part of a section after a key change from the change. Loop and layered have one
key.

Still open: rounded binary 16 with `modulate` rejects the return home at B's half (10/1) in 10 rounds over 20 runs (no run
ends stuck), and the 4-bar A' is heard with no cadence; recorded, not investigated.

## Test notes

Causes:

- Half-cadence sentences in a minor key no longer stick (P44 B6; `grow ... --cadence HC` grows for seeds 1 to 16 in A
  minor 3/4, A minor 4/4 and D minor 4/4, `test_grow_half_cadence`). What still sticks is theme development: a motive
  whose inversion gets no structure, and some 4-bar minor themes.
- Low shares come from what grow does not hear: 4-bar themes are never heard, and periods are never heard
  (see [grow.md](grow.md)).

## Smoke runs

A `smoke` run is the same program on one seed (and the first key and meter row), under 10 s, in the fast set: `test_grow_sentence_smoke`, `test_grow_accompaniment_smoke`, `test_counterpoint_species_3_smoke`, `test_piece_keys_tonic_major_smoke`, `test_form_parts_smoke`, `test_rondo_forms_<form>_smoke` (six forms) and `test_sonata_forms_<form>_smoke` (`sonatina` at seed 3, because seeds 1 and 2 do not compose in C major 4/4). A smoke skips only rate checks over a sweep. Nightly only: `test_api_rondo` and `test_api_sonata` (one fixed request each; the form family smokes and `test_api_forms` cover the path), `test_listen_cli_style_folder` (the Analyzer style path of `listen`; `test_listen_cli_smoke` covers the command), `consumer_build` and `consumer_rejects_internals` (a separate CMake project), the memcheck tests, and every slow test that already has a fast companion (the form template and piece key sweeps, the grow sweeps with `_fast` or `_smoke`, `test_grow_fidelity_sweep`).
