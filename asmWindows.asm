extern memcpy

section .text

global asmAdd0
asmAdd0:
	clc
.L_asmAdd0_0:
		jrcxz .L_asmAdd0_1
		mov rax, [r8]
		adc rax, [r9]
		lea r8, [r8+8]
		lea r9, [r9+8]
		mov [rdx], rax
		lea rdx, [rdx+8]
		lea rcx, [rcx-1]
		jmp .L_asmAdd0_0
.L_asmAdd0_1:
	adc ecx, ecx
	mov eax, ecx
	ret

global asmAdd1
asmAdd1:
	add r9b, 255
.L_asmAdd1_0:
		jrcxz .L_asmAdd1_1
		mov rax, [rdx]
		adc rax, 0
		lea rdx, [rdx+8]
		mov [r8], rax
		lea r8, [r8+8]
		lea rcx, [rcx-1]
		jc .L_asmAdd1_0
	shl rcx, 3
	xchg rcx, r8
	call memcpy
	xor eax, eax
	ret
.L_asmAdd1_1:
	adc ecx, ecx
	mov eax, ecx
	ret

global asmSub0
asmSub0:
	clc
.L_asmSub0_0:
		jrcxz .L_asmSub0_1
		mov rax, [r8]
		sbb rax, [r9]
		lea r8, [r8+8]
		lea r9, [r9+8]
		mov [rdx], rax
		lea rdx, [rdx+8]
		lea rcx, [rcx-1]
		jmp .L_asmSub0_0
.L_asmSub0_1:
	adc ecx, ecx
	mov eax, ecx
	ret

global asmSub1
asmSub1:
	add r9b, 255
.L_asmSub1_0:
		jrcxz .L_asmSub1_1
		mov rax, [rdx]
		sbb rax, 0
		lea rdx, [rdx+8]
		mov [r8], rax
		lea r8, [r8+8]
		lea rcx, [rcx-1]
		jc .L_asmSub1_0
	shl rcx, 3
	xchg rcx, r8
	call memcpy
.L_asmSub1_1:
	ret

global asmShl
asmShl:
	xor r11d, r11d
	lea r8, [r8-8]
.L_asmShl:
		mov r10, [r8+rdx*8]
		shld r11, r10, cl
		mov [r9+rdx*8], r11
		mov r11, r10
		dec rdx
		jnz .L_asmShl
	shl r11, cl
	mov [r9], r11
	ret

global asmShr
asmShr:
	mov r10, [r8]
	lea r8, [r8+rdx*8]
	dec rdx
	lea r9, [r9+rdx*8]
	neg rdx
	jz .L_asmShr_1
.L_asmShr_0:
		mov r11, [r8+rdx*8]
		shrd r10, r11, cl
		mov [r9+rdx*8], r10
		mov r10, r11
		inc rdx
		jnz .L_asmShr_0
.L_asmShr_1:
	shr r10, cl
	mov [r9], r10
	ret

global asmMul0
asmMul0:
	mov r10, rdx
	xor r11d, r11d
.L_asmMul0_0:
		mov rax, [r8]
		mul r10
		add rax, r11
		adc rdx, 0
		mov [r9], rax
		mov r11, rdx
		lea r8, [r8+8]
		lea r9, [r9+8]
		dec rcx
		jnz .L_asmMul0_0
	mov [r9], r11
	ret

global asmMul1
asmMul1:
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
.L_asmMul1_0:
		mov rbx, r8				; rbx = _Src1
		mov rdi, rsi			; for (rdi = rsi; rdi != 0; rdi -= 1)
.L_asmMul1_1:
			mov rax, [rbx]
			mul qword [r9]	
			add r11, rax
			adc r12, rdx
			adc r13, 0
			lea r9, [r9-8]
			lea rbx, [rbx+8]
			dec rdi
			jnz .L_asmMul1_1
		mov [r10], r11
		mov r11, r12
		mov r12, r13
		xor r13d, r13d
		inc rsi
		lea r9, [r9+rsi*8]		; _Src2 += 1
		lea r10, [r10+8]
		cmp rsi, rcx
		jb .L_asmMul1_0
	test r14, r14
	jz .L_asmMul1_4			; for (r14 = _Size2 - _Size1; r14 != 0; r14 -= 1)
