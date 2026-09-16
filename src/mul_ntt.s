	.intel_syntax noprefix
	.text

# ---------------- variant m4q: uniform in-place chains (radix-2 body per chain) ---
# Same two-layer merged semantics as m4/m4p.  Each of the four butterfly chains is
# the register-minimal radix-2 body: only u,v + one scratch are live at any time
# (v dies as soon as z is resolved, so reduce(u) reuses the dead v register as its
# temp).  Register choreography:
#   * chains 1,2 (outer, same twiddle w) keep p_w in rdx across both mulx, and use
#     two DIFFERENT scratch regs (rax, r10) so both Barrett q's fly in parallel;
#   * chain outputs land exactly where the next chain consumes them
#     (a1,b1,c1,d1 end in r12,r13,r14,r15): zero inter-chain shuffle movs;
#   * two advancing pointers r8 (quarter0) / r9 (quarter1), quarter2/3 accessed as
#     [r9+r11] / [r9+r11*2]; r11 = D*8 constant (no stack slot needed).
# Live map: m=rdi End=rbp RootO=rbx RootI=rsi pa=r8 pb=r9 D8=r11 cnt=rcx
#           data/scratch = rax rdx r10 r12 r13 r14 r15
	.globl	nat_asmNtt2_radix4
nat_asmNtt2_radix4:
	push	rbx
	push	rbp
	push	r12
	push	r13
	push	r14
	push	r15
	mov	rbp, r9			# End (byte)
	lea	r11, [rdi*8]		# r11 = D*8
	mov	rdi, rsi		# mod
	mov	rbx, rcx		# RootO
	mov	rsi, r8			# RootI
	mov	r8,  rdx		# pa = Begin
	lea	r9,  [r8+r11]		# pb = Begin + D
.Lm4q_block:
	mov	rcx, r11
	shr	rcx, 3			# cnt = D groups per block
.Lm4q_group:
	mov	r12, [r8]		# a  (u of chain2)
	mov	r13, [r9]		# b  (u of chain1)
	mov	r14, [r9+r11]		# c  (v of chain2)
	mov	r15, [r9+r11*2]		# d  (v of chain1)
	# chain1 = bfly(b=r13, d=r15, w), scratch rax  (p_w kept in rdx for chain2)
	mov	rdx, [rbx+8]		# p_w
	mulx	rax, rax, r15		# q1 = hi(p_w*d)
	imul	r15, [rbx]		# low(d*w)
	imul	rax, rdi		# q1*mod
	sub	r15, rax
	lea	rax, [r15+rdi]
	cmovns	rax, r15		# z1 (d dead)
	# chain2 = bfly(a=r12, c=r14, w), scratch r10
	mulx	r10, r10, r14		# q2 = hi(p_w*c)
	imul	r14, [rbx]		# low(c*w)
	imul	r10, rdi		# q2*mod
	sub	r14, r10
	lea	r10, [r14+rdi]
	cmovns	r10, r14		# z2 (c dead)
	# chain1 out: red(b)->r13 ; b1=r13, d1=r15
	mov	r15, r13
	sub	r13, rdi
	cmovs	r13, r15		# red(b)
	lea	r15, [r13+rdi]
	add	r13, rax		# b1 = red(b)+z1
	sub	r15, rax		# d1 = red(b)-z1+m
	# chain2 out: red(a)->r12 ; a1=r12, c1=r14
	mov	r14, r12
	sub	r12, rdi
	cmovs	r12, r14		# red(a)
	lea	r14, [r12+rdi]
	add	r12, r10		# a1
	sub	r14, r10		# c1
	# chain3 = bfly(a1=r12, b1=r13, w2), scratch rax -> store a2,b2
	mov	rdx, [rsi+8]		# p_w2
	mulx	rax, rax, r13		# q3 = hi(p_w2*b1)
	imul	r13, [rsi]		# low(b1*w2)
	imul	rax, rdi
	sub	r13, rax
	lea	rax, [r13+rdi]
	cmovns	rax, r13		# z3 (b1 dead)
	mov	r13, r12
	sub	r12, rdi
	cmovs	r12, r13		# red(a1)
	lea	r13, [r12+rdi]
	add	r12, rax		# a2
	mov	[r8], r12
	sub	r13, rax		# b2
	mov	[r9], r13
	# chain4 = bfly(c1=r14, d1=r15, w3), scratch r10 -> store c2,d2
	mov	rdx, [rsi+24]		# p_w3
	mulx	r10, r10, r15		# q4 = hi(p_w3*d1)
	imul	r15, [rsi+16]		# low(d1*w3)
	imul	r10, rdi
	sub	r15, r10
	lea	r10, [r15+rdi]
	cmovns	r10, r15		# z4 (d1 dead)
	mov	r15, r14
	sub	r14, rdi
	cmovs	r14, r15		# red(c1)
	lea	r15, [r14+rdi]
	add	r14, r10		# c2
	mov	[r9+r11], r14
	sub	r15, r10		# d2
	mov	[r9+r11*2], r15
	add	r8, 8
	add	r9, 8
	dec	rcx
	jnz	.Lm4q_group
	# block done: pointers consumed D elems; step 3*D*8 more, advance root cursors
	mov	rax, r11
	lea	rax, [rax+rax*2]
	add	r8, rax
	add	r9, rax
	add	rbx, 16
	add	rsi, 32
	cmp	r8, rbp
	jb	.Lm4q_block
