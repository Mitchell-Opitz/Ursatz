# containers

Layer L4 (`music-containers`). Path `src/containers/`, tests `tests/containers/`. Depends on `identity`, `core`, `time`, `pitch`, and `events` only. Must not depend on anything L5+.

## Provides

`Container` (a pure grouping base category, no fields of its own).

`Voice` (Container), Phase-1's concrete container type. Schema `{id}` only.

`Layer` (Container): a notation-layer grouping. Schema `{id, grouping}`, where `grouping` is an opaque required string (Registry marks `Layer` non-normative, "mentioned, no dedicated schema" -- kept minimal rather than guessing a structure for it). Holds no member list; it is grouped by reference from `Staff`, not the other way around.

`Staff` (Container): groups `Voice`(s) and `Layer`(s) by reference. Schema `{id, voice_ids, layer_ids}`, both arrays optional (empty is valid).

`Part` (Container): an instrument/performer grouping. Groups `Staff`(s) by reference. Schema `{id, staff_ids}`, array optional.

`Measure` (Container): a metrical grouping. Schema `{id, meter_map_region_ref}` -- see Known Gaps below.

`Phrase` (Container -- L4 sense only, see Known Gaps below): a mid-level grouping of `Event` references. Schema `{id, event_ids}`, array optional.

`Idea` (Container): the smallest unit of a segmentation analysis, a short group of `Event` references. Schema `{id, event_ids}`, array optional. Not `Motif` (an abstract pattern); an `Idea` is a located group of events that may contain motives.

`Subphrase` (Container): a group of `Event` references between an `Idea` and a `Phrase`. Schema `{id, event_ids}`, array optional. Unlike `Phrase`, it need not end with a cadence. Its role (presentation, continuation) is a `SubphraseRole` assigned by an analysis, not a field here.

`Section` (Container): groups `Phrase` refs. Schema `{id, phrase_ids}`, array optional.

`Movement` (Container): a work subdivision, groups `Section` refs. Schema `{id, section_ids}`, array optional.

`Work` (Entity/Container): the top-level musical work, groups `Movement` refs. Schema `{entity, movement_ids}`, array optional. The eventual root object for anything exported (MIDI/MusicXML) or generated as a complete piece.

`Edition` (Entity): a distinct published version of a `Work`. Schema `{entity, work_id}`.

`Arrangement` (Entity): an adapted version of a `Work`. Schema `{entity, work_id}` -- structurally identical to `Edition`, kept as a distinct type because the two are semantically distinct per the Registry (published version vs. adaptation).

`Revision` (Entity): a version of a `Work` or `Edition`, using `Version` (L0, `identity/version.h`) for its versioning semantics. Schema `{entity, target_id, version}` -- see Known Gaps below.

`SourceVariant` (Value): a variant of a source. Schema `{source_id, descriptor}`, where `descriptor` is an opaque required string. Registry marks `SourceVariant` "shall eventually support" -- deferred normative -- see Known Gaps below.

## Notes

- `Voice`'s schema is `{id}` only (Domain Spec §10). It has no membership field; no owned/mutable list of notes or events. The mechanism by which a Voice's membership is represented is deferred to a later Container/Structure spec that does not exist yet.
- A `Voice` must not be assumed to map one to one to an Instrument, Staff, or Part. A `Voice` may appear on multiple staves; a Staff may contain multiple voices.
- `Voice` has no mutator. A revision is a new `Voice` construction, not an in-place field change. The same holds for every type in this module: none has a mutator, a revision is always a new construction.
- `Voice`'s identity equality (`voice_identity_equals`) compares by `EntityID` only, the same pattern `Entity` uses, reduced to the one field `Voice` actually has. `Layer`, `Staff`, `Part`, `Measure`, `Idea`, `Subphrase`, `Phrase`, `Section`, and `Movement` follow the same pattern: identity equality by `EntityID` only, with referenced ids excluded (they are content, not identity). `Work`, `Edition`, `Arrangement`, and `Revision` are Entity-category and delegate identity equality to `entity_identity_equals` (`EntityID` + `SchemaVersion`). `SourceVariant` is Value-category: equality is by content (`source_id` + `descriptor`), not by an assigned identifier.
- This is the layer where `VoiceReference` (built at L1 in `core`) gets a real resolution target. Nothing in `containers` redefines or duplicates what `VoiceReference` already does.
- Every reference field in this module (`voice_ids`, `layer_ids`, `staff_ids`, `event_ids`, `phrase_ids`, `section_ids`, `movement_ids`, `work_id`, `target_id`, `source_id`) is an `EntityID`, held by reference only -- never an embedded/resolved copy of the referenced object. Resolving a reference to its actual object is a registry/repository concern (L9), out of scope here, same treatment `VoiceReference` already gets.
- Where a reference array is optional (`Staff`, `Part`, `Idea`, `Subphrase`, `Phrase`, `Section`, `Movement`, `Work`), an empty array (count `0`, `NULL`) is valid, not an error -- the same "found nothing is not an error" treatment `generation`'s `CandidateSet` gives its own arrays. This is a deliberate difference from `structure`'s `Structure`, which requires a non-empty `event_ids`.

## Known Gaps

Flagged rather than silently resolved:

- **`Measure.meter_map_region_ref`**: `MeterMap` does not exist yet in this codebase. This field is kept as a bare opaque `EntityID` reference -- the same "opaque, resolved only once the real target type exists" treatment `VoiceReference`/`RelationshipReference` give references to types defined at a higher layer, here applied ahead of schedule to a same-layer dependency that simply is not built yet. Once `MeterMap` exists, this field should be revisited -- it may need to become a dedicated `MeterMapRegionReference` type rather than a bare `EntityID`.
- **`Phrase` naming**: this module's `Phrase` is the L4 Container-layer sense (a grouping object holding `Event` references directly). It is NOT `structure`'s L5 occurrence-based Phrase concept (referred to elsewhere as `PhraseOccurrence`, a situated instance of an abstract Phrase Structure). Same English word, two distinct concepts at two different layers, disambiguated in both modules' header comments -- not conflated or merged.
- **`Revision.target_id`**: nothing in this codebase yet distinguishes "this `EntityID` names a `Work`" from "this `EntityID` names an `Edition`" at the type level (no discriminated/tagged reference exists for that). `target_id` is therefore a bare opaque `EntityID` that may resolve to either, exactly as the Registry's own "Work/Edition" phrasing leaves it.
- **`SourceVariant`**: Registry marks this "shall eventually support" -- deferred normative, no concrete schema given. The `{source_id, descriptor}` shape here is a minimal structural guess at that eventual schema, not a resolution of it. `descriptor` is kept as an opaque string (matching `Layer.grouping`, `Articulation.kind`, `Dynamics.marking`) rather than a bare-EntityID placeholder, since it is content rather than a stand-in for an as-yet-unbuilt entity type (contrast `Measure.meter_map_region_ref` above).

## Dependencies

Depends on `identity` (L0), `core` (L1), `time`/`pitch` (L2), and `events` (L3) only. Every type in this module stays within this same ceiling -- none required extending the allowlist `tests/containers/test_architecture_containers.c` enforces; that test is directory-driven and needed no edits to cover them.