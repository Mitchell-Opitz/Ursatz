# western-periods

Theory Framework instance `western-periods-v1`: the conventional
stylistic periods of Western classical music. Path
`frameworks/western-periods/`, tests in `frameworks/western-periods/tests/`.
A Framework extension module physically separate from `src/theory/`,
same shape as the other `frameworks/` modules.

## Scope

Six periods with their generally agreed years, built from
`HistoricalPeriod` (`src/time/historical_period.h`):

| Period | Years |
| --- | --- |
| Medieval | c. 600-1400 |
| Renaissance | c. 1400-1600 |
| Baroque | c. 1600-1750 |
| Classical | c. 1750-1820 |
| Romantic | c. 1800-1910 |
| Post-tonal | c. 1900-present (open-ended) |

Period boundaries are flexible, so neighbouring periods overlap and a
year can belong to more than one. `western_periods_containing_year`
returns every period that contains a year and never picks one.

Each rule has a backing `Rule`/`Constraint` pair referencing this
Framework. `constraint_evaluate` is never called here.

## Not covered

Sub-periods (early/high/late Baroque and so on), regional variants,
and assigning a work to a period -- that is a judgment about style, not
a lookup by year.