.Lm4q_fin:
	pop	r15
	pop	r14
	pop	r13
	pop	r12
	pop	rbp
	pop	rbx
	ret

# ---------------- nat_asmNtt_radix4: m4q + root==1 first block special-cased -------------
# Same two-layer merged pass as nat_asmNtt2_radix4, but with the first 4D block (block index
# 0, where the outer-layer twiddle w and the first inner-layer twiddle w2 are both 1)
# handled without any twiddle multiply, mirroring what nat_asmNtt does vs nat_asmNtt2.
#   outer butterflies (w=1):  u'=red(u), v'=red(v); lo=u'+v', hi=u'+m-v'
#   inner (a1,b1) w2=1:        same no-multiply butterfly
#   inner (c1,d1) w3 (root of the distance-D block 1, generally != 1): generic chain
# The generic remainder jumps into the nat_asmNtt2_radix4 block loop (.Lm4q_block), so this
# function shares its epilogue; caller must pass roots whose pair 0 is (1, p(1)).
	.globl	nat_asmNtt_radix4
nat_asmNtt_radix4:
	push	rbx
	push	rbp
	push	r12
	push	r13
	push	r14
	push	r15
	mov	rbp, r9			# End
	lea	r11, [rdi*8]		# r11 = D*8
	mov	rdi, rsi		# mod
	mov	rbx, rcx		# RootO
	mov	rsi, r8			# RootI
	mov	r8,  rdx		# pa = Begin
	lea	r9,  [r8+r11]		# pb
	# ---- special first block: D groups, 3 no-multiply + 1 generic (w3) chains ----
	mov	rcx, r11
	shr	rcx, 3			# cnt = D
.Lr4f_group:
	mov	r12, [r8]		# a
	mov	r13, [r9]		# b
	mov	r14, [r9+r11]		# c
	mov	r15, [r9+r11*2]		# d
	# outer BF(b,d) w=1: b1(lo)->r13, d1(hi)->r15, scratch rax
	mov	rax, r13
	sub	r13, rdi
	cmovs	r13, rax		# red(b)
	mov	rax, r15
	sub	r15, rdi
	cmovs	r15, rax		# red(d)
	lea	rax, [r13+rdi]		# red(b)+m
	sub	rax, r15		# d1 = red(b)+m-red(d)
	add	r13, r15		# b1 = red(b)+red(d)
	mov	r15, rax		# d1 -> r15
	# outer BF(a,c) w=1: a1(lo)->r12, c1(hi)->r14, scratch r10
	mov	r10, r12
	sub	r12, rdi
	cmovs	r12, r10		# red(a)
	mov	r10, r14
	sub	r14, rdi
	cmovs	r14, r10		# red(c)
	lea	r10, [r12+rdi]		# red(a)+m
	sub	r10, r14		# c1 = red(a)+m-red(c)
	add	r12, r14		# a1 = red(a)+red(c)
	mov	r14, r10		# c1 -> r14
	# inner BF(a1,b1) w2=1: a2(lo)->[r8], b2(hi)->[r9], scratch rax
	mov	rax, r12
	sub	r12, rdi
	cmovs	r12, rax		# red(a1)
	mov	rax, r13
	sub	r13, rdi
	cmovs	r13, rax		# red(b1)
	lea	rax, [r12+rdi]		# red(a1)+m
	sub	rax, r13		# b2 = red(a1)+m-red(b1)
	add	r12, r13		# a2 = red(a1)+red(b1)
	mov	[r8], r12
	mov	[r9], rax
	# inner BF(c1,d1) w3 (generic chain): c2(lo)->[r9+r11], d2(hi)->[r9+r11*2]
	mov	rdx, [rsi+24]		# p_w3
	mulx	rax, rax, r15		# q
	imul	r15, [rsi+16]		# low(d1*w3)
	imul	rax, rdi
	sub	r15, rax
	lea	rax, [r15+rdi]
	cmovns	rax, r15		# z (d1 dead)
	mov	r15, r14		# save c1
	sub	r14, rdi
	cmovs	r14, r15		# red(c1)
	lea	r15, [r14+rdi]
	add	r14, rax		# c2
	mov	[r9+r11], r14
	sub	r15, rax		# d2
	mov	[r9+r11*2], r15
	add	r8, 8
	add	r9, 8
	dec	rcx
	jnz	.Lr4f_group
	# ---- past the special block: continue with the generic blocks of nat_asmNtt2_radix4
	mov	rax, r11
	lea	rax, [rax+rax*2]
	add	r8, rax
	add	r9, rax
	add	rbx, 16
	add	rsi, 32
	cmp	r8, rbp
	jae	.Lm4q_fin
	jmp	.Lm4q_block

# ---------------- nat_asmINtt2_radix4: merged inverse layers D then 2D ----------------
# Register map: m=rdi End=rbp RootO=rbx RootI=rsi pa=r8 pb=r9 D8=r11 cnt=rcx
#               data/scratch = rax rdx r10 r12 r13 r14 r15
# Chain choreography (no inter-chain shuffle movs):
#   chain1 (a=r12,b=r13,w2): a1(lo)->r12, b1(hi)->r13
#   chain2 (c=r14,d=r15,w3): c1(lo)->r14, d1(hi)->r15
#   chain3 (a1=r12,c1=r14,w): a2->[r8], c2->[r9+r11]
#   chain4 (b1=r13,d1=r15,w): b2->[r9], d2->[r9+r11*2]
	.globl	nat_asmINtt2_radix4
