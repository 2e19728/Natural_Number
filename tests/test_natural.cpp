// test.cpp - self-checking test suite for the natural big-integer library.
//
//	./test                 correctness suite only (default, a few seconds)
//	./test bench [limbs]   multiplication / squaring throughput (default up to 2^20 limbs)
//	./test all [limbs]     both
//
// Everything is checked against an independent oracle, so a green run is meaningful
// without any external library:
//
//	* a plain O(n*m) schoolbook multiplication written here in unsigned __int128
//	  (ref_mul below) pins down the assembly base case and the Toom-22 recursion;
//	* the three multiply paths - base case (_mul_base), NTT (_mul_NTT) and the planner
//	  that chooses between them (operator*) - must agree limb for limb;
//	* the square (sqr / _sqr_base / _sqr_NTT) must agree with a * a;
//	* the wrap-corrected "one size shorter" product must agree with ntt_wrap_enable
//	  turned off, over the sawtooth band where the planner prefers it;
//	* the NTT schedule plan must tile layers 2^(k-2)..2^1 exactly -- the merged ranges
//	  plus the fixed distance-2 tail of the finest level -- leaving one unpaired layer,
//	  the distance-8 layer 3, only when k is odd, and a product driven at an explicit
//	  scale must equal the base case for every shape of the plan (DRAM alone .. all four
//	  levels, even and odd scales);
//	* the AVX-512 IFMA radix-4 kernels must be bit-identical to the scalar kernels they
//	  replace (every D, both directions), and the whole engine with them on and off must
//	  produce identical products and squares;
//	* division must invert multiplication: (a/b)*b + a%b == a, (a*b)/b == a;
//	* shifts, add/sub and hex/decimal round trips.
//
// Build: see compile.sh (g++ -masm=intel -march=native -std=c++20 -O3 mul_basecase.s
//        mul_ntt.s test.cpp -o test).

#include <natural/natural.h>

#include <chrono>

using namespace nat;
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <format>
#include <string>
#include <vector>

// ------------------------------------------------------------------ test harness
static int g_checks = 0, g_fails = 0;
static const char* g_group = "";

static void group(const char* name) {
	g_group = name;
	std::printf("  %-34s", name);
}
static void check(bool ok, const char* what) {
	g_checks++;
	if (!ok) {
		g_fails++;
		std::printf("\n    FAIL: %s [%s]\n", what, g_group);
	}
}
static void done() {
	std::printf(" ok\n");
}

// ------------------------------------------------------------------ helpers
static uint64_t rs = 0x9e3779b97f4a7c15ull;
static uint64_t rnd() { rs ^= rs << 13; rs ^= rs >> 7; rs ^= rs << 17; return rs; }

// n-limb natural with a reproducible random pattern.
//   pat 0 random   1 all-ones   2 alternating   3 top half zero
//   4 single limb in the middle  5 0x8000..   6 small (only the lowest limb set)
static natural rnat(uint64_t n, int pat = 0) {
	natural a;
	a.resize(n);
	for (uint64_t i = 0; i < n; i++) a[i] = rnd();
	if (pat == 1) for (uint64_t i = 0; i < n; i++) a[i] = ~0ull;
	if (pat == 2) for (uint64_t i = 0; i < n; i++) a[i] = (i & 1) ? 0 : ~0ull;
	if (pat == 3) for (uint64_t i = (n + 1) / 2; i < n; i++) a[i] = 0;
	if (pat == 4) { for (uint64_t i = 0; i < n; i++) a[i] = 0; a[n / 2] = ~0ull; }
	if (pat == 5) for (uint64_t i = 0; i < n; i++) a[i] = 1ull << 63;
	if (pat == 6) for (uint64_t i = 1; i < n; i++) a[i] = 0;
	a.std();
	return a;
}

// Independent schoolbook reference: O(n*m) with 128-bit accumulation, no asm involved.
static std::vector<uint64_t> ref_mul(const natural& a, const natural& b) {
	std::vector<uint64_t> c(a.size + b.size, 0);
	for (uint64_t i = 0; i < a.size; i++) {
		uint64_t carry = 0;
		for (uint64_t j = 0; j < b.size; j++) {
			unsigned __int128 t = (unsigned __int128)a[i] * b[j] + c[i + j] + carry;
			c[i + j] = (uint64_t)t;
			carry = (uint64_t)(t >> 64);
		}
		c[i + b.size] = carry;
	}
	return c;
}

// does the natural's limbs equal the reference vector, ignoring leading zeros?
static bool same(const natural& got, const std::vector<uint64_t>& want) {
	uint64_t n = want.size();
	while (n > 1 && want[n - 1] == 0) n--;
	if (got.size != n) return false;
	return std::memcmp(got.data, want.data(), sizeof(uint64_t) * n) == 0;
}

