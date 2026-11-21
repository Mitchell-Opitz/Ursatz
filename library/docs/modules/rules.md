# rules

[Docs index](../README.md): the Ursatz docs by topic.

Path `theory/rules/`. One table for a group of rules, shared by every rule area (`standard-counterpoint`, `standard-harmony`, `galant-schemas`, `scale-harmonization`). Depends on `model` only.

## Provides

`RuleTable` (`rule_table.h`): the group's name, its rule names, a bool table of the soft rules (`SOFT`, indexed by the module's enum; every rule not in it is hard), and the count.

## Notes

- `rule_table_valid`, `rule_table_is_hard` and `rule_table_name` read the table; `rule_table_check` runs every rule on a subject through the module's `holds` adapter and reports the rules it breaks as a hard and a soft bit mask. A rule area asks whether a rule is soft with its own query (`standard_counterpoint_harmonic_rule_soft`, `standard_counterpoint_melodic_rule_soft`, `harmony_phrase_rule_soft`, `harmony_cadence_rule_soft`); the Composer counts and masks its soft rules with them (R-685).
- The Rule, Constraint and Preference objects, their evaluate and score callbacks, the opaque `Context` and `UrsatzOnce` were deleted (P66b B2, R-685): no application read them back, and each rule area answers `*_rule_holds` directly.
- Hard rules state what the subject must be. Soft rules ("limit", "avoid", "prefer") never make a subject invalid.
