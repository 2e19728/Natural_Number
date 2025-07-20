extern memcpy:proc

.code

asmAdd0 proc
	clc
	L_asmAdd0_0:
		jrcxz L_asmAdd0_1
		mov rax, [r8]
		adc rax, [r9]
		lea r8, [r8+8]
		lea r9, [r9+8]
		mov [rdx], rax
		lea rdx, [rdx+8]
		lea rcx, [rcx-1]
		jmp L_asmAdd0_0
L_asmAdd0_1:
	adc ecx, ecx
	mov eax, ecx
	ret
asmAdd0 endp

asmAdd1 proc
	add r9b, 255
	L_asmAdd1_0:
		jrcxz L_asmAdd1_1
		mov rax, [rdx]
		adc rax, 0
		lea rdx, [rdx+8]
		mov [r8], rax
		lea r8, [r8+8]
		lea rcx, [rcx-1]
		jc L_asmAdd1_0
	shl rcx, 3
	xchg rcx, r8
	call memcpy
	xor eax, eax
	ret
L_asmAdd1_1:
	adc ecx, ecx
	mov eax, ecx
	ret
asmAdd1 endp

asmSub0 proc
	clc
	L_asmSub0_0:
		jrcxz L_asmSub0_1
		mov rax, [r8]
		sbb rax, [r9]
		lea r8, [r8+8]
		lea r9, [r9+8]
		mov [rdx], rax
		lea rdx, [rdx+8]
		lea rcx, [rcx-1]
		jmp L_asmSub0_0
L_asmSub0_1:
	adc ecx, ecx
	mov eax, ecx
	ret
asmSub0 endp

asmSub1 proc
	add r9b, 255
	L_asmSub1_0:
		jrcxz L_asmSub1_1
		mov rax, [rdx]
		sbb rax, 0
		lea rdx, [rdx+8]
		mov [r8], rax
		lea r8, [r8+8]
		lea rcx, [rcx-1]
		jc L_asmSub1_0
	shl rcx, 3
	xchg rcx, r8
	call memcpy
L_asmSub1_1:
	ret
asmSub1 endp

asmShl proc
	xor r11d, r11d
	lea r8, [r8-8]
	L_asmShl:
		mov r10, [r8+rdx*8]
		shld r11, r10, cl
		mov [r9+rdx*8], r11
		mov r11, r10
		dec rdx
		jnz L_asmShl
	shl r11, cl
	mov [r9], r11
	ret
asmShl endp

asmShr proc
	mov r10, [r8]
	lea r8, [r8+rdx*8]
	dec rdx
	lea r9, [r9+rdx*8]
	neg rdx
	jz L_asmShr_1
	L_asmShr_0:
		mov r11, [r8+rdx*8]
		shrd r10, r11, cl
		mov [r9+rdx*8], r10
		mov r10, r11
		inc rdx
		jnz L_asmShr_0
L_asmShr_1:
	shr r10, cl
	mov [r9], r10
	ret
asmShr endp

asmMul0 proc
	mov r10, rdx
	xor r11d, r11d
	L_asmMul0_0:
		mov rax, [r8]
		mul r10
		add rax, r11
		adc rdx, 0
		mov [r9], rax
		mov r11, rdx
		lea r8, [r8+8]
		lea r9, [r9+8]
		dec rcx
		jnz L_asmMul0_0
	mov [r9], r11
	ret
asmMul0 endp

