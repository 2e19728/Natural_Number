# ============================ base-case kernels ============================
# Merged from the former nat_mul_basecase_even.s and nat_mul_basecase_odd.s.  Three entries:
#	nat_mul_basecase_even(len2, len1, src2, src1, dst)  len2 even  (long x short)
#	nat_mul_basecase_odd (len2, len1, src2, src1, dst)  len2 odd
#	nat_sqr_basecase     (n, src, dst)                   square, no parity split
# The first two are the long x short base case (m = len2 <= n = len1, dst cursor in r8),
# split by the parity of m because that parity decides where the rise/platform/fall
# phase boundaries land.  A square has both operands equal, so there is no such split
# and only one entry.
.intel_syntax noprefix
.text

# ======================== nat_mul_basecase_even ========================
.globl	nat_mul_basecase_even
nat_mul_basecase_even:
	push	rbx
	push	rbp
	push	r12
	push	r13
	push	r14
	push	r15
	# shuffle: m=rdi n=rsi A=rdx B=rcx C=r8
	mov	rax, rdx		# rax = A
	mov	rbx, rcx		# rbx = B
	mov	rcx, r8			# rcx = C cursor
	lea	r8,  [rdi-1]		# r8  = m-1
	mov	r9,  rsi
	sub	r9,  rdi		# r9  = n-m
# ================= rise: columns 0..m-2 =================
	# column (0,0) alone: the low limb (r10) goes to C[0]; the high limb (r12), shifted
# right by one limb, becomes the window base for column 1 (r13..r15 = 0)
	mov	rdx, qword ptr [rax]	# A[0]
	mulx	r12, r10, qword ptr [rbx]	# r12=hi, r10=lo
	mov	qword ptr [rcx], r10	# C[0] = low limb
	add	rcx, 8
	xor	r13d, r13d		# clear the window
	xor	r14d, r14d
	xor	r15d, r15d
	mov	rbp, 1			# rbp = L = 1 (round (L,L+1))
L_e_rise_round:				# invariants: rax=A base rbx=B base r8=m-1 r9=n-m
	mov	rdi, rbp		# i = L
	xor	esi, esi		# j = 0
	# peel: A[L+1]*B[0] (top of column L+1, one limb)
	mov	rdx, qword ptr [rax + rdi*8 + 8]
	mulx	r11, r10, qword ptr [rbx]
	add	r13, r10
	adc	r14, r11
	adc	r15, 0
L_e_rise_inner:				# U1: one step (i,j); trip count L+1, always even
	mov	rdx, qword ptr [rax + rdi*8]
	mulx	r11, r10, qword ptr [rbx + rsi*8]
	add	r12, r10
	adc	r13, r11
	mulx	r11, r10, qword ptr [rbx + rsi*8 + 8]
	adc	r11, 0
	add	r13, r10
	adc	r14, r11
	adc	r15, 0
	add	rsi, 1
	sub	rdi, 1
	jnc	L_e_rise_inner		# do-while; exits on the borrow when rdi goes 0 -> -1
	# store C[L],C[L+1]; shift the window right by 2 limbs
	mov	qword ptr [rcx],     r12
	mov	qword ptr [rcx+8],   r13
	add	rcx, 16
	mov	r12, r14
	mov	r13, r15
	xor	r14d, r14d
	xor	r15d, r15d
	add	rbp, 2			# L += 2
	cmp	rbp, r8			# L < m-1 ?
	jb	L_e_rise_round
	# end of rise: the window is aligned to column m-1, rbp = m-1 (no longer used as L)
# ================= plateau: full-length columns m-1..n-1 =================
	test	r9, r9
	jz	L_e_plat_done
L_e_plat_round:				# round (L,L+1), L = m-1+2t
	mov	rdi, r8			# i = m-1
	xor	esi, esi		# j = 0 (local; the real B index is 2t+j)
L_e_plat_inner:				# U1: one step (i,j); trip count m, always even
	mov	rdx, qword ptr [rax + rdi*8]
	mulx	r11, r10, qword ptr [rbx + rsi*8]
	add	r12, r10
	adc	r13, r11
	mulx	r11, r10, qword ptr [rbx + rsi*8 + 8]
	adc	r11, 0
	add	r13, r10
	adc	r14, r11
	adc	r15, 0
	add	rsi, 1
	sub	rdi, 1
	jnc	L_e_plat_inner		# do-while; exits on the borrow when rdi goes 0 -> -1
	mov	qword ptr [rcx],     r12
	mov	qword ptr [rcx+8],   r13
	add	rcx, 16
	mov	r12, r14
	mov	r13, r15
	xor	r14d, r14d
	xor	r15d, r15d
	add	rbx, 16			# B base += 2 limbs
	sub	r9, 2
	jg	L_e_plat_round		# continue while the remaining count > 0 (signed); even np ends at 0, odd np at -1