.L_asmMul1_2:
		mov rbx, r8				; rbx = _Src1
		mov rdi, rcx			; for (rdi = _Size1; rdi != 0; rdi -= 1)
.L_asmMul1_3:
			mov rax, [rbx]
			mul qword [r9]
			add r11, rax
			adc r12, rdx
			adc r13, 0
			lea r9, [r9-8]
			lea rbx, [rbx+8]
			dec rdi
			jnz .L_asmMul1_3
		mov [r10], r11
		mov r11, r12
		mov r12, r13
		xor r13d, r13d
		lea r9, [r9+rcx*8+8]	; _Src2 += 1
		lea r10, [r10+8]
		dec r14
		jnz .L_asmMul1_2
.L_asmMul1_4:				; for (rcx = _Size1, r8 = _Src1; rcx != 0; rcx -= 1, r8 += 1)
		mov rbx, r8
		mov rdi, rcx			; for (rdi = rcx; rdi != 0; rdi -= 1)
.L_asmMul1_5:
			mov rax, [rbx]
			mul qword [r9]
			add r11, rax
			adc r12, rdx
			adc r13, 0
			lea r9, [r9-8]
			lea rbx, [rbx+8]
			dec rdi
			jnz .L_asmMul1_5
		mov [r10], r11
		mov r11, r12
		mov r12, r13
		xor r13d, r13d
		lea r8, [r8+8]
		lea r9, [r9+rcx*8]
		lea r10, [r10+8]
		dec rcx
		jnz .L_asmMul1_4
	mov [r10], r11
	pop rbx
	pop rsi
	pop rdi
	pop r12
	pop r13
	pop r14
	ret

global asmSqr
asmSqr:
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
.L_asmSqr_0:				; do while (r8 <= r9)
		mov rsi, r8
		mov rdi, r9				; for (rsi = r8, rdi = r9; rsi < rdi; rsi += 1, rdi -= 1)
		xor r11d, r11d
		xor r12d, r12d
		xor r13d, r13d
.L_asmSqr_1:
			cmp rsi, rdi
			jae .L_asmSqr_2
			mov rax, [rsi]
			mul qword [rdi]
			add r11, rax
			adc r12, rdx
			adc r13, 0
			lea rsi, [rsi+8]
			lea rdi, [rdi-8]
			jmp .L_asmSqr_1
.L_asmSqr_2:
		add r11, r11
		adc r12, r12
		adc r13, r13
		cmp rsi, rdi
		jne .L_asmSqr_3
		mov rax, [rsi]
		mul rax
		add r11, rax
		adc r12, rdx
		adc r13, 0
.L_asmSqr_3:
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
		ja .L_asmSqr_4
		jmp .L_asmSqr_0
.L_asmSqr_4:
	mov [r10], r14
	pop rsi
	pop rdi
	pop r12
	pop r13
	pop r14
	pop r15
	ret

global asmNtt
asmNtt:
	push r15
	push r14
	push r13
	push r12
	push rdi
	push rsi
	push rbp
	push rbx
	mov rsi, rcx			; rsi = N
	mov rdi, rdx			; rdi = mod
	mov rbp, [rsp+68h]		; rbp = end
	shr rcx, 1
.L_asmNtt_0:				; <optimization for root[0] == 1>
		mov r12, [r8]
		mov r13, [r8+8]
		mov r14, [r8+8*rsi]
		mov r15, [r8+8*rsi+8]
		
		mov r10, r12
		sub r12, rdi
		cmovs r12, r10
		mov r11, r13
		sub r13, rdi
		cmovs r13, r11
		
		mov r10, r14
		sub r14, rdi
		cmovs r14, r10
		mov r11, r15
		sub r15, rdi
		cmovs r15, r11

		lea r10, [r12+rdi]
		lea r11, [r13+rdi]
		add r12, r14
		add r13, r15
		sub r10, r14
		sub r11, r15

		mov [r8], r12
		mov [r8+8], r13
		mov [r8+8*rsi], r10
		mov [r8+8*rsi+8], r11
		
		lea r8, [r8+16]
		dec rcx
		jnz .L_asmNtt_0
