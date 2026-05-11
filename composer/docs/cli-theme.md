# theme command

```sh
ursatz-composer theme <request.json> [--seed <n>] [--suggest <n>] [--check] [--out <file.json>] [--midi <file>] [--musicxml <file>] [--plain-midi] [--explain <file>] [--format text|json]
```

Grows a theme from the motives of a request ([theme-request.md](theme-request.md)): up to four motives, one for each slot
of the theme, a pool of saved forms of them and the brief. It is `grow` over the same library (`grow_build_plan` and
`grow_run`, see [grow.md](grow.md)): the options after the request file, the seeds, `--check`, the files and the output are
[grow's](cli-grow.md), and a request with one motive and no pool gives the plan `grow` gives for the same options and seed.

The tempo of the brief (the request's, else the style's) is written to the MIDI and MusicXML files, as for `grow`. The mode
`style` takes the style's most common mode.

## Output

The output of `grow`, with:

- text: a `brief:` line after the tempo, and after each theme's developments a `slots` table, one line for each idea:
  `<idea>: <slot> slot, motive <id>, <source>`, with the item and chain of a pool form and the connection in percent;
- JSON: a `brief` member, `{"tempo": {"qpm", "from"}, "mode": {"mode", "from"}, "density", "without"}` (`from` is
  `request`, `option` or `style`; `density` is the style's attacks per beat, or `null`), and in each theme a `slots` member,
  one entry for each idea: `{"idea", "slot", "motive", "source", "item", "chain", "connection"}`.

`slot` is the function slot of the idea and `motive` the id of the request motive it is made from. `source` is `motive`
(the motive itself, exactly), `pool` (a saved form, named in `item` with its `chain`), `registry` (a form of the variant
registry, with its chain) or `operator` (a motive development operator, with its label). `connection` is the form's
connection score ([grow.md](grow.md#motive-forms)) in percent, `null` for the basic idea and the operators.

`--explain` names a pool form as `pool <item>` with its chain ([explain.md](explain.md)).

## Exit status

| Status | Meaning |
| --- | --- |
| 0 | Success. |
| 1 | Usage, or a request that does not read: not JSON, another schema, an unknown slot, a motive in two slots, degrees that do not read, a `rejected` form. |
| 2 | The request cannot be satisfied: a slot motive that cannot be used, a slot with no idea in the form, no seed that works. |
| 3 | An output file could not be written. |
