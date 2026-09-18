// natural.h - the big-integer front end, rebuilt on the version_1 engine.
//
// This is ntt/original/natural.h (kept here as version_0/natural.h) ported to the
// rebuilt module set.  The port is deliberately mechanical - same algorithms, same
// order, same edge-case handling - so that the two front ends can be diffed against
// each other bit for bit; only the calls into the engine changed:
//
//   version_0 (old)                            version_1 (new)
//   ----------------------------------------   -------------------------------------
//   basic_natural.h (basic_natural)         -> array_u64.h       (array_u64)
//   asm.h / asmSrc.asm: asmShl, asmShr      -> basic_arithmetics.h (shl, shr)
//                        asmAdd0/1, asmSub0/1-> basic_arithmetics.h (add, sub)
//                        asmMul0, asmMul1    -> basic_arithmetics.h (mul1, mul2)
//                        asmSqr              -> mul_basecase.s        (nat_sqr_basecase)
//   base.h: toom22mul/toom22sqr/mul_base    -> mul_toom22.h       (mul_toom22,
//                                              sqr_toom22)
//   asmDiv, asmMod, asmMulMod               -> barrett_reduction.h
//                                              (div_128_64, barrett_mod, barrett_mul_mod)
//   asmDivMod                               -> divmod_128_64()   (below - the rebuilt
//                                              headers only expose the quotient)
//   asmLoad, asmINttShr                     -> ntt_workspace.h: load(), intt_shr()
//   ntt_info.h (NTT_info)                   -> ntt_workspace.h   (ntt_workspace)
//
// Renames that came with ntt_workspace: NTT_info -> ntt_workspace, NTT_scale ->
// ntt_scale, info[3] -> ntt_data[3], NTT()/INTT()/CRT() -> ntt()/intt()/crt().
// The three thresholds that used to live in ntt_info.h (mul_ntt_threshold,
// div_ntt_threshold, mul_ntt_scale_threshold) are kept here, since ntt_workspace.h only
// carries the L1/L2/L3 cut points of the layer schedule.
//
// KNOWN-ISSUE HISTORY / current state of reciprocal():
// The Newton accuracy bookkeeping in reciprocal() is a heuristic.  For some divisors the
// estimate comes out below the accuracy already reached, and the correction it then composes
// can be longer than the new accuracy, so the composition feeds add() a negative length /
// len1 < len2.  version_0 aborts with heap corruption on such inputs (a 137-limb divisor with
// a dividend 300 limbs longer is a one-call reproducer, see verify/repro_recip.cpp).
//
// Current state in version_1:
//   * the 3-argument overload (used by div_iterative) applies a conservative correction -
//     one ulp is subtracted from the complemented error term before it is composed - which
//     keeps the error term on the safe side.  Measured: 190k+ randomized divisions with
//     0 wrong results and 0 crashes, 20k fork-isolated cases with 0 underflow /
//     0 length-mismatch events (verify/divstress_rand.cpp, verify/divstress.cpp), and the
//     reproducer above now completes instead of segfaulting.
//   * the 2-argument overload (used by calc_dec_base) instead clamps the accuracy estimate
//     to the accuracy already reached and keeps the composition in range, and it skips a
//     zero-length add (version_1's limb adder requires len2 >= 1).
//   * FIXED by the schoolbook dispatch below (see div_schoolbook_divisor_max): a divisor that
//     is two limbs with a top limb of 1 (b in [2^64, 2^65)) is left unnormalized by
//     div_iterative's n_shl = (countl_zero(top) + 1) & 63, which is 0 for that top limb.
//     reciprocal() then has a single Newton iteration to run and starts it from a zero
//     estimate, so the estimate came back degenerate (size 1), the quotient limb was 0 on
//     every pass, and div_iterative spun forever.  The reproducer, kept as a regression case:
//         natural a; a.resize(3); a[0] = a[1] = a[2] = ~0ull;   // 2^192 - 1
//         natural b; b.resize(2); b[0] = 1; b[1] = 1;          // 2^64 + 1
//         a / b;                    // used to spin; now schoolbook, q = 2^128 - 2^64, r = 2^64-1
//     Which low limbs hung was data-dependent (0, 1, 2, 3, 0x1234...f0, 0xFFFF...FD hung;
//     0x8000...0, 0xFFFF...FE, 0xFFFF...FF completed), so the whole family was treated as at
//     risk rather than one boundary.  Traced state on every hanging pass: b.size = 2,
//     n_shl = 0, rec.size = 1, idx = 2, r.size = 4 (7M+ identical iterations).  Found by
//     verify/divverify.cpp, whose "power-of-two shapes" section is this family.
//   * The same shape at five limbs and above was swept with the abort() marker that found the
//     above (8144 cases: 2^(64k)+1 for k = 5..400, top-limb-1 divisors with a second limb of
//     0/1/2, all-ones, sparse) and never degenerated, so the repair is the dispatch plus a
//     bounded fallback inside div_iterative, not a rewrite of the heuristic.
// A rigorous replacement of the bookkeeping is still the standing recommendation: Newton with
// an exact error term per step, or a different division strategy.
//
// Two consequences worth knowing:
//   * squaring: the base case now has its own kernel (mul_basecase.s's nat_sqr_basecase, which
//     halves the multiplies by doubling the strict pairs and adding the middle square
//     once) and its own Karatsuba recursion (mul_toom22.h's sqr_toom22 - one operand,
//     no unbalanced loop, 3m instead of 4m workspace per level).  _sqr_NTT goes through
//     nat_asmNttMul with nw_a == nw_b (the kernel reads all four butterfly inputs of a group
//     before storing any of them, so the transform-domain square is in-place safe -
//     this is what version_0 did too).
//   * the limb add/sub of version_1 return the final carry/borrow and take the two
//     source pointers before the two lengths, hence the shuffled argument lists below.
//
// Deliberate deviations from version_0 (all behaviour-preserving, both needed to build
// with g++ 13 on Linux):
//   * std::formatter<natural>::parse used <cctype>'s isupper/tolower, which are not
//     constant expressions under GCC; since parse() runs at compile time for a literal
//     format string, std::format("{:x}", n) did not compile at all.  It now does the
//     ASCII test inline (identical accept/reject behaviour), and format() ends with a
//     `return ctx.out();` on the path parse() makes unreachable.
//   * div_base() needs the old asmDivMod (quotient *and* remainder); barrett_reduction.h
//     only ships the quotient-only div_128_64, so divmod_128_64() is defined below.
//
// Everything here is `inline` (member functions, free functions and the namespace-scope
// objects) so that the header can be included from more than one translation unit,
// following the convention of array_u64.h / basic_arithmetics.h / barrett_reduction.h /
// mul_toom22.h / ntt_workspace.h.  version_0's copy could be included only once.
//
// Needs: g++ -masm=intel -march=native -std=c++20, x86-64 with BMI2.

