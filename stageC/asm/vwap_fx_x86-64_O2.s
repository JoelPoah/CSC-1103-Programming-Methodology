	.file	"vwap_fx.c"
	.text
	.p2align 4
	.globl	vwap_fx_init
	.def	vwap_fx_init;	.scl	2;	.type	32;	.endef
vwap_fx_init:
	pxor	%xmm0, %xmm0
	movups	%xmm0, (%rcx)
	ret
	.p2align 4
	.globl	vwap_fx_update
	.def	vwap_fx_update;	.scl	2;	.type	32;	.endef
vwap_fx_update:
	imulq	%r8, %rdx
	movdqu	(%rcx), %xmm2
	movq	%r8, %xmm1
	movq	%rdx, %xmm0
	punpcklqdq	%xmm1, %xmm0
	paddq	%xmm2, %xmm0
	movups	%xmm0, (%rcx)
	ret
	.p2align 4
	.globl	vwap_fx_value
	.def	vwap_fx_value;	.scl	2;	.type	32;	.endef
vwap_fx_value:
	movq	8(%rcx), %rax
	pxor	%xmm0, %xmm0
	testq	%rax, %rax
	je	.L4
	pxor	%xmm1, %xmm1
	pxor	%xmm0, %xmm0
	cvtsi2sdq	(%rcx), %xmm0
	cvtsi2sdq	%rax, %xmm1
	divsd	%xmm1, %xmm0
	divsd	.LC1(%rip), %xmm0
.L4:
	ret
	.section .rdata,"dr"
	.align 8
.LC1:
	.long	0
	.long	1079574528
