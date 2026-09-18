# natural (AVX-512 variant)

A C++20 arbitrary-precision unsigned integer on a hand-written, x86-64
number-theoretic-transform engine whose transforms run on AVX-512 IFMA. One thread, no
external dependencies.

> **This tree is the AVX-512 variant** of the `natural` library. It differs from the scalar
> tree in exactly two places: `src/mul_ntt_avx512.s` (the zmm kernels) and, in
> `include/natural/ntt_workspace.h`, the kernel declarations, the runtime dispatch and the
> finest level's *tail* peel that the `D >= 8` kernels require. `nat_asmNttMul`,
> `nat_asmCRT`, `load()`, `save()`, `intt_shr()` and the whole base-case / Toom-22 / square
> path are byte-identical to the scalar tree's.

Every scheduled pass of the transform is a zmm kernel: the merged radix-4 layers, the
unpaired radix-2 layer, and the finest level's fixed distance-2 pair (both directions).
Hardware AVX-512 F/BW/DQ/VL/IFMA is auto-detected at start-up (`ntt_zmm_cpu_ok()`); without
it the same scalar kernels run, bit for bit.

Same session, rotated A/B against the scalar library, one pinned core, mains power, powers of
two from 896 to 4 194 304 limbs:

| | product | square |
|---|---|---|
| geomean over 14 sizes, AVX / scalar | **0.584–0.589 (1.70–1.71x)** | **0.592–0.604 (1.66–1.69x)** |
| 2^22 limbs, against GNU MP 6.3.0 | **2.45–2.56x** | **2.34–2.39x** |
| 2^16…2^22 limbs, against GNU MP | 1.8–2.5x | 1.7–2.4x |

The scalar tree measures 1.46x / 1.47x against GNU MP at 2^22 limbs, i.e. all of the gain is
the transform kernels. The measurement protocol, the kernel-by-kernel story, the verification
and the four optimisations that were *measured and rejected* are in
[`docs/avx512.md`](docs/avx512.md); the long-form write-ups are `report/framework.md`
(plain language) and `report/implementation.md` (technical), both in Chinese, mirroring the
scalar tree's `report/`.

## Requirements

* **x86-64** (System V ABI), Linux. The assembly needs **BMI2** (`mulx`) and is Intel-syntax
  GNU as; `-march=x86-64-v3` is the lowest level that provides it. The zmm kernels need
  **AVX-512 F/BW/DQ/VL/IFMA** at *run* time, not at compile time — the build still targets
  x86-64-v3 and the kernels are reached only when the CPU check passes.
* A C++20 standard library with `<format>` (GCC 13+, Clang 17+ with libstdc++ 13+).
* Consumers must compile with `-masm=intel`: the public headers contain Intel-syntax inline
  assembly (`include/natural/basic_arithmetics.h`). **The CMake target adds this for you**;
  if you build by hand, do not forget it (without it you get ~160 assembler errors).

## Using it

CMake, as a subdirectory or via `FetchContent`:

```cmake
add_subdirectory(natural)              # or FetchContent_MakeAvailable(natural)
target_link_libraries(app PRIVATE natural::natural)
```

Installed and consumed from another project:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local
cmake --build build -j && cmake --install build
```

```cmake
find_package(natural 4 REQUIRED)
target_link_libraries(app PRIVATE natural::natural)
```

Then:

```cpp
#include <natural/natural.h>

int main() {
	nat::natural a("123456789012345678901234567890");
	nat::natural b = nat::sqr(a);          // or sqr(a): ADL finds it
	return b == a * a ? 0 : 1;
}
```

Without CMake, the `Makefile` does the same thing with plain `g++`:

```sh
make test        # build + run the self-checking suite
make bench       # throughput of the multiply/square paths
make example     # build + run examples/square.cpp
```

Or manually, if you prefer:

```sh
g++ -O3 -march=x86-64-v3 -std=c++20 -masm=intel -Iinclude \
    src/mul_basecase.s src/mul_ntt.s src/mul_ntt_avx512.s your_program.cpp -o your_program
