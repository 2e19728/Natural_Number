// ================= Toom-22 (Karatsuba) =================
// mul_toom22(dst, src1, src2, len1, len2)  src1 * src2,  len1 >= len2

#include <cstdint>
#include "basic_arithmetics.h"

// Largest operand size (limbs) still handled by the recursive Toom-22; below it the
// base-case kernels run.  inline constexpr: one shared definition across TUs (a plain
// namespace-scope `const` would be internal linkage, i.e. one private copy per TU).
inline constexpr uint64_t mul_toom22_threshold = 24;

// Workspace size (limbs) for an equal-length product of s-limb operands.
// A node keeps a1(m)+b1(m)+d1(2m) = 4m with m = ceil(s/2)+1 and the middle child
// runs after that region; low/high children run sequentially and reuse the space.
inline uint64_t mul_toom22_need(uint64_t s) {
	uint64_t w = 0;
	for (uint64_t L = s; L >= mul_toom22_threshold; ) {
		uint64_t m = ((L + 1) >> 1) + 1;
		w += m << 2;
		L = m;
	}
	return w;
}

// Equal-length Toom-22 recursion with caller-provided workspace w (>= mul_toom22_need(s)).
// s >= 3
inline void mul_toom22_r(uint64_t* d, const uint64_t* a, const uint64_t* b, uint64_t s, uint64_t* w) {
	if (s < mul_toom22_threshold)
		return mul_basecase_default(d, a, b, s, s);
	uint64_t s0 = (s + 1) >> 1, s1 = s - s0, m = s0 + 1;
	mul_toom22_r(d, a, b, s0, w);                        // P0 -> d[0 .. 2*s0)
	mul_toom22_r(d + (s0 << 1), a + s0, b + s0, s1, w);  // P2 -> d[2*s0 .. 2*s)
	uint64_t* a1 = w;             // m limbs
	uint64_t* b1 = w + m;         // m limbs
	uint64_t* d1 = w + (m << 1);  // 2m limbs
	uint64_t* wc = w + (m << 2);  // workspace for the middle child
	a1[s0] = add(a1, a, a + s0, s0, s1);  // a1 = a0 + a1 (carry at a1[s0])
	b1[s0] = add(b1, b, b + s0, s0, s1);
	uint64_t mid = m;                          // middle product length
	if (!a1[s0] && !b1[s0])                    // both sums carry-free: fits in s0 limbs
		mid = s0;
	mul_toom22_r(d1, a1, b1, mid, wc);
	if (mid < m)
		d1[mid << 1] = d1[(mid << 1) + 1] = 0;   // clear the 2 tail slots
	sub(d1, d1, d, m << 1, s0 << 1);             // d1 -= P0
	sub(d1, d1, d + (s0 << 1), m << 1, s1 << 1); // d1 -= P2
	add(d + s0, d + s0, d1, s + s1, m << 1);     // d[s0..] += cross
}

// Unbalanced entry: len1 >= len2.  The longer operand is chopped into len2-limb
// blocks.  The first (lowest) block product is written directly into dst (dst is
// empty there, no accumulation needed); the tail dst[2*len2 .. len1+len2) is zeroed
// once, then every further full block is an equal-length (len2 x len2) Toom-22
// product accumulated at its shifted offset with carry forwarding.  The final
// partial block (r < len2 limbs) is handled by recursion on (len2, r); its short
// operand shrinks each level until the schoolbook base case applies.  When
// len1 == len2 the single block write already produced the whole product and the
// function just returns.  dst capacity must be at least len1+len2 limbs.
inline void mul_toom22(uint64_t* dst, const uint64_t* src1, const uint64_t* src2, uint64_t len1, uint64_t len2) {
	if (len2 == 1)
		return mul1(dst, src1, src2[0], len1);
	if (len2 == 2)
		return mul2(dst, src1, src2[1], src2[0], len1);
	if (len2 < mul_toom22_threshold)
		return mul_basecase_default(dst, src1, src2, len1, len2);

	const uint64_t total = len1 + len2, m = len2;
	uint64_t* ws = new uint64_t[mul_toom22_need(m)];

	// First (lowest) block: dst is empty there, write the (len2 x len2) product directly.
	mul_toom22_r(dst, src1, src2, m, ws);

	// Later blocks accumulate over dst[2*len2 .. total); zero only that tail.
	for (uint64_t i = m << 1; i < total; i++) dst[i] = 0;

	uint64_t* tmp = nullptr;
	uint64_t pos = m;
	for (; pos + m <= len1; pos += m) {
		if (!tmp)
			tmp = new uint64_t[m << 1];          // one block product buffer
		mul_toom22_r(tmp, src1 + pos, src2, m, ws);
		uint64_t c = add(dst + pos, dst + pos, tmp, m << 1, m << 1);
		if (pos + (m << 1) < total)
			dst[pos + (m << 1)] = c;   // final carry is 0
	}
	delete[] tmp;
	delete[] ws;

	uint64_t r = len1 - pos;                           // partial tail block (< len2)
	if (r) {
		uint64_t* t = new uint64_t[len2 + r];          // src2(len2) x tail(r)
		mul_toom22(t, src2, src1 + pos, len2, r);
		uint64_t c = add(dst + pos, dst + pos, t, len2 + r, len2 + r);
		delete[] t;
		if (pos + len2 + r < total)
			dst[pos + len2 + r] = c;
	}
}