nat_asmINtt2_radix4:
	push	rbx
	push	rbp
	push	r12
	push	r13
	push	r14
	push	r15
	mov	rbp, r9			# End
	lea	r11, [rdi*8]		# r11 = D*8
	mov	rdi, rsi		# mod
	mov	rbx, rcx		# RootO
	mov	rsi, r8			# RootI
	mov	r8,  rdx		# pa = Begin
	lea	r9,  [r8+r11]		# pb = Begin + D
.Li4_block:
	mov	rcx, r11
	shr	rcx, 3			# cnt = D groups per block
.Li4_group:
	mov	r12, [r8]		# a
	mov	r13, [r9]		# b
	mov	r14, [r9+r11]		# c
	mov	r15, [r9+r11*2]		# d
	# chain1 = inv-bfly(a=r12, b=r13, w2): a1->r12, b1->r13, scratch rax
	mov	rdx, [rsi+8]		# p_w2
	lea	rax, [r12+r13]		# s
	sub	r12, r13		# a-b
	add	r12, rdi		# x
	mulx	r13, r13, r12		# q (hi of p_w2*x) -> r13
	imul	r12, [rsi]		# low(x*w2)
	imul	r13, rdi
	sub	r12, r13
	lea	r13, [r12+rdi]
	cmovns	r13, r12		# b1 (r12 dead)
	mov	r12, rax
	sub	rax, rdi
	cmovns	r12, rax		# a1 = red(a+b) (rax dead)
	# chain2 = inv-bfly(c=r14, d=r15, w3): c1->r14, d1->r15, scratch r10
	mov	rdx, [rsi+24]		# p_w3
	lea	r10, [r14+r15]		# s
	sub	r14, r15		# c-d
	add	r14, rdi		# x
	mulx	r15, r15, r14		# q -> r15
	imul	r14, [rsi+16]		# low(x*w3)
	imul	r15, rdi
	sub	r14, r15
	lea	r15, [r14+rdi]
	cmovns	r15, r14		# d1 (r14 dead)
	mov	r14, r10
	sub	r10, rdi
	cmovns	r14, r10		# c1 = red(c+d) (r10 dead)
	# chain3 = inv-bfly(a1=r12, c1=r14, w): a2(lo)->[r8], c2(hi)->[r9+r11]
	mov	rdx, [rbx+8]		# p_w (rdx kept for chain4)
	lea	rax, [r12+r14]		# s
	sub	r12, r14		# a1-c1
	add	r12, rdi		# x
	mov	r10, rax
	sub	rax, rdi
	cmovns	r10, rax		# lo
	mov	[r8], r10		# store a2
	mulx	r10, r10, r12		# q (hi of p_w*x)
	imul	r12, [rbx]		# low(x*w)
	imul	r10, rdi
	sub	r12, r10
	lea	r10, [r12+rdi]
	cmovns	r10, r12		# hi = c2
	mov	[r9+r11], r10		# store c2
	# chain4 = inv-bfly(b1=r13, d1=r15, w): b2(lo)->[r9], d2(hi)->[r9+r11*2]
	lea	rax, [r13+r15]		# s
	sub	r13, r15		# b1-d1
	add	r13, rdi		# x
	mov	r10, rax
	sub	rax, rdi
	cmovns	r10, rax		# lo
	mov	[r9], r10		# store b2
	mulx	r10, r10, r13		# q (rdx still p_w)
	imul	r13, [rbx]		# low(x*w)
	imul	r10, rdi
	sub	r13, r10
	lea	r10, [r13+rdi]
	cmovns	r10, r13		# hi = d2
	mov	[r9+r11*2], r10		# store d2
	add	r8, 8
	add	r9, 8
	dec	rcx
	jnz	.Li4_group
	# block done: pointers consumed D elems; step 3*D*8 more, advance root cursors
	mov	rax, r11
	lea	rax, [rax+rax*2]
	add	r8, rax
	add	r9, rax
	add	rbx, 16
	add	rsi, 32
	cmp	r8, rbp
	jb	.Li4_block
.Li4_fin:
	pop	r15
	pop	r14
	pop	r13
	pop	r12
	pop	rbp
	pop	rbx
	ret

# ---------------- nat_asmINtt_radix4: inverse m4q + root==1 first block special-cased --
# Same two-layer merged pass as nat_asmINtt2_radix4, but the first 4D block (block index 0:
# stage-D twiddle w2 == 1 on (a,b) and stage-2D twiddle w == 1 on the inner pairs)
# is handled without twiddle multiplies, mirroring nat_asmINtt vs nat_asmINtt2.
#   no-multiply inverse butterfly (u,v) < m: lo=red(u+v); hi=(u-v) mod m
#   stage-D (c,d) still uses generic w3 chain (block-1 root, generally != 1).
# The generic remainder jumps into the nat_asmINtt2_radix4 block loop (.Li4_block).
	.globl	nat_asmINtt_radix4
nat_asmINtt_radix4:
	push	rbx
	push	rbp
	push	r12
	push	r13
	push	r14
	push	r15
	mov	rbp, r9			# End
	lea	r11, [rdi*8]		# r11 = D*8
	mov	rdi, rsi		# mod
	mov	rbx, rcx		# RootO
	mov	rsi, r8			# RootI
	mov	r8,  rdx		# pa = Begin
	lea	r9,  [r8+r11]		# pb
	# ---- special first block: D groups, 3 no-multiply + 1 generic (w3) chains ----
	mov	rcx, r11
	shr	rcx, 3			# cnt = D