asmMul1 proc
	push r14
	push r13
	push r12
	push rdi
	push rsi
	push rbx
	mov r10, [rsp+58h]		; r10 = _Dst
	xor r11d, r11d
	xor r12d, r12d
	xor r13d, r13d
	mov r14, rdx
	sub r14, rcx			; r14 = _Size2 - _Size1
	mov esi, 1				; for (rsi = 1; rsi < _Size1; rsi += 1)
	L_asmMul1_0:
		mov rbx, r8				; rbx = _Src1
		mov rdi, rsi			; for (rdi = rsi; rdi != 0; rdi -= 1)
		L_asmMul1_1:
			mov rax, [rbx]
			mul qword ptr [r9]	
			add r11, rax
			adc r12, rdx
			adc r13, 0
			lea r9, [r9-8]
			lea rbx, [rbx+8]
			dec rdi
			jnz L_asmMul1_1
		mov [r10], r11
		mov r11, r12
		mov r12, r13
		xor r13d, r13d
		inc rsi
		lea r9, [r9+rsi*8]		; _Src2 += 1
		lea r10, [r10+8]
		cmp rsi, rcx
		jb L_asmMul1_0
	test r14, r14
	jz L_asmMul1_4			; for (r14 = _Size2 - _Size1; r14 != 0; r14 -= 1)
	L_asmMul1_2:
		mov rbx, r8				; rbx = _Src1
		mov rdi, rcx			; for (rdi = _Size1; rdi != 0; rdi -= 1)
		L_asmMul1_3:
			mov rax, [rbx]
			mul qword ptr [r9]
			add r11, rax
			adc r12, rdx
			adc r13, 0
			lea r9, [r9-8]
			lea rbx, [rbx+8]
			dec rdi
			jnz L_asmMul1_3
		mov [r10], r11
		mov r11, r12
		mov r12, r13
		xor r13d, r13d
		lea r9, [r9+rcx*8+8]	; _Src2 += 1
		lea r10, [r10+8]
		dec r14
		jnz L_asmMul1_2
	L_asmMul1_4:				; for (rcx = _Size1, r8 = _Src1; rcx != 0; rcx -= 1, r8 += 1)
		mov rbx, r8
		mov rdi, rcx			; for (rdi = rcx; rdi != 0; rdi -= 1)
		L_asmMul1_5:
			mov rax, [rbx]
			mul qword ptr [r9]
			add r11, rax
			adc r12, rdx
			adc r13, 0
			lea r9, [r9-8]
			lea rbx, [rbx+8]
			dec rdi
			jnz L_asmMul1_5
		mov [r10], r11
		mov r11, r12
		mov r12, r13
		xor r13d, r13d
		lea r8, [r8+8]
		lea r9, [r9+rcx*8]
		lea r10, [r10+8]
		dec rcx
		jnz L_asmMul1_4
	mov [r10], r11
	pop rbx
	pop rsi
	pop rdi
	pop r12
	pop r13
	pop r14
	ret
asmMul1 endp

asmSqr proc
	push r15
	push r14
	push r13
	push r12
	push rdi
	push rsi
	mov r9, r8				; r8 = r9 = _Src
	mov r10, rdx			; r10 = _Dst
	xor r14d, r14d
	xor r15d, r15d
	L_asmSqr_0:				; do while (r8 <= r9)
		mov rsi, r8
		mov rdi, r9				; for (rsi = r8, rdi = r9; rsi < rdi; rsi += 1, rdi -= 1)
		xor r11d, r11d
		xor r12d, r12d
		xor r13d, r13d
		L_asmSqr_1:
			cmp rsi, rdi
			jae L_asmSqr_2
			mov rax, [rsi]
			mul qword ptr [rdi]
			add r11, rax
			adc r12, rdx
			adc r13, 0
			lea rsi, [rsi+8]
			lea rdi, [rdi-8]
			jmp L_asmSqr_1
	L_asmSqr_2:
		add r11, r11
		adc r12, r12
		adc r13, r13
		cmp rsi, rdi
		jne L_asmSqr_3
		mov rax, [rsi]
		mul rax
		add r11, rax
		adc r12, rdx
		adc r13, 0
	L_asmSqr_3:
		add r11, r14
		adc r12, r15
		adc r13, 0
		mov [r10], r11
		mov r14, r12
		mov r15, r13
		lea r10, [r10+8]
		dec rcx
		lea rax, [r9+8]
		cmovg r9, rax			; if (rcx > 0) r9 += 1
		lea rdx, [r8+8]
		cmovle r8, rdx			; else r8 += 1
		cmp r8, r9
		ja L_asmSqr_4
		jmp L_asmSqr_0
L_asmSqr_4:
	mov [r10], r14
	pop rsi
	pop rdi
	pop r12
	pop r13
	pop r14
	pop r15
	ret
asmSqr endp

