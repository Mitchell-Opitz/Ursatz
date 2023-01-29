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

This runs the kernel tests under `tests/` (220 as of PR #97), including one architecture-guard
test per module. CI runs the same three commands on Ubuntu.

## Framework tests

Each `frameworks/<name>/CMakeLists.txt` adds its `tests/` directory only when that framework
is the top-level CMake project. From the root build, framework libraries compile but their
tests (248 as of PR #97) do not run, and CI does not run them either. This is a known gap to
fix. Until then, run them through a wrapper project:

```cmake
cmake_minimum_required(VERSION 3.20)
project(fwtests C)
enable_testing()
add_subdirectory(/path/to/Ursatz ${CMAKE_BINARY_DIR}/ursatz)
file(GLOB FWS LIST_DIRECTORIES true /path/to/Ursatz/frameworks/*)
foreach(fw ${FWS})
  if(IS_DIRECTORY ${fw}/tests)
    get_filename_component(n ${fw} NAME)
    add_subdirectory(${fw}/tests ${CMAKE_BINARY_DIR}/fwtests/${n})
  endif()
endforeach()
```

As of PR #97, 247 of 248 pass. `architecture_common_practice_minimal` fails because its
allowlist omits the module's own seventh-chord headers.