.Lr4i_group:
	mov	r12, [r8]		# a
	mov	r13, [r9]		# b
	mov	r14, [r9+r11]		# c
	mov	r15, [r9+r11*2]		# d
	# stage-D inv-BF(a,b) w2=1: a1(lo)->r12, b1(hi)->r13, scratch rax
	lea	rax, [r12+r13]		# s
	sub	r12, r13		# d
	lea	r13, [r12+rdi]		# d+m
	cmovns	r13, r12		# b1 = (a-b) mod m
	mov	r12, rax		# r12 = s (d dead)
	sub	rax, rdi		# s-m
	cmovns	r12, rax		# a1 = red(s)
	# stage-D inv-BF(c,d) w3 generic: c1(lo)->r14, d1(hi)->r15, scratch r10
	mov	rdx, [rsi+24]		# p_w3
	lea	r10, [r14+r15]		# s
	sub	r14, r15		# d
	add	r14, rdi		# x
	mulx	r15, r15, r14		# q (hi of p_w3*x) -> r15
	imul	r14, [rsi+16]		# low(x*w3)
	imul	r15, rdi
	sub	r14, r15
	lea	r15, [r14+rdi]
	cmovns	r15, r14		# d1 (r14 dead)
	mov	r14, r10
	sub	r10, rdi
	cmovns	r14, r10		# c1 = red(c+d)
	# stage-2D inv-BF(a1,c1) w=1: a2(lo)->[r8], c2(hi)->[r9+r11], scratch rax
	lea	rax, [r12+r14]		# s
	sub	r12, r14		# d
	lea	r14, [r12+rdi]		# d+m
	cmovns	r14, r12		# c2 = (a1-c1) mod m
	mov	r12, rax		# r12 = s (d dead)
	sub	rax, rdi		# s-m
	cmovns	r12, rax		# a2 = red(s)
	mov	[r8], r12
	mov	[r9+r11], r14
	# stage-2D inv-BF(b1,d1) w=1: b2(lo)->[r9], d2(hi)->[r9+r11*2], scratch r10
	lea	r10, [r13+r15]		# s
	sub	r13, r15		# d
	lea	r15, [r13+rdi]		# d+m
	cmovns	r15, r13		# d2 = (b1-d1) mod m
	mov	r13, r10		# r13 = s (d dead)
	sub	r10, rdi		# s-m
	cmovns	r13, r10		# b2 = red(s)
	mov	[r9], r13
	mov	[r9+r11*2], r15
	add	r8, 8
	add	r9, 8
	dec	rcx
	jnz	.Lr4i_group
	# ---- past the special block: continue with the generic blocks of nat_asmINtt2_radix4
	mov	rax, r11
	lea	rax, [rax+rax*2]
	add	r8, rax
	add	r9, rax
	add	rbx, 16
	add	rsi, 32
	cmp	r8, rbp
	jae	.Li4_fin
	jmp	.Li4_block

# ---------------- nat_asmNtt2: single-layer DIF forward pass (generic, 2 butterflies/iter)
#   void nat_asmNtt2(uint64_t N, uint64_t mod, uint64_t* Begin, const uint64_t* Root, uint64_t* End)
#   per 2N-block (block root pair (b,p), p = floor(b*2^64/m)+1), butterflies at distance N:
#     u' = red(u);  z = Barrett(v*b) mod m;  a[i] = u'+z ;  a[i+N] = u'-z+m
#   Register state after entry shuffle (shared with nat_asmNtt):
#     rsi=N rdi=mod r8=B r9=Root rbp=End ; rcx cnt, rdx=p, rbx=b
	.globl	nat_asmNtt2
nat_asmNtt2:
	push	rbx
	push	rbp
	push	r12
	push	r13
	push	r14
	push	r15
	mov	rax, r8			# End
	mov	r9,  rcx		# Root
	mov	r8,  rdx		# Begin
	mov	r10, rsi		# mod
	mov	rsi, rdi		# N
	mov	rdi, r10
	mov	rbp, rax
.Lnt2_block:
	mov	rcx, rsi
	mov	rbx, [r9]		# b
	mov	rdx, [r9+8]		# p
	shr	rcx, 1
.Lnt2_bfly:
	mov	r12, [r8]		# u0
	mov	r13, [r8+8]		# u1
	mov	r14, [r8+rsi*8]		# v0
	mov	r15, [r8+rsi*8+8]	# v1
	mulx	r10, rax, r14		# q0
	mulx	r11, rax, r15		# q1
	imul	r14, rbx		# low(v0*b)
	imul	r15, rbx		# low(v1*b)
	imul	r10, rdi		# q0*mod
	imul	r11, rdi		# q1*mod
	sub	r14, r10
	lea	r10, [r14+rdi]
	cmovns	r10, r14		# z0
	sub	r15, r11
	lea	r11, [r15+rdi]
	cmovns	r11, r15		# z1
	mov	r14, r12
	sub	r12, rdi
	cmovs	r12, r14		# red(u0)
	mov	r15, r13
	sub	r13, rdi
	cmovs	r13, r15		# red(u1)
	lea	r14, [r12+rdi]
	lea	r15, [r13+rdi]
	add	r12, r10		# lo0
	add	r13, r11		# lo1
	sub	r14, r10		# hi0
	sub	r15, r11		# hi1
	mov	[r8], r12
	mov	[r8+8], r13
	mov	[r8+rsi*8], r14
	mov	[r8+rsi*8+8], r15
	add	r8, 16
	dec	rcx
	jnz	.Lnt2_bfly
	lea	r8, [r8+rsi*8]
	add	r9, 16
	cmp	r8, rbp
	jb	.Lnt2_block
