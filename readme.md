# natural

A single-header C++20 arbitrary-precision unsigned integer (`natural`) on a hand-written
x86-64 number-theoretic-transform engine.

For balanced products of **2^16 … 2^26 limbs** (64 KiB … 4 MiB operands, 4 … 268 Mbit) it
matches or beats **GNU MP 6.3.0** on most sizes, by up to **1.47x**.

```
  bal. multiply, 41 sizes 2^16..2^26 limbs, interleaved A/B, mains power
  ----------------------------------------------------------------------
  29 clear wins (>1.05x)     7 ties (0.95..1.05x)     5 losses (<0.95x)
  largest lead    1.47x @ 1,048,576 limbs
  powers of two   all ten of 2^17..2^26 are clear wins, 1.25x .. 1.47x
  where GMP wins  81,920 .. 196,608 limbs (0.83x .. 0.94x), see below
```

Everything runs in one thread. There is no AVX/AVX-512 code in the library — the whole
engine is scalar GPR code, and the speed comes from the algorithm and the schedule.

## Build, test, benchmark

```sh
bash compile.sh            # g++ -masm=intel -march=native -std=c++20 -O3 mul_basecase.s mul_ntt.s test.cpp -o test
./test                     # self-checking correctness suite (3.6k assertions, ~0.1 s)
./test bench 16777216      # multiply/square throughput up to 2^24 limbs
```

`./test` needs no external library: it checks every path against oracles that are
independent of it (a plain O(n·m) 128-bit schoolbook written in the test, and every pair of
internal paths against each other — base case vs Toom-22 vs NTT, planner on/off, wrap
correction on/off, `sqr` vs `a*a`, division inverting multiplication).

Requirements: x86-64 Linux (System V ABI), **BMI2** (`mulx`), C++20, GCC 13 or newer.
The assembly is Intel-syntax GNU as; there is no portable fallback.

## How it works

**Engine.** Three 56-bit NTT-friendly primes, `m = 27 / 58 / 87 · 2^56 + 1`, a transform of
`2^scale` points, DIF on the way in and DIT on the way out so **no bit reversal** is ever
needed, with a bit-reversed root table that is extended by prefix reuse. Butterfly outputs
stay in the lazy range `[0, 2p)`, so add/sub need no conditional correction; the twiddle
multiply uses Shoup's trick with the precomputed reciprocal, which tolerates `v < 2^64`.
Three residues per coefficient are recombined by `asmCRT`, which keeps the carry words in a
separate array, so the whole transform never touches the carry structure.

**Schedule.** The three level cut points (`la = 16`, `lb = 12`) give a level-A pass over the
whole array, a level-B pass per 512 KiB chunk and a level-C pass per 32 KiB sub-chunk, each
merged as radix-4 (two layers per pass) with the twiddle cursor computed positionally. The
three edge passes are fused into the passes that were already touching the data: the
distance-`N/2` fold in `load()`, the last forward layer + pointwise product + first inverse
layer in `asmNttMul`, and the final inverse layer + the `2^-(scale-1)` shift in `intt_shr`.
Result: no pass over the array is spent on a layer that is not also doing something else.

**One size shorter.** `get_NTT_scale()` must round up to a power of two, so an operand a few
limbs over a boundary gets a transform that is half empty. A cyclic convolution of length
`N` gives `c_i = p_i + p_{i+N}`, and for `len1+len2-1 <= 2N` only the top `w` coefficients
alias. With `C = Σ c_i B^i` and `H` the alias part, `a·b = C + (B^N − 1)·H` exactly — one
cheap correction product instead of a transform twice as large. A cost model picks between
the direct transform, the classic `a0/a1` split and this wrap path per size. This is where
the sawtooth in the GMP comparison comes from: inside the band the planner cannot reach
(GMP wins at 62.5…87.5 % transform fill), the loss is bounded at 0.83x.

**Base cases and squaring.** Below 24 limbs the product goes to `mul_basecase.s`, a long × short
schoolbook whose two kernels handle the short operand's parity so that every inner loop has
an even trip count. Squaring has its own path: `sqr_basecase` exploits symmetry
(`s_j = 2·Σ_{i<k} a_i a_k + [j even] a²_{j/2}`, so ~half the multiplies), keeps both columns'
pair sums in one four-limb window and doubles it once per round, and `sqr_toom22` recurses
with one operand, no unbalanced block loop and `3m` instead of `4m` workspace limbs per
level. Measured against the previous "square as an equal-operand product" path: **1.42x**
geometric mean over 24 … 8192 limbs.

**Workspaces.** A transform at scale 23 needs three 64 MiB arrays per operand. They come
from a small thread-local cache (`nat_block_pool`) instead of `malloc/free`, which removes
~150 000 minor faults per multiply at that size.

## Why GMP is the baseline, and where it stays ahead

GMP 6.3.0 on this CPU classifies as `skylake` (no icelake/tigerlake `mparam` ships with it)
and was rebuilt `-march=native -O3`. Its SSA is already close to its own limb-movement
ceiling (measured: 93 % of a cache-resident `mpn_add_n` pass), so the comparison is not won
by arithmetic: the cycle decomposition at 4.19 M limbs splits 1.38x into 1.15x fewer
instructions (GMP retires 15 % more) times 1.20x IPC, and `natural` moves **2.9x fewer
load/store operations** (3.4e9 vs 9.9e9), because GMP's SSA spends most of its traffic on
the coefficient arrays it re-reads per layer. The price is memory: peak RSS is **1.9x**
GMP's for one multiply.

## Files

| file | what |
|---|---|
| `natural.h` | the public type, operators, the multiply/square/divide planners and thresholds |
| `ntt_workspace.h` | the NTT schedule (three-level radix-4), load/save, the `intt_shr` edge pass |
| `ntt_wrap.h` | the wrap-corrected "one size shorter" product and its cost model |
| `mul_toom22.h` | Toom-22 (Karatsuba) multiply and square recursions, thresholds |
| `mul_basecase.s` | `mul_basecase_even/odd` (long × short schoolbook) and `sqr_basecase` |
| `mul_ntt.s` | the transform kernels: `asmNtt*`, `asmINtt*`, `asmNttMul`, `asmCRT` |
| `basic_arithmetics.h` | limb add/sub/shift/mul1/mul2 and the base-case dispatch |
| `array_u64.h` | the limb array and the large-block cache |
| `barrett_reduction.h` | 128/64 division, Barrett mod and modmul |
| `test.cpp`, `compile.sh` | self-checking test suite and the one-line build |

## Notes and limitations

* Single-threaded; no SIMD. Peak memory is ~2x GMP's for the same product.
* The 5 sizes where GMP wins (81,920 … 196,608 limbs) are the transform-fill valleys noted
  above; a 32-point fine sweep around 2^20 limbs shows both sides of that band.
* Division uses a Newton iteration with a conservative accuracy heuristic — see the
  `KNOWN-ISSUE HISTORY` comment at the top of `natural.h`; multiplication, squaring and the
  NTT are unaffected.
* The measurements above come from the project's `verify/` harness (interleaved A/B, best of
  N, mains power, 41 sizes); the design write-ups are in `report/`. Those directories are
  part of the larger project this directory is extracted from.
