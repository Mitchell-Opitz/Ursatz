# Two-voice fixtures

The Composer's two-voice exports, read by `tests/analysis/test_two_voice_spans.c`: a bass line under a melody with no inner voices
(R-379, R-384).

Source: Mitchell-Opitz/Ursatz-Composer at commit `987e0a5`, a Debug build of its CLI.

| File | How it was made |
|---|---|
| `sentence-c-major.mid` | `ursatz-composer export tests/fixtures/sentence_c_major.json --midi sentence-c-major.mid` |
| `period-a-minor.mid` | `ursatz-composer export tests/fixtures/period_a_minor.json --midi period-a-minor.mid` |
| `fill.mid` | a copy of the Composer's `tests/snapshots/fill.mid` |

To regenerate, attach the Composer, run `git submodule update --init --recursive`, and build `ursatz-composer`.
