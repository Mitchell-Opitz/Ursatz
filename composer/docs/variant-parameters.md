# Variant parameters

The parameters of the variant operations in the registry. Back to the [docs index](README.md).

## Parameters

The parameters are the `develop` command's options without the dashes. A row names its library relation, and its parameters are the relation's, in the relation's order: the name, the kind and the description of each come from `motive_relation_param_info` (`analysis/motive_relation.h`) when `variant_operation_param` is asked for it, so the list of an operation can not drift from the relation. The row keeps only what the relation does not say, one `VariantDrawn` for each parameter: the range a chain draws open values from and a value is checked against, and how the draw treats it (a count of the notes, a feature or chord index, a ratio drawn below 1, a MIDI pitch, a fragment position, never drawn). The ranges are the registry's composing choices, not the relation's bounds: `by` is drawn from -12 to 12 where the relation accepts any value that stays in MIDI range, and "never drawn" is the registry's `optional`, where the library's `optional` means that the default is accepted as it stands. The descriptions below are the Composer's; `--list-ops` prints the library's.

| Parameter | Kind | Range drawn | Meaning |
| --- | --- | --- | --- |
| `tonal` | bool | 0 or 1 | Move or mirror in steps of the key's scale instead of semitones. Drawn as 0 without a key. |
| `by` | int | -12 to 12, not 0 | How far to transpose: semitones, or steps when tonal. |
| `degree` | int | 1 to 7 | Transpose so the first note sits on this degree, the nearest way, tonal. Optional: never drawn. |
| `axis` | pitch | one of the motive's pitches | The pitch mirrored about. The first note when not set. |
| `interval` | int | 0 to the note count less 1 | The 1-based interval changed; 0 for every one. |
| `amount` | int | -6 to 6, not 0 | Semitones to widen by, negative to narrow. |
| `keep-contour` | bool | 0 or 1 | Refuse a result that changes a direction. |
| `position` | position | leading, trailing, middle | Where a fragment is taken from. |
| `length` | int | 1 to the note count less 1 | How many notes a fragment has. |
| `drop` | int | 1 to the note count less 1 | How many notes to drop, the weakest first. |
| `keep-rhythm` | bool | 0 or 1 | A dropped note's time stays as a rest. |
| `min` | int | 1 to 16 | The fewest notes a fragment or liquidation may have, 2 when not set. Optional: never drawn. |
| `ratio` | ratio | 2:1, 3:1, 3:2 (augment); 1:2, 1:3, 2:3 (diminish); 1:2, 1:3, 2:3 (condense) | The factor durations are multiplied by. |
| `note` | int | 0 to the note count less 1 | The 0-based note `note-repeat` splits. |
| `count` | int | 1 to 3 (`add-upbeat`), 2 to 4 (`note-repeat`) | How many equal notes a note becomes; how many upbeat notes lead in. |
| `first`, `last`, `target` | int | 0 to the note count less 1 | 0-based notes: the first note whose durations `rhythm-repeat` copies and the note they start on; the first and last note `condense` compresses. |
| `offset` | ratio | 1:2, 1:3, 2:3, 1:4, 3:4 | How far later, in whole notes: `rhythm-shift` in the bar, `shift-feature` for the feature. |
| `approach` | int | -1 or 1 | The upbeat steps in from below (1) or above (-1). |
| `num`, `den` | int | 1 to 16 | The new meter of `change-meter`; `den` must be 1, 2, 4, 8 or 16. |
| `steps` | int | -7 to 7, not 0 | Scale steps: from the note before the interval for `add-interval`; each repeat of `repeat-feature` is moved by it. |
| `kind` | int | 0 or 1 | `ancillary-fill`: 0 passing notes, 1 a neighbor note. |
| `feature` | int | 0 to the feature count less 1 | The 0-based labelled feature. |
| `times` | int | 1 or 2 | How many times `repeat-feature` repeats the feature. |
| `chord` | int | 0 to the chord count less 1 (the one implied chord when the motive has none of its own) | The 0-based chord a harmony row changes: inverted, inserted after, replaced, or the first of the two a passing chord goes between. |
| `inversion` | int | 0 to 3 | `harmony-inversion`: root position, first, second or third inversion (third needs a seventh chord). |
| `degree` | int | 1 to 7 | The scale degree of the chord a harmony row adds or puts in place. |
| `seventh` | bool | 0 or 1 | `harmony-append`: the added chord is a seventh chord. |
| `then` | int | 1 to 7 | `harmony-substitute`: a second chord that follows the first in the same time. Optional: never drawn. |
