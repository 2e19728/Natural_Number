// ntt_wrap.h - wrap-corrected ("one size shorter") NTT multiplication for version_2.
//
// Why
// ---
// get_NTT_scale() forces 2^(scale-1) < len1+len2-2 <= 2^scale, so the transform is the next
// power of two of ~2n: for operands just above a power-of-two boundary the transform is half
// empty, and because the transform cost is N*log N (and load/CRT are O(N)) the time jumps by
// up to ~1.7x for a 0.2% larger input.  Measured here (balanced, best of 5): n=1024 -> 114 us,
// n=1026 -> 192 us with the version_1 schedule.
//
// How
// ---
// A cyclic convolution of length N yields c_i = p_i + p_{i+N} (p = true linear convolution).
// For len1+len2-1 <= 2N only the first w = len1+len2-1-N coefficients are aliased, and those
// only involve the top w+1 limbs of each operand: with A' = floor(a / B^(len1-w-1)),
// B' = floor(b / B^(len2-w-1)) (B = 2^64) and tau the convolution coefficients of A'*B',
//
//      p_{i+N} = tau_{w+1+i}                    for 0 <= i < w.
//
// With  C = sum_{i<N} c_i B^i   and   H = sum_{i<w} p_{i+N} B^i,  both exact integers,
//
//      C = (sum_{i<N} p_i B^i) + H        and      a*b = (sum_{i<N} p_i B^i) + B^N * H,
//
// therefore, with no per-coefficient correction at all,
//
//      a*b = C + (B^N - 1) * H                                          (*)
//
// Cost: one cyclic convolution of N = 2^(scale-1) points (3 transforms) plus a small product
// of (w+1)-limb operands for H (3 transforms of ~2(w+1) points).  When w is small - exactly
// in the "just above a power of two" band - the correction is nearly free, and (*) replaces
// version_1's 5 transforms of N points (its a0/a1 split) or 3 transforms of 2N points.
// Measured: n=1026 192 -> 128 us (1.50x), n=2052 393 -> 264 us (1.49x), n=4104 859 -> 559 us
// (1.54x).  Once the wrap reaches ~1/4 of N the correction product is as big as the main one,
// and the planner below keeps the version_1 path.
//
// Correctness notes
//   * C comes from crt(N) plus its two carry words, H from crt(w) plus its two carry words.
//     Both carry values are < 2^(64+log2(...)) < 2^128, so nat_asmCRT's two carry words suffice.
//   * B^N * H >= H, so the subtraction in (*) never borrows out.
//   * all arithmetic before the CRT is on reduced residues in [0, mod).
//
// Verified by verify/mulcheck.cpp (shape grid; wrap on vs off vs Toom-22) and by
// verify/run.sh (byte-for-byte against version_0).

#pragma once

#include <cstdint>
#include <cstring>

#include "array_u64.h"
#include "basic_arithmetics.h"          // add()/sub() for the final C + B^N*H - H assembly
#include "ntt_workspace.h"