.Lnt2_fin:
	pop	r15
	pop	r14
	pop	r13
	pop	r12
	pop	rbp
	pop	rbx
	ret

# ---------------- nat_asmNtt: nat_asmNtt2 + root==1 first block no-multiply ----------------
# Same single-layer pass as nat_asmNtt2, but block 0 (root value 1) uses the multiply-free
# butterfly: u'=red(u), v'=red(v), lo=u'+v', hi=u'+m-v'.  After the special block the
# generic remainder jumps into nat_asmNtt2's block loop (.Lnt2_block), sharing its
# epilogue (.Lnt2_fin); caller must pass a root table whose pair 0 is (1, p(1)).
	.globl	nat_asmNtt
nat_asmNtt:
	push	rbx
	push	rbp
	push	r12
	push	r13
	push	r14
	push	r15
	mov	rax, r8			# End
	mov	r9,  rcx		# Root
	mov	r8,  rdx		# Begin
	mov	r10, rsi		# mod
	mov	rsi, rdi		# N
	mov	rdi, r10
	mov	rbp, rax
	# ---- special first block (root == 1), N/2 iterations of 2 butterflies ----
	mov	rcx, rsi
	shr	rcx, 1
.Lnt_sp:
	mov	r12, [r8]		# u0
	mov	r13, [r8+8]		# u1
	mov	r14, [r8+rsi*8]		# v0
	mov	r15, [r8+rsi*8+8]	# v1
	mov	r10, r12
	sub	r12, rdi
	cmovs	r12, r10		# red(u0)
	mov	r11, r13
	sub	r13, rdi
	cmovs	r13, r11		# red(u1)
	mov	r10, r14
	sub	r14, rdi
	cmovs	r14, r10		# red(v0)
	mov	r11, r15
	sub	r15, rdi
	cmovs	r15, r11		# red(v1)
	lea	r10, [r12+rdi]		# red(u0)+m
	lea	r11, [r13+rdi]		# red(u1)+m
	add	r12, r14		# lo0
	add	r13, r15		# lo1
	sub	r10, r14		# hi0
	sub	r11, r15		# hi1
	mov	[r8], r12
	mov	[r8+8], r13
	mov	[r8+rsi*8], r10
	mov	[r8+rsi*8+8], r11
	add	r8, 16
	dec	rcx
	jnz	.Lnt_sp
	# ---- advance to block 1 and jump into nat_asmNtt2's generic block loop ----
	lea	r8, [r8+rsi*8]
	add	r9, 16
	cmp	r8, rbp
	jae	.Lnt2_fin
	jmp	.Lnt2_block

# ---------------- nat_asmINtt2: single-layer DIT inverse pass (generic, 2 butterflies/iter)
#   void nat_asmINtt2(uint64_t N, uint64_t mod, uint64_t* Begin, const uint64_t* Root, uint64_t* End)
#   per 2N-block (block root pair (b,p)), butterflies at distance N, inputs/outputs in [0,m):
#     s = u+v ;  lo = red(s) -> a[i] ;  x = u-v+m ;  hi = Barrett(x*b) mod m -> a[i+N]
	.globl	nat_asmINtt2
nat_asmINtt2:
	push	rbx
	push	rbp
	push	r12
	push	r13
	push	r14
	push	r15
	mov	rax, r8			# End
	mov	r9,  rcx		# Root
	mov	r8,  rdx		# Begin
	mov	r10, rsi		# mod
	mov	rsi, rdi		# N
	mov	rdi, r10
	mov	rbp, rax
.Lit2_block:
	mov	rcx, rsi
	mov	rbx, [r9]		# b
	mov	rdx, [r9+8]		# p
	shr	rcx, 1
.Lit2_bfly:
	mov	r12, [r8]		# u0
	mov	r13, [r8+8]		# u1
	mov	r14, [r8+rsi*8]		# v0
	mov	r15, [r8+rsi*8+8]	# v1
	lea	r10, [r12+r14]		# s0
	lea	r11, [r13+r15]		# s1
	sub	r12, r14		# d0
	sub	r13, r15		# d1
	mov	r14, r10
	sub	r10, rdi
	cmovs	r10, r14		# lo0 = red(s0)
	mov	r15, r11
	sub	r11, rdi
	cmovs	r11, r15		# lo1 = red(s1)
	add	r12, rdi		# x0
	add	r13, rdi		# x1
	mov	[r8], r10
	mov	[r8+8], r11
	mulx	r10, rax, r12		# q0
	mulx	r11, rax, r13		# q1
	imul	r12, rbx		# low(x0*b)
	imul	r13, rbx		# low(x1*b)
	imul	r10, rdi		# q0*mod
	imul	r11, rdi		# q1*mod
	sub	r12, r10
	lea	r10, [r12+rdi]
	cmovns	r10, r12		# hi0
	sub	r13, r11
	lea	r11, [r13+rdi]
	cmovns	r11, r13		# hi1
	mov	[r8+rsi*8], r10
	mov	[r8+rsi*8+8], r11
	add	r8, 16
	dec	rcx
	jnz	.Lit2_bfly
	lea	r8, [r8+rsi*8]
	add	r9, 16
	cmp	r8, rbp
	jb	.Lit2_block
.Lit2_fin:
	pop	r15
	pop	r14
	pop	r13
	pop	r12
	pop	rbp
	pop	rbx
	ret

