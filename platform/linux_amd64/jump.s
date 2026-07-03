.global save, jump

.text
save:
	mov %rbx,   (%rdi)
	pop %rbx /* pop the return address */
	mov %rsp,  8(%rdi)
	mov %rbp, 16(%rdi)
	mov %r12, 24(%rdi)
	mov %r13, 32(%rdi)
	mov %r14, 40(%rdi)
	mov %r15, 48(%rdi)
	mov %rbx, 56(%rdi)
	xor %rax, %rax
	jmp *%rbx

jump:
	mov (%rdi),   %rbx
	mov 8(%rdi),  %rsp
	mov 16(%rdi), %rbp
	mov 24(%rdi), %r12
	mov 32(%rdi), %r13
	mov 40(%rdi), %r14
	mov 48(%rdi), %r15
	mov %rsi, %rax
	jmp *56(%rdi)
