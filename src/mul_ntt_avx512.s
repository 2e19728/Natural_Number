# mul_ntt_avx512.s - AVX-512 / IFMA merged radix-4 kernels for the natural NTT engine.
#
#   void nat_asmNtt_zmm_radix4 (uint64_t D, uint64_t mod, uint64_t* Begin,
#                               const uint64_t* RootO, const uint64_t* RootI, uint64_t* End);
#   void nat_asmINtt_zmm_radix4(uint64_t D, uint64_t mod, uint64_t* Begin,
#                               const uint64_t* RootO, const uint64_t* RootI, uint64_t* End);
#   void nat_asmNtt_zmm_radix2 (uint64_t N, uint64_t mod, uint64_t* Begin,
#                               const uint64_t* Root, uint64_t* End);
#   void nat_asmINtt_zmm_radix2(uint64_t N, uint64_t mod, uint64_t* Begin,
#                               const uint64_t* Root, uint64_t* End);
#
# Same signature, same two merged layers and the same twiddle cursors as the scalar
# nat_asmNtt2_radix4 / nat_asmINtt2_radix4 (src/mul_ntt.s); one zmm per quarter of the 4D
# macro-block, so D must be a multiple of 8.  The schedule guarantees D >= 8 for every
# merged pass and hands the fixed distance-2 pair (the "tail") to the scalar kernel, so
# D = 4 and D = 2 never reach these entry points.  Both are bit-identical to the scalar
# kernels on the values the engine produces (checked per D in tests/test_natural.cpp).
#
# One 4D block (D elements per quarter, eight per zmm group):
#     pa = Begin, pb = Begin + D
#     a = pa[k]  b = pb[k]  c = pb[D+k]  d = pb[2D+k],   k = 0 .. D-1
#
# forward: outer BFs at distance 2D with w on (b,d) and (a,c), then inner BFs at distance D
#          with w2 on (a1,b1) and w3 on (c1,d1).
# inverse: inner BFs at distance D with w2 on (a,b) and w3 on (c,d), then outer BFs at
#          distance 2D with w on (a1,c1) and (b1,d1).
#
# Twiddles: RootO = (w, w'), RootI = (w2, w2'), (w3, w3'), broadcast per block -- the scalar
# kernel is uniform within a block too.  w' is the Shoup reciprocal and w'>>52 is the 52-bit
# split the IFMA sequence needs.  RootO advances 16 bytes per block, RootI 32, exactly as in
# the scalar kernels.
#
# The same sequence as the verified experiment verify/experiments/butterfly_r4_ifma.s: the
# Shoup reduction uses vpmadd52 (IFMA) throughout, exploiting the moduli's m = a*2^56 + 1
# shape to get lo64(q*m) cheaply, and corrects the sign of z with a masked +m.
	.intel_syntax noprefix
	.text

# ---- 56-bit Shoup: DST = z = lo64(v*w) - lo64(q*m), corrected to [0, m) ----
# P28 = m, P29 = the sign bit (so "DST >= 2^63" is "DST < 0" signed), P30 = ((m-1)>>56)<<16.
	.macro S56 P, DST, V, W, WP, WPHI, VHI, ACC2, ACC3
	vpsrlq		\VHI, \V, 52
	vpxorq		\ACC2, \ACC2, \ACC2
	vpmadd52huq	\ACC2, \V, \WP
	vpmadd52luq	\ACC2, \V, \WPHI
	vpmadd52luq	\ACC2, \WP, \VHI
	vpmuludq	\ACC3, \VHI, \WPHI
	vpmadd52huq	\ACC3, \V, \WPHI
	vpmadd52huq	\ACC3, \WP, \VHI
	vpsrlq		\ACC2, \ACC2, 12
	vpmadd52luq	\ACC3, \ACC2, \P\()30
	vpsllq		\ACC3, \ACC3, 40
	vpaddq		\ACC2, \ACC2, \ACC3
	vpmullq		\DST, \V, \W
	vpsubq		\DST, \DST, \ACC2
	vpcmpuq		k1, \DST, \P\()29, 5
	vpaddq		\DST{k1}, \DST, \P\()28
	.endm

# ---- X = red(X): X - m if X >= m ----
	.macro RED P, X
	vpcmpuq		k2, \X, \P\()28, 5
	vpsubq		\X{k2}, \X, \P\()28
	.endm

