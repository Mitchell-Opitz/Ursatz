# identity

[Docs index](../README.md): the Ursatz docs by topic.

Layer L0. Path `src/identity/`, tests `tests/identity/`. No dependencies on any other module.

## Provides

`EntityID`, `Version`, `VersionModel`, `SchemaVersion`.

## Notes

- `EntityID` values are stable, opaque scalars. Nothing at this layer or above derives one from a position (array index, row number, measure number).
- No mutators exist anywhere in this module. Every value is constructed once and never modified in place.

## Dependencies

L0 has no lower layer to depend on. Every other module in the tree depends on `identity` directly or transitively.
