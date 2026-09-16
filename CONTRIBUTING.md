# Contributing

Thanks for looking.  This is a small, performance-obsessed library, so the bar for accepting
a change is "it is measurably better or measurably clearer".

## Building and testing

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Without CMake there is a `Makefile` with the same defaults:

```sh
make test
```

## What CI checks

GitHub Actions builds with GCC and Clang on x86-64 using `-march=x86-64-v3` (the lowest
level that provides BMI2, which the assembly needs) and runs the full test suite.  A change
that only passes on your own CPU (`-march=native`) is not accepted.

## Ground rules

- **Correctness first.** Every kernel change must keep `tests/test_natural.cpp` green; new
  arithmetic paths need an oracle that is independent of the code under test (the test file
  already contains a 128-bit schoolbook reference and cross-checks every pair of paths).
- **Measure.** Performance claims need a number and a method - see the `benchmarks/`
  target and the design notes in `docs/`.  The CPU must be on mains power, `taskset`-pinned,
  and the comparison interleaved round by round; otherwise the numbers are not comparable.
- **Style.** Tabs, C++20, no external dependencies in the library itself.  Assembly is
  Intel-syntax GNU as with `.intel_syntax noprefix` at the top of each file.
- Keep the public API in `namespace nat`; do not add new un-prefixed global symbols.