// ------------------------------------------------------------------ groups
static void t_parse_format() {
	group("parse / format round trip");
	const char* vals[] = { "0", "1", "2", "0xffffffffffffffff", "0x10000000000000000",
		"0xffffffffffffffffffffffffffffffff", "340282366920938463463374607431768211455",
		"12345678901234567890123456789012345678901234567890",
		"0x123456789abcdef0123456789abcdef0" };
	for (const char* v : vals) {
		natural x{ std::string_view(v) };
		check(natural(std::format("{}", x)) == x, "decimal round trip");
		check(natural(std::format("0x{:x}", x)) == x, "hex round trip");
	}
	for (int i = 0; i < 40; i++) {
		natural x = rnat(1 + rnd() % 600);
		check(natural(std::format("{}", x)) == x, "random decimal round trip");
		check(natural(std::format("0x{:x}", x)) == x, "random hex round trip");
	}
	done();
}

static void t_add_sub_shift() {
	group("add / sub / shift identities");
	for (int i = 0; i < 120; i++) {
		natural a = rnat(1 + rnd() % 700), b = rnat(1 + rnd() % 700);
		check((a + b) - b == a, "(a+b)-b == a");
		check((a + b) - a == b, "(a+b)-a == b");
		check(a - a == natural(0), "a-a == 0");
		uint64_t k = rnd() % 200;
		check((a << k) >> k == a, "(a<<k)>>k == a");
		check((a << k) == a * (natural(1) << k), "a<<k == a*2^k");
	}
	done();
}

static void t_mul_reference() {
	group("multiply vs 128-bit reference");
	for (uint64_t n = 1; n <= 40; n++)
		for (uint64_t m : { 1ull, 2ull, 3ull, 7ull, 17ull, 40ull })
			for (int pat = 0; pat < 4; pat++) {
				natural a = rnat(n, pat), b = rnat(m, pat);
				natural p = a * b;
				check(same(p, ref_mul(a, b)), "planner product vs reference");
				natural q;
				q._mul_base(&a, &b);
				check(q == p, "_mul_base vs planner");
			}
	for (uint64_t n : { 64ull, 100ull, 127ull, 128ull, 200ull })
		for (uint64_t m : { 1ull, 2ull, 3ull, 17ull, 63ull }) {
			natural a = rnat(n), b = rnat(m);
			check(same(a * b, ref_mul(a, b)), "larger product vs reference");
		}
	done();
}

static void t_mul_paths() {
	group("multiply paths agree (base / NTT)");
	for (uint64_t n : { 24ull, 25ull, 100ull, 200ull, 448ull, 896ull, 897ull, 1000ull, 1500ull,
			2048ull, 3000ull, 4096ull })
		for (uint64_t m : { 24ull, 25ull, 100ull, 400ull, 896ull, 1000ull }) {
			if (m > n) continue;
			natural a = rnat(n), b = rnat(m);
			natural p = a * b, q;
			q._mul_base(&a, &b);
			check(q == p, "_mul_base == planner");
			q._mul_NTT(&a, &b);
			check(q == p, "_mul_NTT == planner");
		}
	done();
}

static void t_sqr() {
	group("square (vs a*a, base / NTT, in-place)");
	for (uint64_t n : { 1ull, 2ull, 3ull, 5ull, 23ull, 24ull, 25ull, 36ull, 37ull, 38ull, 47ull,
			48ull, 100ull, 400ull, 895ull, 896ull, 897ull, 1000ull, 2048ull, 4096ull })
		for (int pat = 0; pat < 4; pat++) {
			natural a = rnat(n, pat);
			natural s = sqr(a);
			check(s == a * a, "sqr(a) == a*a");
			natural b;
			b._sqr_base(&a);
			check(b == s, "_sqr_base == sqr");
			if (n >= 24) {
				b._sqr_NTT(&a);
				check(b == s, "_sqr_NTT == sqr");
			}
			natural c = a;
			c._sqr(&c);
			check(c == s, "in-place square");
		}
	done();
}

