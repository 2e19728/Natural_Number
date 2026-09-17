# mul_ntt_avx512.s - AVX-512 / IFMA merged radix-4 kernels for the natural NTT engine.
#
#   void nat_asmNtt_zmm_radix4 (uint64_t D, uint64_t mod, uint64_t* Begin,
#                               const uint64_t* RootO, const uint64_t* RootI, uint64_t* End);
#   void nat_asmINtt_zmm_radix4(uint64_t D, uint64_t mod, uint64_t* Begin,
#                               const uint64_t* RootO, const uint64_t* RootI, uint64_t* End);
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

	.section	.note.GNU-stack,"",@progbits