asmNtt proc
	push r12
	push rdi
	push rsi
	push rbx
	mov rsi, rcx			; rsi = N
	mov rdi, rdx			; rdi = mod
	mov r10, [rsp+48h]		; r10 = end
	L_asmNtt_0:				; <optimization for root[0] == 1>
		mov rbx, [r8+8*rsi]
		mov rax, [r8]
		lea rdx, [rbx+rax]
		sub rax, rbx
		lea rbx, [rax+rdi]
		cmovs rax, rbx			; rax = ([r8] - [r8 + N]) % mod
		mov rbx, rdx
		sub rdx, rdi
		cmovs rdx, rbx			; rdx = ([r8] + [r8 + N]) % mod
		mov [r8], rdx
		mov [r8+8*rsi], rax
		lea r8, [r8+8]
		dec rcx
		jnz L_asmNtt_0
	L_asmNtt_1:				; for (r8 = begin + 2 * N, r9 = root + 1; r8 < end; r8 += N, r9 += 2)
		lea r8, [r8+8*rsi]
		lea r9, [r9+16]
		cmp r8, r10
		jae L_asmNtt_3
		mov rcx, rsi
		mov r11, [r9]			; r11 = root[]
		mov r12, [r9+8]			; r12 = ceil(2^64 * root[] / mod)
	L_asmNtt_2:
		mov rax, [r8+8*rsi]
		mov rbx, rax
		mul r12
		imul rbx, r11
		imul rdx, rdi
		sub rbx, rdx
		lea rax, [rbx+rdi]
		cmovs rbx, rax			; rbx = [r8 + N] * root[] % mod
		mov rax, [r8]
		lea rdx, [rbx+rax]
		sub rax, rbx
		lea rbx, [rax+rdi]
		cmovs rax, rbx			; rax = ([r8] - [r8 + N] * root[]) % mod
		mov rbx, rdx
		sub rdx, rdi
		cmovs rdx, rbx			; rdx = ([r8] + [r8 + N] * root[]) % mod
		mov [r8], rdx
		mov [r8+8*rsi], rax
		lea r8, [r8+8]
		dec rcx
		jnz L_asmNtt_2
		jmp L_asmNtt_1
L_asmNtt_3:
	pop rbx
	pop rsi
	pop rdi
	pop r12
	ret
asmNtt endp

asmNtt2 proc
	push r12
	push rdi
	push rsi
	push rbx
	mov rsi, rcx
	mov rdi, rdx
	mov r10, [rsp+48h]
L_asmNtt2_0:
	mov rcx, rsi
	mov r11, [r9]
	mov r12, [r9+8]
L_asmNtt2_1:
	mov rax, [r8+8*rsi]
	mov rbx, rax
	mul r12
	imul rbx, r11
	imul rdx, rdi
	sub rbx, rdx
	lea rax, [rbx+rdi]
	cmovs rbx, rax
	mov rax, [r8]
	lea rdx, [rbx+rax]
	sub rax, rbx
	lea rbx, [rax+rdi]
	cmovs rax, rbx
	mov rbx, rdx
	sub rdx, rdi
	cmovs rdx, rbx
	mov [r8], rdx
	mov [r8+8*rsi], rax
	lea r8, [r8+8]
	dec rcx
	jnz L_asmNtt2_1
	lea r8, [r8+8*rsi]
	lea r9, [r9+16]
	cmp r8, r10
	jb L_asmNtt2_0
	pop rbx
	pop rsi
	pop rdi
	pop r12
	ret
asmNtt2 endp

asmINtt proc
	push r12
	push rdi
	push rsi
	push rbx
	mov rsi, rcx
	mov rdi, rdx
	mov r10, [rsp+48h]
L_asmINtt_0:
	mov rbx, [r8+8*rsi]
	mov rax, [r8]
	lea rdx, [rbx+rax]
	sub rax, rbx
	lea rbx, [rax+rdi]
	cmovs rax, rbx
	mov rbx, rdx
	sub rdx, rdi
	cmovs rdx, rbx
	mov [r8], rdx
	mov [r8+8*rsi], rax
	lea r8, [r8+8]
	dec rcx
	jnz L_asmINtt_0
L_asmINtt_1:
	lea r8, [r8+8*rsi]
	lea r9, [r9+16]
	cmp r8, r10
	jae L_asmINtt_3
	mov rcx, rsi
	mov r11, [r9]
	mov r12, [r9+8]
L_asmINtt_2:
	mov rbx, [r8+8*rsi]
	mov rax, [r8]
	lea rdx, [rbx+rax]
	sub rax, rbx
	lea rbx, [rax+rdi]
	cmovs rax, rbx
	mov rbx, rdx
	sub rdx, rdi
	cmovs rdx, rbx
	mov [r8], rdx
	mov rbx, rax
	mul r12
	imul rbx, r11
	imul rdx, rdi
	sub rbx, rdx
	lea rax, [rbx+rdi]
	cmovs rbx, rax
	mov [r8+8*rsi], rbx
	lea r8, [r8+8]
	dec rcx
	jnz L_asmINtt_2
	jmp L_asmINtt_1
L_asmINtt_3:
	pop rbx
	pop rsi
	pop rdi
	pop r12
	ret
asmINtt endp

asmINtt2 proc
	push r12
	push rdi
	push rsi
	push rbx
	mov rsi, rcx
	mov rdi, rdx
	mov r10, [rsp+48h]
