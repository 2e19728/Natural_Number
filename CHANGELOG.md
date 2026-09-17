# Changelog

All notable changes to this project are documented here.  The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the project adheres to
[Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- AVX-512 / IFMA kernels for the merged radix-4 passes, `nat_asmNtt_zmm_radix4` and
  `nat_asmINtt_zmm_radix4`, plus single-layer `nat_asmNtt_zmm_radix2` /
  `nat_asmINtt_zmm_radix2` for the schedule's unpaired pass (`src/mul_ntt_avx512.s`): one
  zmm per quarter of the 4D block, respectively per half of a 2N block.
  They are bit-identical to the scalar kernels they replace and are selected by
  `ntt_zmm_enable`, whose default is a runtime CPU check (AVX-512 F/BW/DQ/VL/IFMA), so a
  machine -- or CI runner -- without the ISA silently runs the scalar path.  The finest level of the schedule peels its fixed distance-2 pair
  (layers 2 and 1) into `ntt_level::tail`, which stays scalar, so every merged pass has
  `D >= 8` and `D = 4` never occurs; the unpaired layer moves from layer 1 to the
  distance-8 layer 3.  End to end this is 1.57x the scalar library at powers of two
  896..4194304 limbs and 2.18x GNU MP at 2^22 limbs.  See `docs/avx512.md`.

### Changed
- Synced the schedule refactor from the scalar library (`sched-refactor`): the NTT layer
  schedule is now one rule-based plan instead of three hard-coded nesting levels plus two
  fallback schedules.  `ntt_sched_for(k)` returns up to four *levels* (`ntt_sched`,
  `ntt_level`) — DRAM (the whole array) and the L3 / L2 / L1 working sets — cut by the
  `inline constexpr` `ntt_scale_l1_threshold` / `_l2_` / `_l3_` constants, clamped to the array
  size and de-duplicated per transform, so a small scale simply has fewer levels.  The
  `ntt_sched_v3` A/B switch and the `ntt_sched_min_scale` boundary are gone, as are the plain
  single-layer loop and the two-level schedule they selected: every scale now runs the same
  code.  The runtime tunables `ntt_sched_la` / `ntt_sched_lb` and
  `ntt_workspace::sched_levels()` are gone too; the cut points can still be scanned from the
  command line (`-Dntt_scale_l2_threshold=14`).  Level boundaries are parity aligned, so the
  only unpaired layer the schedule can leave is the bottom layer of the finest level's merged
  range, always run at the minimum-distance end of both the forward and the inverse order.

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
