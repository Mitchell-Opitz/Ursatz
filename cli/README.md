# ursatz

The one program for Claude sessions and scripts. `ursatz <command>` runs the Composer's commands today (progression, motive, develop, variants, recognize, random-motive, counterpoint, grow, theme, piece, plan, fill, lock, regenerate, variations, harmonize, export, listen); the Analyzer's modes and the data folder commands join it as the one command line replaces the old ones. It is the part `cli/`: a thin program over the [workshop](../workshop/docs/README.md) from P64 B4 on. Back to the [README](../README.md).

## Build and run

```sh
cmake -S . -B build
cmake --build build -j
build/cli/ursatz --help
```

The program is `build/cli/ursatz`. Its pages are in [docs/README.md](docs/README.md).