namespace nat {

// Same formula as natural.h's get_NTT_scale(), duplicated here so this header does not depend
// on natural.h (the caller passes the same value in as `scale`).
inline int ntt_wrap_scale(uint64_t len1, uint64_t len2) {
	return 64 - std::countl_zero(len1 + len2 - 2);
}

// ---------------------------------------------------------------- cost model
// Units are "butterfly points": pt(M) = M * log2(M).
//   full  : 3 transforms of 2^scale
//   split : 5 transforms of 2^(scale-1)  (version_1's a0/a1 split, reuses one transform)
//   wrap  : 3 transforms of N + 3 transforms of the correction size, plus CRT/assembly
// The per-size constants are calibrated against verify/sawthooth.sh measurements; the planner
// only has to rank three options, and verify/mulcheck.cpp checks that the ranking is sane.
inline double ntt_pt_cost(uint64_t points) {
	return (double)points * (double)(64 - __builtin_clzll(points));
}

inline double ntt_full_cost(int scale) {
	uint64_t points = 1ull << scale;
	return 3.0 * ntt_pt_cost(points) + 3.0 * (double)points;      // + load/CRT, ~2 passes
}

inline double ntt_split_cost(int scale) {
	uint64_t points = 1ull << (scale - 1);
	return 5.0 * ntt_pt_cost(points) + 6.0 * (double)points;      // 2 x (load/CRT) + assembly
}

inline bool mul_ntt_wrap_applicable(uint64_t len1, uint64_t len2, int scale, uint64_t* N_out, uint64_t* w_out) {
	if (len1 == 0 || len2 == 0 || scale < 2)
		return false;
	uint64_t N = 1ull << (scale - 1);
	uint64_t w = len1 + len2 - 1 - N;                 // number of aliased coefficients
	if (len1 > N || len2 > N || w == 0 || len1 < w + 1 || len2 < w + 1)
		return false;
	if (N_out) *N_out = N;
	if (w_out) *w_out = w;
	return true;
}

inline double ntt_wrap_correction_cost(uint64_t w) {
	// H is the CRT of the w raw wrap coefficients; they come from a product of (w+1)-limb
	// slices, so its transform is get_NTT_scale(w+1, w+1), plus one more pass for crt(w).
	if (2 * (w + 1) - 2 < 1)
		return 0.0;
	uint64_t points = 1ull << ntt_wrap_scale(w + 1, w + 1);
	return 3.0 * ntt_pt_cost(points) + 4.0 * (double)points;
}

// version_2 switch: false reproduces the version_1 schedule exactly (for A/B testing)
inline bool ntt_wrap_enable = true;

// Safety margin on the planner: the cost model is calibrated, but a misprediction costs the
// whole speedup and more (measured: without the margin the planner picks wrap at 68-73% fill
// and loses 4-7%).  Wrap is therefore only taken when it is clearly cheaper.  With 0.90 the
// remaining mispredictions are ties (measured within +-1%).
inline constexpr double ntt_wrap_margin = 0.90;

inline bool ntt_wrap_preferred(uint64_t len1, uint64_t len2, int scale) {
	if (!ntt_wrap_enable)
		return false;
	uint64_t N, w;
	if (!mul_ntt_wrap_applicable(len1, len2, scale, &N, &w))
		return false;
	double cw = 3.0 * ntt_pt_cost(N) + 4.0 * (double)N + ntt_wrap_correction_cost(w);
	double cf = ntt_full_cost(scale);
	double cs = 1e300;
	uint64_t hi = len1 > len2 ? len1 : len2, lo = len1 > len2 ? len2 : len1;
	if (ntt_wrap_scale((hi + 1) >> 1, lo) != scale)        // version_1 takes the split here
		cs = ntt_split_cost(scale);
	return cw < ntt_wrap_margin * cf && cw < ntt_wrap_margin * cs;
}

// ---------------------------------------------------------------- the kernel

// Cyclic convolution of length 2^scale: wa <- INTT(NTT(a) * NTT(b)).  The pointwise
// multiply is out-of-place, so a second workspace is needed.
inline void ntt_cyclic_mul(ntt_workspace& wa, const uint64_t* a, uint64_t len1, const uint64_t* b, uint64_t len2, int scale) {
	ntt_workspace wb(b, len2, scale);
	wa.load(a, len1, scale);
	wa.ntt();
	wb.ntt();
	wa.mul(wa, wb);
	wa.intt();
}

// The CRT of coefficients [off, off+len) of ws, as an array_u64 holding the len merged
// words followed by the two carry words (nat_asmCRT's contract), trimmed of trailing zero
// limbs.  nat_asmCRT always merges from index 0, so a nonzero offset is served by gathering
// the three columns into scratch first.  Coefficients past the end of the residue arrays
// read as 0, which is what the wrap coefficients want: the correction product's support
// ends inside the array, so its top coefficients are genuine zeros.
inline array_u64 ntt_crt_slice(ntt_workspace& ws, uint64_t off, uint64_t len) {
	ntt_workspace scratch;
	ntt_workspace* src = &ws;
	if (off != 0) {
		const uint64_t lim = ws.ntt_data[0].size;
		const uint64_t have = off < lim ? lim - off : 0;
		const uint64_t n = have < len ? have : len;
		for (int i = 0; i < 3; i++) {
			scratch.ntt_data[i].resize(len + 2);
			if (n)
				memcpy(scratch.ntt_data[i].data, ws.ntt_data[i].data + off, sizeof(uint64_t) * n);
			memset(scratch.ntt_data[i].data + n, 0, sizeof(uint64_t) * (len + 2 - n));
		}
		src = &scratch;
	}
	uint64_t mid = src->crt(len);
	array_u64 out;
	out.resize(len + 2);
	memcpy(out.data, src->ntt_data[0].data, sizeof(uint64_t) * len);
	out[len] = mid;
	out[len + 1] = src->ntt_data[1][1];
	while (out.size > 1 && out[out.size - 1] == 0)
		out.size--;
	return out;
}

// dst = a * b, dst holds len1+len2 limbs, via (*) above.  The caller must have checked
// mul_ntt_wrap_applicable().
inline void mul_ntt_wrap(uint64_t* dst, const uint64_t* a, uint64_t len1, const uint64_t* b, uint64_t len2, int scale) {
	const uint64_t N = 1ull << (scale - 1);
	const uint64_t w = len1 + len2 - 1 - N;
	const uint64_t total = len1 + len2;

	// 1. C = sum_{i<N} c_i B^i, c from the length-N cyclic convolution of the operands
	ntt_workspace wa;
	ntt_cyclic_mul(wa, a, len1, b, len2, scale - 1);
	array_u64 C = ntt_crt_slice(wa, 0, N);

	// 2. H = sum_{i<w} p_{i+N} B^i, the CRT of the raw wrap coefficients tau_{w+1+i}
	array_u64 H;
	uint64_t sl = len1 - (w + 1), sb = len2 - (w + 1);
	uint64_t Al = w + 1, Bl = w + 1;
	while (Al > 1 && a[sl + Al - 1] == 0)
		Al--;
	while (Bl > 1 && b[sb + Bl - 1] == 0)
		Bl--;
	if (Al + Bl < 3) {
		// both slices are single limbs: A'*B' has no coefficient at index >= w+1
		H.resize(1);
		H[0] = 0;
	}
	else {
		ntt_workspace wt;
		ntt_cyclic_mul(wt, a + sl, Al, b + sb, Bl, ntt_wrap_scale(Al, Bl));
		H = ntt_crt_slice(wt, w + 1, w);
	}

	// 3. dst = C + B^N * H - H, i.e. H lifted by N limbs, minus H itself.  add()'s carry
	//    out and sub()'s borrow out are dropped, as before: the assembly is understood
	//    modulo B^total, and a*b < B^total, so the wrapped value is exactly the product.
	//    Both inputs are clamped to the output size so the bounds are obvious here
	//    instead of being inherited from the caller's preconditions.
	uint64_t cn = C.size < total ? C.size : total;
	uint64_t hn = H.size < total - N ? H.size : total - N;
	memset(dst, 0, sizeof(uint64_t) * total);
	memcpy(dst, C.data, sizeof(uint64_t) * cn);
	add(dst + N, dst + N, H.data, total - N, hn);
	sub(dst, dst, H.data, total, H.size);
}

}  // namespace nat
