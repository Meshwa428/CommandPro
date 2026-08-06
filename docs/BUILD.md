# Building & Running Synapse

## Prerequisites

- **C++20 compiler** — GCC or Clang
- **CMake ≥ 3.25**
- A generator — default (`Unix Makefiles`) is fine; Ninja works too

**Optional** (auto-detected; missing ones just disable a feature, build still succeeds):

| Dependency | Enables | Without it |
|---|---|---|
| `libX11` + `libXtst` + `libpng` | Real Linux automation (`syn_linux_platform.so`: mouse/keyboard/window/screenshot) | Automation runs against the mock backend only |
| GNU `readline` | REPL arrow-keys + persistent history | REPL falls back to plain `getline` |

Arch: `sudo pacman -S base-devel cmake libx11 libxtst libpng readline`
Debian/Ubuntu: `sudo apt install build-essential cmake libx11-dev libxtst-dev libpng-dev libreadline-dev`

## Build

Two CMake presets: `debug` and `release`.

```sh
cmake --preset release          # configure  → build/release/
cmake --build --preset release  # compile
```

Swap `release` → `debug` for an unoptimized build with assertions.

**Binaries produced:**

| Path | What |
|---|---|
| `build/release/syn` | The interpreter/compiler CLI |
| `build/release/tests/syn_tests` | Catch2 unit tests |
| `build/release/syn_linux_platform.so` | Real-automation plugin (only if X11/PNG found) |

> The plugin is a separate `.so` on purpose — it keeps X11/libpng's ~12
> transitive shared libraries out of the main `syn` binary.

## Run

```sh
./build/release/syn                    # REPL
./build/release/syn script.syn         # run a file
./build/release/syn --disasm script.syn   # dump bytecode, then run
```

### RAT mouse model

```sh
./build/release/syn rat status              # show active profile (baked / user)
./build/release/syn rat calibrate           # fullscreen overlay → per-user profile
./build/release/syn rat calibrate --quick   # fewer dots, faster
./build/release/syn rat reset               # delete user calibration
```

Calibration saves to `~/.config/synapse/rat_user.bin`.

### Demo (moves your real cursor)

```sh
./build/release/syn examples/rat_demo.syn
```

Human-like curved trajectories vs. straight-line "linear" mode. Keep hands off
the mouse while it runs.

## Test

**Conformance** (end-to-end `.syn` golden tests — **auto-builds the preset first**):

```sh
python tests/run.py
```

**Unit** (Catch2):

```sh
./build/release/tests/syn_tests           # all unit tests, fast
./build/release/tests/syn_tests "[rat]"   # one tag
```

`ctest --test-dir build/release` runs everything — but its last test,
`golden_tests`, re-runs the full conformance suite (auto-build + all `.syn`
goldens), so the whole ctest run takes ~2 min. Use the `syn_tests` binary
directly for a quick unit-only pass.

> Conformance uses `build/<preset>/syn`. A bare `cmake --build build` writes
> `build/syn`, which the harness ignores — always go through the preset.

## Environment flags

| Var | Effect |
|---|---|
| `SYN_NO_JIT=1` | Force the interpreter (correctness ground truth). The JIT is the default path and has silently produced wrong answers before — verify output here, not just speed. |
| `SYN_MOCK_PLATFORM=1` | Automation commands hit the deterministic mock backend instead of real X11 — how CI runs them without a display. |
| `SYN_DUMP_PLATFORM_LOG=1` | Print the recorded platform events (mouse/key/window calls). |
| `SYN_RAT_DEBUG=1` | Print RAT calibration Fitts'-law fit details. |
