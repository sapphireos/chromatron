# Chromatron host unit tests (Phase 1)

Host-side unit tests for the pure-logic sapphireos/lib_gfx modules, built
with plain `gcc` - no cross toolchain, no sapphiremake, no hardware.

## Usage

    make                  # build + run all suites + coverage report
    make ASAN=1           # same under ASan/UBSan (separate build dir)
    make cov-baseline     # record current coverage as the new baseline
    make clean

No Python dependencies are required: the coverage pipeline is stdlib-only
and drives `gcov --json-format` from your GCC (>= 9).

## Layout

    Makefile             build + run + coverage in one
    coverage_report.py   gcov aggregation -> summary.txt/.json + coverage.xml
    cov_check.py         baseline regression gate (runs on every make)
    gen_vectors.py       kv_hashes.json / catbus_hashes.json -> build/vectors.h
    gen_kv_defs.py       -D__KV__<name>=<hash> defines, as sapphiremake emits
    test_<name>.c        one suite per file, one binary per suite
    stubs/               minimal stubs, audited in stubs/README.md
    baseline.json        committed coverage baseline (created by cov-baseline)
    build/               per-suite objects and runs (gitignored)
    coverage/            reports (gitignored)

Each suite links its sources under test (instrumented with
`-fprofile-arcs -ftest-coverage`) plus the vendored Unity
(`src/FX4/unity.c`), the sim HAL files it needs, and its stubs. Objects
live in `build/<suite>/obj/` so suites never share `.gcno/.gcda` files.

## Adding a suite

1. Add `test_foo.c` with a `main()` that does `UNITY_BEGIN()`,
   `RUN_TEST(...)` per test, `return UNITY_END();` (same style as
   `src/FX4/parser.c`). This Unity build calls `setUp()`/`tearDown()`
   unconditionally, so define both (empty is fine) in the test file.
2. In the Makefile add `foo` to `SUITES` and list the sources under test
   in `SUITE_foo` (paths relative to the repo root; keep them in
   `src/sapphireos` or `src/lib_gfx` or the coverage report will not
   include them). Add `HAL_foo` / `STUB_foo` as needed.
3. If you add a stub, document it in `stubs/README.md`.
4. If a suite exercises flash, it runs in its own working dir
   (`build/<suite>/run`) - the sim flash HAL creates `sim_flash.bin`
   there, so state does not leak between suites.
5. **Keyvalue defines**: the firmware build receives
   `-D__KV__<name>=<hash>` for every `__KV__<name>` token in the project
   sources (sapphiremake generates these). Any SUT that uses `__KV__`
   constants (memory.c, keyvalue.c, ...) needs them too - the Makefile
   runs `gen_kv_defs.py` per suite and passes the result to the compiler
   automatically, so nothing to do beyond adding the sources.

## Coverage

`make` always produces `coverage/{summary.txt, summary.json,
coverage.xml}` (Cobertura) and prints the table. Coverage is measured
per line for `src/sapphireos/**` and `src/lib_gfx/**` only; harness,
stubs, sim HAL and Unity are compiled uninstrumented.

`cov_check.py` then runs on **every** `make` (it is phony, so an
up-to-date build cannot skip it) and **fails** the build if overall or
any file's line coverage drops by more than 0.5pp against
`baseline.json`. After a deliberate coverage change (new suite,
refactored SUT), review the diff and run `make cov-baseline` to move
the baseline.

Note: this environment had no network, so gcovr could not be installed;
`coverage_report.py` produces equivalent artifacts directly from
`gcov --json-format` (GCC >= 9). If you later `pip install gcovr` (pinned
here for reference), you may replace the script - the report formats
intentionally match.

    # optional, needs network:
    # pip install "gcovr>=7,<8"

## Flags

The C flags mirror `targets/sim/settings.json` (`-fpack-struct`,
`-fshort-enums`, `__SIM__`, ...). Keep them in sync: the tests must see
exactly the struct layouts the firmware does.