```

## The kernels

One 4D macro-block of the transform holds four quarters of `D` elements. The scalar kernels
walk them one element at a time; the zmm kernels keep **one zmm per quarter**, which needs
`D >= 8`:

| kernel | pass | shape |
|---|---|---|
| `nat_asmNtt_zmm_radix4` / `nat_asmINtt_zmm_radix4` | the merged two-layer pass | one zmm per quarter, both layers in one sweep of the array |
| `nat_asmNtt_zmm_radix2` / `nat_asmINtt_zmm_radix2` | the schedule's unpaired layer (`N >= 8`) | one zmm per half of a 2N block |
| `nat_asmNtt_zmm_radix4_d2` / `nat_asmINtt_zmm_radix4_d2` | the finest level's fixed `(2,1)` pair | 16 elements (two 4D blocks) per iteration |

The first two are the scalar radix-4 / radix-2 kernels with the quarter's `D` elements
replaced by one vector: same two merged layers, same twiddle cursors, **bit-identical**
results. The multiply is the project's IFMA Shoup — `vpmadd52huq/luq` build the quotient and
the modulus' `m = a·2^56 + 1` shape folds the correction into a shift — so no 64x64->128
emulation and no `mulx` is involved.

The `D = 2` tail is the one shape the quarter-sized mapping cannot express (a 4D block is only
eight elements there), so the planner peels the finest level's fixed distance-2 pair off into
`ntt_level::tail`. Its kernel regroups 16 elements three times with `vpermt2q` so that each
pass pairs the right lanes, builds both twiddle vectors straight out of the root tables, and
keeps the inverse a mirror image of the forward — same three regroupings, same two Shoup
multiplies and two reductions, no mask register. Why it exists, why `D = 4` never occurs and
how the two directions differ is in [`docs/avx512.md`](docs/avx512.md).

## How it works (shared with the scalar tree)

* **Engine.** Three 56-bit NTT-friendly primes (`m = 27 / 58 / 87 · 2^56 + 1`), a `2^scale`
  transform, DIF forward / DIT inverse so **no bit reversal** is needed anywhere, and
  butterfly outputs kept in the lazy range `[0, 2p)` so add/sub need no correction step.
  Twiddle multiplication is Shoup's trick with a precomputed reciprocal; the three residues
  are recombined by `nat_asmCRT`.
* **DRAM / L3 / L2 / L1 schedule.** The scheduled layers are run level by level, chunk-major,
  one level per memory level: the whole array (DRAM), then `2^20`-element chunks (8 MiB, one
  modulus at a time, under the 24 MiB L3), `2^16` (512 KiB, L2) and `2^12` (32 KiB, L1). Each
  level merges two layers per pass (radix-4) with positionally computed twiddle cursors, and
  the cut points are clamped to the array size and de-duplicated per transform, so a small
  scale simply has fewer levels. In this variant the clamp is `>= 4` rather than `>= 2`,
  because the finest level has to host the tail's 16-element iteration. Boundaries are parity
  aligned, so the only unpaired layer a level can leave is the bottom layer of the finest
  merged range, run last forward and first inverse (the minimum-distance end in both
  directions). The three edge passes are *fused* into passes that were already touching the
  data (fold in `load()`, last forward layer + pointwise + first inverse layer in
  `nat_asmNttMul`, last inverse layer + the final shift in `intt_shr`).
* **One size shorter.** Just above a power of two the transform would be half empty; instead
  a cyclic convolution of length `N` plus the exact correction `a·b = C + (B^N − 1)·H`
  replaces it, and a cost model picks the cheaper of the direct, split and wrap paths.
* **Base cases.** Below 24 limbs a long × short schoolbook (`nat_mul_basecase_even/odd`),
  then Toom-22, then the NTT above 896 limbs.
* **Squaring** has its own path: a symmetric leaf that does ~half the multiplies
  (`nat_sqr_basecase`) and a Karatsuba square that needs one operand and `3m` instead of
  `4m` workspace limbs per level (`sqr_toom22`, cutoff 37 limbs).

## What is still scalar, and why

The transforms are vectorised; the pieces around them are deliberately not:

* `load()`, `save()` — constant multiplies, but the hollow twin shows they run within 3–10 %
  of a pure copy: **memory-bound**, ceiling 0.1–0.3 % of a product.
* `intt_shr()` — vectorisable and exact (measured 1.6–1.7x on the isolated stage while it is
  cache-resident, ~9 % at 2^22 limbs), but the stage is at most ~1.3 % of a product, so the
  end-to-end effect is 0.1–0.4 % — inside the A/B noise.
* `nat_asmNttMul` — the pointwise product multiplies two *live* limbs; there is no reciprocal
  to Shoup with, and a 2x52-bit limb representation would double the already memory-bound
  traffic.
* `nat_asmCRT()` — every one of its eleven per-coefficient multiplies has a constant operand,
  so it is IFMA-able in principle, but a working vector draft reaches only 1.3–1.4x on the
  arithmetic and 1.5x *slower* overall with the carry left scalar. Measured and dropped; the
  numbers and the one non-obvious finding are in [`docs/avx512.md`](docs/avx512.md).

## Testing and benchmarking

```sh
ctest --test-dir build --output-on-failure
./build/natural_bench 67108864          # limbs, powers of two from 2^14
```

The test suite is self-contained: it checks every arithmetic path against a plain O(n·m)
128-bit schoolbook written inside the test, and checks every pair of internal paths against
each other (base case vs Toom-22 vs NTT, planner on/off, wrap correction on/off, `sqr` vs
`a*a`, the NTT schedule plan and its explicit-scale products vs the base case, division
inverting multiplication). On top of the scalar suite it adds, **per pass**: every radix-4
`D` and radix-2 `N` at every offset, and the two `D = 2` tail kernels at every chunk size
`T = 4…9` at every offset — each **bit-identical** to the scalar kernel it replaces — plus an
end-to-end check that the whole engine with the zmm kernels on and off produces identical
products and squares, and the division regression group (the short-divisor/short-quotient
cases that used to spin). 4 090 checks, 0 failures with the ISA present; with
`ntt_zmm_cpu_ok()` forced to false the zmm-only groups skip themselves and the suite still
passes **4 014 checks, 0 failures** on the scalar path. With the ISA present the suite runs in
a few seconds.

## Repository layout

```
include/natural/    public headers; natural.h is the entry point
src/                the assembly kernels (mul_basecase.s, mul_ntt.s, mul_ntt_avx512.s)
tests/              self-checking test suite (CTest)
benchmarks/         throughput benchmark
examples/           minimal usage example
docs/               design and measurement notes (avx512.md is the variant's own)
report/             long-form write-up (framework.md, implementation.md; Chinese)
cmake/              package config for find_package(natural)
```

## Limitations

* Single-threaded. x86-64 only. Peak memory is about 1.9x GNU MP's per product (three residue
  arrays plus the twiddle tables — the redundancy is inherent to the CRT engine).
* The AVX-512 path is used only when the CPU reports F/BW/DQ/VL/IFMA. On a machine without it
  the library falls back to the scalar kernels; the two are bit-identical, but the speedup is
  the variant's whole point.
* The five sizes where GNU MP wins the *scalar* library (81,920 … 196,608 limbs) are
  transform-fill valleys: the wrap path cannot reach them and a half-empty transform costs
  more than it saves. The planner is shared, so this variant does not move which sizes those
  are, but it has not been measured on them — the A/B above covers powers of two plus 896
  limbs only.
* Division uses a Newton iteration with a conservative accuracy heuristic. The shapes where
  that heuristic degenerates -- a short divisor, or a short quotient -- are now dispatched to
  an exact multi-limb schoolbook division instead (`div_schoolbook_divisor_max` /
  `div_schoolbook_quotient_max`), and the iterative path keeps a bounded fallback as a safety
  net, so the one-call hang this used to have (`(2^192-1) / (2^64+1)`) is gone. Replacing the
  heuristic itself remains the standing recommendation; the history and the measurements are
  at the top of `include/natural/natural.h`. Multiplication, squaring and the NTT are
  unaffected.
* The headers use inline assembly, which is why `-masm=intel` is a public requirement;
  moving those few routines into the assembly files would remove it.

## License

MIT — see [LICENSE](LICENSE).