# ---- forward merged radix-4: outer layer 2D then inner layer D ----
#   P = register prefix, STEP = bytes per group (64 for zmm), SHIFT = log2(STEP/8)
#   R0..R3 a,b,c,d  R4,R7 z(b,w),z(a,w)  R5,R6 b1,d1  R8,R9 a1,c1  R10,R11 z(a1,w2),z(c1,w3)
#   R12,R13,R14 Shoup scratch and a2/b2/c2/d2 staging  R20..R22 w, w', w'>>52
#   R23..R25 w2 group, R26,R27,R31 w3 group, R28 m, R29 sign, R30 (m>>56)<<16
	.macro F4GEN P, STEP, SHIFT
	.globl	nat_asmNtt_\P\()_radix4
nat_asmNtt_\P\()_radix4:
	push	rbx
	push	rbp
	mov	rbp, r9				# End
	lea	r11, [rdi*8]			# D*8
	mov	rdi, rsi			# mod
	mov	rbx, rcx			# RootO
	mov	rsi, r8				# RootI
	mov	r8,  rdx			# pa = Begin
	lea	r9,  [r8+r11]			# pb = Begin + D
	vpbroadcastq	\P\()28, rdi		# m
	movabs	rax, 0x8000000000000000
	vpbroadcastq	\P\()29, rax		# sign bit
	mov	rax, rdi
	shr	rax, 56				# a = (m-1)>>56
	shl	rax, 16
	vpbroadcastq	\P\()30, rax		# a<<16
.Lfbk_\P:
	vpbroadcastq	\P\()20, [rbx]		# w
	vpbroadcastq	\P\()21, [rbx+8]	# w'
	vpsrlq		\P\()22, \P\()21, 52
	vpbroadcastq	\P\()23, [rsi]		# w2
	vpbroadcastq	\P\()24, [rsi+8]	# w2'
	vpsrlq		\P\()25, \P\()24, 52
	vpbroadcastq	\P\()26, [rsi+16]	# w3
	vpbroadcastq	\P\()27, [rsi+24]	# w3'
	vpsrlq		\P\()31, \P\()27, 52
	mov	rcx, r11
	shr	rcx, \SHIFT			# groups per block
.Lfgrp_\P:
	vmovdqu64	\P\()0, [r8]		# a
	vmovdqu64	\P\()1, [r9]		# b
	vmovdqu64	\P\()2, [r9+r11]	# c
	vmovdqu64	\P\()3, [r9+r11*2]	# d
	S56 \P, \P\()4, \P\()3, \P\()20, \P\()21, \P\()22, \P\()12, \P\()13, \P\()14
	RED \P, \P\()1
	vpaddq		\P\()5, \P\()1, \P\()4	# b1
	vpaddq		\P\()6, \P\()1, \P\()28
	vpsubq		\P\()6, \P\()6, \P\()4	# d1
	S56 \P, \P\()7, \P\()2, \P\()20, \P\()21, \P\()22, \P\()12, \P\()13, \P\()14
	RED \P, \P\()0
	vpaddq		\P\()8, \P\()0, \P\()7	# a1
	vpaddq		\P\()9, \P\()0, \P\()28
	vpsubq		\P\()9, \P\()9, \P\()7	# c1
	S56 \P, \P\()10, \P\()5, \P\()23, \P\()24, \P\()25, \P\()12, \P\()13, \P\()14
	RED \P, \P\()8
	vpaddq		\P\()12, \P\()8, \P\()10	# a2
	vpaddq		\P\()13, \P\()8, \P\()28
	vpsubq		\P\()13, \P\()13, \P\()10	# b2
	vmovdqu64	[r8], \P\()12
	vmovdqu64	[r9], \P\()13
	S56 \P, \P\()11, \P\()6, \P\()26, \P\()27, \P\()31, \P\()12, \P\()13, \P\()14
	RED \P, \P\()9
	vpaddq		\P\()12, \P\()9, \P\()11	# c2
	vpaddq		\P\()13, \P\()9, \P\()28
	vpsubq		\P\()13, \P\()13, \P\()11	# d2
	vmovdqu64	[r9+r11], \P\()12
	vmovdqu64	[r9+r11*2], \P\()13
	add	r8, \STEP
	add	r9, \STEP
	dec	rcx
	jnz	.Lfgrp_\P
	lea	rax, [r11+r11*2]
	add	r8, rax
	add	r9, rax
	add	rbx, 16
	add	rsi, 32
	cmp	r8, rbp
	jb	.Lfbk_\P
	vzeroupper
	pop	rbp
	pop	rbx
	ret
	.endm

