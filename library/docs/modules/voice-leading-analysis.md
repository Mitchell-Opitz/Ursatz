# Voice leading analysis

Two framework adapters read a voice-pitch grid (one row per segment, one column per voice, voice 0 on top) and report voice-leading facts as durable claims. Neither framework includes the other; the chain (voice separation → grid → adapters) is built only in `tests/integration/`.

## Voice-pitch grid

Segment-major cells: `cells[segment * voice_count + voice]`. A cell has `present`, `pitch` (numeric), an optional spelling, the `note_id`, `attacked` and a `confidence` (the voice-assignment confidence from separation). A cell without a spelling gets one derived from the pitch (sharp-preferring, MIDI 60 = C4). `standard-counterpoint/analysis/analysis_grid.h` and `standard-harmony/analysis/analysis_voices.h` each declare their own grid type.

## standard-counterpoint (`frameworks/standard-counterpoint/analysis/`)

- **Motion** (`analysis_motion.h`): every voice pair at every consecutive segment pair where both voices sound is classified with `MotionType` (parallel, similar, contrary, oblique, none). Claim type `standard-counterpoint/motion`.
- **Parallel and direct perfects** (`analysis_perfects.h`), using the existing harmonic rules:
  - `parallel-fifths`, `parallel-octaves` (unisons count as octaves; compound intervals count): every voice pair.
  - `direct-perfects`: outer pair only, arrival at a perfect interval by similar motion with the top voice leaping. A pair already parallel is not also direct.
  - Claim type `standard-counterpoint/voice-leading-violation`.
- Each result is a `Structure` shell of the note ids involved plus an `Interpretation` targeting it.
- Uncertainty = 1 − the least cell confidence involved.

`MotionType` moved from `src/semantics/` into `frameworks/standard-counterpoint/motion_type.h`: only this framework uses it.

## standard-harmony (`frameworks/standard-harmony/analysis/`)

- Rules (`harmony_resolution_rules.h`): `SEVENTH_RESOLVES_DOWN` (the chord seventh moves down by step) and `LEADING_TONE_RESOLVES` (the leading tone moves up to tonic; an inner voice of a complete dominant seventh may drop to sol).
- `analysis_resolution.h` judges each voice at chord-run boundaries. Roles come from the pitch class relative to the chord root, found from the bass pitch and inversion. Semitone motion is converted to diatonic steps. Claim type `standard-harmony/resolution`; satisfied cases are emitted only with `emit_satisfied`.
- Uncertainty = 1 − c(1 − u), c the cell confidence and u the chord uncertainty.

## Tests

Unit tests per framework. `tests/fixtures/voice_leading/errors_satb.txt` is a synthetic SATB fixture with six planted errors (two parallel fifths, one parallel octave, one direct perfect, one unresolved seventh, one unresolved leading tone), asserted exactly. `tests/integration/test_voice_leading_report.c` runs a chorale fixture through true parts and L2.7 separated voices and reports every violation; zero parallels are asserted on the true parts unless hand-verified in `VERIFIED_PARALLELS`.

## Limits

- Results on separated voices are only as good as voice separation.
- No doubling, spacing or voice-crossing checks yet.
- No species check mode on real music.