L_asmINtt2_0:
	mov rcx, rsi
	mov r11, [r9]
	mov r12, [r9+8]
	L_asmINtt2_1:
	mov rbx, [r8+8*rsi]
	mov rax, [r8]
	lea rdx, [rbx+rax]
	sub rax, rbx
	lea rbx, [rax+rdi]
	cmovs rax, rbx
	mov rbx, rdx
	sub rdx, rdi
	cmovs rdx, rbx
	mov [r8], rdx
	mov rbx, rax
	mul r12
	imul rbx, r11
	imul rdx, rdi
	sub rbx, rdx
	lea rax, [rbx+rdi]
	cmovs rbx, rax
	mov [r8+8*rsi], rbx
	lea r8, [r8+8]
	dec rcx
	jnz L_asmINtt2_1
	lea r8, [r8+8*rsi]
	lea r9, [r9+16]
	cmp r8, r10
	jb L_asmINtt2_0
	pop rbx
	pop rsi
	pop rdi
	pop r12
	ret
asmINtt2 endp

asmMod proc
	mov rax, rcx
	mul rdx
	div r8
	mov rax, rdx
	ret
asmMod endp

asmINttShr proc
	push rdi
	push rsi
	push rbx
	mov r9d, 1
	shl r9, cl
	lea r10, [r9-1]
	shr r9, 1
	mov r11, rdx
	mov rdi, r9
L_asmINttShr_0:
	mov rsi, [r8]
	mov rax, [r8+r9*8]
	lea rbx, [rax+rsi]
	sub rsi, rax
	lea rdx, [rsi+r11]
	cmovs rsi, rdx
	mov rdx, rbx
	sub rbx, r11
	cmovs rbx, rdx
	mov rax, rbx
	neg rax
	and rax, r10
	mul r11
	add rax, rbx
	adc rdx, 0
	shrd rax, rdx, cl
	mov [r8], rax
	mov rax, rsi
	neg rax
	and rax, r10
	mul r11
	add rax, rsi
	adc rdx, 0
	shrd rax, rdx, cl
	mov [r8+r9*8], rax
	lea r8, [r8+8]
	dec rdi
	jnz L_asmINttShr_0
	pop rbx
	pop rsi
	pop rdi
	ret
asmINttShr endp

asmSave proc
	push r15
	push r14
	push r13
	push r12
	push rbx
	push rsi
	push rdi
	mov r12, r9
	mov r13, [rsp+60h]
	xor r14d, r14d
	xor r15d, r15d
	mov r9, rdx
L_asmSave_0:
	mov r10, [rcx]
	mov r11, 5700000000000001h	; m2
	mov rbx, [r8]
	sub rbx, r10
	add rbx, r11
	mov rax, 599999999999999eh	; m0^-1/m2
	mul rbx
	mov rax, 1e73333333333335h	; m0^-1
	imul rbx, rax
	imul rdx, r11
	sub rbx, rdx
	lea rax, [rbx+r11]
	cmovs rbx, rax
	mov r11, 3a00000000000001h	; m1
	mov rax, 1b00000000000001h	; m0
	mul rbx
	mov rsi, rax
	mov rdi, rdx
	mov rax, 772c234f72c234fah	; m0/m1
	mul rbx
	lea rbx, [rdx+2]
	imul rbx, r11
	sub rbx, rsi
	add rbx, [r9]
	sub rbx, r10
	mov rax, 5294a5294a529495h	; m0m2^-1/m1
	mul rbx
	mov rax, 12b5ad6b5ad6b5aah	; m0m2^-1
	imul rbx, rax
	imul rdx, r11
	sub rbx, rdx
	lea rax, [rbx+r11]
	cmovs rbx, rax
	add rsi, r10
	adc rdi, 0
	mov rax, 7200000000000001h	; m0m2_low
	mul rbx
	add rsi, rax
	adc rdi, rdx
	mov rax, 92d000000000000h	; m0m2_high
	mul rbx
	add rax, rdi
	adc rdx, 0
	add rsi, r14
	adc rax, r15
	adc rdx, 0
	mov [r13], rsi
	mov r14, rax
	mov r15, rdx
	lea rcx, [rcx+8]
	lea r8, [r8+8]
	lea r9, [r9+8]
	lea r13, [r13+8]
	dec r12
	jnz L_asmSave_0
	pop rdi
	pop rsi
	pop rbx
	pop r12
	pop r13
	pop r14
	pop r15
	ret
asmSave endp

end