# ---------------- nat_asmINtt: nat_asmINtt2 + root==1 first block no-multiply ----------------
# Same single-layer inverse pass as nat_asmINtt2, but block 0 (root value 1) uses the
# multiply-free inverse butterfly: lo=red(u+v), hi=(u-v) mod m.  The generic remainder
# jumps into nat_asmINtt2's block loop (.Lit2_block), sharing its epilogue (.Lit2_fin);
# caller must pass a root table whose pair 0 is (1, p(1)).
	.globl	nat_asmINtt
nat_asmINtt:
	push	rbx
	push	rbp
	push	r12
	push	r13
	push	r14
	push	r15
	mov	rax, r8			# End
	mov	r9,  rcx		# Root
	mov	r8,  rdx		# Begin
	mov	r10, rsi		# mod
	mov	rsi, rdi		# N
	mov	rdi, r10
	mov	rbp, rax
	# ---- special first block (root == 1), N/2 iterations of 2 butterflies ----
	mov	rcx, rsi
	shr	rcx, 1
.Lit_sp:
	mov	r12, [r8]		# u0
	mov	r13, [r8+8]		# u1
	mov	r14, [r8+rsi*8]		# v0
	mov	r15, [r8+rsi*8+8]	# v1
	lea	r10, [r12+r14]		# s0
	lea	r11, [r13+r15]		# s1
	sub	r12, r14		# d0
	lea	r14, [r12+rdi]		# d0+m
	cmovns	r14, r12		# hi0 = (u0-v0) mod m
	sub	r13, r15		# d1
	lea	r15, [r13+rdi]		# d1+m
	cmovns	r15, r13		# hi1 = (u1-v1) mod m
	mov	r12, r10
	sub	r10, rdi
	cmovs	r10, r12		# lo0 = red(s0)
	mov	r13, r11
	sub	r11, rdi
	cmovs	r11, r13		# lo1 = red(s1)
	mov	[r8], r10
	mov	[r8+8], r11
	mov	[r8+rsi*8], r14
	mov	[r8+rsi*8+8], r15
	add	r8, 16
	dec	rcx
	jnz	.Lit_sp
	# ---- advance to block 1 and jump into nat_asmINtt2's generic block loop ----
	lea	r8, [r8+rsi*8]
	add	r9, 16
	cmp	r8, rbp
	jae	.Lit2_fin
	jmp	.Lit2_block

# ================================ nat_asmNttMul ======================================
# void nat_asmNttMul(uint64_t N, uint64_t* Dst, const uint64_t* Src1, const uint64_t* Src2,
#                const uint64_t* Root, uint64_t i)         (SysV: rdi rsi rdx rcx r8 r9)
#
# Fused last NTT layer x pointwise multiply x first INTT layer, for modulus i.
# Per group of four elements (g = 0 .. N/4-1), with a_k = Src1[4g+k], b_k = Src2[4g+k]
# reduced mod m, w = Root[2g] (twiddle pair g) and
#     z  = ( b1*w) mod m        zn = (-b3*w) mod m
# the four outputs are
#     Dst[4g+0] = a0*b0 + a1*z      Dst[4g+2] = a2*b2 + a3*zn
#     Dst[4g+1] = a0*b1 + a1*b0     Dst[4g+3] = a2*b3 + a3*b2
#
# Why this shape: the distance-1 forward layer uses twiddle p for block 2g and q = p*zeta
# for block 2g+1 (zeta^2 = -1), and the table pair g holds w = p^2, so the second block
# needs -w: that is exactly what "zn" is.  The inverse layer's twiddles cancel against
# the pointwise product, so only the forward root table is needed and the operator carries
# the implicit 1/2 that balances the final 2^-(scale-1) shift.
# (Derivation + validation: bench/bench_nttmul_model.cpp, bench/bench_mul.cpp.)
#
# This version is generated three times from NTTMUL_BODY so that each modulus gets
#   * an immediate shld count (llvm-mca: 1 uOp instead of 4 for the CL form),
#   * its own Barrett correction count: 1 for m0/m2, 2 for m1 (the m1 reciprocal is one
#     bit coarser relative to the modulus, so its quotient estimate can be two too small;
#     criterion: floor-2^t/m + 2m^2/2^(t+64) < 1 for t = 60+i  ->  0.95/1.37/0.97).
# Liveness (see NTTMUL_PHASE): rbx=Dst rbp=Src1 rsi=Src2 rdi=Root r11=m r12=R r13=counter,
# and a0=r8 a1=r9 b0=r10 b1=rcx z=r14 kept live across the first Barrett so that no input
# has to be reloaded/re-reduced for the second output; rdx:rax = accumulator, r15 scratch.

# r -= m if r >= m   (one conditional subtraction; also used for the Barrett corrections)
	.macro	NTTMUL_REDUCE r, t
	mov	\t, \r
	sub	\r, r11
	cmovb	\r, \t
	.endm

# dst = (v * w) mod m      (v consumed; t scratch, clobbers rdx)
	.macro	NTTMUL_SHOUP_POS v, dst, t
	mov	rdx, \v
	mulx	\t, \dst, [rdi+8]	# t = high(v * pw)
	imul	\v, [rdi]		# v = low(v * w)
	imul	\t, r11			# q * m
	sub	\v, \t
	lea	\dst, [\v+r11]
	cmovns	\dst, \v		# +m if that went negative
	.endm

