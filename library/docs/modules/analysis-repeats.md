# Analysis: repeats and returns

`standard_form_find_repeats` (`frameworks/standard-form/analysis/analysis_repeats.h`) finds written-out repeats and melodic returns over measures, and `standard_form_phrase_grid_follow_repeats` (`analysis_phrase_grid.h`) lets a phrase grid follow the repeats (R-400). Nothing in the chain calls either yet.

## Input

The notes of a piece, each with its span and its semitone (`StandardFormRepeatNote`, borrowed), and the `MeterMap`. A note belongs to the measure of its start. A measure's content is its notes as (onset minus the measure start, semitone, duration), sorted by onset, then semitone, then duration; its soprano is the highest semitone among the notes starting at each onset, in onset order. Times are compared exactly, as Rationals. A measure with no notes matches nothing. The caller chooses the notes: the whole texture finds repeats; for returns the soprano is read from the same notes.

## Repeat

A maximal run of at least `min_measures` (default 4, `StandardFormRepeatConfig`) measures whose content equals that of an earlier run, measure for measure: pairs (a, b), (a + 1, b + 1) and so on, where the run is not the continuation of a run at (a - 1, b - 1) and b - a is at least the run length (no overlap; a run that overlaps its source is refused whole). Earlier measures are scanned from the first, later ones after each; a later measure is in at most one repeat, the first found keeps it.

## Return

A maximal run of at least `min_measures` measure pairs whose sopranos have equal onsets and differ by one constant semitone count over the whole run (0 and multiples of 12 included), that is not the continuation of a run at (a - 1, b - 1), with b - a at least the run length. A run whose pairs are all exact content repeats is not a return (it is a repeat, or an overlap that is refused).

A return is dropped when a repeat implies it: its later span lies inside a repeat's later span and the same return exists from the repeat's source, or its earlier span lies inside a repeat's later span and the same return exists from the repeat's earlier span. "The same" is read pair by pair over the shifted measures: equal sopranos with the same semitone count. A source, its repeat and a return of the source after both list only the return from the first statement.

## Output

`StandardFormRepeats`: items (`StandardFormRepeat`: kind, earlier and later first and last measure as MeterMap numbers, transposition in semitones, later minus earlier, 0 for a repeat) ordered by later first measure, then earlier first measure; repeats and returns together. Release with `standard_form_repeats_destroy`.

## The grid follows repeats

For each repeat in order (returns are ignored), when the earlier span is tiled by whole units, the units covering the later span are replaced by copies of the earlier span's units moved by later_first - earlier_first. Tiled means: a unit starts on earlier_first (not elided) or starts on earlier_first - 1 and is elided, and a unit ends on earlier_last. The later span is replaced when the unit before it ends on later_first - 1 and a unit ends on later_last. A copy moves first, counted first, last and half measures; the elided flag and the lengths are kept. Otherwise the grid is unchanged. On an allocation failure the grid is untouched.

## Measured values

Measured with the rules prototyped on scratch copies of Ursatz `ceac466` over the framework-v1 MIDI (whole-texture repeats; returns over the chain's notes):

| Piece | Repeats | Returns |
| --- | --- | --- |
| Clementi 36 | none | 1-4 to 24-27 (-12); 8-12 to 31-35 (-7) |
| Mozart 545 | 1-28 to 29-56; 57-101 to 102-146 | 1-8 to 70-77 (+5) |
| Field 5 | none | 1-4 to 9-12 (0); 18-21 to 38-41 (0) |
| Field 1 | none | none |

Mozart's second development, on the grid: the units 102-103, 104-107, 108-115, 115-122e, 123-130, 131-146 become 101-108e, 108-115e, 115-122e, 123-130, 131-146; the first repeat's units are already equal.