L_e_plat_done:
	test	r9, 1
	jnz	L_e_fall_prep		# np odd: the plateau pairs everything
	# ---- leftover column n-1 (np even): rbx is now &B[n-m], local j = 0..m-1 ----
	mov	rdi, r8			# i = m-1
	xor	esi, esi		# j = 0
L_e_left_inner:				# single products into limb 0 (i+j = n-1)
	mov	rdx, qword ptr [rax + rdi*8]
	mulx	r11, r10, qword ptr [rbx + rsi*8]
	add	r12, r10
	adc	r13, r11
	adc	r14, 0
	add	rsi, 1
	sub	rdi, 1
	jnc	L_e_left_inner
	mov	qword ptr [rcx], r12	# C[n-1]
	add	rcx, 8
	mov	r12, r13		# shift right by one limb -> aligned to column n
	mov	r13, r14
	xor	r14d, r14d
	add	rbx, 8
L_e_fall_prep:
	lea	rax, [rax + r8*8 + 8]	# rax = A + m*8 (base for negative indices)
	lea	rbx, [rbx + r8*8]
	mov	rbp, 1
	sub	rbp, r8			# rbp = i0 = 2-m = -(m-2)
L_e_fall_outer:				# round: column pair (m+n+i0-2, m+n+i0-1)
	mov	rdi, rbp		# i = i0
	mov	rsi, -1			# j = -1
	# peel: A'[i0-1] * B'[-1] (bottom of column L, into limb 0)
	mov	rdx, qword ptr [rax + rdi*8 - 8]
	mulx	r11, r10, qword ptr [rbx - 8]
	add	r12, r10
	adc	r13, r11
	adc	r14, 0
L_e_fall_inner:				# U1: one step, i up / j down; trip count m-2t-2, always even
	mov	rdx, qword ptr [rax + rdi*8]
	mulx	r11, r10, qword ptr [rbx + rsi*8 - 8]
	add	r12, r10
	adc	r13, r11
	mulx	r11, r10, qword ptr [rbx + rsi*8]
	adc	r11, 0
	add	r13, r10
	adc	r14, r11
	adc	r15, 0
	sub	rsi, 1
	add	rdi, 1
	js	L_e_fall_inner		# loop while i<0 (js tests the sign of add rdi,1)
	mov	qword ptr [rcx],     r12
	mov	qword ptr [rcx+8],   r13
	add	rcx, 16
	mov	r12, r14
	mov	r13, r15
	xor	r14d, r14d
	xor	r15d, r15d
	add	rbp, 2			# i0 += 2
	js	L_e_fall_outer		# continue while i0<0 (the last step goes -2 -> 0 and exits)
	# ---- tail column (m-1,n-1) = A'[-1]*B'[-1], into limb 0 ----
	mov	rdx, qword ptr [rax - 8]
	mulx	r11, r10, qword ptr [rbx - 8]
	add	r12, r10
	adc	r13, r11
	mov	qword ptr [rcx],     r12	# C[m+n-2]
	mov	qword ptr [rcx+8],   r13	# C[m+n-1]
	pop	r15
	pop	r14
	pop	r13
	pop	r12
	pop	rbp
	pop	rbx
	ret

# ======================== nat_mul_basecase_odd ========================
.globl	nat_mul_basecase_odd
nat_mul_basecase_odd:
	push	rbx
	push	rbp
	push	r12
	push	r13
	push	r14
	push	r15
	# shuffle: m=rdi n=rsi A=rdx B=rcx C=r8
	mov	rax, rdx		# rax = A
	mov	rbx, rcx		# rbx = B
	mov	rcx, r8			# rcx = C cursor
	lea	r8,  [rdi-1]		# r8  = m-1
	mov	r9,  rsi
	sub	r9,  rdi		# r9  = n-m
	# column (0,0) alone: the low limb (r10) goes to C[0]; the high limb (r12), shifted