.L_asmNtt_1:
		lea r8, [r8+8*rsi]
		lea r9, [r9+16]
		cmp r8, rbp
		jae .L_asmNtt_3
		mov rcx, rsi
		mov rbx, [r9]			; rbx = root[]
		mov rdx, [r9+8]			; rdx = ceil(2^64 * root[] / mod)
		shr rcx, 1
.L_asmNtt_2:
			mov r12, [r8]
			mov r13, [r8+8]
			mov r14, [r8+8*rsi]
			mov r15, [r8+8*rsi+8]

			mulx r10, rax, r14
			mulx r11, rax, r15
			imul r14, rbx
			imul r15, rbx
			imul r10, rdi
			imul r11, rdi

			sub r14, r10
			lea r10, [r14+rdi]
			cmovns r10, r14
			sub r15, r11
			lea r11, [r15+rdi]
			cmovns r11, r15
			
			mov r14, r12
			sub r12, rdi
			cmovs r12, r14
			mov r15, r13
			sub r13, rdi
			cmovs r13, r15

			lea r14, [r12+rdi]
			lea r15, [r13+rdi]
			add r12, r10
			add r13, r11
			sub r14, r10
			sub r15, r11

			mov [r8], r12
			mov [r8+8], r13
			mov [r8+8*rsi], r14
			mov [r8+8*rsi+8], r15
			
			lea r8, [r8+16]
			dec rcx
			jnz .L_asmNtt_2
		jmp .L_asmNtt_1
.L_asmNtt_3:
	pop rbx
	pop rbp
	pop rsi
	pop rdi
	pop r12
	pop r13
	pop r14
	pop r15
	ret

global asmNtt2
asmNtt2:
	push r15
	push r14
	push r13
	push r12
	push rdi
	push rsi
	push rbp
	push rbx
	mov rdi, rdx
	mov rbp, [rsp+68h]
.L_asmNtt2:
		mov rbx, [r9]
		mov rdx, [r9+8]
		mov rsi, [r9+16]
		mov rcx, [r9+24]

		mov r12, [r8]
		mov r14, [r8+8]
		mov r13, [r8+16]
		mov r15, [r8+24]

		mulx r10, rax, r14
		mov rdx, rcx
		mulx r11, rax, r15
		imul r14, rbx
		imul r15, rsi
		imul r10, rdi
		imul r11, rdi

		sub r14, r10
		lea r10, [r14+rdi]
		cmovns r10, r14
		sub r15, r11
		lea r11, [r15+rdi]
		cmovns r11, r15
			
		mov r14, r12
		sub r12, rdi
		cmovs r12, r14
		mov r15, r13
		sub r13, rdi
		cmovs r13, r15

		lea r14, [r12+r10]
		lea r15, [r13+r11]
		sub r12, r10
		lea r10, [r12+rdi]
		cmovns r10, r12
		sub r13, r11
		lea r11, [r13+rdi]
		cmovns r11, r13
		mov r12, r14
		sub r14, rdi
		cmovs r14, r12
		mov r13, r15
		sub r15, rdi
		cmovs r15, r13

		mov [r8], r14
		mov [r8+8], r10
		mov [r8+16], r15
		mov [r8+24], r11

		lea r8, [r8+32]
		lea r9, [r9+32]
		cmp r8, rbp
		jb .L_asmNtt2
	pop rbx
	pop rbp
	pop rsi
	pop rdi
	pop r12
	pop r13
	pop r14
	pop r15
	ret

global asmNtt3
asmNtt3:
	push r15
	push r14
	push r13
	push r12
	push rdi
	push rsi
	push rbp
	push rbx
	mov rsi, rcx
	mov rdi, rdx
	mov rbp, [rsp+68h]
.L_asmNtt3_0:
		mov rcx, rsi
		mov rbx, [r9]
		mov rdx, [r9+8]
		shr rcx, 1
