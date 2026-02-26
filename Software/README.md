# Software

## Functional Directory Structure

- `Makefile`: top-level software entrypoint for build/test orchestration.
- `Production/`: production modules and shared unit-test framework.
  - `nvidia_jetson/`: Rust module (Cargo-based), with inline `#[test]` unit tests.
  - `esp32_joystick/`: C module and C unit tests.
  - `stm_nucleo/`: C module and C unit tests.
  - `test_framework/`: shared C test harness and module build/test logic.
- `build_output/`: generated test/build artifacts.
- `sandbox/`: non-production experiments and tooling.

## Build And Test Commands

Run from `Software/`:

```sh
make build-tests
```

Build test executables only (do not run):

```sh
make test RUN_TESTS=false
```

Build and run all enabled module tests:

```sh
make test
```

Run tests without rebuilding:

```sh
make test-no-build
```

Clean test/build outputs:

```sh
make clean-tests
```

## Module Selection And Output Flags

Defaults are `true`:

- `BUILD_ESP=true|false`
- `BUILD_NUCLEO=true|false`
- `BUILD_NVIDIA=true|false`
- `SHOW_BUILD_OUTPUT=true|false` (default `false`)

Examples:

```sh
make test BUILD_NVIDIA=false
make test BUILD_ESP=false BUILD_NUCLEO=true BUILD_NVIDIA=true
make test SHOW_BUILD_OUTPUT=true
```

## Test Output

- Per-module runners:
  - `build_output/production/<module>/unit_tests/run_tests.sh`
- Combined runner:
  - `build_output/production/run_all_tests.sh`
- Unified C/Rust case output format:
  - `"[RUN] Case: [...]"`
  - `"[OK ] Case: [...]"` or `"[FAIL] Case: [...]"`
- Final combined summary:
  - `"[Ran X tests from Y modules. Z/X Passed.]"`
