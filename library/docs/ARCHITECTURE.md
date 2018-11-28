# Architecture

Ursatz is organized into layered modules, each living under `src/<module>/`
with its tests mirrored under `tests/<module>/`.

## Layers

- **L0** — no dependencies on any other module. Currently: `identity`.
- **L1+** — may depend only on lower layers, never sideways or upward.

A module must not import anything outside its own layer's allowed
dependencies. Each module has an architecture test enforcing this
(e.g. `tests/identity/test_architecture_identity.c`).

## Modules

| Module     | Layer | Path             | Tests                |
|------------|-------|------------------|-----------------------|
| identity   | L0    | `src/identity/`  | `tests/identity/`     |