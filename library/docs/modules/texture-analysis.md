# Texture analysis (standard-texture, L3.9)

Adapter in `frameworks/standard-texture/analysis/` (`texture_analysis_*.c`). It measures texture features per measure from separated voices, classifies each measure with `standard-texture`'s rules, and merges the measures into passages with change points. The adapter includes no other framework and no kernel analysis; the architecture test scans `analysis/` as well as the framework.

## Input

`StandardTextureInput` (`texture_analysis_input.h`) is built with `standard_texture_input_init`, `_add_note` and `_destroy`. Ids are copied; the `TimeSpan`s and the `MeterMap` are borrowed and must outlive the input and every result.

| Field | Meaning |
|---|---|
| id, `TimeSpan` | the note's id and its onset to release |
| pitch | a semitone number |
| track | the importer track the note comes from (a piano file's hand) |
| voice | the voice index the separation gave it inside the track (0 the top); a voice is the pair (track, voice) |
| voice_confidence | the separation's confidence, 0 to 1 |

`tests/integration/texture_util.h` builds it from the rule-based voice separation (L2.7), run per importer track, and the import.

### Config and snap

`StandardTextureConfig` holds the snap and the thresholds.

- Snap: `onset_grid_denominator` (32), `onset_tolerance_denominator` (256), `release_tolerance_denominator` (64). An onset within 1/256 of a whole note of a multiple of 1/32 is read as that multiple, a release within 1/64 is. Near-simultaneous attacks of a performed file are then simultaneous. The release tolerance may not be wider than half a grid step.
- Thresholds, below.

## Windows and passages

- Window: one measure, hop of one measure. A pickup is its own short window. Windows run from the first measure with a note to the last.
- A window is classified from its features to a single texture (or none).
- A window with no texture (no note sounding, or features between the thresholds) takes the texture of the window before it; leading ones take the first window's.
- A passage is a maximal run of windows with the same texture. A change point is the first measure of a passage after the first; it falls on a barline because windows are measures.
- A piece with no window that has a texture has no passage.

## Features

Each feature is its own function in `texture_analysis_features.h`, over the events that sound in the window. An event attacks in the window when its onset is in it; an attack time is a distinct onset time in it. Window length in beats is (end − start) / beat; the beat is the time signature's unit, or three units in a compound meter.

| Feature | Formula |
|---|---|
| voice counts | distinct voices sounding at an instant: the maximum over the window, and the mean weighted by duration over the whole window (a rest counts 0) |
| synchrony | \|{t : two or more voices attack at t}\| / \|attack times\|; 0 with no attack |
| independence | per pair of voices that both attack: 1 − \|A ∩ B\| / \|A ∪ B\| over their attack times; the mean and the minimum over the pairs |
| prominence | top voice = highest mean pitch of the sounding voices; ratio = top attacks per beat / mean of the other sounding voices' attacks per beat (a sustaining voice counts 0); prominence = max(ratio, 1/ratio), capped at 100; 0 with fewer than two voices |
| top gap | mean over the top voice's attacks of its pitch minus the highest pitch of the other sounding voices |
| register span, spacing | highest − lowest sounding pitch; the mean gap between adjacent voices (each at its highest sounding pitch) at each attack time and the window start |
| density | attack times per beat |
| pattern score | per voice with ≥ 3 attacks: the share of its (interval, inter-onset) pairs covered by the cell of 1 to 4 pairs, ≤ 1 beat long, that repeats most (occurrences left to right, no overlap, at least 2); the mean and the maximum over voices |
| unison | the share of times with ≥ 2 voices sounding at which every voice has the same pitch class; unison when that is 1 and synchrony is 1 |
| voice confidence | the mean voice confidence of the attacking events (1 with none) |

## Classification

`standard_texture_classify` (`texture_classification_rules.h`) takes five yes-or-no features. standard-texture gives no numeric threshold, so the configuration turns the measurements into them. The defaults are library choices fixed before any piece was read.

| Rule feature | From the measurements |
|---|---|
| voice_count | voices_max |
| unison | the window's unison flag |
| rhythmic_unison | two or more voices and synchrony ≥ 0.75 |
| predominant_melody | not unison, two or more voices, prominence ≥ 1.5 and top gap ≥ 3 semitones |
| independent_lines | not unison and not predominant melody, a pair of attacking voices, mean independence ≥ 0.6, and synchrony < 0.5 |
| shared_melody | never (see Limits) |

An independent line must not be a predominant melody: a melody over an accompaniment in another rhythm is melody and accompaniment, not two independent lines. The single answer follows standard-texture's precedence: monophony, heterophony, polyphony, homorhythm, melody and accompaniment. Homophony is the category of the last two and is never the answer.

## Confidence

The margin m (0 to 1) is how far the deciding measurement is from its threshold, as a share of the room it has (the least of them when several decided it): 1 for monophony; (synchrony − 0.75) / 0.25 for homorhythm; for polyphony the least of the independence, synchrony and not-prominent margins; for melody and accompaniment the least of the prominence and gap margins. The texture confidence is 0.5 + 0.5 m (a window right on a threshold is a coin flip). The window's confidence is that times the mean voice confidence of its attacking notes, so an uncertain voice separation lowers it. A passage's confidence is the mean of its classified windows'; its uncertainty is 1 − confidence.

## Output

Per passage, a `Texture` (`interpretation/texture.h`) labeled with the texture's name whose Interpretation (FrameworkReference standard-texture, scalar confidence) targets the passage id `standard-texture-passage-<n>`. Its claim `standard-texture/passage` holds the time span (as numerators), first and last measure, texture, category, window counts, confidences, and the feature summary (voices, synchrony, independence, prominence, gap, register, spacing, density, pattern). Between consecutive passages, a Relationship `changes-to` with an Interpretation `standard-texture/change` (from and to texture, boundary measure, whether the category changes too). The change points come from `standard_texture_change_points`.

## Accompaniment subtypes

standard-texture defines no accompaniment patterns (Alberti, arpeggiated, block chords, oom-pah) as categories or subtypes, so none is classified and no category is added. The pattern score is measured per window and reported in the passage summary as evidence for later subtypes (Analyzer A5.3 waits for them).

## Limits

- Measure granularity: a texture change inside a measure is not seen, and a change point is always a barline.
- Everything depends on the voice separation. Overlapping notes of one hand can be split into voices that attack alternately, and a voice separated wrongly changes the voice count, independence and prominence.
- Piano tracks are read as hands: voices are separated per track.
- Heterophony is never found: the source gives no way to measure that voices are variants of one line.
- A window whose measurements fall between the thresholds has no texture and takes its neighbour's.
- The thresholds are library defaults, not learned; a piece can fall near them.
- Field 1 (`tests/fixtures/annotations/field1_texture.txt`) is a draft hand reading, unchecked against an edition.