#pragma once

#include "array_u64.h"
#include "barrett_reduction.h"
#include "basic_arithmetics.h"
#include "mul_toom22.h"
#include "ntt_workspace.h"
#include "ntt_wrap.h"

#include <queue>
#include <cctype>
#include <utility>
#include <format>
#include <string>
#include <iostream>
#include <algorithm>
#include <string_view>

namespace nat {

//	Thresholds.  mul_ntt_threshold was re-measured for this engine (verify/threshold.cpp,
//	verify/threshold.sh): below it _mul_base (Toom-22) wins, above it the NTT path wins.
//	The measured crossover for equal operands is ~900 limbs (ratio 0.99 at 896, 0.80 at
//	1024) and the loss curve over a 512..4096 grid is flat between 512 and 1024, minimum
//	at 896, while version_0's 384 costs ~6% on average and up to 53% on the shapes where
//	the transform is half empty.  div_ntt_threshold / mul_ntt_scale_threshold are unchanged:
//	they are used by the division/dec-conversion drivers, not by _mul.
inline constexpr uint64_t mul_ntt_threshold = 896;
inline constexpr uint64_t div_ntt_threshold = 512;
inline constexpr int mul_ntt_scale_threshold = 10;

//	Division dispatch: a shape this small goes to the schoolbook (Knuth D) path instead of
//	the Newton/iterative one.  Both bounds are limb counts and either one is enough to
//	qualify (a short divisor makes the reciprocal's setup the dominant cost; a short quotient
//	makes the iteration count trivial while that setup is still paid for).
//
//	Correctness is the reason the dispatch exists at all: a divisor whose top limb is 1 is
//	left unnormalized, and a *two-limb* one then gives reciprocal() a single Newton iteration
//	starting from a zero estimate, which comes back degenerate -- the quotient limb stays 0
//	and div_iterative never reduces r.  64 is a 32x margin over that shape.
//
//	Performance is why the bounds are this large rather than 2.  Measured with
//	verify/divschool.cpp (same process, alternating, min of 5 rounds, random digits, one
//	division per call) over a (divisor limbs x quotient limbs) grid: every cell with
//	divisor <= 256 and quotient <= 256
//	is at or below 1.00 (0.48-0.95 typical), the first clear losses appear at divisor 768
//	(1.16-1.63), and a short quotient keeps schoolbook competitive even at divisor 1024
//	(quotient 96: 0.79).  The iterative path pays for a fresh reciprocal per call, which is
//	what makes a schoolbook pass cheaper for so long; 64 sits well inside the winning region
//	without depending on the noisy part of the boundary.
inline constexpr uint64_t div_schoolbook_divisor_max = 64;
inline constexpr uint64_t div_schoolbook_quotient_max = 64;

//	floor((_High : _Low) / _Mod) with the remainder written to *_Rem: the old asmDivMod.
//	The rebuilt barrett_reduction.h only provides the quotient-only div_128_64, so the
//	remainder-returning twin lives here (div needs it for every single-limb step).
//	Requires _High < _Mod.
inline uint64_t divmod_128_64(uint64_t _Low, uint64_t _High, uint64_t _Mod, uint64_t* _Rem) {
	uint64_t _Q, _R;
	asm ("div %[m]" : "=a"(_Q), "=d"(_R) : "a"(_Low), "d"(_High), [m]"r"(_Mod) : "cc");
	*_Rem = _R;
	return _Q;
}

struct natural :array_u64 {

	natural(uint64_t n = 0) {
		resize(1);
		data[0] = n;
	}

	natural(std::string_view sv) {
		*this = sv;
	}

	natural(const natural& n) :array_u64(n) {}

	natural(std::initializer_list<uint64_t> n) :array_u64(n) {}

	natural(natural&& n) noexcept :array_u64(std::move(n)) {}

	natural& std();

	natural& operator=(uint64_t);

	natural& operator=(std::string_view);

	natural& operator=(const natural&);

	natural& operator=(natural&&) noexcept;

	natural& hex(std::string_view);

	natural& dec(std::string_view);

	natural& _or(const natural*, const natural*);

	natural& _xor(const natural*, const natural*);

	natural& _and(const natural*, const natural*);

	natural& _shl(const natural*, uint64_t);

	natural& _shr(const natural*, uint64_t);

	natural& _add(const natural*, const natural*);

	natural& _sub(const natural*, const natural*);

	natural& _mul_base(const natural*, const natural*);

	natural& _sqr_base(const natural*);

	natural& _mul_NTT(const natural*, const natural*);

	natural& _sqr_NTT(const natural*);

	natural& _mul(const natural*, const natural*);

	natural& _sqr(const natural*);

	natural& _div(const natural*, const natural*, natural*);

	void _dec_split(int, natural&, natural&)const;

	void _print_dec(std::ostream&, int, bool)const;

