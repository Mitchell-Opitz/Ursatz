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

## Cadences

`analysis_cadence.h`. Cadences are found from harmony and meter alone, since no phrases exist yet (L3.6). The classification is the existing rules' (`harmony_cadence_classify`, `harmony_deceptive_of`, `harmony_plagal_progression_is`, `harmony_function_of`); the adapter chooses candidates and supplies what they need. `harmony_analysis_cadence_points` returns the candidates; `harmony_analysis_find_cadences` classifies them.

**Cadence points.** Chords are runs of consecutive segments making the same chord. A candidate is a diatonic chord, the arrival, directly after another diatonic chord, where the arrival starts on a beat at least as strong as `min_arrival_strength` (default MEDIUM) and lasts at least `arrival_min_beats` beats (default 1; a beat is the segment's optional `beat_length`, or `beat_length` in the config, default a quarter note), or the soprano rests right after it (`soprano_rest_counts`, default true).

**Types.**
- **PAC**: root-position V or V7 to root-position I with the soprano on 1 at the arrival.
- **IAC**: the same with the soprano on any other degree (3 or 5 usually; the existing rule calls every other degree imperfect).
- **AUTHENTIC_UNSPECIFIED**: the same with the soprano absent at the arrival. It is the only case left unclassified, and its uncertainty carries `soprano_absent_penalty` (default 0.3). PAC and IAC are otherwise distinguished by the soprano line.
- **HALF**: arrival on a root-position V (V7 allowed by the framework's soft rule) after a tonic or a predominant; a cadential 6/4 directly before the V is part of the dominant and the chord before it counts. A 6/4 is never the chord before the V. A half cadence is the end of a phrase on V, so a V that the next cadence resolves (an authentic, deceptive or subverted cadence whose dominant is this V) is that cadence's dominant and no half cadence is reported for it.
- **DECEPTIVE**: root-position V(7) to vi (VI in minor) or IV6 (`harmony_deceptive_of`).
- **PLAGAL**: IV to I, only directly after a PAC, and then marked `post_cadential`. Elsewhere IV to I is a prolongation, as standard-harmony says, and is not a cadence here.
- **SUBVERTED**: a cadential dominant (V(7) after a predominant, or with a cadential 6/4) resolving to I6 or another chord that is not a tonic, the V again, a deceptive substitute or a 6/4. `SUBVERTED` was already in `CadenceType` and ranks 0 in `harmony_cadence_strength`; nothing was added to the enum.

An inverted V or an arrival on an inverted tonic is not a cadence (it prolongs, as L3.2 reads it), and viio to I is not an authentic cadence in the framework's rule. A candidate matching none of these gives no cadence.

**Output.** One `Cadence` (the L5 shell: an id and the segment ids from the dominant's first segment to the arrival's last) and one `Interpretation` targeting the Cadence's id per cadence: FrameworkReference standard-harmony, claim = a `HarmonyCadenceClaim` (type, soprano degree, post-cadential, six-four, the arrival and dominant segment ids, key ref), and a `TimeSpan` from the dominant's start to the arrival's end on the result. The soprano degree is the one of the arrival's first segment.

**Uncertainty** = 1 - (1 - u)(1 - penalty), u the largest input uncertainty of the chords involved (the dominant, a cadential 6/4 in it, the arrival), and penalty `soprano_absent_penalty` when the type is AUTHENTIC_UNSPECIFIED for want of a soprano, 0 otherwise.

## Relocated enums

`HarmonicFunction`, `SeventhResolution`, `PhraseModelLabel`, `SixFourType` and `ModulationKind` were used only by standard-harmony and moved from `src/semantics/` into it, one commit each. `AppliedChordKind` (standard-tonality), `KeyChangeExtent` (common-practice-minimal), `CadenceType` and `BeatStrength` are shared and stay.

## Limits

- Phrases are supplied by the caller until L3.6. Cadences are now found from harmony and meter alone, with no phrase context: a cadence is a candidate at a cadence point, not the end of a known phrase.
- The arrival thresholds (MEDIUM beat, one beat long, soprano rest) are stated defaults, not learned values. An arrival on a weak offbeat, an applied chord before a half cadence, and a dominant that is inverted are not found by design.
- No modulation classification (`ModulationKind` is not used on real music; cadence-confirmed modulation is later work), and no strong/weak predominant reporting.
- A cadential 6/4 is recognised by its beat (not weaker than the V), its bass on 5 and the V after it; a 6/4 on a weaker beat than its V is read as another type or left unclassified.
- The alto and tenor are not in the input, so a 6/4 whose inner voices move is read from the soprano alone.
- Tonic lines are the framework's, so a tonic prolongation not of the listed kinds is not found, and dominant prolongation is limited as above.