# ---- inverse merged radix-4: inner layer D then outer layer 2D ----
#   R0..R3 a,b,c,d (in)  R4 a1, R5 b1, R6 c1, R7 d1 (after the distance-D layer)
#   R8,R9,R10,R11 staging for the stores  R16..R18 Shoup scratch
#   R20..R22 w group, R23..R25 w2 group, R26,R27,R31 w3 group, R28 m, R29 sign, R30 a<<16
# Chain order follows the scalar nat_asmINtt2_radix4: (a,b) with w2, (c,d) with w3, then
# (a1,c1) and (b1,d1) with w.
	.macro I4GEN P, STEP, SHIFT
	.globl	nat_asmINtt_\P\()_radix4
nat_asmINtt_\P\()_radix4:
	push	rbx
	push	rbp
	mov	rbp, r9				# End
	lea	r11, [rdi*8]			# D*8
	mov	rdi, rsi			# mod
	mov	rbx, rcx			# RootO
	mov	rsi, r8				# RootI
	mov	r8,  rdx			# pa = Begin
	lea	r9,  [r8+r11]			# pb = Begin + D
	vpbroadcastq	\P\()28, rdi		# m
	movabs	rax, 0x8000000000000000
	vpbroadcastq	\P\()29, rax		# sign bit
	mov	rax, rdi
	shr	rax, 56
	shl	rax, 16
	vpbroadcastq	\P\()30, rax		# a<<16
.Libk_\P:
	vpbroadcastq	\P\()20, [rbx]		# w
	vpbroadcastq	\P\()21, [rbx+8]	# w'
	vpsrlq		\P\()22, \P\()21, 52
	vpbroadcastq	\P\()23, [rsi]		# w2
	vpbroadcastq	\P\()24, [rsi+8]	# w2'
	vpsrlq		\P\()25, \P\()24, 52
	vpbroadcastq	\P\()26, [rsi+16]	# w3
	vpbroadcastq	\P\()27, [rsi+24]	# w3'
	vpsrlq		\P\()31, \P\()27, 52
	mov	rcx, r11
	shr	rcx, \SHIFT
.Ligrp_\P:
	vmovdqu64	\P\()0, [r8]		# a
	vmovdqu64	\P\()1, [r9]		# b
	vmovdqu64	\P\()2, [r9+r11]	# c
	vmovdqu64	\P\()3, [r9+r11*2]	# d
	# (a,b) with w2: a1 = red(a+b), b1 = z((a-b)+m, w2)
	vpaddq		\P\()4, \P\()0, \P\()1
	vpsubq		\P\()5, \P\()0, \P\()1
	vpaddq		\P\()5, \P\()5, \P\()28
	RED \P, \P\()4
	S56 \P, \P\()5, \P\()5, \P\()23, \P\()24, \P\()25, \P\()16, \P\()17, \P\()18
	# (c,d) with w3: c1 = red(c+d), d1 = z((c-d)+m, w3)
	vpaddq		\P\()6, \P\()2, \P\()3
	vpsubq		\P\()7, \P\()2, \P\()3
	vpaddq		\P\()7, \P\()7, \P\()28
	RED \P, \P\()6
	S56 \P, \P\()7, \P\()7, \P\()26, \P\()27, \P\()31, \P\()16, \P\()17, \P\()18
	# (a1,c1) with w: lo = red(a1+c1) -> a, hi = z((a1-c1)+m, w) -> c
	vpaddq		\P\()8, \P\()4, \P\()6
	vpsubq		\P\()9, \P\()4, \P\()6
	vpaddq		\P\()9, \P\()9, \P\()28
	RED \P, \P\()8
	vmovdqu64	[r8], \P\()8
	S56 \P, \P\()9, \P\()9, \P\()20, \P\()21, \P\()22, \P\()16, \P\()17, \P\()18
	vmovdqu64	[r9+r11], \P\()9
	# (b1,d1) with w: lo = red(b1+d1) -> b, hi = z((b1-d1)+m, w) -> d
	vpaddq		\P\()10, \P\()5, \P\()7
	vpsubq		\P\()11, \P\()5, \P\()7
	vpaddq		\P\()11, \P\()11, \P\()28
	RED \P, \P\()10
	vmovdqu64	[r9], \P\()10
	S56 \P, \P\()11, \P\()11, \P\()20, \P\()21, \P\()22, \P\()16, \P\()17, \P\()18
	vmovdqu64	[r9+r11*2], \P\()11
	add	r8, \STEP
	add	r9, \STEP
	dec	rcx
	jnz	.Ligrp_\P
	lea	rax, [r11+r11*2]
	add	r8, rax
	add	r9, rax
	add	rbx, 16
	add	rsi, 32
	cmp	r8, rbp
	jb	.Libk_\P
	vzeroupper
	pop	rbp
	pop	rbx
	ret
	.endm

	F4GEN	zmm, 64, 6
	I4GEN	zmm, 64, 6