.L_asmNtt3_1:
			mov r12, [r8]
			mov r13, [r8+8]
			mov r14, [r8+8*rsi]
			mov r15, [r8+8*rsi+8]

			mulx r10, rax, r14
			mulx r11, rax, r15
			imul r14, rbx
			imul r15, rbx
			imul r10, rdi
			imul r11, rdi

			sub r14, r10
			lea r10, [r14+rdi]
			cmovns r10, r14
			sub r15, r11
			lea r11, [r15+rdi]
			cmovns r11, r15
			
			mov r14, r12
			sub r12, rdi
			cmovs r12, r14
			mov r15, r13
			sub r13, rdi
			cmovs r13, r15

			lea r14, [r12+rdi]
			lea r15, [r13+rdi]
			add r12, r10
			add r13, r11
			sub r14, r10
			sub r15, r11

			mov [r8], r12
			mov [r8+8], r13
			mov [r8+8*rsi], r14
			mov [r8+8*rsi+8], r15
			
			lea r8, [r8+16]
			dec rcx
			jnz .L_asmNtt3_1
		lea r8, [r8+8*rsi]
		lea r9, [r9+16]
		cmp r8, rbp
		jb .L_asmNtt3_0
	pop rbx
	pop rbp
	pop rsi
	pop rdi
	pop r12
	pop r13
	pop r14
	pop r15
	ret

global asmINtt
asmINtt:
	push r15
	push r14
	push r13
	push r12
	push rdi
	push rsi
	push rbp
	push rbx
	mov rsi, rcx			; rsi = N
	mov rdi, rdx			; rdi = mod
	mov rbp, [rsp+68h]		; rbp = end
	shr rcx, 1
.L_asmINtt_0:			; <optimization for root[0] == 1>
		mov r12, [r8]
		mov r13, [r8+8]
		mov r14, [r8+8*rsi]
		mov r15, [r8+8*rsi+8]
			
		lea r10, [r12+r14]
		lea r11, [r13+r15]

		sub r12, r14
		lea r14, [r12+rdi]
		cmovns r14, r12
		sub r13, r15
		lea r15, [r13+rdi]
		cmovns r15, r13
		
		mov r12, r10
		sub r10, rdi
		cmovs r10, r12
		mov r13, r11
		sub r11, rdi
		cmovs r11, r13

		mov [r8], r10
		mov [r8+8], r11
		mov [r8+8*rsi], r14
		mov [r8+8*rsi+8], r15
		
		lea r8, [r8+16]
		dec rcx
		jnz .L_asmINtt_0
.L_asmINtt_1:			; for (r8 = begin + 2 * N, r9 = iroot + 2; r8 < end; r8 += N, r9 += 2)
		lea r8, [r8+8*rsi]
		lea r9, [r9+16]
		cmp r8, rbp
		jae .L_asmINtt_3
		mov rcx, rsi
		mov rbx, [r9]			; r11 = iroot[]
		mov rdx, [r9+8]			; r12 = ceil(2^64 * iroot[] / mod)
		shr rcx, 1
.L_asmINtt_2:
			mov r12, [r8]
			mov r13, [r8+8]
			mov r14, [r8+8*rsi]
			mov r15, [r8+8*rsi+8]
			
			lea r10, [r12+r14]
			lea r11, [r13+r15]
			sub r12, r14
			sub r13, r15
			
			mov r14, r10
			sub r10, rdi
			cmovs r10, r14
			mov r15, r11
			sub r11, rdi
			cmovs r11, r15
			add r12, rdi
			add r13, rdi
			mov [r8], r10
			mov [r8+8], r11
			
			mulx r10, rax, r12
			mulx r11, rax, r13
			imul r12, rbx
			imul r13, rbx
			imul r10, rdi
			imul r11, rdi

			sub r12, r10
			lea r10, [r12+rdi]
			cmovns r10, r12
			sub r13, r11
			lea r11, [r13+rdi]
			cmovns r11, r13
			mov [r8+8*rsi], r10
			mov [r8+8*rsi+8], r11

			lea r8, [r8+16]
			dec rcx
			jnz .L_asmINtt_2
		jmp .L_asmINtt_1
.L_asmINtt_3:
	pop rbx
	pop rbp
	pop rsi
	pop rdi
	pop r12
	pop r13
	pop r14
	pop r15
	ret

