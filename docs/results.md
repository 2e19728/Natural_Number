# Measurements

All numbers below are balanced products, single-threaded, against **GNU MP 6.3.0** built
with `-march=native -O3 -mtune=native` on the same machine.

**Protocol.** Both engines run in the *same process*, alternating round by round, on a
`taskset`-pinned physical core, with the laptop **on mains power** (4.38–4.59 GHz; on battery
the clock drops to 2.6–2.9 GHz and every number changes, so battery-era data was discarded).
Sizes are limb counts; a "limb" is 64 bits. `ratio = GMP time / nat time`, so > 1 means `nat`
is faster.

**Machine.** Intel i7-11800H (Tiger Lake-H), 8 cores, L1d 48 KiB / L2 1.25 MiB per core,
L3 24 MiB, WSL2 (kernel 6.6.87), GCC 13.3.0. GNU MP classifies this CPU as `skylake` — no
icelake/tigerlake `mparam` ships with 6.3.0. A paired A/B over all 22 x86-64 `mparam` tables
shipped with GMP shows its shipped choice is the *favourable* one for GMP here: switching the
FFT threshold table changes its time by 0.989–1.176x, and in the 10.5M–29.4M limb band it
makes GMP slower. So the baseline is not a straw man.

## Multiply, 41 sizes from 2^16 to 2^26 limbs

| | |
|---|---|
| clear wins (> 1.05x) / ties (0.95–1.05x) / losses (< 0.95x) | **29 / 7 / 5** |
| largest lead | **1.4737x** at 1,048,576 limbs |
| largest deficit | **0.8299x** at 81,920 limbs |
| instruction count (4.19M limbs, cycle decomposition) | 1.148x fewer (GMP retires 15 % more) |
| IPC (same measurement) | 1.199x higher (3.296 vs 2.750) |
| cycle ratio (same measurement) | **1.376x** |

The five sizes where GNU MP wins, and the reason they are exactly these:

| limbs | ratio | transform fill |
|---|---|---|
| 81,920 | 0.830 | 62.5 % |
| 98,304 | 0.836 | 75.0 % |
| 114,688 | 0.943 | 87.5 % |
| 163,840 | 0.924 | 62.5 % |
| 196,608 | 0.915 | 75.0 % |

They are the sawtooth valleys just above a power of two: the transform is forced to the next
power of two while being only 62.5–87.5 % full, and the wrap path (see
[`design.md`](design.md) §4) cannot reach that far, so the cost model has to accept a
half-empty transform. Everywhere else the same band is won, and the loss is bounded at 0.83x.

Powers of two, where the engine is at its best:

| limbs | 2^16 | 2^17 | 2^18 | 2^19 | 2^20 | 2^21 | 2^22 | 2^23 | 2^24 | 2^25 | 2^26 |
|---|---|---|---|---|---|---|---|---|---|---|---|
| ratio | 1.036 | 1.247 | 1.325 | 1.366 | **1.474** | 1.455 | 1.458 | 1.419 | 1.395 | 1.382 | 1.453 |

## Why: traffic, not arithmetic

At 4.19 M limbs, counted in one process with the same compiler:

| | nat | GNU MP |
|---|---|---|
| load+store ops | 3.425e9 | 9.924e9 |
| instructions | 1.477e10 | 1.649e10 |
| IPC | 3.05 | 2.69 |

**2.90x fewer load/store operations** is where the win comes from; the instruction counts are
within 12 %. Consistently, GNU MP's SSA is limited by limb movement rather than arithmetic:
timing a cache-resident `mpn_add_n` pass, its simplest limb-moving primitive, shows its entire
SSA running at **93 % of that ceiling** (five mains runs gave 1.125 / 1.048 / 1.074 / 1.116 /
1.064 times the ceiling). The price of the lower traffic is memory: peak RSS is **1.90–1.95x**
GNU MP's across 1M–32M limbs, and the workspace block cache removes roughly half the minor
faults a cold multiply would take (152,997 → 76,533 at 4.19 M limbs).

## Squaring

The square has its own kernels (`nat_sqr_basecase`, `sqr_toom22`). Compared against the
previous "square it as an equal-operand product" route, interleaved in one process:

| limbs | 24 | 48 | 96 | 192 | 384 | 512 | 768 | 895 | 1024 | 2048 | 4096 | 8192 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| speedup | 1.41x | 1.53x | 1.41x | 1.37x | 1.40x | 1.41x | 1.40x | 1.39x | 1.43x | 1.42x | 1.46x | 1.43x |

Geometric mean **1.42x**. Above 896 limbs the library switches to the NTT square, which is
untouched by this work, so those rows double as a control on the measurement noise (the
user-visible `sqr()` entry measures 1.00x there while the changed base path is 1.4x faster).

Correctness for the square path is checked limb-for-limb against the paths it replaces
(`mul_basecase_default(d, a, a, n, n)` and `mul_toom22(d, a, a, n, n)`) with guard limbs on
the sources, the destination and the workspace, over ~2 800 cases including all sizes 1–130,
the recursion boundaries, and all-ones / alternating / sparse patterns that force both
branches of the middle-product length test.

## Where the raw data lives

These tables were produced by the project's measurement harness (`gmpsweep`, `steady_state`,
`traffic`, `gmp_wall`, `sqrtoom`, …), which is part of the research tree rather than this
library repository. The canonical files are `gmpsweep_version_3.csv` (41 sizes, all rows
verified against a full comparison), `steady_state.csv` (cycle decomposition),
`traffic.csv`, `gmp_wall.md` and `sqr_path.md` (squaring).
