// bench_natural.cpp - multiply / square throughput.
//
//	./natural_bench [max_limbs]      default 2^24 limbs, doubling from 2^14
//
// "MiB/s" counts the operand bytes involved in one product (2 * limbs * 8), i.e. it is a
// size-normalised way to read the same table across a 1000x size range.
#include <natural/natural.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>

using namespace nat;

static uint64_t rs = 0x9e3779b97f4a7c15ull;
static uint64_t rnd() { rs ^= rs << 13; rs ^= rs >> 7; rs ^= rs << 17; return rs; }

int main(int argc, char** argv) {
	setbuf(stdout, NULL);
	const uint64_t max_limbs = argc > 1 ? std::strtoull(argv[1], nullptr, 0) : (1ull << 24);
	std::printf("%12s %14s %12s %12s %12s %12s\n",
		"limbs", "bits", "mul_ms", "sqr_ms", "mul_MiB/s", "sqr_MiB/s");
	for (uint64_t n = 1ull << 14; n <= max_limbs; n <<= 1) {
		natural a, b, d;
		a.resize(n);
		b.resize(n);
		for (uint64_t i = 0; i < n; i++) a[i] = rnd();
		for (uint64_t i = 0; i < n; i++) b[i] = rnd();
		a[n - 1] |= 1;
		b[n - 1] |= 1;
		a.std();
		b.std();
		auto best = [](auto&& f) {
			double best = 1e300;
			for (int r = 0; r < 3; r++) {
				auto t0 = std::chrono::steady_clock::now();
				f();
				auto t1 = std::chrono::steady_clock::now();
				double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
				if (ms < best) best = ms;
			}
			return best;
		};
		const double tm = best([&] { d = a * b; });
		const double ts = best([&] { d = sqr(a); });
		const double bytes = 2.0 * (double)n * 8.0;
		std::printf("%12llu %14llu %12.3f %12.3f %12.1f %12.1f\n",
			(unsigned long long)n, (unsigned long long)(n * 64), tm, ts,
			bytes / (tm * 1e-3) / (1 << 20), bytes / (ts * 1e-3) / (1 << 20));
		(void)d;
	}
	return 0;
}