// Structural invariants of the schedule itself: the levels must tile the layers
// 2^(k-2) .. 2^1 exactly once, in decreasing distance order, each level's top layer must
// fit that level's chunk (a 2^T chunk holds layers j <= T-1).  This AVX-512 build peels the
// fixed distance-2 pair (layers 2 and 1) off the finest level into lv.tail -- the zmm
// kernels need D >= 8, and D = 4 must not occur -- so the merged ranges tile 3 .. k-2 and
// the single unpaired layer, only ever present when k is odd, is the distance-8 layer 3.
static void t_sched_plan() {
	group("schedule plan (ntt_sched_for)");
	// The tail is part of the design at the shipping cut points; a scan point small enough to
	// crowd the levels near layer 1 can push the pair (2,1) into a coarser level, where the
	// D >= 8 dispatch sends it to the scalar kernel instead -- correct, just not vectorised.
	const bool default_cuts = ntt_scale_l1_threshold == 12 && ntt_scale_l2_threshold == 16 &&
		ntt_scale_l3_threshold == 20;
	for (int k = 2; k <= 24; k++) {
		const ntt_sched S = ntt_sched_for(k);
		bool chunk_ok = S.nlevel >= 1 && S.nlevel <= ntt_sched::MAXLEVEL && S.chunk[0] == k;
		bool fits = true, tiling = true, shape = true, lone_ok = true, tail_ok = true;
		int layers = 0, lones = 0, last = 0;
		for (int i = 0; i < S.nlevel; i++)
			if (S.lv[i].hi >= S.lv[i].lo || S.lv[i].tail) last = i;   // last non-empty level
		for (int i = 0; i < S.nlevel; i++)
			if (k >= 4 && S.chunk[i] < 4) chunk_ok = false;   // tail block + the layer-3 lone
		for (int i = 0; i < S.nlevel; i++) {
			const ntt_level& lv = S.lv[i];
			if (i) {
				if (S.chunk[i - 1] <= S.chunk[i]) chunk_ok = false;
				if (S.lv[i - 1].lo != lv.hi + 1) tiling = false;
			}
			if (lv.tail && (i + 1 != S.nlevel || lv.lo != 3)) tail_ok = false;
			if (lv.tail) layers += 2;                     // the pair (2, 1)
			const int count = lv.hi - lv.lo + 1;
			if (count <= 0)
				continue;
			if (lv.hi > S.chunk[i] - 1) fits = false;
			if (lv.pairs != count / 2 || lv.lone != ((count & 1) != 0)) shape = false;
			if (lv.lone && (lv.lo != (lv.tail ? 3 : 1) || i != last)) lone_ok = false;
			if (lv.lone) lones++;
			layers += count;
		}
		if (default_cuts && k >= 4 && !S.lv[S.nlevel - 1].tail) tail_ok = false;   // hi >= 2 there
		if (!S.lv[S.nlevel - 1].tail && S.lv[S.nlevel - 1].lo != 1) tail_ok = false;
		check(chunk_ok, "chunks: k first, then strictly decreasing, >= 4 when k >= 4");
		check(fits, "every level layer fits that level's chunk (j <= T-1)");
		check(tiling, "merged ranges tile the layers without a gap");
		check(shape, "pairs / lone match the layer count of the level");
		check(tail_ok, "only the finest level owns the tail, and it starts at layer 3");
		check(layers == k - 2, "every scheduled layer is covered exactly once");
		check(lone_ok && lones == ((k & 1) ? 1 : 0),
			"one unpaired layer iff k is odd, at the bottom of the finest merged range");
	}
	done();
}

// End to end across the whole schedule: the product of two operands, driven through
// ntt()/intt() at an explicit scale, must equal the Toom-22 base case.  The operands stay
// small enough for the base case to be cheap while the explicit scale sweeps the shapes of
// the plan -- DRAM alone at small scales, DRAM+L3+L2+L1 at the largest ones -- including
// the odd scales, where the distance-2 layer is left unpaired.
static void t_sched_scales() {
	group("schedule: product vs base at explicit scales");
	for (int k = 10; k <= 22; k++) {
		const uint64_t n = 300, m = 200;
		natural a = rnat(n), b = rnat(m), p, q;
		p._mul_base(&a, &b);
		ntt_workspace na(a, k), nb(b, k);
		na.ntt();
		nb.ntt();
		na.mul(na, nb);
		na.intt();
		na.save(q, n + m);
		check(q == p, "explicit-scale product == Toom-22");
	}
	done();
}

