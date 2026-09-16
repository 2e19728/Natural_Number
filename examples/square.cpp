// Smallest useful example: parse, square, format.
//
//	./natural_example
//
// `sqr` is found by argument-dependent lookup, so `sqr(a)` works as well as `nat::sqr(a)`.
#include <natural/natural.h>

#include <cstdio>
#include <format>

int main() {
	nat::natural a("1234567890123456789012345678901234567890");
	nat::natural b = nat::sqr(a);
	std::printf("a     = %s\n", std::format("{}", a).c_str());
	std::printf("a^2   = %s\n", std::format("{}", b).c_str());
	std::printf("hex   = %s\n", std::format("0x{:x}", b).c_str());
	std::printf("a*a   = %s\n", b == a * a ? "matches" : "MISMATCH");
	return b == a * a ? 0 : 1;
}
