# Changelog

All notable changes to this project are documented here.  The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the project adheres to
[Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [4.0.0] - 2026-09-16

First packaged release of the `nat` library.

### Added
- Dedicated squaring path: `nat_sqr_basecase` (symmetric leaf, ~half the multiplies of an
  equal-length product) and `sqr_toom22` (one operand, no unbalanced loop, `3m` instead of
  `4m` workspace limbs per level), selected by `sqr_toom22_threshold = 37`.  Measured 1.42x
  faster (geometric mean, 24 .. 8192 limbs) than squaring via the multiply path.
- CMake package: `find_package(natural)` provides the `natural::natural` target; the headers
  are installed under `include/natural/`.
- Self-checking test suite (`tests/`, 3600+ assertions against an independent 128-bit
  schoolbook reference) wired to CTest, plus a throughput benchmark.

### Changed
- Everything now lives in `namespace nat`; the assembly entry points are exported with a
  `nat_` prefix (`nat_asmNtt*`, `nat_asmINtt*`, `nat_asmNttMul`, `nat_asmCRT`,
  `nat_mul_basecase_even/odd`, `nat_sqr_basecase`).
- The multiply base-case kernels (`mul_basecase_even/odd`) and the square leaf
  (`sqr_basecase`) were merged into one translation unit, `src/mul_basecase.s`.
- Default include is `<natural/natural.h>`.

### Known limitations
- x86-64 only (System V ABI), requires BMI2 (`mulx`) and a C++20 standard library with
  `<format>`.  Consumers must compile with `-masm=intel` (the CMake target adds it).
- Single-threaded; no SIMD.  Peak memory is about 1.9x GNU MP's for one product.
- `reciprocal()` uses a conservative Newton accuracy heuristic - see the notes at the top of
  `include/natural/natural.h`.  Multiplication, squaring and the NTT are unaffected.
