# Motive analysis (standard-form, L3.7)

Adapter in `frameworks/standard-form/analysis/` (`analysis_motive*.c`). Entry point: `standard_form_find_motives(input, phrases, segmentation, themes, config, matcher, matcher_context, out)`; release with `standard_form_motives_destroy`.

## Candidate discovery

- Runs over the soprano; the bass too when `config.include_bass` is set.
- Candidates are windows of `motive_min_notes` (default 3) to `motive_max_notes` (default 8) notes.
- A window starts on the first note of an idea or on a beat of the MeterMap. It may not cross a rest or gap and needs positive inter-onset intervals.
- Windows are grouped by key: dimension, interval steps, inter-onset intervals. Occurrences are chosen greedily without overlap; a group needs at least 2 occurrences.
- Maximality: a group is suppressed when every occurrence lies inside an occurrence of a longer kept group. The prototype is the first occurrence.

## Dimensions

- Generic: scale-step intervals (7 per octave) from the note degrees. Used when all degrees are known.
- Semitone: semitone intervals otherwise.
- Rhythm: exact onset-to-onset rationals.

An occurrence with the same semitone intervals is `REAL`; same generic intervals with different semitones is `TONAL`; identical pitch is `EXACT`.

## Transformations

| Kind | Rule |
|---|---|
| Exact | same pitches, same rhythm |
| Real / Tonal | transposition, same rhythm |
| Inverted | intervals negated |
| Retrograde | order reversed, intervals reversed and negated |
| Augmented / Diminished | rhythm ×2 / ×½, same intervals |
| Fragmented | prefix or suffix of the prototype; continuation spans (L3.6) are searched first |
| Varied | up to `varied_max_mismatch` interval or rhythm mismatches |

Precedence: exact > transposed > inverted/retrograde > augmented/diminished > fragmented > varied. A window is assigned its highest-precedence kind. Longer families win conflicts; a final longest-first pass drops occurrences held by an equal or longer family.

## Confidence

`score = (1 - mismatches / intervals) × rhythm factor`; interpretation uncertainty = `1 - score`.

## Kernel reuse

Exact and transposed matching uses kernel Motif Window Matching v1 (contour direction + exact rhythm), injected through the `StandardFormWindowMatcher` callback. Frameworks do not include kernel analysis; `tests/integration/form_kernel_bridge.h` supplies the bridge.

## Output

- One kernel `Motif` per family (`standard-form-motive-<line>-<k>`).
- One `MotifOccurrence` per occurrence, with an Interpretation (FrameworkReference standard-form, confidence = 1 − uncertainty).
- Relationship `occurrence-of` (occurrence → family) and `contained-in` (occurrence → containing phrase/idea).

## Limits

- Outer lines only; no inner-voice or cross-voice motives.
- Fixed scaling factors 2 and ½.
- Fragments: prefix and suffix only.
- Retrograde inversion, displacement and embellishment are not matched.
- Starts only at idea starts or beats.
- Development chains (A4.6) and similarity models (L5.5) are out of scope.
