# Building Ursatz

## Requirements
- CMake 3.20+
- A C11 compiler (MSVC, MinGW, or clang)

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