global asmINtt2
asmINtt2:
	push r15
	push r14
	push r13
	push r12
	push rdi
	push rsi
	push rbp
	push rbx
	mov rdi, rdx
	mov rbp, [rsp+68h]
.L_asmINtt2:
		mov rbx, [r9]
		mov rdx, [r9+8]
		mov rsi, [r9+16]
		mov rcx, [r9+24]

		mov r12, [r8]
		mov r14, [r8+8]
		mov r13, [r8+16]
		mov r15, [r8+24]
			
		lea r10, [r12+r14]
		lea r11, [r13+r15]
		sub r12, r14
		sub r13, r15
			
		mov r14, r10
		sub r10, rdi
		cmovs r10, r14
		mov r15, r11
		sub r11, rdi
		cmovs r11, r15
		add r12, rdi
		add r13, rdi
		mov [r8], r10
		mov [r8+16], r11
			
		mulx r10, rax, r12
		mov rdx, rcx
		mulx r11, rax, r13
		imul r12, rbx
		imul r13, rsi
		imul r10, rdi
		imul r11, rdi

		sub r12, r10
		lea r10, [r12+rdi]
		cmovns r10, r12
		sub r13, r11
		lea r11, [r13+rdi]
		cmovns r11, r13
		mov [r8+8], r10
		mov [r8+24], r11

		lea r8, [r8+32]
		lea r9, [r9+32]
		cmp r8, rbp
		jb .L_asmINtt2
	pop rbx
	pop rbp
	pop rsi
	pop rdi
	pop r12
	pop r13
	pop r14
	pop r15
	ret

global asmINtt3
asmINtt3:
	push r15
	push r14
	push r13
	push r12
	push rdi
	push rsi
	push rbp
	push rbx
	mov rsi, rcx
	mov rdi, rdx
	mov rbp, [rsp+68h]
.L_asmINtt3_0:
		mov rcx, rsi
		mov rbx, [r9]
		mov rdx, [r9+8]
		shr rcx, 1
.L_asmINtt3_1:
			mov r12, [r8]
			mov r13, [r8+8]
			mov r14, [r8+8*rsi]
			mov r15, [r8+8*rsi+8]
			
			lea r10, [r12+r14]
			lea r11, [r13+r15]
			sub r12, r14
			sub r13, r15
			
			mov r14, r10
			sub r10, rdi
			cmovs r10, r14
			mov r15, r11
			sub r11, rdi
			cmovs r11, r15
			add r12, rdi
			add r13, rdi
			mov [r8], r10
			mov [r8+8], r11
			
			mulx r10, rax, r12
			mulx r11, rax, r13
			imul r12, rbx
			imul r13, rbx
			imul r10, rdi
			imul r11, rdi

			sub r12, r10
			lea r10, [r12+rdi]
			cmovns r10, r12
			sub r13, r11
			lea r11, [r13+rdi]
			cmovns r11, r13
			mov [r8+8*rsi], r10
			mov [r8+8*rsi+8], r11

			lea r8, [r8+16]
			dec rcx
			jnz .L_asmINtt3_1
		lea r8, [r8+8*rsi]
		lea r9, [r9+16]
		cmp r8, rbp
		jb .L_asmINtt3_0
	pop rbx
	pop rbp
	pop rsi
	pop rdi
	pop r12
	pop r13
	pop r14
	pop r15
	ret

global asmDiv
asmDiv:
	mov rax, rcx
	div r8
	ret

global asmDivMod
asmDivMod:
	mov rax, rcx
	div r8
	mov [r9], rdx
	ret

global asmMod
asmMod:
	mov rax, rcx
	mul rdx
	imul rdx, r8
	sub rcx, rdx
	lea rax, [rcx+r8]
	cmovns rax, rcx
	ret

global asmMulMod
asmMulMod:
	mov rax, rcx
	mul rdx
	mov rcx, rax
	mov r10, rdx
	mul r9
	mov rax, r10
	mov r10, rdx
	mul r9
	add rax, r10
	adc rdx, 0
	shld rdx, rax, 4
	imul rdx, r8
	sub rcx, rdx
	lea rax, [rcx+r8]
	cmovns rax, rcx
	ret