# ---- forward single-layer pass (radix-2), distance N, one twiddle per 2N block ----
#   Same layers and positional cursors as the scalar nat_asmNtt2: per 2N block with root
#   pair (b, b'):
#       z = S56(v, b);  lo = red(u) + z;  hi = red(u) + m - z
#   One zmm per half of a block, so N must be a multiple of 8 -- which the schedule
#   guarantees: its unpaired layer is the distance-8 one.
#   Register map: R0 u, R1 v, R2 z, R3 lo, R4 hi, R16..R18 Shoup scratch,
#                 R20..R22 w/w'/w'>>52, R28 m, R29 sign, R30 (m>>56)<<16
	.macro F2GEN P, STEP, SHIFT
	.globl	nat_asmNtt_\P\()_radix2
nat_asmNtt_\P\()_radix2:
	push	rbx
	push	rbp
	mov	rbp, r8				# End (5 arguments, not the radix-4 six)
	lea	r11, [rdi*8]			# N*8: one half of a block
	mov	rdi, rsi			# mod
	mov	rbx, rcx			# Root
	mov	r8,  rdx			# Begin
	vpbroadcastq	\P\()28, rdi		# m
	movabs	rax, 0x8000000000000000
	vpbroadcastq	\P\()29, rax		# sign bit
	mov	rax, rdi
	shr	rax, 56
	shl	rax, 16
	vpbroadcastq	\P\()30, rax		# a<<16
.Lfbk2_\P:
	vpbroadcastq	\P\()20, [rbx]		# w
	vpbroadcastq	\P\()21, [rbx+8]	# w'
	vpsrlq		\P\()22, \P\()21, 52
	mov	rcx, r11
	shr	rcx, \SHIFT			# groups per half
	lea	r9, [r8+r11]			# second half
.Lfgrp2_\P:
	vmovdqu64	\P\()0, [r8]		# u
	vmovdqu64	\P\()1, [r9]		# v
	S56 \P, \P\()2, \P\()1, \P\()20, \P\()21, \P\()22, \P\()16, \P\()17, \P\()18
	RED \P, \P\()0
	vpaddq		\P\()3, \P\()0, \P\()2	# lo
	vpaddq		\P\()4, \P\()0, \P\()28
	vpsubq		\P\()4, \P\()4, \P\()2	# hi
	vmovdqu64	[r8], \P\()3
	vmovdqu64	[r9], \P\()4
	add	r8, \STEP
	add	r9, \STEP
	dec	rcx
	jnz	.Lfgrp2_\P
	add	r8, r11				# consume the second half
	add	rbx, 16
	cmp	r8, rbp
	jb	.Lfbk2_\P
	vzeroupper
	pop	rbp
	pop	rbx
	ret
	.endm

# ---- inverse single-layer pass (radix-2), layers D then 2D ----
#   Same as the scalar nat_asmINtt2:  s = u+v; lo = red(s); x = (u-v)+m; hi = S56(x, b).
	.macro I2GEN P, STEP, SHIFT
	.globl	nat_asmINtt_\P\()_radix2