# right by one limb, becomes the window base for column 1 (r13..r15 = 0)
	mov	rdx, qword ptr [rax]	# A[0]
	mulx	r12, r10, qword ptr [rbx]	# r12=hi, r10=lo
	mov	qword ptr [rcx], r10	# C[0] = low limb
	add	rcx, 8
	xor	r13d, r13d
	xor	r14d, r14d
	xor	r15d, r15d
# ================= rise: rounds (1,2),(3,4),...,(m-2,m-1) =================
	mov	rbp, 1			# rbp = L = 1 (round (L,L+1))
L_o_rise_round:				# invariants: rax=A base rbx=B base r8=m-1 r9=n-m
	mov	rdi, rbp		# i = L
	xor	esi, esi		# j = 0
	# peel: A[L+1]*B[0] (top of column L+1, one limb)
	mov	rdx, qword ptr [rax + rdi*8 + 8]
	mulx	r11, r10, qword ptr [rbx]
	add	r13, r10
	adc	r14, r11
	adc	r15, 0
L_o_rise_inner:				# U1: one step, i down / j up; trip count L+1 (odd)
	mov	rdx, qword ptr [rax + rdi*8]
	mulx	r11, r10, qword ptr [rbx + rsi*8]
	add	r12, r10
	adc	r13, r11
	mulx	r11, r10, qword ptr [rbx + rsi*8 + 8]
	adc	r11, 0
	add	r13, r10
	adc	r14, r11
	adc	r15, 0
	add	rsi, 1
	sub	rdi, 1
	jnc	L_o_rise_inner		# do-while, exits on the borrow (i = 0 included)
	# store C[L],C[L+1]; shift the window right by 2 limbs
	mov	qword ptr [rcx],     r12
	mov	qword ptr [rcx+8],   r13
	add	rcx, 16
	mov	r12, r14
	mov	r13, r15
	xor	r14d, r14d
	xor	r15d, r15d
	add	rbp, 2			# L += 2
	cmp	rbp, r8			# L < m-1 ?
	jb	L_o_rise_round
	# end of rise: the window is aligned to column m-1
# ================= plateau: full-length columns m..n-1 (one fewer; first round B[1]) ===
	add	rbx, 8			# B base = &B[1] (the lowest B index in column m is 1)
	cmp	r9, 2
	jb	L_o_plat_done		# D<2: no paired round (D = 0/1)
L_o_plat_round:				# round (L,L+1), L = m+2t
	mov	rdi, r8			# i = m-1
	xor	esi, esi		# j = 0 (local; the real B index is (2t+1)+j)
L_o_plat_inner:				# U1: one step; trip count m (odd)
	mov	rdx, qword ptr [rax + rdi*8]
	mulx	r11, r10, qword ptr [rbx + rsi*8]
	add	r12, r10
	adc	r13, r11
	mulx	r11, r10, qword ptr [rbx + rsi*8 + 8]
	adc	r11, 0
	add	r13, r10
	adc	r14, r11
	adc	r15, 0
	add	rsi, 1
	sub	rdi, 1
	jnc	L_o_plat_inner
	mov	qword ptr [rcx],     r12
	mov	qword ptr [rcx+8],   r13
	add	rcx, 16
	mov	r12, r14
	mov	r13, r15
	xor	r14d, r14d
	xor	r15d, r15d
	add	rbx, 16			# B base += 2 limbs
	sub	r9, 2
	cmp	r9, 2
	jae	L_o_plat_round		# continue while the remaining D' >= 2
L_o_plat_done:
	test	r9, 1
	jz	L_o_fall_prep		# D even: the plateau pairs everything, no leftover column
	# ---- leftover column n-1 (D odd): rbx is now &B[D] = &B[n-m], local j = 0..m-1 ----
	mov	rdi, r8			# i = m-1
	xor	esi, esi		# j = 0
L_o_left_inner:				# single products into limb 0 (i+j = n-1)
	mov	rdx, qword ptr [rax + rdi*8]
	mulx	r11, r10, qword ptr [rbx + rsi*8]
	add	r12, r10
	adc	r13, r11
	adc	r14, 0
	add	rsi, 1
	sub	rdi, 1
	jnc	L_o_left_inner
	mov	qword ptr [rcx], r12	# C[n-1]
	add	rcx, 8
	mov	r12, r13		# shift right by one limb -> aligned to column n
	mov	r13, r14
	xor	r14d, r14d
	add	rbx, 8
