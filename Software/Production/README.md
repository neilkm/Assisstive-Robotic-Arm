# Production Unit Test Framework

Primary entrypoint is from `Software/`:

- `make test`

Production-local commands still work:

- `make -C Production test`
- `make -C Production test-no-build`
- `make -C Production build-tests`
- `make -C Production clean-tests`

Flags (default `true`):

- `RUN_TESTS=true|false`
- `BUILD_ESP=true|false`
- `BUILD_NUCLEO=true|false`
- `BUILD_NVIDIA=true|false`
- `SHOW_BUILD_OUTPUT=true|false` (default `false`)

Build output path for all production modules:

- `Software/build_output/production/<module>/unit_tests/`
- Compiling tests generates `run_tests.sh` in that folder for direct reruns.
- Compiling tests also generates `Software/build_output/production/run_all_tests.sh`.
- Combined runner prints final summary: `[Ran X tests from X modules. X/X Passed.]`

Module expectations:

- `src/` for C sources.
- `include/` for headers.
- `tests/*_test.c` for unit tests.
- Module `Makefile` including `../test_framework/module.mk`.
- Optional Rust tests by setting `RUST_MANIFEST=<path to Cargo.toml>` in a module `Makefile`.