nat_asmINtt_\P\()_radix2:
	push	rbx
	push	rbp
	mov	rbp, r8				# End (5 arguments, not the radix-4 six)
	lea	r11, [rdi*8]
	mov	rdi, rsi
	mov	rbx, rcx
	mov	r8,  rdx
	vpbroadcastq	\P\()28, rdi
	movabs	rax, 0x8000000000000000
	vpbroadcastq	\P\()29, rax
	mov	rax, rdi
	shr	rax, 56
	shl	rax, 16
	vpbroadcastq	\P\()30, rax
.Libk2_\P:
	vpbroadcastq	\P\()20, [rbx]
	vpbroadcastq	\P\()21, [rbx+8]
	vpsrlq		\P\()22, \P\()21, 52
	mov	rcx, r11
	shr	rcx, \SHIFT
	lea	r9, [r8+r11]
.Ligrp2_\P:
	vmovdqu64	\P\()0, [r8]		# u
	vmovdqu64	\P\()1, [r9]		# v
	vpaddq		\P\()2, \P\()0, \P\()1	# s
	vpsubq		\P\()3, \P\()0, \P\()1	# u-v
	vpaddq		\P\()3, \P\()3, \P\()28	# x
	RED \P, \P\()2			# lo
	S56 \P, \P\()4, \P\()3, \P\()20, \P\()21, \P\()22, \P\()16, \P\()17, \P\()18
	vmovdqu64	[r8], \P\()2
	vmovdqu64	[r9], \P\()4
	add	r8, \STEP
	add	r9, \STEP
	dec	rcx
	jnz	.Ligrp2_\P
	add	r8, r11
	add	rbx, 16
	cmp	r8, rbp
	jb	.Libk2_\P
	vzeroupper
	pop	rbp
	pop	rbx
	ret
	.endm

	F2GEN	zmm, 64, 6
	I2GEN	zmm, 64, 6


# ---- D = 2 tail: the finest level's fixed (2,1) pair, packed into zmm ----
#   void nat_asmNtt_zmm_radix4_d2(uint64_t D, uint64_t mod, uint64_t* Begin,
#                                 const uint64_t* RootO, const uint64_t* RootI, uint64_t* End);
#   Same two merged layers as the scalar nat_asmNtt_radix4(2, ...) (which is what the tail
#   uses when ntt_zmm_enable is off), same positional cursors, bit-identical results.
#
# With D = 2 the four quarters of a macro-block are two elements each, so a 16-element
# iteration covers two blocks and every lane needs a different twiddle.  The kernel therefore
# regroups the lanes three times (2x vpermt2q each) so that all lanes of a vector share one
# multiplier, and makes the butterflies *vertical* between two vectors:
#
#   [a0 b0 a1 b1]  (gather 1)  x  [c0 d0 c1 d1]   outer layer, twiddles [w0 x4, w1 x4]
#   [b0 d0 b1 d1]  (gather 2)  x  [a0 c0 a1 c1]   inner layer, twiddles 2-lane interleaved
#   [a0 b0 c0 d0], [a1 b1 c1 d1]  (restore)       store back in memory order
#
# The twiddle vectors come straight out of the root table, which stores (w, w') pairs.  The
# outer layer wants one twiddle per block, i.e. [w0 x4, w1 x4]: a full vpbroadcastq of w1 and
# then the same broadcast of w0 under k4 = 0x0f, which fills the low four lanes and leaves the
# high half alone -- four broadcasts in all, no cross-lane shuffle and no 64-byte load for
# data it does not use.  The inner layer wants [w2_0 x2, w3_0 x2, w2_1 x2, w3_1 x2], i.e. every
# w of a pair duplicated; that is a register vpshufd with 0x44 (0xee for the w' vector) on the
# single 64-byte load of the four pairs the iteration consumes.  The block-0 twiddle (1) needs
# no special case: S56 with w = 1 reproduces the multiply-free butterfly, which is why the
# generic scalar kernel and nat_asmNtt_radix4 agree on block 0 too.
#
# Register map: R0/R1,R3/R4/R5 outer then inner lo/hi pairs, R2/R6 Z, R8/R9 loads,
#   R10/R19/R27 staging, R11..R15/R26 permutation indices (set once), R16..R18 S56 scratch,
#   R20..R22 outer w/w'/w'>>52, R23..R25 inner, R28 m, R29 sign, R30 a<<16, k4 = 0x0f (the
#   outer broadcast mask, set once; S56 and RED own k1 and k2).
# The loop steps 128 bytes = 16 elements; the schedule's smallest level chunk is 2^4, so a
# span is always a multiple of 16.
	.globl	nat_asmNtt_zmm_radix4_d2