global asmINttShr
asmINttShr:
	push r15
	push r14
	push r13
	push r12
	push rdi
	push rsi
	push rbp
	push rbx
	mov esi, 1
	shl rsi, cl				; rsi = NTT_Size
	lea rbx, [rsi-1]		; rbx = mask
	shr rsi, 1				; rsi = N
	mov rdi, rdx			; rdi = mod
	mov rbp, rsi
	mov r9, rdx
	shr rbp, 1
	shr r9, cl
.L_asmINttShr:
		mov r12, [r8]
		mov r13, [r8+8]
		mov r14, [r8+8*rsi]
		mov r15, [r8+8*rsi+8]
			
		lea r10, [r12+r14]
		lea r11, [r13+r15]

		sub r12, r14
		lea r14, [r12+rdi]
		cmovns r14, r12
		sub r13, r15
		lea r15, [r13+rdi]
		cmovns r15, r13
		
		mov r12, r10
		sub r10, rdi
		cmovs r10, r12
		mov r13, r11
		sub r11, rdi
		cmovs r11, r13

		mov rax, rbx
		mov rdx, rbx
		mov r12, rbx
		mov r13, rbx

		dec r10
		dec r11
		dec r14
		dec r15

		or rax, r10
		or rdx, r11
		or r12, r14
		or r13, r15

		xor rax, r10
		xor rdx, r11
		xor r12, r14
		xor r13, r15
		
		imul rax, r9
		imul rdx, r9
		imul r12, r9
		imul r13, r9
		
		sar r10, cl
		sar r11, cl
		sar r14, cl
		sar r15, cl

		inc r10
		inc r11
		inc r14
		inc r15
		
		add r10, rax
		add r11, rdx
		add r14, r12
		add r15, r13

		mov [r8], r10
		mov [r8+8], r11
		mov [r8+8*rsi], r14
		mov [r8+8*rsi+8], r15

		lea r8, [r8+16]
		dec rbp
		jnz .L_asmINttShr
	pop rbx
	pop rbp
	pop rsi
	pop rdi
	pop r12
	pop r13
	pop r14
	pop r15
	ret

global asmNttMul
asmNttMul:
	push r13
	push r12
	push rdi
	push rsi
	push rbx
	mov rbx, rdx
	mov rsi, [rsp+58h]		; rsi = mod_124
	mov rdi, [rsp+50h]		; rdi = mod
.L_asmNttMul:
		mov rdx, [r8]
		mov rax, [r9]
		mulx r11, r10, rax
		mov rdx, rsi
		mulx r12, rax, r10
		mulx r13, rax, r11
		add r12, rax
		adc r13, 0
		shld r13, r12, 4
		imul r13, rdi
		sub r10, r13
		lea r11, [r10+rdi]
		cmovns r11, r10
		mov [rbx], r11
		lea rbx, [rbx+8]
		lea r8, [r8+8]
		lea r9, [r9+8]
		dec rcx
		jnz .L_asmNttMul
	pop rbx
	pop rsi
	pop rdi
	pop r12
	pop r13
	ret

global asmLoad
asmLoad:
	push r13
	push r12
	push rdi
	push rsi
	push rbx
	mov rdi, [rsp+58h]		; rdi = mod
	mov r10, [rsp+50h]		; r10 = _Src
.L_asmLoad:
		mov r11, [r10]
		mulx rbx, rax, r11
		imul rbx, rdi
		sub r11, rbx
		lea rax, [r11+rdi]
		cmovns rax, r11
		mov [r8], rax
		mov [r9], rax
		lea r8, [r8+8]
		lea r9, [r9+8]
		lea r10, [r10+8]
		dec rcx
		jnz .L_asmLoad
	pop rbx
	pop rsi
	pop rdi
	pop r12
	pop r13
	ret

global asmSave
asmSave:			; what the F is this
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
.L_asmSave_0:
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
		jnz .L_asmSave_0
	pop rdi
	pop rsi
	pop rbx
	pop r12
	pop r13
	pop r14
	pop r15
	ret