	void _format_dec(std::format_context&, int, bool)const;

};

inline natural& natural::std() {
	if (!size) {
		resize(1);
		data[0] = 0;
		return *this;
	}
	while (--size)
		if (data[size])
			break;
	size++;
	return *this;
}

inline natural& natural::operator=(uint64_t n) {
	resize(1);
	data[0] = n;
	return *this;
}

inline natural& natural::operator=(std::string_view sv) {
	if (sv.size() > 1 && sv[0] == '0' && tolower(sv[1]) == 'x') {
		sv.remove_prefix(2);
		hex(sv);
	}
	else
		dec(sv);
	return *this;
}

inline natural& natural::operator=(const natural& n) {
	array_u64::operator=(n);
	return *this;
}

inline natural& natural::operator=(natural&& n) noexcept {
	array_u64::operator=(std::move(n));
	return *this;
}

inline natural& natural::hex(std::string_view sv) {
	static const char hexStr[8] = { 0, 48, 55, 87 };
	if (sv.empty()) {
		*this = 0;
		return *this;
	}
	resize((sv.size() + 15) >> 4);
	//	i = number of zero digits the first (partial) 16-digit group is padded with, so
	//	every group is folded high-to-low with exactly sv.size() digits read.
	uint64_t i = -sv.size() & 15, j = size, k = 0;
	while (j--) {
		uint64_t x = 0;
		for (; i < 16; i++) {
			char c = sv[k++];
			c -= hexStr[c >> 5 & 3];
			x = x << 4 | c;
		}
		data[j] = x;
		i = 0;
	}
	std();
	return *this;
}

inline bool operator<(const natural& a, const natural& b) {
	if (a.size != b.size)
		return a.size < b.size;
	uint64_t i = a.size;
	while (i--)
		if (a[i] != b[i])
			return a[i] < b[i];
	return 0;
}

inline bool operator>(const natural& a, const natural& b) {
	return b < a;
}

inline bool operator<=(const natural& a, const natural& b) {
	return !(b < a);
}

inline bool operator>=(const natural& a, const natural& b) {
	return !(a < b);
}

inline bool operator==(const natural& a, const natural& b) {
	if (a.size != b.size)
		return 0;
	uint64_t i = a.size;
	while (i--)
		if (a[i] != b[i])
			return 0;
	return 1;
}

inline bool operator!=(const natural& a, const natural& b) {
	return !(a == b);
}

inline natural& natural::_or(const natural* ap, const natural* bp) {
	if (ap->size < bp->size)
		std::swap(ap, bp);
	resize(ap->size);
	for (uint64_t i = 0; i < bp->size; i++)
		data[i] = ap->data[i] | bp->data[i];
	memcpy(data + bp->size, ap->data + bp->size, (ap->size - bp->size) * sizeof(uint64_t));
	return *this;
}

inline natural& operator|=(natural& a, const natural& b) {
	a._or(&a, &b);
	return a;
}

inline natural operator|(const natural& a, const natural& b) {
	natural d;
	d._or(&a, &b);
	return d;
}

inline natural& natural::_xor(const natural* ap, const natural* bp) {
	if (ap->size < bp->size)
		std::swap(ap, bp);
	resize(ap->size);
	for (uint64_t i = 0; i < bp->size; i++)
		data[i] = ap->data[i] ^ bp->data[i];
	memcpy(data + bp->size, ap->data + bp->size, (ap->size - bp->size) * sizeof(uint64_t));
	std();
	return *this;
}

inline natural& operator^=(natural& a, const natural& b) {
	a._xor(&a, &b);
	return a;
}

inline natural operator^(const natural& a, const natural& b) {
	natural d;
	d._xor(&a, &b);
	return d;
}

inline natural& natural::_and(const natural* ap, const natural* bp) {
	uint64_t s = std::min(ap->size, bp->size);
	resize(s);
	for (uint64_t i = 0; i < s; i++)
		data[i] = ap->data[i] & bp->data[i];
	std();
	return *this;
}

inline natural& operator&=(natural& a, const natural& b) {
	a._and(&a, &b);
	return a;
}

inline natural operator&(const natural& a, const natural& b) {
	natural d;
	d._and(&a, &b);
	return d;
}

inline natural& natural::_shl(const natural* ap, uint64_t b) {
	if (*ap == 0) {
		*this = 0;
		return *this;
	}
	uint64_t as = ap->size, n = b >> 6;
	b &= 63;
	if (b == 0) {
		resize(as + n);
		memmove(data + n, ap->data, as * sizeof(uint64_t));
		memset(data, 0, n * sizeof(uint64_t));
		return *this;
	}
	resize(as + n + 1);
	shl(data + n, ap->data, b, as);
	memset(data, 0, n * sizeof(uint64_t));
	std();
	return *this;
}

inline natural& operator<<=(natural& a, uint64_t b) {
	a._shl(&a, b);
	return a;
}

inline natural operator<<(const natural& a, uint64_t b) {
	natural d;
	d._shl(&a, b);
	return d;
}

inline natural& natural::_shr(const natural* ap, uint64_t b) {
	uint64_t as = ap->size, n = b >> 6;
	if (as <= n) {
		*this = 0;
		return *this;
	}
	b &= 63;
	resize(as - n);
	if (b == 0) {
		memmove(data, ap->data + n, (as - n) * sizeof(uint64_t));
		return *this;
	}
	shr(data, ap->data + n, b, as - n);
	std();
	return *this;
}

inline natural& operator>>=(natural& a, uint64_t b) {
	a._shr(&a, b);
	return a;
}

inline natural operator>>(const natural& a, uint64_t b) {
	natural d;
	d._shr(&a, b);
	return d;
}

inline natural& natural::_add(const natural* ap, const natural* bp) {
	if (ap->size < bp->size)
		std::swap(ap, bp);
	uint64_t as = ap->size, bs = bp->size;
	resize(as + 1);
	data[as] = add(data, ap->data, bp->data, as, bs);
	std();
	return *this;
}

inline natural& operator+=(natural& a, const natural& b) {
	a._add(&a, &b);
	return a;
}

inline natural operator+(const natural& a, const natural& b) {
	natural d;
	d._add(&a, &b);
	return d;
}

inline natural& natural::_sub(const natural* ap, const natural* bp) {
	if (ap->size < bp->size)
		std::swap(ap, bp);
	uint64_t as = ap->size, bs = bp->size;
	if (as == bs) {
		while (--bs)
			if ((*ap)[bs] != (*bp)[bs])
				break;
		if ((*ap)[bs] < (*bp)[bs])
			std::swap(ap, bp);
		resize(++bs);
		sub(data, ap->data, bp->data, bs, bs);
	}
	else {
		resize(as);
		sub(data, ap->data, bp->data, as, bs);
	}
	std();
	return *this;
}

inline natural& operator-=(natural& a, const natural& b) {
	a._sub(&a, &b);
	return a;
}

inline natural operator-(const natural& a, const natural& b) {
	natural d;
	d._sub(&a, &b);
	return d;
}

inline natural& natural::_mul_base(const natural* ap, const natural* bp) {
	if (this == ap || this == bp) {
		natural ans;
		ans._mul_base(ap, bp);
		*this = std::move(ans);
		return *this;
	}
	if (ap->size < bp->size)
		std::swap(ap, bp);
	resize(ap->size + bp->size);
	mul_toom22(data, ap->data, bp->data, ap->size, bp->size);
	std();
	return *this;
}

inline natural& natural::_sqr_base(const natural* p) {
	if (this == p) {
		natural ans;
		ans._sqr_base(p);
		*this = std::move(ans);
		return *this;
	}
	resize(p->size << 1);
	sqr_toom22(data, p->data, p->size);
	std();
	return *this;
}

inline int get_NTT_scale(uint64_t _Size1, uint64_t _Size2) {
	return 64 - std::countl_zero(_Size1 + _Size2 - 2);
}

inline void mul_save_NTT_info(natural&, const natural&, const natural&, ntt_workspace&);
inline void mul_load_NTT_info(natural&, const natural&, const natural&, const ntt_workspace&);

inline natural& natural::_mul_NTT(const natural* ap, const natural* bp) {
//*
	if (ap->size < bp->size)
		std::swap(ap, bp);
	//	Version_2: in the "just above a power of two" band the transform is half empty and the
	//	wrap-corrected one-size-shorter product (ntt_wrap.h) is cheaper than both the direct
	//	transform and the a0/a1 split below.  The planner picks by cost model; everything after
	//	this block is the unchanged version_1 schedule.
	{
		int sc = get_NTT_scale(ap->size, bp->size);
		if (ntt_wrap_preferred(ap->size, bp->size, sc)) {
			//	*this may alias ap (operator*=, and every internal caller such as
			//	div/dec_base/calc_e does `x *= y`) or bp.  The product is therefore built
			//	in a separate buffer and moved into place: resizing *this first would both
			//	change the length ap->size that the planner above validated and, when the
			//	block moves, leave ap->data dangling -- and `data` would alias a source.
			//	(Measured before the fix: heap-buffer-overflow in ntt_workspace::load via
			//	mul_ntt_wrap, then glibc "malloc(): invalid size"; see verify/wrapalias.cpp.)
			const uint64_t l1 = ap->size, l2 = bp->size;
			natural prod;
			prod.resize(l1 + l2);
			mul_ntt_wrap(prod.data, ap->data, l1, bp->data, l2, sc);
			*this = std::move(prod);
			std();
			return *this;
		}
	}
	uint64_t a0_size = (ap->size + 1) >> 1;
	int NTT_scale = get_NTT_scale(ap->size, bp->size);
	int NTT_scale_opt = get_NTT_scale(a0_size, bp->size);
	if (NTT_scale == NTT_scale_opt) {
		ntt_workspace ni_a(*ap, NTT_scale), ni_b(*bp, NTT_scale);
		ni_a.ntt();
		ni_b.ntt();
		ni_a.mul(ni_a, ni_b);
		ni_a.intt();
		ni_a.save(*this, ap->size + bp->size);
		std();
		return *this;
	}
	natural a0, a1;
	a0.resize(a0_size);
	a1.resize(ap->size - a0_size);
	memcpy(a0.data, ap->data, sizeof(uint64_t) * a0_size);
	memcpy(a1.data, ap->data + a0_size, sizeof(uint64_t) * a1.size);
	ntt_workspace ni_b;
	ni_b.ntt_scale = NTT_scale_opt;
	mul_save_NTT_info(a0, *bp, a0, ni_b);
	mul_load_NTT_info(a1, *bp, a1, ni_b);
	*this = (a1 << (a0_size << 6)) + a0;
	return *this;
//*/
/*
	int NTT_scale = get_NTT_scale(ap->size, bp->size);
	ntt_workspace ni_a(*ap, NTT_scale), ni_b(*bp, NTT_scale);
	ni_a.ntt();
	ni_b.ntt();
	ni_a.mul(ni_a, ni_b);
	ni_a.intt();
	ni_a.save(*this, ap->size + bp->size);
	std();
	return *this;
//*/
}

inline natural& natural::_sqr_NTT(const natural* p) {
	int NTT_scale = get_NTT_scale(p->size, p->size);
	ntt_workspace ni(*p, NTT_scale);
	ni.ntt();
	ni.mul(ni, ni);
	ni.intt();
	ni.save(*this, p->size * 2);
	std();
	return *this;
}

inline natural& natural::_mul(const natural* ap, const natural* bp) {
	if (std::min(ap->size, bp->size) < mul_ntt_threshold)
		_mul_base(ap, bp);
	else
		_mul_NTT(ap, bp);
	return *this;
}

inline void mul_save_NTT_info(natural& d, const natural& a, const natural& b, ntt_workspace& ni_a) {
	if (ni_a.ntt_scale < mul_ntt_scale_threshold) {
		d._mul_base(&a, &b);
		ni_a.ntt_scale = 0;
	}
	else {
		ni_a.load(a, ni_a.ntt_scale);
		ntt_workspace ni_b(b, ni_a.ntt_scale);
		ni_a.ntt();
		ni_b.ntt();
		ni_b.mul(ni_a, ni_b);
		ni_b.intt();
		ni_b.save(d, a.size + b.size);
		d.std();
	}
}

inline void mul_save_NTT_info(natural& d, const natural& a, const natural& b, ntt_workspace& ni_a, ntt_workspace& ni_b) {
	if (ni_a.ntt_scale < mul_ntt_scale_threshold) {
		d._mul_base(&a, &b);
		ni_a.ntt_scale = 0;
		ni_b.ntt_scale = 0;
	}
	else {
		ni_a.load(a, ni_a.ntt_scale);
		ni_b.load(b, ni_a.ntt_scale);
		ni_a.ntt();
		ni_b.ntt();
		ntt_workspace ni;
		ni.mul(ni_a, ni_b);
		ni.intt();
		ni.save(d, a.size + b.size);
		d.std();
	}
}

inline void sqr_save_NTT_info(natural& d, const natural& a, ntt_workspace& ni_a) {
	if (ni_a.ntt_scale < mul_ntt_scale_threshold) {
		d._sqr_base(&a);
		ni_a.ntt_scale = 0;
	}
	else {
		ni_a.load(a, ni_a.ntt_scale);
		ntt_workspace ni_b;
		ni_a.ntt();
		ni_b.mul(ni_a, ni_a);
		ni_b.intt();
		ni_b.save(d, a.size * 2);
		d.std();
	}
}

inline void mul_load_NTT_info(natural& d, const natural& a, const natural& b, const ntt_workspace& ni_a) {
	if (ni_a.ntt_scale == 0)
		d._mul_base(&a, &b);
	else {
		ntt_workspace ni_b(b, ni_a.ntt_scale);
		ni_b.ntt();
		ni_b.mul(ni_a, ni_b);
		ni_b.intt();
		ni_b.save(d, a.size + b.size);
		d.std();
	}
}

inline void mul_load_NTT_info(natural& d, const natural& a, const natural& b, const ntt_workspace& ni_a, const ntt_workspace& ni_b) {
	if (ni_a.ntt_scale == 0)
		d._mul_base(&a, &b);
	else {
		ntt_workspace ni;
		ni.mul(ni_a, ni_b);
		ni.intt();
		ni.save(d, a.size + b.size);
		d.std();
	}
}

inline natural& natural::_sqr(const natural* p) {
	if (p->size < mul_ntt_threshold)
		_sqr_base(p);
	else
		_sqr_NTT(p);
	return *this;
}

inline natural& operator*=(natural& a, const natural& b) {
	a._mul(&a, &b);
	return a;
}

inline natural operator*(const natural& a, const natural& b) {
	natural d;
	d._mul(&a, &b);
	return d;
}

inline natural sqr(const natural& a) {
	natural d;
	d._sqr(&a);
	return d;
}

inline natural pow(const natural& b, uint64_t e) {
	if (e == 0)
		return 1;
	if (e == 1 || b < 2)
		return b;
	natural p = pow(b, e >> 1);
	p._sqr(&p);
	if (e & 1)
		p *= b;
	return p;
}

inline uint64_t div_base(const natural& a, uint64_t b, natural& q) {
	q.resize(a.size);
	uint64_t r = 0, i = a.size;
	while (i--)
		q[i] = divmod_128_64(a[i], r, b, &r);
	q.std();
	return r;
}

//	Multi-limb long division (Knuth's algorithm D): q = a / b, returns a % b.  Used for the
//	shapes below the dispatch bounds and, unlike div_iterative(), it has no heuristic in it --
//	every step is an exact estimate plus the standard corrections, so it either returns the
//	right answer or does not return at all.
//
//	Preconditions: a >= b, b.size >= 2 (the caller has already handled b.size == 1 and a < b).
inline natural divmod_schoolbook(const natural& a, const natural& b, natural& q) {
	const uint64_t n = a.size, m = b.size;
	const uint64_t s = std::countl_zero(b[m - 1]);      // normalise: top bit of v[m-1] set

	natural v = b << s;
	natural u = a << s;                                 // n or n+1 limbs
	if (u.size < n + 1) {
		u.resize(n + 1);                                // resize() does not zero-fill
		u[n] = 0;
	}
	q.resize(n - m + 1);

	for (uint64_t j = n - m + 1; j-- > 0; ) {
		//	qhat = (u[j+m] : u[j+m-1]) / v[m-1].  The invariant is u[j+m] <= v[m-1]; at
		//	equality the true quotient does not fit in a limb (x86's div would trap), so the
		//	estimate is clamped and rhat is carried in 128 bits -- the correction below then
		//	still distinguishes "too large" from "rhat already past 2^64".  The other branch
		//	can use divmod_128_64 because it requires High < Mod.
		uint64_t qhat;
		unsigned __int128 rhat;
		if (u[j + m] == v[m - 1]) {
			qhat = ~0ull;
			rhat = (unsigned __int128)u[j + m - 1] + v[m - 1];
		} else {
			uint64_t r64 = 0;
			qhat = divmod_128_64(u[j + m - 1], u[j + m], v[m - 1], &r64);
			rhat = r64;
		}
		//	Knuth's correction (D3): qhat may be one or two too large.  Once rhat reaches
		//	2^64 the test can no longer fail, which is what bounds the loop -- and it leaves
		//	the estimate at most one too large, so the multiply-subtract needs one add-back.
		while (rhat < ((unsigned __int128)1 << 64) &&
		       (unsigned __int128)qhat * v[m - 2] > (rhat << 64) + u[j + m - 2]) {
			qhat -= 1;
			rhat += v[m - 1];
		}
		//	u[j .. j+m] -= qhat * v[0 .. m-1], with the product formed on the fly.
		uint64_t carry = 0;
		uint64_t borrow = 0;
		for (uint64_t i = 0; i < m; i++) {
			unsigned __int128 p = (unsigned __int128)v[i] * qhat + carry;
			carry = (uint64_t)(p >> 64);
			unsigned __int128 t = (unsigned __int128)u[j + i] - (uint64_t)p - borrow;
			u[j + i] = (uint64_t)t;
			borrow = (uint64_t)((t >> 64) & 1);
		}
		unsigned __int128 t = (unsigned __int128)u[j + m] - carry - borrow;
		u[j + m] = (uint64_t)t;
		if (t >> 64) {                                  // qhat was one too large: add it back
			qhat -= 1;
			carry = 0;
			for (uint64_t i = 0; i < m; i++) {
				unsigned __int128 t2 = (unsigned __int128)u[j + i] + v[i] + carry;
				u[j + i] = (uint64_t)t2;
				carry = (uint64_t)(t2 >> 64);
			}
			u[j + m] = (uint64_t)((unsigned __int128)u[j + m] + carry);
		}
		q[j] = qhat;
	}
	q.std();

	natural r;
	r.resize(m);
	for (uint64_t i = 0; i < m; i++)
		r[i] = u[i];
	r.std();
	if (s)
		r >>= s;
	return r;
}

//	divs[divs.size - 1] == 1;
inline natural reciprocal(const natural& divs, uint64_t divd_size) {
	uint64_t expected_acc = divd_size + 1 - divs.size;
	natural ans = (1ull << 63 | divs[divs.size - 2] >> 1) + 1;
	if (ans[0] == 0)
		ans[0] = 1ull << 63;
	else
		ans[0] = div_128_64(0, 1ull << 63, ans[0]);

	while (1) {
		uint64_t nxt_acc = 0, cur_acc = ans.size;
		if (cur_acc >= expected_acc + 1) {
			ans >>= (cur_acc - expected_acc) << 6;
			return ans;
		}

		natural tmp;
		if (divs.size > 2 * cur_acc + 1)
			tmp._shr(&divs, (divs.size - 2 * cur_acc - 1) << 6);
		else
			tmp._shl(&divs, (2 * cur_acc + 1 - divs.size) << 6);

		ntt_workspace ni_ans(get_NTT_scale(tmp.size, 1));
		mul_save_NTT_info(tmp, ans, tmp, ni_ans);

		tmp.resize(tmp.size - cur_acc);
		for (uint64_t i = 0; i < tmp.size; i++)
			tmp[i] = ~tmp[i + cur_acc];
		tmp.std();

		int l_zero = std::countl_zero(tmp[tmp.size - 1]);
		nxt_acc = std::min(2 * cur_acc, (2 * cur_acc - tmp.size) * 2 + ((l_zero + 31) >> 5));
		//	This estimate is a heuristic and can come out below the accuracy already reached
		//	(the product it looks at need not have a zero top limb); the shifts and the add
		//	below then underflow.  Newton never loses accuracy, so clamp it.
		if (nxt_acc < cur_acc)
			nxt_acc = cur_acc;
		if (nxt_acc == div_ntt_threshold && nxt_acc > cur_acc)
			nxt_acc = div_ntt_threshold - 1;
		tmp >>= (2 * cur_acc - nxt_acc) << 6;
		if (tmp == 0)
			tmp = 1;

		mul_load_NTT_info(tmp, ans, tmp, ni_ans);
		ans <<= (nxt_acc - cur_acc) << 6;
		//	tmp.size <= cur_acc means there is nothing to add; version_0's add(dst, src1, n,
		//	src2, 0) was a no-op there, while version_1's fused limb adder needs len2 >= 1.
		//	The correction is also not allowed to be longer than the destination.
		if (tmp.size > cur_acc) {
			if (tmp.size - cur_acc > ans.size)
				tmp.resize(cur_acc + ans.size);
			add(ans.data, ans.data, tmp.data + cur_acc, ans.size, tmp.size - cur_acc);
		}
	}
}

//	divisor[divisor.size - 1] == 1;
inline natural reciprocal(const natural& divisor, uint64_t dividend_size, ntt_workspace& ni_divisor) {
	uint64_t expected_acc = dividend_size + 1 - divisor.size;
	natural ans = (1ull << 63 | divisor[divisor.size - 2] >> 1) + 1;
	ans[0] = ans[0] ? div_128_64(0, 1ull << 63, ans[0]) : 1ull << 63;

	for (uint64_t i = 1; i < divisor.size; i *= 2) {
		uint64_t prev_acc = ans.size, divisor_seg_size = 2 * prev_acc + 1;
		if (2 * i >= divisor.size)
			divisor_seg_size = divisor.size;
		uint64_t *divisor_seg = divisor.data + divisor.size - divisor_seg_size;

		natural tmp;
		ntt_workspace ni_ans;
		if (2 * i <= div_ntt_threshold) {
			tmp.resize(divisor_seg_size + prev_acc);
			mul_toom22(tmp.data, divisor_seg, ans.data, divisor_seg_size, prev_acc);
			tmp.std();
		}
		else {
			int NTT_scale = get_NTT_scale(i, i);
			ni_ans.load(ans, NTT_scale);
			ni_divisor.load(divisor_seg, divisor_seg_size, NTT_scale);
			ni_ans.ntt();
			ni_divisor.ntt();
			ntt_workspace ni_tmp;
			ni_tmp.mul(ni_ans, ni_divisor);
			ni_tmp.intt();
			ni_tmp.save(tmp, divisor_seg_size + prev_acc);
			tmp.std();
		}

		uint64_t n_shl = divisor_seg_size - 1 - prev_acc;
		while (tmp.size > n_shl + 1 && ~tmp[tmp.size - 1] == 0)
			tmp.size -= 1;
		int l_zero = std::countl_zero(~tmp[tmp.size - 1]);
		uint64_t cur_acc = std::min(2 * prev_acc, (2 * prev_acc - tmp.size + n_shl) * 2 + ((l_zero + 31) >> 5));
		if (cur_acc == div_ntt_threshold)
			cur_acc -= 1;
		n_shl += 2 * prev_acc - cur_acc;
		if (tmp.size <= n_shl)
			tmp = 1;
		else {
			tmp.resize(tmp.size - n_shl);
			for (uint64_t i = 0; i < tmp.size; i++)
				tmp[i] = ~tmp[i + n_shl];
			tmp.std();
			tmp -= 1;
		}

		mul_load_NTT_info(tmp, ans, tmp, ni_ans);
		tmp >>= prev_acc << 6;
		uint64_t inc_acc = cur_acc - prev_acc;
		if (inc_acc >= tmp.size) {
			memset(tmp.data + tmp.size, 0, sizeof(uint64_t) * (inc_acc - tmp.size));
			memcpy(tmp.data + inc_acc, ans.data, sizeof(uint64_t) * prev_acc);
		}
		else
			add(tmp.data + inc_acc, ans.data, tmp.data + inc_acc, prev_acc, tmp.size - inc_acc);
		tmp.resize(cur_acc);
		ans = std::move(tmp);

		if (cur_acc >= expected_acc + 1) {
			ans >>= (cur_acc - expected_acc) << 6;
			return ans;
		}
	}
	ans >>= 64;
	return ans;
}

inline void div_iterative(const natural& divd, const natural& divs, natural& q, natural& r) {
	int n_shl = (std::countl_zero(divs[divs.size - 1]) + 1) & 63;
	natural b = divs << n_shl;
	r = divd << n_shl;

	ntt_workspace ni_b;
	natural rec = reciprocal(b, r.size, ni_b);
	ntt_workspace ni_rec(get_NTT_scale(rec.size, rec.size));
	int NTT_scale_b = get_NTT_scale(b.size, 1);
	uint64_t NTT_size_b = 1ull << NTT_scale_b;
	if (ni_b.ntt_scale != NTT_scale_b && NTT_scale_b >= mul_ntt_scale_threshold - 1) {
		ni_b.load(b, NTT_scale_b);
		ni_b.ntt();
	}

	q.resize(r.size - b.size + 1);
	memset(q.data, 0, sizeof(uint64_t) * q.size);

	natural tmp;
	int first = 1;
	while (1) {
		uint64_t idx = 0;
		if (r.size > rec.size + b.size - 1)
			idx = r.size - rec.size - b.size + 1;

		if (first)
			mul_save_NTT_info(tmp, rec, r >> ((idx + b.size - 1) << 6), ni_rec);
		else
			mul_load_NTT_info(tmp, rec, r >> ((idx + b.size - 1) << 6), ni_rec);

		first = 0;
		tmp >>= rec.size << 6;
		//	The quotient limb estimate has degenerated (the accuracy heuristic came back
		//	unusable, which is what makes this loop spin: the limb stays 0 and r never
		//	shrinks).  The dispatch above keeps the shapes where that is known to happen away
		//	from here, so this is the belt-and-braces path -- finish with the exact one.
		if (tmp == 0) {
			natural qq;
			natural rr = divmod_schoolbook(divd, divs, qq);
			q = std::move(qq);
			r = std::move(rr);
			return;
		}
		add(q.data + idx, q.data + idx, tmp.data, q.size - idx, tmp.size);

		mul_load_NTT_info(tmp, b, tmp, ni_b);
		sub(r.data + idx, r.data + idx, tmp.data, r.size - idx, tmp.size);
		r.std();

		if (ni_b.ntt_scale) {
			if (r.size - idx > NTT_size_b) {
				uint64_t cf = add(r.data + idx, r.data + idx, r.data + idx + NTT_size_b, NTT_size_b, r.size - idx - NTT_size_b);
				add(r.data + idx, r.data + idx, &cf, NTT_size_b, 1);
				r.resize(idx + NTT_size_b);
			}
			if (r.size == idx + NTT_size_b && r[r.size - 1] >> 63)
				r.resize(idx);
			r.std();
		}

		if (idx == 0)
			break;
	}

	while (r >= b) {
		q += 1;
		r -= b;
	}
	r >>= n_shl;
	//	q was sized len(dividend) - len(divisor) + 1 up front, so a quotient whose top
	//	limb came out zero kept that limb (making q.size, and therefore q == a, wrong).
	q.std();
}

inline natural& natural::_div(const natural* ap, const natural* bp, natural* r) {
	if (*bp == 0) {
		if (*ap == 0)
			*this = 1ull;
		else
			*this = 0;
		*r = 0;
		return *this;
	}
	if (*ap < *bp) {
		*this = 0;
		*r = *ap;
		return *this;
	}
	if (bp->size == 1) {
		*r = div_base(*ap, (*bp)[0], *this);
		return *this;
	}
	if (bp->size <= div_schoolbook_divisor_max ||
	    ap->size - bp->size + 1 <= div_schoolbook_quotient_max) {
		*r = divmod_schoolbook(*ap, *bp, *this);
		return *this;
	}
	div_iterative(*ap, *bp, *this, *r);
	return *this;
}

inline natural& operator/=(natural& a, const natural& b) {
	natural r;
	a._div(&a, &b, &r);
	return a;
}

inline natural operator/(const natural& a, const natural& b) {
	natural q, r;
	q._div(&a, &b, &r);
	return q;
}

inline natural& operator%=(natural& a, const natural& b) {
	natural q;
	q._div(&a, &b, &a);
	return a;
}

inline natural operator%(const natural& a, const natural& b) {
	natural q, r;
	q._div(&a, &b, &r);
	return r;
}

inline natural factorial(uint64_t n) {
	if (n <= 1)
		return 1;
	if (n == 2)
		return 2;
	bool* is_prime = new bool[n + 1];
	uint64_t* prime = new uint64_t[n], * prime_exp = new uint64_t[n], prime_cnt = 0;
	memset(is_prime, 1, (n + 1) * sizeof(bool));
	for (uint64_t i = 2; i <= n; i++) {
		if (is_prime[i]) {
			prime[prime_cnt] = i;
			prime_exp[prime_cnt] = 0;
			uint64_t _n = n;
			while (_n)
				prime_exp[prime_cnt] += _n /= i;
			prime_cnt++;
		}
		for (uint64_t j = 0; j < prime_cnt && i * prime[j] <= n; j++) {
			is_prime[i * prime[j]] = 0;
			if (i % prime[j] == 0)
				break;
		}
	}
	natural ans = 1;
	for (int i = 63 - std::countl_zero(prime_exp[1]); i >= 0; i--) {
		std::priority_queue<natural, std::vector<natural>, std::greater<natural> > pq;
		for (uint64_t j = 1; j < prime_cnt; j++) {
			if (prime_exp[j] < 1ull << i)
				break;
			if (prime_exp[j] & 1ull << i)
				pq.push(prime[j]);
		}
		while (pq.size() > 1) {
			natural n1 = pq.top();
			pq.pop();
			natural n2 = pq.top();
			pq.pop();
			pq.push(n1 * n2);
		}
		ans = sqr(ans) * pq.top();
	}
	ans <<= prime_exp[0];
	delete[] is_prime;
	delete[] prime;
	delete[] prime_exp;
	return ans;
}

inline void calc_e(natural& num, natural& den, uint64_t begin, uint64_t end) {
	if (end - begin == 2) {
		num = end;
		den = begin + 1;
		if (begin)
			den *= begin;
		return;
	}
	natural num1, den1;
	calc_e(num, den, begin, (begin + end) >> 1);
	calc_e(num1, den1, (begin + end) >> 1, end);
	ntt_workspace ni_den1;
	ni_den1.ntt_scale = get_NTT_scale(std::max(den.size, num.size), den1.size);
	mul_save_NTT_info(num, den1, num, ni_den1);
	mul_load_NTT_info(den, den1, den, ni_den1);
	num += num1;
}

inline void sqrt_10005(natural& num, natural& den, uint64_t acc) {
	num = 4001;
	den = 40;
	natural cache1, cache2;
	while (num.size + den.size - 2 < acc) {
		ntt_workspace ni_num, ni_den;
		ni_num.ntt_scale = ni_den.ntt_scale = get_NTT_scale(num.size, num.size);
		sqr_save_NTT_info(cache1, den, ni_den);
		sqr_save_NTT_info(cache2, num, ni_num);
		mul_load_NTT_info(den, num, den, ni_num, ni_den);
		den <<= 1;
		num = cache1 * 10005 + cache2;
	}
}

inline void calc_pi(natural& num, natural& den, natural& numc, uint64_t begin, uint64_t end) {
	if (end - begin == 1) {
		den = begin;
		den = den * den * den * 0x26dd041d878000ull;
		numc = 2 * begin - 1;
		numc *= 6 * begin - 1;
		numc *= 6 * begin - 5;
		num = begin;
		num = (0x207e2da6ull * num + 0xcf6371) * numc;
		return;
	}
	natural num1, den1, numc1;
	calc_pi(num, den, numc, begin, (begin + end) >> 1);
	calc_pi(num1, den1, numc1, (begin + end) >> 1, end);
	if (end - begin == 2) {
		num = numc * num1 - num * den1;
		den = den * den1;
		numc = numc * numc1;
		return;
	}
	ntt_workspace ni_den1, ni_numc;
	ni_den1.ntt_scale = get_NTT_scale(std::max(den.size, num.size), den1.size);
	mul_save_NTT_info(num, den1, num, ni_den1);
	mul_load_NTT_info(den, den1, den, ni_den1);
	ni_numc.ntt_scale = get_NTT_scale(std::max(numc1.size, num1.size), numc.size);
	mul_save_NTT_info(num1, numc, num1, ni_numc);
	mul_load_NTT_info(numc, numc, numc1, ni_numc);
	num = num + num1;
}

inline constexpr uint64_t dec_length = 18, dec_base_0 = 1'000'000'000'000'000'000ull;
inline natural dec_base[64] = { dec_base_0 }, dec_base_r[64];
inline ntt_workspace ni_dec_base[64], ni_dec_base_r[64];

//	Calculate dec_base[i + 1] dec_base_r[i]
inline void calc_dec_base(int i) {
	if (dec_base[i + 1] == 0) {
		ni_dec_base[i].ntt_scale = get_NTT_scale(dec_base[i].size, dec_base[i].size);
		sqr_save_NTT_info(dec_base[i + 1], dec_base[i], ni_dec_base[i]);
		int n_shl = (std::countl_zero(dec_base[i][dec_base[i].size - 1]) + 1) & 63;
		dec_base_r[i] = reciprocal(dec_base[i] << n_shl, dec_base[i].size * 2 + 1);
		ni_dec_base_r[i].ntt_scale = get_NTT_scale(dec_base_r[i].size, dec_base[i].size + 2);
		if (ni_dec_base_r[i].ntt_scale < mul_ntt_scale_threshold)
			ni_dec_base_r[i].ntt_scale = 0;
		else {
			ni_dec_base_r[i].load(dec_base_r[i], ni_dec_base_r[i].ntt_scale);
			ni_dec_base_r[i].ntt();
		}
	}
}

inline natural& natural::dec(std::string_view sv) {
	int i = 63 - std::countl_zero(uint64_t((sv.size() + dec_length - 1) / dec_length - 1));
	if (i == -1) {
		*this = 0;
		for (uint64_t i = 0; i < sv.size(); i++)
			data[0] = data[0] * 10 + sv[i] - '0';
		return *this;
	}
	if (dec_base[i] == 0)
		for (int j = 0; j < i; j++)
			calc_dec_base(j);
	natural hi, lo;
	hi.dec(sv.substr(0, sv.size() - (dec_length << i)));
	lo.dec(sv.substr(sv.size() - (dec_length << i)));
	*this = hi * dec_base[i] + lo;
	return *this;
}

inline std::istream& operator >>(std::istream& is, natural& n) {
	std::string s;
	is >> s;
	if (is.flags() & std::ios::hex)
		n.hex(s);
	else if (is.flags() & std::ios::dec)
		n.dec(s);
	return is;
}

inline void natural::_dec_split(int i, natural& hi, natural& lo)const {
	if (i == 0)
		lo = div_base(*this, dec_base_0, hi);
	else {
		int n_shl = (std::countl_zero(dec_base[i][dec_base[i].size - 1]) + 1) & 63;
		mul_load_NTT_info(hi, dec_base_r[i], *this >> ((dec_base[i].size - 1) << 6), ni_dec_base_r[i]);
		hi >>= ((dec_base[i].size + 2) << 6) - n_shl;
		mul_load_NTT_info(lo, dec_base[i], hi, ni_dec_base[i]);
		lo = *this - lo;
		while (lo >= dec_base[i]) {
			hi += 1;
			lo -= dec_base[i];
		}
	}
}

inline void natural::_print_dec(std::ostream& os, int i, bool is_first)const {
	if (i == -1) {
		if (is_first) {
			os.setf(std::ios_base::right);
			os << data[0];
			os.unsetf(std::ios_base::showbase);
			os.fill('0');
		}
		else {
			os.width(dec_length);
			os << data[0];
		}
		return;
	}
	natural hi, lo;
	_dec_split(i, hi, lo);
	if (is_first && hi == 0) {
		lo._print_dec(os, i - 1, true);
		return;
	}
	hi._print_dec(os, i - 1, is_first);
	lo._print_dec(os, i - 1, false);
	return;
}

inline void natural::_format_dec(std::format_context& ctx, int i, bool is_first)const {
	if (i == -1) {
		if (is_first)
			std::format_to(ctx.out(), "{}", data[0]);
		else
			std::format_to(ctx.out(), "{:0{}}", data[0], dec_length);
		return;
	}
	natural hi, lo;
	_dec_split(i, hi, lo);
	if (is_first && hi == 0) {
		lo._format_dec(ctx, i - 1, true);
		return;
	}
	hi._format_dec(ctx, i - 1, is_first);
	lo._format_dec(ctx, i - 1, false);
	return;
}

inline int dec_init(const natural& n) {
	int i = 0;
	for (; n >= dec_base[i]; i++)
		calc_dec_base(i);
	return i - 1;
}

inline std::ostream& operator <<(std::ostream& os, const natural& n) {
	std::ios_base::fmtflags oldFlags = os.flags();
	char oldFill = os.fill();
	if (oldFlags & std::ios::hex) {
		uint64_t i = n.size - 1;
		os.setf(std::ios_base::right);
		os << n[i];
		os.unsetf(std::ios_base::showbase);
		os.fill('0');
		while (i--) {
			os.width(16);
			os << n[i];
		}
	}
	else if (oldFlags & std::ios::dec)
		n._print_dec(os, nat::dec_init(n), true);
	os.fill(oldFill);
	os.flags(oldFlags);
	return os;
}


}  // namespace nat

