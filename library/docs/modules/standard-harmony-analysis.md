# standard-harmony analysis adapter (L3)

`frameworks/standard-harmony/analysis/` turns chord segments into standard-harmony's inputs and returns Interpretations: harmonic function, phrase model labels, six-four types and prolongations. It uses the existing rules (`harmony_function_of`, `harmony_phrase_model_label`, `harmony_six_four_classify`, `harmony_prolongation_of`, `harmony_applied_function`) and does not repeat them. It detects no cadences and segments no phrases (L3.3, L3.6).

## Input

standard-harmony does not include standard-tonality. `HarmonySegmentInput` (`analysis_input.h`) is the neutral per-segment record; the caller fills it from whatever produced the Roman numerals. `tests/integration/` chains standard-tonality's output into it; framework sources never do.

| Field | Meaning |
|---|---|
| `segment_id` | the segment's Sonority id; the target of every Interpretation |
| `span`, `position`, `beat_strength` | its `TimeSpan`, the `MetricalPosition` of its start, and that start's `BeatStrength` |
| `degree` | the chord's root, 1..7, in the key it is read in; for an applied chord, in the temporary key (5 for V/V, 7 for vii-diminished/x) |
| `intervals`, `interval_count` | the quality as a root-relative interval set (3 or 4 entries) |
| `inversion` | the index of the bass chord member in `intervals` |
| `applied_to` | 0, or the home-key degree the chord tonicizes |
| `bass_degree`, `soprano_degree` | scale degrees in the home key, or 0 when absent |
| `key_ref`, `mode` | the key's id and its mode (`mode` is needed by the rules and is not in the original field list) |
| `uncertainty` | [0, 1], carried into every Interpretation |

All pointers are borrowed. `harmony_analysis_prepare` reads the inputs into `HarmonySegment` records (the `HarmonyChord`, or the `HarmonyAppliedChord`). A segment that is not a chord the framework reads (a degree outside 1..7, an inversion beyond the chord, an unnamed interval set, an applied chord with a target that cannot be tonicized) passes through as not valid and gets no Interpretation.

Every Interpretation targets the segment's Sonority id (a prolongation's targets its Relationship), has FrameworkReference standard-harmony, a typed claim `Value`, and the scalar confidence `1 - uncertainty`.

## Function

One Interpretation per valid segment (`analysis_function.h`). A diatonic chord's function is `harmony_function_of`: I tonic, ii and IV strong and iii and vi weak predominants, V and vii dominant, the inversion not mattering; in minor VII is a weak predominant. An applied chord is dominant toward its target (`harmony_applied_function`, added in this branch because no rule gave one): the claim records DOMINANT, `applied` and `applied_to`.

## Six-fours

`analysis_six_four.h`. Every valid diatonic triad in second inversion is a 6/4 segment; consecutive segments of the same chord are one run, judged by the chord before and after it. The existing classifier decides the type; the adapter supplies its inputs:

- **Cadential**: a tonic 6/4 with the bass on degree 5, on a beat not weaker than the V that follows, which also has the bass on 5. Equal strengths count, so a 6/4 held over a whole measure that resolves to a V on the next downbeat is cadential (found on Mozart 545, measures 24-25). `harmony_analysis_prepare` sets the chord's `cadential` flag, so the function (dominant), phrase and six-four steps all read it. A 6/4 on a weaker beat than its V is not marked.
- **Passing**: stepwise bass in one direction between chords of the same function, on a weak beat (weaker than the chord before, or weak).
- **Neighbor**: static bass between the same root-position chord.
- **Arpeggiating**: the bass leaps to the fifth of the same chord with the upper voices sustaining it.

A 6/4 matching none is unclassified: it is marked `is_six_four` and has no Interpretation. The input has only bass and soprano degrees, so the bass steps are the degrees each placed in the octave nearest the one before; the alto and tenor are taken to hold the soprano's step ("the upper voices sustain" means the soprano sustains), and with no soprano they are taken to sustain. A 6/4 over a missing bass degree is unclassified.

## Prolongations

`analysis_prolongation.h`. A prolongation is a span from one tonic (or dominant) run to the next, whose interior chords are all explained: a passing, neighbor or arpeggiating 6/4; the middle chord of a tonic line `harmony_prolongation_of` recognises (I, V6 or an inverted V7 or the leading-tone chord, I or I6); or an applied chord whose target is the closing chord. Chains of tonic lines are consecutive spans. Dominant prolongation is what those rules explain, such as V I6/4 V; no further rule for it exists. The existing rules model spans only from one T or D to the next, so spans do not nest.

For each interior segment: a `Relationship`, predicate `prolongs`, source = the span's first segment, target = the interior segment, scope = a `HarmonyProlongationScope` (the function, the closing segment's id, the interior count), qualifiers = the reason; and an `Interpretation` targeting that Relationship's id.

## Phrase model

`analysis_phrase.h`. Phrases and cadences do not exist yet, so the caller supplies `HarmonyPhraseSpan`s: segment index range plus the `CadenceType` that ends the phrase (authentic, or half). Within a span the existing `harmony_phrase_model_label` labels the phrase's chords: Tb the opening tonic, PD the first strong predominant of the run before the dominant, D the cadential dominant, Te the final tonic (none after a half cadence). Consecutive segments of the same chord are one chord and share its label; invalid segments, applied chords and prolonging 6/4s (passing, neighbor, arpeggiating) are left out of the chord list and take no label. A phrase that does not end as its cadence says is unlabeled.

## Relocated enums

`HarmonicFunction`, `SeventhResolution`, `PhraseModelLabel`, `SixFourType` and `ModulationKind` were used only by standard-harmony and moved from `src/semantics/` into it, one commit each. `AppliedChordKind` (standard-tonality), `KeyChangeExtent` (common-practice-minimal), `CadenceType` and `BeatStrength` are shared and stay.

## Limits

- Phrases and cadences are supplied by the caller until L3.6 and L3.3.
- No modulation classification (`ModulationKind` is not used on real music; it needs cadences), and no strong/weak predominant reporting.
- A cadential 6/4 is recognised by its beat (not weaker than the V), its bass on 5 and the V after it; a 6/4 on a weaker beat than its V is read as another type or left unclassified.
- The alto and tenor are not in the input, so a 6/4 whose inner voices move is read from the soprano alone.
- Tonic lines are the framework's, so a tonic prolongation not of the listed kinds is not found, and dominant prolongation is limited as above.
