	.file	"signal_fx.c"
	.text
	.p2align 4
	.globl	crossover_fx_init
	.def	crossover_fx_init;	.scl	2;	.type	32;	.endef
crossover_fx_init:
	movq	$0, (%rcx)
	movq	$0, 8(%rcx)
	movl	$0, 16(%rcx)
	ret
	.p2align 4
	.globl	crossover_fx_update
	.def	crossover_fx_update;	.scl	2;	.type	32;	.endef
crossover_fx_update:
	movslq	40(%rsp), %rax
	movslq	%r8d, %r8
	imulq	%r9, %r8
	imulq	%rdx, %rax
	movl	8(%rcx), %edx
	subq	%r8, %rax
	testl	%edx, %edx
	je	.L4
	movq	(%rcx), %rdx
	testq	%rdx, %rdx
	jle	.L14
.L5:
	testq	%rax, %rax
	js	.L15
	xorl	%edx, %edx
.L4:
	movq	%rax, (%rcx)
	movl	%edx, %eax
	movl	$1, 8(%rcx)
	ret
	.p2align 4,,10
	.p2align 3
.L14:
	testq	%rax, %rax
	jle	.L6
	movl	$1, %edx
	movq	%rax, (%rcx)
	addl	$1, 12(%rcx)
	movl	%edx, %eax
	movl	$1, 8(%rcx)
	ret
	.p2align 4,,10
	.p2align 3
.L15:
	addl	$1, 16(%rcx)
	movl	$-1, %edx
	jmp	.L4
	.p2align 4,,10
	.p2align 3
.L6:
	testq	%rdx, %rdx
	je	.L5
	xorl	%edx, %edx
	jmp	.L4
