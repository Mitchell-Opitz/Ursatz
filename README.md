# Ursatz

Four parts in one repo, each in its own folder: `library/` (the music theory library), `analyzer/`, `composer/` and `gui/`. Each part builds on the ones before it in that order.

Build and test everything from the repo root:

```
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure --timeout 1800
```

One part's tests: `ctest --test-dir build/<part>`. The fast set skips tests labeled slow: add `-LE slow`. Needs cmake 3.20+, a C11 compiler, pkg-config and the SQLite3 dev files. The build type defaults to Debug; do not build Release, which compiles out the asserts the tests rely on.
