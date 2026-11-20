	.file	"ohlc_fx.c"
	.text
	.p2align 4
	.globl	ohlc_fx_init
	.def	ohlc_fx_init;	.scl	2;	.type	32;	.endef
ohlc_fx_init:
	pxor	%xmm0, %xmm0
	movq	$0, 32(%rcx)
	movq	$0, 40(%rcx)
	movups	%xmm0, (%rcx)
	movups	%xmm0, 16(%rcx)
	ret
	.p2align 4
	.globl	ohlc_fx_update
	.def	ohlc_fx_update;	.scl	2;	.type	32;	.endef
ohlc_fx_update:
	movabsq	$4835703278458516699, %rax
	movq	%rdx, %r10
	imulq	%rdx
	movl	44(%rcx), %eax
	sarq	$63, %r10
	sarq	$18, %rdx
	subq	%r10, %rdx
	testl	%eax, %eax
	je	.L4
	cmpq	%rdx, (%rcx)
	je	.L5
	movdqu	(%rcx), %xmm0
	movl	$1, %eax
	movups	%xmm0, (%r9)
	movdqu	16(%rcx), %xmm0
	movups	%xmm0, 16(%r9)
	movdqu	32(%rcx), %xmm0
	movups	%xmm0, 32(%r9)
.L4:
	movq	%rdx, (%rcx)
	movabsq	$4294967297, %rdx
	movq	%r8, 32(%rcx)
	movq	%r8, 24(%rcx)
	movq	%r8, 16(%rcx)
	movq	%r8, 8(%rcx)
	movq	%rdx, 40(%rcx)
	ret
	.p2align 4,,10
	.p2align 3
.L5:
	cmpq	%r8, 16(%rcx)
	jge	.L7
	movq	%r8, 16(%rcx)
.L7:
	cmpq	%r8, 24(%rcx)
	jle	.L8
	movq	%r8, 24(%rcx)
.L8:
	addl	$1, 40(%rcx)
	xorl	%eax, %eax
	movq	%r8, 32(%rcx)
	ret