nat_asmNtt_zmm_radix4_d2:
	push	rbx
	push	rbp
	mov	rbp, r9				# End
	mov	rdi, rsi			# mod
	mov	rbx, rcx			# RootO
	mov	rsi, r8				# RootI
	mov	r8,  rdx			# Begin
	vpbroadcastq	zmm28, rdi		# m
	movabs	rax, 0x8000000000000000
	vpbroadcastq	zmm29, rax		# sign bit
	mov	rax, rdi
	shr	rax, 56
	shl	rax, 16
	vpbroadcastq	zmm30, rax		# a<<16
	mov	eax, 0x0f
	kmovw	k4, eax			# the outer layer's low-half broadcast mask
	vmovdqa64	zmm11, [rip + .Ld2_i0]
	vmovdqa64	zmm12, [rip + .Ld2_i1]
	vmovdqa64	zmm13, [rip + .Ld2_i2]
	vmovdqa64	zmm14, [rip + .Ld2_i3]
	vmovdqa64	zmm15, [rip + .Ld2_i4]
	vmovdqa64	zmm26, [rip + .Ld2_i5]
.Ld2_loop:
	vpbroadcastq	zmm20, [rbx+16]		# w1 x4 in the high half, then w0 x4 under k4
	vpbroadcastq	zmm20{k4}, [rbx]
	vpbroadcastq	zmm21, [rbx+24]		# the same for the reciprocals
	vpbroadcastq	zmm21{k4}, [rbx+8]
	vpsrlq		zmm22, zmm21, 52
	vmovdqu64	zmm24, [rsi]		# the (w, w') pairs of both inner twiddles
	vpshufd		zmm23, zmm24, 0x44	# [w2_0 x2, w3_0 x2, w2_1 x2, w3_1 x2]
	vpshufd		zmm24, zmm24, 0xee	# and their reciprocals
	vpsrlq		zmm25, zmm24, 52
	vmovdqu64	zmm8, [r8]		# block 0, elements 0..7
	vmovdqu64	zmm9, [r8+64]		# block 1, elements 8..15
	vmovdqa64	zmm0, zmm8
	vpermt2q	zmm0, zmm11, zmm9	# V0 = [a0,b0,a1,b1]
	vmovdqa64	zmm1, zmm8
	vpermt2q	zmm1, zmm12, zmm9	# V1 = [c0,d0,c1,d1]
	# the scalar's outer butterfly multiplies the hi element of each pair (d on (b,d),
	# c on (a,c)) and reds the lo one: V1 (c,d) carries the multiply, V0 (a,b) the red
	S56		zmm, zmm2, zmm1, zmm20, zmm21, zmm22, zmm16, zmm17, zmm18
	RED		zmm, zmm0		# U = red(V0)
	vpaddq		zmm1, zmm0, zmm28
	vpsubq		zmm1, zmm1, zmm2	# V1 = U + m - Z   (c', d')
	vpaddq		zmm0, zmm0, zmm2	# V0 = U + Z       (a', b')
	vmovdqa64	zmm4, zmm0
	vpermt2q	zmm4, zmm13, zmm1	# V2 = [b0,d0,b1,d1]  (takes the inner multiply)
	vmovdqa64	zmm5, zmm0
	vpermt2q	zmm5, zmm14, zmm1	# V3 = [a0,c0,a1,c1]
	S56		zmm, zmm6, zmm4, zmm23, zmm24, zmm25, zmm16, zmm17, zmm18
	RED		zmm, zmm5		# U2 = red(V3)
	vpaddq		zmm4, zmm5, zmm28
	vpsubq		zmm4, zmm4, zmm6	# V2 = U2 + m - Z2  (b'', d'')
	vpaddq		zmm5, zmm5, zmm6	# V3 = U2 + Z2      (a'', c'')
	vmovdqa64	zmm10, zmm5
	vpermt2q	zmm10, zmm15, zmm4	# block 0 back in memory order
	vmovdqa64	zmm27, zmm5
	vpermt2q	zmm27, zmm26, zmm4	# block 1
	vmovdqu64	[r8], zmm10
	vmovdqu64	[r8+64], zmm27
	add	r8, 128
	add	rbx, 32			# one RootO pair per block
	add	rsi, 64			# two RootI pairs per block
	cmp	r8, rbp
	jb	.Ld2_loop
	vzeroupper
	pop	rbp
	pop	rbx
	ret


