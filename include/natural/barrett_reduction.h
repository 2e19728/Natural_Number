// barrett_reduction.h - the three small arithmetic primitives of the NTT engine, as inline
// functions (they used to be the assembly helpers asmDiv / asmMod / asmMulMod in
// ntt_info.s).  Same semantics, same results, no call overhead.
//
//   old asm symbol                                   new inline function
//   ---------------------------------------------    ---------------------------------
//   asmDiv(Low, High, Mod)                        -> div_128_64(Low, High, Mod)
//   asmMod(A, R, Mod)     R = ceil(2^64  / Mod)   -> barrett_mod(A, R, Mod)
//   asmMulMod(A, B, Mod, R) R = ceil(2^124 / Mod) -> barrett_mul_mod(A, B, Mod, R)
//
// Preconditions (identical to the assembly versions):
//   * mod > 2^62, true for the NTT moduli alpha*2^56+1 (alpha = 27/58/87, all > 2^60),
//     which is what keeps the Barrett remainder inside ONE conditional correction;
//   * barrett_mod / barrett_mul_mod: R exactly the reciprocal described above;
//   * barrett_mul_mod: A, B < Mod (then A*B < 2^124, so floor(A*B*R / 2^124) overestimates
//     A*B/Mod by at most 1).
//
// Inline assembly here is written in INTEL syntax: the whole project is compiled with
// -masm=intel, x86-64 and BMI2 (mulx) are the baseline, so no dialect/ISA guards are
// needed or wanted.
//
// Why assembly at all for two of the three:
//   * div_128_64: GCC lowers ((u128)High << 64 | Low) / Mod to a call into libgcc's
//     __udivti3 (it cannot use the knowledge that High < Mod), while `div` is the
//     4-instruction sequence the old helper had;
//   * barrett_mul_mod: mulx removes the rax/rdx juggling of the old `mul` version and gives
//     14 instructions instead of 18, and it is the only one of the three where the
//     compiler's output was clearly worse than the hand-written code.
//   * barrett_mod stays plain C++: GCC emits exactly the old 9-instruction Barrett step.
#pragma once

#include <cstdint>

namespace nat {

// floor((High : Low) / Mod)  -- two-limb dividend, one-limb divisor.
// Usage in the root table build: div_128_64(0, w, mod) == floor(w * 2^64 / mod)
// (the Shoup multiplier p_w of the twiddle w).  Requires High < Mod (else the quotient
// does not fit in 64 bits and `div` faults).
inline uint64_t div_128_64(uint64_t _Low, uint64_t _High, uint64_t _Mod) {
	uint64_t _Q, _Rem;
	asm ("div %[m]" : "=a"(_Q), "=d"(_Rem) : "a"(_Low), "d"(_High), [m]"r"(_Mod));
	return _Q;
}

// A % Mod, Barrett with R = ceil(2^64 / Mod):
//   q = high(A * R)  is floor(A/Mod) or floor(A/Mod) + 1, so a single conditional add
//   of Mod repairs the remainder.  Valid for every A < 2^64.
inline uint64_t barrett_mod(uint64_t _A, uint64_t _R, uint64_t _Mod) {
	uint64_t _Q = (uint64_t)(((unsigned __int128)_A * _R) >> 64);
	uint64_t _D = _A - _Q * _Mod;
	return (int64_t)_D < 0 ? _D + _Mod : _D;
}

// A * B % Mod, Barrett with R = ceil(2^124 / Mod), A, B < Mod  (BMI2 mulx, Intel syntax).
//
// Register plan:  r8 = low(A*B) (kept to the end), r9 = high(A*B) then reused, r10/rcx
// = the two halves needed for the quotient estimate, rdx = mulx's implicit operand.
//
//   r9:r8   = A * B                       (mulx)
//   rcx     = high(low(A*B) * R)          (mulx)
//   r9:r10  = high(A*B) * R               (mulx)
//   r9:r10 += rcx      (mid word, adc carries into the high word)
//   r9      = shld(r9, r10, 4) = floor(A*B*R / 2^124)   -- the quotient estimate q
//   r8     -= q * Mod, and if that went negative, += Mod  (one correction, as usual)
//
// The dropped low half of low(A*B)*R is < 2^64 and therefore cannot carry into the
// result of the >> 60, which is why only three multiplies (and no 192-bit product) are
// needed.
inline uint64_t barrett_mul_mod(uint64_t _A, uint64_t _B, uint64_t _Mod, uint64_t _R) {
	uint64_t _D;
	asm (
		"mov  rdx, %[a]        \n"
		"mulx r9, r8, %[b]     \n"   // r9:r8 = A * B
		"mov  rdx, r8          \n"
		"mulx rcx, %[d], %[r]  \n"   // rcx = high(low(A*B) * R), _D used as scratch
		"mov  rdx, r9          \n"
		"mulx r9, r10, %[r]    \n"   // r9:r10 = high(A*B) * R
		"add  r10, rcx         \n"
		"adc  r9, 0            \n"
		"shld r9, r10, 4       \n"   // q = floor(A*B*R / 2^124)
		"imul r9, %[m]         \n"   // q * Mod
		"sub  r8, r9           \n"   // r8 = low(A*B) - q*Mod
		"lea  r10, [r8+%[m]]   \n"
		"cmovs r8, r10         \n"   // negative -> + Mod   (flags still from `sub`)
		"mov  %[d], r8         \n"
		: [d] "=&r"(_D)
		: [a] "r"(_A), [b] "r"(_B), [m] "r"(_Mod), [r] "r"(_R)
		: "rcx", "rdx", "r8", "r9", "r10", "cc"
	);
	return _D;
}

}  // namespace nat
