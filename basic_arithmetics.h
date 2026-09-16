#pragma once

#include <cstdint>

// add: dst = src1 + src2 ; src1 has len1 limbs, src2 has len2, len1 >= len2.
// Returns the final carry out of limb len1-1.
inline uint64_t add(uint64_t* dst, const uint64_t* src1, const uint64_t* src2, uint64_t len1, uint64_t len2) {
	dst += len2;
	src1 += len2;
	src2 += len2;
	len1 -= len2;
	asm volatile (
		"mov r11, %4				\n"
		"and r11, 3					\n"
		"neg %4						\n"
		"and %4, -4					\n"
		"cmp r11, 1					\n"
		"je	3f						\n"
		"cmp r11, 2					\n"
		"je	2f						\n"
		"test r11, r11				\n"
		"jnz 1f						\n"
		"0:							\n"
			"mov r8, [%1+%4*8]		\n"
			"adc r8, [%2+%4*8]		\n"
			"mov [%0+%4*8], r8		\n"
		"1:							\n"
			"mov r9, [%1+%4*8+8]	\n"
			"adc r9, [%2+%4*8+8]	\n"
			"mov [%0+%4*8+8], r9	\n"
		"2:							\n"
			"mov r10, [%1+%4*8+16]	\n"
			"adc r10, [%2+%4*8+16]	\n"
			"mov [%0+%4*8+16], r10	\n"
		"3:							\n"
			"mov r11, [%1+%4*8+24]	\n"
			"adc r11, [%2+%4*8+24]	\n"
			"mov [%0+%4*8+24], r11	\n"
			"lea %4, [%4+3]			\n"
			"inc %4					\n"
			"jnz 0b					\n"
		"lea %0, [%0+%3*8]			\n"
		"lea %1, [%1+%3*8]			\n"
		"jc 1f						\n"
		"neg %3						\n"
		"jmp 2f						\n"
		"1:							\n"
		"inc %4						\n"
		"neg %3						\n"
		"jz 2f						\n"
		"0:							\n"
			"mov r8, [%1+%3*8]		\n"
			"sub r8, -1				\n"
			"mov [%0+%3*8], r8		\n"
			"inc %3					\n"
			"ja 0b					\n"
		"sbb %4, 0					\n"
		"2:							\n"
		:"+r"(dst), "+r"(src1), "+r"(src2), "+r"(len1), "+c"(len2)
		:
		: "cc", "memory", "r8", "r9", "r10", "r11"
	);
	if (dst != src1)
		__builtin_memcpy(dst + len1, src1 + len1, sizeof(uint64_t) * -len1);
	return len2;
}

// sub: dst = src1 - src2 ; len1 >= len2.  Returns the final borrow (always 0 here).
inline uint64_t sub(uint64_t* dst, const uint64_t* src1, const uint64_t* src2, uint64_t len1, uint64_t len2) {
	dst  += len2;
	src1 += len2;
	src2 += len2;
	len1 -= len2;
	asm volatile (
		"mov r11, %4				\n"
		"and r11, 3					\n"
		"neg %4						\n"
		"and %4, -4					\n"
		"cmp r11, 1					\n"
		"je	3f						\n"
		"cmp r11, 2					\n"
		"je	2f						\n"
		"test r11, r11				\n"
		"jnz 1f						\n"
		"0:							\n"
			"mov r8, [%1+%4*8]		\n"
			"sbb r8, [%2+%4*8]		\n"
			"mov [%0+%4*8], r8		\n"
		"1:							\n"
			"mov r9, [%1+%4*8+8]	\n"
			"sbb r9, [%2+%4*8+8]	\n"
			"mov [%0+%4*8+8], r9	\n"
		"2:							\n"
			"mov r10, [%1+%4*8+16]	\n"
			"sbb r10, [%2+%4*8+16]	\n"
			"mov [%0+%4*8+16], r10	\n"
		"3:							\n"
			"mov r11, [%1+%4*8+24]	\n"
			"sbb r11, [%2+%4*8+24]	\n"
			"mov [%0+%4*8+24], r11	\n"
			"lea %4, [%4+3]			\n"
			"inc %4					\n"
			"jnz 0b					\n"
		"lea %0, [%0+%3*8]			\n"
		"lea %1, [%1+%3*8]			\n"
		"jc 1f						\n"
		"neg %3						\n"
		"jmp 2f						\n"
		"1:							\n"
		"inc %4						\n"
		"neg %3						\n"
		"jz 2f						\n"
		"0:							\n"
			"mov r8, [%1+%3*8]		\n"
			"add r8, -1				\n"
			"mov [%0+%3*8], r8		\n"
			"inc %3					\n"
			"ja 0b					\n"
		"sbb %4, 0					\n"
		"2:							\n"
		:"+r"(dst), "+r"(src1), "+r"(src2), "+r"(len1), "+c"(len2)
		:
		: "cc", "memory", "r8", "r9", "r10", "r11"
	);
	if (dst != src1)
		__builtin_memcpy(dst + len1, src1 + len1, sizeof(uint64_t) * -len1);
	return len2;
}

inline void shl(uint64_t* dst, const uint64_t* src, uint64_t cnt, uint64_t len) {
	const uint64_t l = cnt;
	const uint64_t r = 64u - cnt;
	dst[len] = src[len - 1] >> r;
	for (uint64_t i = len - 1; i > 0; --i)
		dst[i] = (src[i] << l) | (src[i - 1] >> r);
	dst[0] = src[0] << l;
}