# ---- D = 2 tail, inverse: the same (2,1) pair as nat_asmINtt_radix4(2, ...) ----
#   void nat_asmINtt_zmm_radix4_d2(uint64_t D, uint64_t mod, uint64_t* Begin,
#                                  const uint64_t* RootO, const uint64_t* RootI, uint64_t* End);
#
# The forward kernel's three regroupings, run in the opposite order: the inverse applies the
# distance-D layer first, so it is the *gather* that has to bring the D-pairs into vertical
# lanes and the *regroup* that puts the 2D-pairs back into vertical lanes.
#
#   gather:  [a c ...] = [m0 m1 m4 m5 ...]  x  [b d ...] = [m2 m3 m6 m7 ...]       (iA / iB)
#   inner:   s = a + b -> red(s) at the a/c slots, x = a - b + m -> S56(x, w2/w3) at b/d
#   regroup: [a1 b1 a1' b1' ...] x [c1 d1 c1' d1' ...]                              (i3 / i2)
#   outer:   s = a1 + c1 -> red(s) at the a/b slots, x = a1 - c1 + m -> S56(x, w) at c/d
#   restore: the two quarters of each block interleave back into memory order        (i0 / i1)
#
# Both layers are then ordinary vertical butterflies between two vectors that hold *all* the
# lanes of the iteration, and each costs one S56 and one RED -- the forward kernel's two of
# each, with no half-vector case and no mask register.  Nothing is folded over a lane
# boundary, so the sign of the S56 argument (a - b, a1 - c1) is automatically the one its own
# lane wants; the two extra vpermt2q stages are exactly what buys the uniform butterflies.
#
# Both twiddle vectors are built as in the forward kernel, from the same tables: the outer one
# is four vpbroadcastq (the low block's w and w' merge-masked in under k4 = 0x0f, set once
# outside the loop), the inner one is one 64-byte load plus two register vpshufd.  The outer
# twiddle is still one broadcast per block -- the pair members share w -- and the inner one is
# still the 2-lane-interleaved [w2_0 w3_0 w2_1 w3_1] pattern, because the gather leaves the
# D-pairs in the same lanes the forward's regroup put them in.
#
# Registers as in the forward kernel; k4 (0x0f, the outer broadcast) is set once outside the
# loop.  No blend mask is needed.
	.globl	nat_asmINtt_zmm_radix4_d2
nat_asmINtt_zmm_radix4_d2:
	push	rbx
	push	rbp
	mov	rbp, r9				# End
	mov	rdi, rsi			# mod
	mov	rbx, rcx			# RootO (outer layer)
	mov	rsi, r8				# RootI (inner layer)
	mov	r8,  rdx			# Begin
	vpbroadcastq	zmm28, rdi		# m
	movabs	rax, 0x8000000000000000
	vpbroadcastq	zmm29, rax		# sign bit
	mov	rax, rdi
	shr	rax, 56
	shl	rax, 16
	vpbroadcastq	zmm30, rax		# a<<16
	mov	eax, 0x0f
	kmovw	k4, eax			# the outer broadcast mask
	vmovdqa64	zmm11, [rip + .Ld2_iA]	# gather: [a, c, ...]
	vmovdqa64	zmm12, [rip + .Ld2_iB]	# gather: [b, d, ...]
	vmovdqa64	zmm13, [rip + .Ld2_i3]	# regroup: [a1, b1, ...]
	vmovdqa64	zmm14, [rip + .Ld2_i2]	# regroup: [c1, d1, ...]
	vmovdqa64	zmm15, [rip + .Ld2_i0]	# restore block 0
	vmovdqa64	zmm26, [rip + .Ld2_i1]	# restore block 1