# dst = (-v * w) mod m     (v consumed; t scratch, clobbers rdx)
	.macro	NTTMUL_SHOUP_NEG v, dst, t
	mov	rdx, \v
	mulx	\t, \dst, [rdi+8]
	imul	\v, [rdi]
	imul	\t, r11
	sub	\t, \v			# q*m - low(v * w)
	lea	\dst, [\t+r11]
	cmovns	\dst, \t
	.endm

# One element pair: outputs at [rbx+off] and [rbx+off+8].
#   zneg = 0 -> z  = ( b1*w) mod m       zneg = 1 -> zn = (-b3*w) mod m
	.macro	NTTMUL_PHASE zneg, off, shimm, corr
	mov	rax, [rsi + \off + 8]	# b1 (or b3)
	NTTMUL_REDUCE rax, r15
	.if \zneg
	NTTMUL_SHOUP_NEG rax, r14, r15	# r14 = zn
	.else
	NTTMUL_SHOUP_POS rax, r14, r15	# r14 = z
	.endif
	# the four inputs, all reduced; b1 is kept in rcx for the second output
	mov	r8,  [rbp + \off]	# a0
	mov	r9,  [rbp + \off + 8]	# a1
	mov	r10, [rsi + \off]	# b0
	mov	rcx, [rsi + \off + 8]	# b1
	NTTMUL_REDUCE r8, r15
	NTTMUL_REDUCE r9, r15
	NTTMUL_REDUCE r10, r15
	NTTMUL_REDUCE rcx, r15
	# first output: a0*b0 + a1*z
	mov	rdx, r9
	mulx	r15, r14, r14		# r15:r14 = a1*z    (z dies into the product)
	mov	rax, r8
	mul	r10			# rdx:rax = a0*b0
	add	rax, r14
	adc	rdx, r15
	shld	rdx, rax, \shimm	# A = (rdx:rax) >> (60+i)
	mulx	r15, r14, r12		# q = high(A * R)
	imul	r15, r11		# q * m
	sub	rax, r15
	NTTMUL_REDUCE rax, r14
	.if \corr == 2
	NTTMUL_REDUCE rax, r14
	.endif
	mov	[rbx + \off], rax
	# second output: a0*b1 + a1*b0   (everything still in registers)
	mov	rdx, r8
	mulx	r15, r14, rcx		# r15:r14 = a0*b1
	mov	rax, r9
	mul	r10			# rdx:rax = a1*b0
	add	rax, r14
	adc	rdx, r15
	shld	rdx, rax, \shimm
	mulx	r15, r14, r12
	imul	r15, r11
	sub	rax, r15
	NTTMUL_REDUCE rax, r14
	.if \corr == 2
	NTTMUL_REDUCE rax, r14
	.endif
	mov	[rbx + \off + 8], rax
	.endm

# Complete specialized body for one modulus.
	.macro	NTTMUL_BODY funclab, Mi, Ri, shimm, corr
\funclab:
	push	rbx
	push	rbp
	push	r12
	push	r13
	push	r14
	push	r15
	mov	r13, rdi		# N
	shr	r13, 2			# groups = N/4
	mov	rbx, rsi		# Dst
	mov	rbp, rdx		# Src1
	mov	rsi, rcx		# Src2
	mov	rdi, r8			# Root
	mov	r11, \Mi		# modulus
	mov	r12, \Ri		# floor(2^(124+i)/m)
	test	r13, r13
	je	2f
1:
	NTTMUL_PHASE 0, 0,  \shimm, \corr
	NTTMUL_PHASE 1, 16, \shimm, \corr
	add	rbx, 32
	add	rbp, 32
	add	rsi, 32
	add	rdi, 16
	dec	r13
	jnz	1b
2:
	pop	r15
	pop	r14
	pop	r13
	pop	r12
	pop	rbp
	pop	rbx
	ret
	.endm

	NTTMUL_BODY .Lnttmul_m0, 0x1b00000000000001, 0x97b425ed097b4259, 4, 1
	NTTMUL_BODY .Lnttmul_m1, 0x3a00000000000001, 0x8d3dcb08d3dcb08a, 3, 2
	NTTMUL_BODY .Lnttmul_m2, 0x5700000000000001, 0xbc52640bc52640ba, 2, 1

	.globl	nat_asmNttMul
nat_asmNttMul:
	cmp	r9, 1
	je	.Lnttmul_m1
	ja	.Lnttmul_m2
	jmp	.Lnttmul_m0

	.text

