# Building Ursatz

## Requirements
- CMake 3.20+
- A C11 compiler (gcc, clang, MSVC, or MinGW)
- SQLite3, found through pkg-config (the only third-party dependency)
- valgrind (optional; enables the `*_valgrind` tests)

## Build
```sh
cmake -S . -B build
cmake --build build
```

## Test
```sh
ctest --test-dir build -C Debug --output-on-failure
```
(Drop `-C Debug` if you're not using a multi-config generator like Visual Studio.)

This runs every test from the root build: the kernel tests under `tests/` (220) and the
framework tests under `frameworks/*/tests/` (248), including one architecture-guard test per
module and per framework. CI runs the same three commands on Ubuntu.

The tests include `accuracy_harness`, which measures the chain against annotated chorales and the reference
fixtures and fails when an overall metric drops below `tests/accuracy/baseline.json`. See
[`docs/testing/accuracy.md`](testing/accuracy.md).

Tests are controlled by the `URSATZ_BUILD_TESTS` option. It defaults to `ON` when Ursatz is the
top-level project and `OFF` when another project adds Ursatz with `add_subdirectory`. Pass
`-DURSATZ_BUILD_TESTS=ON` or `OFF` to override.