.Ld2i_loop:
	vpbroadcastq	zmm20, [rbx+16]		# outer: w1 x4 in the high half, then w0 x4 under k4
	vpbroadcastq	zmm20{k4}, [rbx]
	vpbroadcastq	zmm21, [rbx+24]
	vpbroadcastq	zmm21{k4}, [rbx+8]
	vpsrlq		zmm22, zmm21, 52
	vmovdqu64	zmm24, [rsi]		# inner: the (w, w') pairs
	vpshufd		zmm23, zmm24, 0x44	# [w2_0 x2, w3_0 x2, w2_1 x2, w3_1 x2]
	vpshufd		zmm24, zmm24, 0xee
	vpsrlq		zmm25, zmm24, 52
	vmovdqu64	zmm8, [r8]
	vmovdqu64	zmm9, [r8+64]
	vmovdqa64	zmm0, zmm8
	vpermt2q	zmm0, zmm11, zmm9	# Vf = [a, c] -- the distance-D pairs, now vertical
	vmovdqa64	zmm1, zmm8
	vpermt2q	zmm1, zmm12, zmm9	# Vs = [b, d]
	# inner layer (distance D)
	vpaddq		zmm2, zmm0, zmm1	# s = a + b
	vpsubq		zmm3, zmm0, zmm1
	vpaddq		zmm3, zmm3, zmm28	# x = a - b + m
	S56		zmm, zmm1, zmm3, zmm23, zmm24, zmm25, zmm16, zmm17, zmm18
	RED		zmm, zmm2			# red(s) -> a1, c1
	# regroup: the distance-2D partners are now (a1,c1) and (b1,d1), so split the same two
	# vectors by quarter instead; the outer layer is then a vertical butterfly again.
	vmovdqa64	zmm4, zmm2
	vpermt2q	zmm4, zmm13, zmm1	# [a1, b1, ...]
	vmovdqa64	zmm5, zmm2
	vpermt2q	zmm5, zmm14, zmm1	# [c1, d1, ...]
	# outer layer (distance 2D)
	vpaddq		zmm6, zmm4, zmm5	# s = a1 + c1
	vpsubq		zmm0, zmm4, zmm5
	vpaddq		zmm0, zmm0, zmm28	# x = a1 - c1 + m
	S56		zmm, zmm5, zmm0, zmm20, zmm21, zmm22, zmm16, zmm17, zmm18
	RED		zmm, zmm6			# red(s) -> a2, b2
	vmovdqa64	zmm10, zmm6
	vpermt2q	zmm10, zmm15, zmm5	# block 0 back in memory order
	vmovdqa64	zmm27, zmm6
	vpermt2q	zmm27, zmm26, zmm5	# block 1
	vmovdqu64	[r8], zmm10
	vmovdqu64	[r8+64], zmm27
	add	r8, 128
	add	rbx, 32
	add	rsi, 64
	cmp	r8, rbp
	jb	.Ld2i_loop
	vzeroupper
	pop	rbp
	pop	rbx
	ret

	.section	.rodata
	.align	64
.Ld2_i0:	.quad 0,1,2,3, 8,9,10,11	# fwd gather 1 / inv restore 0 -> [a,b ...]
.Ld2_i1:	.quad 4,5,6,7, 12,13,14,15	# fwd gather 1 / inv restore 1 -> [c,d ...]
.Ld2_i2:	.quad 2,3,10,11, 6,7,14,15	# fwd regroup -> [b,d ...] / inv regroup -> [c1,d1 ...]
.Ld2_i3:	.quad 0,1,8,9, 4,5,12,13	# fwd regroup -> [a,c ...] / inv regroup -> [a1,b1 ...]
.Ld2_i4:	.quad 0,1,8,9, 2,3,10,11	# fwd restore block 0
.Ld2_i5:	.quad 4,5,12,13, 6,7,14,15	# fwd restore block 1
# The inverse needs its own pair for the gather: it starts from the two half-arrays, not from
# the forward's post-outer V0/V1, so the same lane contents live in different source registers.
.Ld2_iA:	.quad 0,1,4,5, 8,9,12,13	# inv gather -> [a,c ...]
.Ld2_iB:	.quad 2,3,6,7, 10,11,14,15	# inv gather -> [b,d ...]
	.text

	.section	.note.GNU-stack,"",@progbits