# ================================= nat_asmCRT ========================================
# uint64_t nat_asmCRT(uint64_t* _info_0, uint64_t* _info_1, uint64_t* _info_2, uint64_t _Size)
#   SysV: rdi = info0 (in/out), rsi = info1, rdx = info2, rcx = Size
#
# Per coefficient i: merge the three residues (info0[i], info1[i], info2[i]) into the
# exact value x < m0*m1*m2 (Garner / CRT, three words):
#     w2 = ((r2 - r0) mod m2) * inv(m0 mod m2) mod m2
#     u  = m0 * w2                                   (128 bit)
#     t  = ((r1 - r0 - u) mod m1) * inv(m0*m2 mod m1) mod m1
#     x  = r0 + u + (m0*m2) * t
# then add the running carry, store the low word into info0[i] and keep the upper two
# words as the carry.  After the loop info1[0] = middle, info1[1] = high and the middle
# word is returned.  (The "+2" in the m1 estimate is the original's way of folding
# "u mod m1" into a single Shoup step; it is kept verbatim.)
#
# This is the same algorithm as the original MASM nat_asmCRT (bench/asmCRT_orig.s, checked
# against the CRT congruences in bench/bench_crt.cpp) with three llvm-mca driven changes:
#   * the ten movabs constants moved out of the loop into .rodata (they cost 10 uOps and
#     ~110 bytes of immediate per coefficient, all on the contended ALU ports),
#   * the two `mul rdx` steps (which clobber rdx and showed up as a loop-carried
#     dependency in -timeline) became mulx,
#   * two coefficients per iteration: the per-coefficient CRT maths are independent, only
#     the two-word carry is serial, so the scheduler gets two chains to overlap.
# Registers: rcx=info0 rbp=info1(base) r8=info2 r9=info1 r11=modulus-in-use
#            r14:r15=carry  temps: rax rbx rdx rsi rdi r10 r13.

	.macro	CRT_LIMB off
	mov	r10, [rcx + \off]		# r0
	mov	rbx, [r8 + \off]		# r2
	mov	r13, [r9 + \off]		# r1
	mov	r11, [rip + .Lcrt_m2]
	sub	rbx, r10			# r2 - r0
	add	rbx, r11			# + m2   (positive representative)
	mov	rax, [rip + .Lcrt_cm2]
	mul	rbx				# rdx = high(v * CM2)
	mov	rsi, [rip + .Lcrt_inv0_2]
	imul	rbx, rsi			# low(v * inv0_2)
	imul	rdx, r11			# q * m2
	sub	rbx, rdx
	lea	rdx, [rbx+r11]
	cmovns	rdx, rbx			# rdx = w2
	mov	rbx, [rip + .Lcrt_m0]
	mov	r11, [rip + .Lcrt_m1]
	mulx	rdi, rsi, rbx			# rdi:rsi = u = m0 * w2
	mulx	rbx, rax, [rip + .Lcrt_cm1]	# rbx = high(w2 * CM1)
	add	rbx, 2
	imul	rbx, r11			# (q + 2) * m1
	sub	r13, rsi			# r1 - low(u)
	sub	r13, r10			#      - r0
	add	rbx, r13			# e
	mov	rax, [rip + .Lcrt_cm02_1]
	mul	rbx				# rdx = high(e * CM02_1)
	mov	rax, [rip + .Lcrt_inv02_1]
	imul	rbx, rax			# low(e * inv02_1)
	imul	rdx, r11			# q * m1
	sub	rbx, rdx
	lea	rdx, [rbx+r11]
	cmovns	rdx, rbx			# rdx = t
	add	rsi, r10
	adc	rdi, 0				# rdi:rsi = u + r0
	mov	r13, [rip + .Lcrt_m0m2_lo]
	mulx	r11, r10, r13			# r11:r10 = t * (m0*m2)_low
	mulx	r13, rax, [rip + .Lcrt_m0m2_hi]	# r13:rax = t * (m0*m2)_high   (rdx = t still)
	add	rsi, r10
	adc	rdi, r11
	add	rax, rdi
	adc	r13, 0				# x = r13:rax:rsi
	add	rsi, r14
	adc	rax, r15
	adc	r13, 0				# + running carry
	mov	[rcx + \off], rsi
	mov	r14, rax			# new carry
	mov	r15, r13
	.endm

	.globl	nat_asmCRT
nat_asmCRT:
	push	rbx
	push	rbp
	push	r12
	push	r13
	push	r14
	push	r15
	push	rcx				# keep _Size for the odd tail
	xor	r14d, r14d			# carry low
	xor	r15d, r15d			# carry high
	mov	rbp, rsi			# info1 base
	mov	r9, rsi				# info1 running
	mov	rcx, rdi			# info0 running
	mov	r8, rdx				# info2 running
	mov	r12, [rsp]
	shr	r12, 1				# pairs = Size/2
	je	.Lcrt_tail
.Lcrt_loop:
	CRT_LIMB 0
	CRT_LIMB 8
	add	rcx, 16
	add	r8, 16
	add	r9, 16
	dec	r12
	jnz	.Lcrt_loop
.Lcrt_tail:
	pop	rax				# _Size
	test	al, 1
	jz	.Lcrt_done
	CRT_LIMB 0
.Lcrt_done:
	mov	[rbp], r14			# middle word
	mov	[rbp+8], r15			# high word
	mov	rax, r14			# return the middle word
	pop	r15
	pop	r14
	pop	r13
	pop	r12
	pop	rbp
	pop	rbx
	ret

	.section .rodata
	.align	16
.Lcrt_m0:	.quad	0x1b00000000000001
.Lcrt_m1:	.quad	0x3a00000000000001
.Lcrt_m2:	.quad	0x5700000000000001
.Lcrt_cm2:	.quad	0x599999999999999e	# Shoup multiplier of inv(m0 mod m2)
.Lcrt_inv0_2:	.quad	0x1e73333333333335	# inv(m0 mod m2)
.Lcrt_cm1:	.quad	0x772c234f72c234fa	# Shoup multiplier used for the m1 estimate
.Lcrt_cm02_1:	.quad	0x5294a5294a529495	# Shoup multiplier of inv(m0*m2 mod m1)
.Lcrt_inv02_1:	.quad	0x12b5ad6b5ad6b5aa	# inv(m0*m2 mod m1)
.Lcrt_m0m2_lo:	.quad	0x7200000000000001	# (m0*m2) mod 2^64
.Lcrt_m0m2_hi:	.quad	0x092d000000000000	# (m0*m2) >> 64

	.text
	.section .note.GNU-stack,"",@progbits