// The AVX-512 kernels must be bit-identical to the scalar kernels they replace: same two
// merged layers, same twiddle cursors, same lazy representatives.  One pass at a time, both
// directions, every D the engine can use at this scale, all three moduli.
static void t_zmm_kernels() {
	group("avx512: zmm radix-4 == scalar radix-4");
	const int scale = 16;
	const uint64_t size = 1ull << scale;
	ntt_workspace w(rnat(size >> 1), scale);
	std::vector<std::vector<uint64_t>> orig(3);
	for (int i = 0; i < 3; i++)
		orig[i].assign(w.ntt_data[i].data, w.ntt_data[i].data + size);
	std::vector<uint64_t> ref(size);
	for (int k = 3; k + 2 <= scale; k++) {
		const uint64_t D = 1ull << k;
		bool fwd_ok = true, inv_ok = true;
		for (int i = 0; i < 3; i++) {
			const uint64_t M = ntt_workspace::mods[i];
			uint64_t* data = w.ntt_data[i].data;
			uint64_t* end = data + size;
			memcpy(data, orig[i].data(), size * 8);
			nat_asmNtt2_radix4(D, M, data, ntt_workspace::root[i].data, ntt_workspace::root[i].data, end);
			memcpy(ref.data(), data, size * 8);
			memcpy(data, orig[i].data(), size * 8);
			nat_asmNtt_zmm_radix4(D, M, data, ntt_workspace::root[i].data, ntt_workspace::root[i].data, end);
			if (memcmp(ref.data(), data, size * 8)) fwd_ok = false;
			memcpy(data, orig[i].data(), size * 8);
			nat_asmINtt2_radix4(D, M, data, ntt_workspace::iroot[i].data, ntt_workspace::iroot[i].data, end);
			memcpy(ref.data(), data, size * 8);
			memcpy(data, orig[i].data(), size * 8);
			nat_asmINtt_zmm_radix4(D, M, data, ntt_workspace::iroot[i].data, ntt_workspace::iroot[i].data, end);
			if (memcmp(ref.data(), data, size * 8)) inv_ok = false;
		}
		check(fwd_ok, "forward zmm pass == scalar pass (bit for bit)");
		check(inv_ok, "inverse zmm pass == scalar pass (bit for bit)");
	}
	done();
}

// End to end: with the zmm kernels on and off the whole engine must produce identical
// results.  This is what covers the non-zero-base chunks, the level walk and the scalar
// tail, none of which the single-pass check above reaches.
static void t_zmm_paths() {
	group("avx512: zmm engine vs scalar engine");
	for (uint64_t n : { 896ull, 1000ull, 2048ull, 4096ull, 9000ull, 20000ull, 70000ull }) {
		natural a = rnat(n), b = rnat(n);
		ntt_zmm_enable = true;
		natural p = a * b, q = sqr(a);
		ntt_zmm_enable = false;
		natural r = a * b, t = sqr(a);
		ntt_zmm_enable = true;
		check(p == r, "product identical with the zmm kernels on and off");
		check(q == t, "square identical with the zmm kernels on and off");
	}
	done();
}

static void t_wrap() {
	group("wrap-corrected path vs plain");
	// shapes the planner picks wrap for: just above a power of two
	for (uint64_t n : { 1026ull, 1050ull, 1100ull, 1150ull, 1200ull, 2052ull, 2100ull, 2200ull,
			2400ull, 4104ull, 4400ull, 5000ull, 8200ull, 9000ull }) {
		natural a = rnat(n), b = rnat(n - 100);
		ntt_wrap_enable = true;
		natural p = a * b;
		ntt_wrap_enable = false;
		natural q = a * b;
		ntt_wrap_enable = true;
		check(p == q, "wrap product == version_1 schedule");
	}
	done();
}

static void t_div() {
	group("division inverts multiplication");
	for (int i = 0; i < 60; i++) {
		uint64_t n = 1 + rnd() % 400;
		natural a = rnat(n), b = rnat(1 + rnd() % n);
		natural q = a / b, r = a % b;
		check(q * b + r == a, "(a/b)*b + a%b == a");
		check(r < b, "a%b < b");
		natural p = a * b;
		check(p / b == a, "(a*b)/b == a");
		check(p % b == natural(0), "(a*b)%b == 0");
	}
	for (uint64_t n : { 1ull, 2ull, 63ull, 64ull, 65ull, 511ull, 512ull, 513ull, 896ull, 900ull }) {
		natural a = rnat(n + 200), b = rnat(n);
		natural q = a / b, r = a % b;
		check(q * b + r == a, "larger division identity");
	}
	done();
}

static void t_patterns() {
	group("carry stress patterns");
	const uint64_t sizes[] = { 3, 4, 5, 8, 23, 24, 25, 37, 48, 64, 127, 128, 129, 256, 512, 1000 };
	for (uint64_t n : sizes)
		for (int pat = 0; pat < 7; pat++) {
			natural a = rnat(n, pat);
			natural p = a * a, q;
			check(sqr(a) == p, "sqr == mul for the pattern");
			q._mul_base(&a, &a);
			check(q == p, "base == planner for the pattern");
			if (n <= 64)                       // reference stays cheap for small operands
				check(same(p, ref_mul(a, a)), "pattern vs 128-bit reference");
		}
	done();
}

int main() {
	setbuf(stdout, NULL);
	std::printf("natural self-check\n");
	t_parse_format();
	t_add_sub_shift();
	t_mul_reference();
	t_mul_paths();
	t_sqr();
	t_sched_plan();
	t_sched_scales();
	t_zmm_kernels();
	t_zmm_paths();
	t_wrap();
	t_div();
	t_patterns();
	std::printf("\n%d checks, %d failures\n", g_checks, g_fails);
	std::printf("%s\n", g_fails ? "FAILED" : "ALL OK");
	return g_fails != 0;
}