inline void shr(uint64_t* dst, const uint64_t* src, uint64_t cnt, uint64_t len) {
	const uint64_t r = cnt;
	const uint64_t l = 64u - cnt;
	uint64_t i = 0;
	for (; i + 1 < len; ++i)
		dst[i] = src[i] >> r | src[i + 1] << l;
	dst[i] = src[i] >> r;
}

// mul1: dst = src(len limbs) * scalar n -> dst[0..len].  The kernel steps back by
// x = -len & 3 internally, but the partial-store jump compensates for it, so no
// headroom below dst/src is required (verified by ASAN with pointers at allocation
// start); dst capacity must be len+1 limbs.
inline void mul1(uint64_t* dst, const uint64_t* src, uint64_t n, uint64_t len) {
	uint64_t x = -len & 3;
	uint64_t q = (len + x) >> 2;
	dst -= x;
	src -= x;
	asm volatile (
		"xor r8d, r8d				\n"
		"xor r9d, r9d				\n"
		"xor r10d, r10d				\n"
		"xor r11d, r11d				\n"
		"cmp %4, 3					\n"
		"je 3f						\n"
		"cmp %4, 2					\n"
		"je 2f						\n"
		"test %4, %4				\n"
		"jnz 1f						\n"
		"0:							\n"
			"mulx r8, rax, [%1]		\n"
			"adc r11, rax			\n"
			"mov [%0], r11			\n"
		"1:							\n"
			"mulx r9, rax, [%1+8]	\n"
			"adc r8, rax			\n"
			"mov [%0+8], r8			\n"
		"2:							\n"
			"mulx r10, rax, [%1+16]	\n"
			"adc r9, rax			\n"
			"mov [%0+16], r9		\n"
		"3:							\n"
			"mulx r11, rax, [%1+24]	\n"
			"adc r10, rax			\n"
			"mov [%0+24], r10		\n"
			"lea %0, [%0+32]		\n"
			"lea %1, [%1+32]		\n"
			"dec %2					\n"
			"jnz 0b					\n"
		"adc r11, 0					\n"
		"mov [%0], r11				\n"
		: "+r"(dst), "+r"(src), "+r"(q)
		: "d"(n), "r"(x)
		: "rax", "r8", "r9", "r10", "r11", "cc", "memory"
	);
}

// mul2: dst = src(len limbs) * (hi:lo).
inline void mul2(uint64_t* dst, const uint64_t* src, uint64_t hi, uint64_t lo, uint64_t len) {
	asm volatile (
		"xor r12d, r12d				\n"
		"xor r13d, r13d				\n"
		"0:							\n"
			"mov rdx, [%1]			\n"
			"mulx r11, r10, %4		\n"
			"add r10, r12			\n"
			"adc r11, r13			\n"
			"mulx r13, r12, %3		\n"
			"adc r13, 0				\n"
			"add r12, r11			\n"
			"adc r13, 0				\n"
			"mov [%0], r10			\n"
			"add %0, 8				\n"
			"add %1, 8				\n"
			"dec %2					\n"
			"jnz 0b					\n"
		"mov [%0], r12				\n"
		"mov [%0+8], r13			\n"
		: "+r"(dst), "+r"(src), "+r"(len)
		: "r"(hi), "r"(lo)
		: "rdx", "r10", "r11", "r12", "r13", "cc", "memory"
	);
}

extern "C" {
	void mul_basecase_odd(uint64_t len2, uint64_t len1, const uint64_t* src2, const uint64_t* src1, uint64_t* dst);
	void mul_basecase_even(uint64_t len2, uint64_t len1, const uint64_t* src2, const uint64_t* src1, uint64_t* dst);
	// dst[0 .. 2n-1] = src[0 .. n-1]^2, n >= 3 (see mul_basecase.s)
	void sqr_basecase(uint64_t n, const uint64_t* src, uint64_t* dst);
}

// Base case: a(len1) x b(len2), len1 >= len2 >= 3.  Small len2 (1,2) uses the
// scalar kernels; otherwise the parity-specialised asm kernels handle it.
inline void mul_basecase_default(uint64_t* dst, const uint64_t* src1, const uint64_t* src2, uint64_t len1, uint64_t len2) {
	if (len2 & 1)
		mul_basecase_odd(len2, len1, src2, src1, dst);
	else
		mul_basecase_even(len2, len1, src2, src1, dst);
}

// Square base case: dst[0 .. 2n-1] = src(n)^2, n >= 3.  A square has one operand, so
// unlike mul_basecase_default there is no length pair and no parity dispatch: mul_basecase.s
// carries a single sqr_basecase entry.  (The even/odd split in that file exists only for
// the multiply, where the *short* operand's parity decides where the rise/platform/fall
// phase boundaries land; with both operands equal that distinction disappears, see the
// comment there.)
inline void sqr_basecase_default(uint64_t* dst, const uint64_t* src, uint64_t n) {
	sqr_basecase(n, src, dst);
}

// The 1- and 2-limb squares go to the scalar kernels: sqr_basecase_default, like
// mul_basecase_default, assumes n >= 3.
inline void sqr_basecase_small(uint64_t* dst, const uint64_t* src, uint64_t n) {
	if (n == 1)
		return mul1(dst, src, src[0], 1);
	if (n == 2)
		return mul2(dst, src, src[1], src[0], 2);
	sqr_basecase_default(dst, src, n);
}
