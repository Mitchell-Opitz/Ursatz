# Section controls in the Pieces view

The controls a section has beside its fill (R-610, Composer API 1.19). They are in the body's section members and in the request; the
Composer reads and checks them. The frontend names no theory.

## Region

`region` is absent or null (as the form has it), `"home"`, `{tonic, material}` (a key) or `{relation, anchor}` (a relation of the
Composer's vocabulary `relations` to a section's key; `anchor` is `home` or a section's id or, in an overlay, its name). The view shows
"Key of the section": As the form has it, Home, A key or A relation. Each member is text of at most 64 characters. The save route
refuses any other shape; the Composer refuses a key or relation it does not know when it composes.

## Harmony

`harmony` is absent or null, `{progression, cells?}` (numerals as text, 1 to 8 cells for each chord), `{schema}` (a name from the
vocabulary's `schemas`) or `{chords: [{bar, text}]}` (1 to 16 chords, `bar` from 1 in the section). The harmony stage fills what the
harmony leaves of the section. A stored `fill` of kind `progression` is read as `harmony.progression` with an empty fill
(`workshop_material_body_migrate`); a section that has a harmony keeps it and loses the fill's numerals.

## Overlays

A sized form (rounded binary, ternary, variations and the others with sizes) has no sections of its own. Its body `sections` are
overlays: `{name, region?, harmony?, fill?}`, `name` a section of the size the piece picks (by `bars` and `count`, else the default),
each once. A fill is a motif with a motive of its own, a theme, or none. A theme fill names the copy of its plan by the section's name
(`theme-<name>.plan.json`). The request holds them as `sections` of a piece request with a form; a piece without overlays sends none.

## Result

The result's section table has a Region column (the key each section is in) and a Harmony column (given or found).