L_o_fall_prep:
	lea	rax, [rax + r8*8 + 8]	# rax = A + m*8 (base for negative indices)
	lea	rbx, [rbx + r8*8]	# rbx = B + n*8
	mov	rbp, 1
	sub	rbp, r8			# rbp = i0 = 2-m = -(m-2)
L_o_fall_outer:				# round: column pair (m+n+i0-2, m+n+i0-1)
	mov	rdi, rbp		# i = i0
	mov	rsi, -1			# j = -1
	# peel: A'[i0-1] * B'[-1] (bottom of column L, into limb 0)
	mov	rdx, qword ptr [rax + rdi*8 - 8]
	mulx	r11, r10, qword ptr [rbx - 8]
	add	r12, r10
	adc	r13, r11
	adc	r14, 0
L_o_fall_inner:				# U1: i up / j down; trip count m-2t-2 (odd)
	mov	rdx, qword ptr [rax + rdi*8]
	mulx	r11, r10, qword ptr [rbx + rsi*8 - 8]
	add	r12, r10
	adc	r13, r11
	mulx	r11, r10, qword ptr [rbx + rsi*8]
	adc	r11, 0
	add	r13, r10
	adc	r14, r11
	adc	r15, 0
	sub	rsi, 1
	add	rdi, 1
	js	L_o_fall_inner		# loop while i<0 (js tests the sign of add rdi,1)
	mov	qword ptr [rcx],     r12
	mov	qword ptr [rcx+8],   r13
	add	rcx, 16
	mov	r12, r14
	mov	r13, r15
	xor	r14d, r14d
	xor	r15d, r15d
	add	rbp, 2			# i0 += 2
	js	L_o_fall_outer		# after i0 = -1, +2 -> 1 and exit (odd m pairs everything)
	# ---- top limb C[m+n-1] = w0 (no separate tail product) ----
	mov	qword ptr [rcx], r12
	pop	r15
	pop	r14
	pop	r13
	pop	r12
	pop	rbp
	pop	rbx
	ret

# ======================== nat_sqr_basecase ========================
# nat_sqr_basecase(n, src, dst)   rdi = n, rsi = src, rdx = dst,   n >= 3
# dst[0 .. 2n-1] = src[0 .. n-1]^2
#
# Column j of the square is
#	s_j = sum_{i+k=j} a_i*a_k = 2*sum_{i<k} a_i*a_k + (j even ? a_{j/2}^2 : 0).
# One round handles the adjacent columns (j, j+1) with j always odd, so the middle
# square a[(j+1)/2]^2 always lands in the second column.
#
# The window r12..r15 accumulates both columns' pair sums at the fixed weights 2^0 and
# 2^64.  Doubling is linear and preserves those weights, so one doubling of the window
# at the end of a round doubles both columns at once.  The previous round's carry is
# kept out of the window (in rbx, rbp) because it must not be doubled; it is folded in
# only after the doubling.
#
# The walk visits only the i > k half (the other half is the mirrored duplicate, which is
# exactly where the halved multiply count comes from), giving each column one product
# per step:
#	p1 = a[u]*a[j-u]    -> position 0  (column j)
#	p2 = a[u]*a[j+1-u]  -> position 1  (column j+1)
# The walk stops at u = u_end+1.  At u = u_end the p2 product is exactly the middle
# square a[u_end]^2, which must be counted once, so it is peeled off as a half step
# placed after the doubling: the half step does p1 only, then the window is doubled and
# the middle square is added once.
# The low cursor is deliberately kept one limb above the pair's low end, so the single
# test "cmp rdi,rsi" yields both the entry condition (a[j] against a[1]) and the exit
# condition (the two cursors meet at &a[u_end]); the loop needs nothing extra.
#
# Two phases share the same walk and tail:
#	rise (j <= n-2): both windows start at a[0] and column j+1 tops out at a[j+1], so
#	  a[j+1]*a[0] is peeled first;
#	fall (j >  n-2): both windows top out at a[n-1] and start at a[j-n+1], no peel.
# Each round stores C[j], C[j+1] and carries the window's top 2 limbs; the closing carry
# finally becomes C[2n-1].
#
# Difference from the two mul base cases above: parity is not a real fork here.  The mul
# split comes from the short operand m deciding the phase boundaries; with both operands
# equal, starting every round at an odd column keeps the middle square in the second
# column and the boundary j <= n-2 falls out automatically - hence a single
# nat_sqr_basecase instead of an even/odd pair.
	.globl	nat_sqr_basecase
