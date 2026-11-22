# dimension

[Docs index](../README.md): the Ursatz docs by topic.

Path `analysis/dimension/`, tests `tests/dimension/` of the analysis tests. Depends on `model`, `theory/vocabulary` (for `ContrastAspect`), `analysis/notes` and `analysis/similarity`. Enforced by the layer table. No application reads it before P67 B5, so `dimension.h` is not staged yet and each function has a row in `tools/ci/reach-allow.txt`.

## Provides

The dimensions of music a span of notes can be measured in, and how far two spans contrast (R-709, target architecture 5.3). Every value is an exact `Rational`.

- `contrast_aspect.h` (`theory/vocabulary`): `ContrastAspect` (rhythm, contour, register, density) with `contrast_aspect_name` and `contrast_aspect_from_name`. A contrast target, whether a formal function's or a plan constraint's, names an aspect by this name.
- `dimension.h`: a `DimensionSpan` is the notes of a span in onset order (`DimensionNote`: onset from the span's start, duration, MIDI pitch), the span's length and its beat, all in whole notes. `Dimension` names the measures, held in one table in `dimension.c` (`MEASURES`, name and function), so a new dimension is a row: `register` is the mean MIDI pitch, `density` the notes per beat. `dimension_measure` gives one, `dimension_name` the name.
- `dimension_contrast` gives a `ContrastVector`, the distance of two spans in each aspect: rhythm and contour are 1 minus the similarity score of the duration ratios and of the directions between successive pitches (a uniform augmentation does not contrast; two one-note spans have contour distance 0, a one-note span against a longer one 1), register and density the absolute difference of the measures.

```c
DimensionSpan a = {notes_a, count_a, rational_ratio(1, 1), rational_ratio(1, 4)};
ContrastVector far;
dimension_contrast(&a, &b, &far); /* far.distance[CONTRAST_ASPECT_RHYTHM] is 1/2 for q. e h against q. e q */
```

An empty span, a length, beat or duration that is not positive, or a NULL argument is refused (`DIMENSION_ERR_INVALID_INPUT`, `_NULL_ARGUMENT`); an overflow or a failed allocation is its own result. Brightness, harmonic tension, dynamics and intensity are later dimensions (P69, P70, P76). The test `dimension_measures` holds the values, `contrast_aspect` the names.
