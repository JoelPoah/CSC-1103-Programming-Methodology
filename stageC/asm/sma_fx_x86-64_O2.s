	.file	"sma_fx.c"
	.text
	.section .rdata,"dr"
	.align 8
.LC0:
	.ascii "SMA INIT FAILED: window size out of bounds\0"
	.text
	.p2align 4
	.globl	sma_fx_init
	.def	sma_fx_init;	.scl	2;	.type	32;	.endef
sma_fx_init:
	movl	%edx, 1024(%rcx)
	subl	$1, %edx
	movq	$0, 1028(%rcx)
	movq	$0, 1040(%rcx)
	cmpl	$127, %edx
	ja	.L8
	ret
	.p2align 4,,10
	.p2align 3
.L8:
	subq	$40, %rsp
	movq	%rcx, 48(%rsp)
	leaq	.LC0(%rip), %rcx
	call	puts
	movq	48(%rsp), %r8
	movl	$1, 1024(%r8)
	addq	$40, %rsp
	ret
	.p2align 4
	.globl	sma_fx_push
	.def	sma_fx_push;	.scl	2;	.type	32;	.endef
sma_fx_push:
	movslq	1028(%rcx), %r9
	movl	1024(%rcx), %r10d
	movq	%rdx, %r8
	pushq	%rbx
	movl	1032(%rcx), %r11d
	movq	1040(%rcx), %rbx
	leal	1(%r9), %eax
	cltd
	idivl	%r10d
	cmpl	%r10d, %r11d
	je	.L13
	movq	%r8, (%rcx,%r9,8)
	addq	%rbx, %r8
	movq	%r8, 1040(%rcx)
	movl	%edx, 1028(%rcx)
	cmpl	%r10d, %r11d
	jge	.L9
	addl	$1, %r11d
	movl	%r11d, 1032(%rcx)
.L9:
	popq	%rbx
	ret
	.p2align 4,,10
	.p2align 3
.L13:
	subq	(%rcx,%r9,8), %rbx
	movq	%r8, (%rcx,%r9,8)
	addq	%r8, %rbx
	movl	%edx, 1028(%rcx)
	movq	%rbx, 1040(%rcx)
	popq	%rbx
	ret
	.p2align 4
	.globl	sma_fx_ready
	.def	sma_fx_ready;	.scl	2;	.type	32;	.endef
sma_fx_ready:
	movl	1024(%rcx), %eax
	cmpl	%eax, 1032(%rcx)
	sete	%al
	movzbl	%al, %eax
	ret
	.p2align 4
	.globl	sma_fx_sum
	.def	sma_fx_sum;	.scl	2;	.type	32;	.endef
sma_fx_sum:
	movq	1040(%rcx), %rax
	ret
	.p2align 4
	.globl	sma_fx_value
	.def	sma_fx_value;	.scl	2;	.type	32;	.endef
sma_fx_value:
	movl	1024(%rcx), %eax
	pxor	%xmm0, %xmm0
	cmpl	%eax, 1032(%rcx)
	jne	.L16
	pxor	%xmm1, %xmm1
	pxor	%xmm0, %xmm0
	cvtsi2sdq	1040(%rcx), %xmm0
	cvtsi2sdl	%eax, %xmm1
	divsd	%xmm1, %xmm0
	divsd	.LC2(%rip), %xmm0
.L16:
	ret
	.section .rdata,"dr"
	.align 8
.LC2:
	.long	0
	.long	1079574528
	.def	puts;	.scl	2;	.type	32;	.endef