nat_sqr_basecase:
	push	rbx
	push	rbp
	push	r12
	push	r13
	push	r14
	push	r15
	mov	rcx, rdx		# rcx = dst cursor
	mov	rax, rsi		# rax = src base
	mov	r9,  rdi		# r9  = n
	# C[0] = the low limb of a0^2; its high limb is the carry out of column 0, so the
# initial carry is (rbx, rbp) = (high limb, 0)
	mov	rdx, qword ptr [rax]
	mulx	r11, r10, rdx
	mov	qword ptr [rcx], r10
	add	rcx, 8
	mov	rbx, r11
	xor	ebp, ebp
	mov	r8, 8			# r8 = j*8, j = 1
L_sqr_round:
	lea	rdx, [r9*8 - 16]	# (n-2)*8
	cmp	r8, rdx
	ja	L_sqr_fall
	# ---- rise round: peel a[j+1]*a[0] into column j+1, walk starts at &a[j] / &a[1]
	xor	r12d, r12d
	xor	r13d, r13d
	xor	r14d, r14d
	xor	r15d, r15d
	mov	rdx, qword ptr [rax + r8 + 8]
	mulx	r11, r10, qword ptr [rax]
	add	r13, r10
	adc	r14, r11
	adc	r15, 0
	lea	rdi, [rax + r8]		# rdi = &a[u], u descending from j
	lea	rsi, [rax + 8]		# rsi = &a[j-u+1], ascending (one limb above the pair's low end)
	jmp	L_sqr_test
L_sqr_fall:
	lea	rdx, [r9*8]
	lea	rdx, [rdx*2 - 24]	# (2n-3)*8
	cmp	r8, rdx
	ja	L_sqr_done
	# ---- fall round: both columns top out at &a[n-1], starting at &a[j-n+1]
	xor	r12d, r12d
	xor	r13d, r13d
	xor	r14d, r14d
	xor	r15d, r15d
	lea	rdi, [rax + r9*8 - 8]	# rdi = &a[n-1]
	mov	rsi, rax
	add	rsi, r8
	lea	r11, [r9*8]
	sub	rsi, r11
	add	rsi, 16			# rsi = &a[j-n+2]
L_sqr_test:				# the pairs run down to u = u_end+1; u = u_end is left to the half step
	cmp	rdi, rsi
	jbe	L_sqr_half
L_sqr_walk:				# one step: one pair for each column (same shape as the mul kernel's inner loop)
	mov	rdx, qword ptr [rdi]
	mulx	r11, r10, qword ptr [rsi - 8]
	add	r12, r10
	adc	r13, r11
	mulx	r11, r10, qword ptr [rsi]
	adc	r11, 0			# fold position 0's carry into p2's high limb (a 64x64 product's high limb is
# < 2^64-1, so this cannot overflow)
	add	r13, r10
	adc	r14, r11
	adc	r15, 0
	sub	rdi, 8
	add	rsi, 8
	cmp	rdi, rsi
	ja	L_sqr_walk
	# ---- half step + tail: here rdi == rsi == &a[u_end]
L_sqr_half:
	mov	rdx, qword ptr [rdi]	# a[u_end]
	mulx	r11, r10, qword ptr [rsi - 8]	# p1 = a[u_end]*a[u_end-1] -> position 0
	add	r12, r10
	adc	r13, r11
	adc	r14, 0
	adc	r15, 0
	add	r12, r12		# double the whole window = doubling each column
	adc	r13, r13
	adc	r14, r14
	adc	r15, r15
	mulx	r11, r10, rdx		# the middle square a[u_end]^2 -> position 1, added once
	add	r13, r10
	adc	r14, r11
	adc	r15, 0
	add	r12, rbx		# fold in the previous round's carry (it was outside the window, so it was not doubled)
	adc	r13, rbp
	adc	r14, 0
	adc	r15, 0
	mov	qword ptr [rcx],     r12	# C[j]
	mov	qword ptr [rcx + 8], r13	# C[j+1]
	add	rcx, 16
	mov	rbx, r14		# new carry
	mov	rbp, r15
	add	r8, 16			# j += 2
	jmp	L_sqr_round
L_sqr_done:
	mov	qword ptr [rcx], rbx	# C[2n-1]
	pop	r15
	pop	r14
	pop	r13
	pop	r12
	pop	rbp
	pop	rbx
	ret
.section .note.GNU-stack,"",@progbits