template<>
struct std::formatter<nat::natural> {
	bool prefix = false;
	bool upper = false;
	char base = 'd';

	constexpr auto parse(std::format_parse_context& ctx) {
		auto it = ctx.begin();
		if (it == ctx.end() || *it == '}')
			return it;
		if (*it == '#') {
			prefix = true;
			++it;
		}
		if (*it == '}')
			return it;
		//	ASCII upper/lower instead of version_0's isupper/tolower: <cctype>'s versions
		//	are not constant expressions under GCC, and parse() is evaluated at compile
		//	time whenever the format string is a literal (so std::format("{:x}", n) was
		//	rejected outright).  Same accept/reject behaviour as the original.
		upper = *it >= 'A' && *it <= 'Z';
		base = upper ? char(*it + ('a' - 'A')) : *it;
		if (base != 'd' && base != 'x')
			throw std::format_error("Invalid format specifier for natural.");
		++it;
		if (*it != '}')
			throw std::format_error("Invalid format specifier for natural.");
		return it;
	}

	auto format(const nat::natural& n, std::format_context& ctx) const {
		if (base == 'x') {
			auto out = ctx.out();
			if (prefix) {
				if (upper)
					out = std::format_to(out, "0X");
				else
					out = std::format_to(out, "0x");
			}
			uint64_t i = n.size - 1;
			if (upper) {
				out = std::format_to(out, "{:X}", n[i]);
				while (i--)
					out = std::format_to(out, "{:016X}", n[i]);
			}
			else {
				out = std::format_to(out, "{:x}", n[i]);
				while (i--)
					out = std::format_to(out, "{:016x}", n[i]);
			}
			return out;
		}
		else if (base == 'd') {
			n._format_dec(ctx, nat::dec_init(n), true);
			return ctx.out();
		}
		return ctx.out();   // unreachable: parse() rejects every base except 'd' and 'x'
	}
};
