# containers

Layer L4 (`music-containers`). Path `src/containers/`, tests `tests/containers/`. Depends on `identity`, `core`, `time`, `pitch`, and `events` only. Must not depend on anything L5+.

## Provides

`Container` (a pure grouping base category, no fields of its own) and `Voice` (Container), the only Phase-1 concrete container type.

## Notes

- `Voice`'s schema is `{id}` only (Domain Spec §10). It has no membership field; no owned/mutable list of notes or events. The mechanism by which a Voice's membership is represented is deferred to a later Container/Structure spec that does not exist yet.
- A `Voice` must not be assumed to map one to one to an Instrument, Staff, or Part. A `Voice` may appear on multiple staves; a Staff may contain multiple voices. These are semantic notes only, since `Staff` does not exist yet.
- `Voice` has no mutator. A revision is a new `Voice` construction, not an in-place field change.
- `Voice`'s identity equality (`voice_identity_equals`) compares by `EntityID` only, the same pattern `Entity` uses, reduced to the one field `Voice` actually has.
- This is the layer where `VoiceReference` (built at L1 in `core`) gets a real resolution target. Nothing in `containers` redefines or duplicates what `VoiceReference` already does.

## Deferred

`Layer`, `Staff`, `Part`, `Measure` (Registry Phase 2); `Phrase` (container use), `Section`, `Movement`, `Work`, `Edition`, `Arrangement`, `Revision`, `SourceVariant` (Registry Phase 5, several marked "Reference Concept," non-normative).

Out of scope entirely, not missing. Resolution logic that takes a `VoiceReference` and looks up the actual `Voice` (a repository/registry pattern, an application/query-layer L9 concern), and any `Voice`-`Instrument`, `Voice`-`Staff`, or `Voice`-`Part` relationship (deferred to `relationship`, L6, or later container work).

## Dependencies

Depends on `identity` (L0), `core` (L1), `time`/`pitch` (L2), and `events` (L3) only.