// ================= Toom-22 squaring =================
// A square needs only the equal-length recursion: the two operands are the same array, so
// there is no unbalanced-block loop, the second sum b1 = b0 + b1 disappears (only
// a1 = a0 + a1 is needed), the middle product's length test collapses from "both sums
// carry-free" to "a1 carry-free", and the per-level workspace drops from 4m to 3m limbs.
//
//	a = a0 + a1*B^s0   =>   a^2 = P0 + 2*a0*a1*B^s0 + P2*B^(2*s0)
//	                                = P0 + ((a0+a1)^2 - P0 - P2)*B^s0 + P2*B^(2*s0)

// Base-case cutoff for squaring.  The square leaf is ~1.6-1.9x cheaper per limb than the
// multiply leaf, so the crossover with the Karatsuba recursion sits much higher than
// mul_toom22_threshold: measured, the asm leaf wins for s <= 36 while one recursion level
// wins from s = 37 onwards (36: 362 vs 364 ns, 40: 438 vs 425, 48: 626 vs 586).  version_0
// used 48 for its own toom22sqr against 24 for toom22mul - same direction, coarser probe.
// Kept as its own constant so it can be scanned without touching the multiply schedule.
inline constexpr uint64_t sqr_toom22_threshold = 37;

// Workspace size (limbs) for an equal-length square of s limbs.  A node keeps
// a1(m) + d1(2m) = 3m; the middle child runs after that region, while the low/high
// children run first and reuse the same space (same argument as mul_toom22_need, which
// needs 4m because it also keeps b1).
inline uint64_t sqr_toom22_need(uint64_t s) {
	uint64_t w = 0;
	for (uint64_t L = s; L >= sqr_toom22_threshold; ) {
		uint64_t m = ((L + 1) >> 1) + 1;
		w += m * 3;
		L = m;
	}
	return w;
}

// Equal-length Toom-22 square with caller-provided workspace w (>= sqr_toom22_need(s)).
inline void sqr_toom22_r(uint64_t* d, const uint64_t* a, uint64_t s, uint64_t* w) {
	if (s < sqr_toom22_threshold)
		return sqr_basecase_small(d, a, s);
	uint64_t s0 = (s + 1) >> 1, s1 = s - s0, m = s0 + 1;
	sqr_toom22_r(d, a, s0, w);                        // P0 = a0^2 -> d[0 .. 2*s0)
	sqr_toom22_r(d + (s0 << 1), a + s0, s1, w);       // P2 = a1^2 -> d[2*s0 .. 2*s)
	uint64_t* a1 = w;             // m limbs
	uint64_t* d1 = w + m;         // 2m limbs
	uint64_t* wc = w + (m * 3);   // workspace for the middle child
	a1[s0] = add(a1, a, a + s0, s0, s1);   // a1 = a0 + a1 (carry at a1[s0])
	uint64_t mid = a1[s0] ? m : s0;        // carry-free: the sum fits in s0 limbs
	sqr_toom22_r(d1, a1, mid, wc);         // (a0 + a1)^2
	if (mid < m)
		d1[mid << 1] = d1[(mid << 1) + 1] = 0;   // clear the 2 tail slots
	sub(d1, d1, d, m << 1, s0 << 1);             // d1 -= P0
	sub(d1, d1, d + (s0 << 1), m << 1, s1 << 1); // d1 -= P2
	add(d + s0, d + s0, d1, s + s1, m << 1);     // d[s0..] += cross
}

// dst[0 .. 2*s-1] = src[0 .. s-1]^2.  dst capacity must be at least 2*s limbs.  Unlike
// mul_toom22 there is no unbalanced entry: both operands are the same array.
inline void sqr_toom22(uint64_t* dst, const uint64_t* src, uint64_t s) {
	if (s < sqr_toom22_threshold)
		return sqr_basecase_small(dst, src, s);
	uint64_t* ws = new uint64_t[sqr_toom22_need(s)];
	sqr_toom22_r(dst, src, s, ws);
	delete[] ws;
}
