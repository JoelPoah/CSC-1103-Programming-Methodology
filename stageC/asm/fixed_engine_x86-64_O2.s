	.file	"fixed_engine.c"
	.text
	.p2align 4
	.globl	fixed_start
	.def	fixed_start;	.scl	2;	.type	32;	.endef
fixed_start:
	pushq	%rsi
	movl	%ecx, %esi
	pushq	%rbx
	movl	%edx, %ebx
	subq	$40, %rsp
	movl	%ecx, short_window(%rip)
	leaq	vwap(%rip), %rcx
	movl	%edx, long_window(%rip)
	call	vwap_fx_init
	leaq	bar(%rip), %rcx
	call	ohlc_fx_init
	leaq	finished(%rip), %rcx
	call	ohlc_fx_init
	movl	%esi, %edx
	leaq	short_sma(%rip), %rcx
	call	sma_fx_init
	movl	%ebx, %edx
	leaq	long_sma(%rip), %rcx
	call	sma_fx_init
	addq	$40, %rsp
	leaq	cross(%rip), %rcx
	popq	%rbx
	popq	%rsi
	jmp	crossover_fx_init
	.p2align 4
	.globl	fixed_step
	.def	fixed_step;	.scl	2;	.type	32;	.endef
fixed_step:
	pushq	%rbx
	movq	%rcx, %rbx
	subq	$64, %rsp
	movq	8(%rcx), %rdx
	movq	16(%rcx), %r8
	leaq	vwap(%rip), %rcx
	call	vwap_fx_update
	movq	8(%rbx), %r8
	movq	(%rbx), %rdx
	leaq	finished(%rip), %r9
	leaq	bar(%rip), %rcx
	call	ohlc_fx_update
	movq	8(%rbx), %rdx
	leaq	short_sma(%rip), %rcx
	call	sma_fx_push
	movq	8(%rbx), %rdx
	leaq	long_sma(%rip), %rcx
	call	sma_fx_push
	leaq	short_sma(%rip), %rcx
	call	sma_fx_ready
	testl	%eax, %eax
	jne	.L13
.L4:
	addq	$64, %rsp
	popq	%rbx
	ret
	.p2align 4,,10
	.p2align 3
.L13:
	leaq	long_sma(%rip), %rcx
	call	sma_fx_ready
	testl	%eax, %eax
	je	.L4
	leaq	long_sma(%rip), %rcx
	movl	long_window(%rip), %ebx
	call	sma_fx_sum
	movl	short_window(%rip), %r8d
	leaq	short_sma(%rip), %rcx
	movq	%rax, 56(%rsp)
	movl	%r8d, 52(%rsp)
	call	sma_fx_sum
	movl	%ebx, 32(%rsp)
	movq	56(%rsp), %r9
	leaq	cross(%rip), %rcx
	movl	52(%rsp), %r8d
	movq	%rax, %rdx
	call	crossover_fx_update
	jmp	.L4
	.p2align 4
	.globl	fixed_price_cents
	.def	fixed_price_cents;	.scl	2;	.type	32;	.endef
fixed_price_cents:
	movq	8(%rcx), %rax
	ret
	.p2align 4
	.globl	fixed_buys
	.def	fixed_buys;	.scl	2;	.type	32;	.endef
fixed_buys:
	movl	12+cross(%rip), %eax
	ret
	.p2align 4
	.globl	fixed_sells
	.def	fixed_sells;	.scl	2;	.type	32;	.endef
fixed_sells:
	movl	16+cross(%rip), %eax
	ret
	.p2align 4
	.globl	fixed_vwap
	.def	fixed_vwap;	.scl	2;	.type	32;	.endef
fixed_vwap:
	leaq	vwap(%rip), %rcx
	jmp	vwap_fx_value
.lcomm long_window,4,4
.lcomm short_window,4,4
.lcomm cross,24,16
.lcomm long_sma,1048,32
.lcomm short_sma,1048,32
.lcomm finished,48,32
.lcomm bar,48,32
.lcomm vwap,16,16
	.def	vwap_fx_init;	.scl	2;	.type	32;	.endef
	.def	ohlc_fx_init;	.scl	2;	.type	32;	.endef
	.def	sma_fx_init;	.scl	2;	.type	32;	.endef
	.def	crossover_fx_init;	.scl	2;	.type	32;	.endef
	.def	vwap_fx_update;	.scl	2;	.type	32;	.endef
	.def	ohlc_fx_update;	.scl	2;	.type	32;	.endef
	.def	sma_fx_push;	.scl	2;	.type	32;	.endef
	.def	sma_fx_ready;	.scl	2;	.type	32;	.endef
	.def	sma_fx_sum;	.scl	2;	.type	32;	.endef
	.def	crossover_fx_update;	.scl	2;	.type	32;	.endef
	.def	vwap_fx_value;	.scl	2;	.type	32;	.endef
