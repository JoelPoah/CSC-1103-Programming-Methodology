	.file	"engine.c"
	.text
	.p2align 4
	.def	step_fx;	.scl	3;	.type	32;	.endef
step_fx:
	subq	$104, %rsp
	movq	%rbx, 72(%rsp)
	movq	%rdx, %rbx
	movq	8(%rdx), %rdx
	movq	16(%rbx), %r8
	movq	%rsi, 80(%rsp)
	movq	%rcx, %rsi
	addq	$2256, %rcx
	movq	%rbp, 96(%rsp)
	leaq	3416(%rsi), %rbp
	call	vwap_fx_update
	movq	8(%rbx), %r8
	movq	(%rbx), %rdx
	leaq	2320(%rsi), %r9
	leaq	2272(%rsi), %rcx
	call	ohlc_fx_update
	movq	8(%rbx), %rdx
	leaq	2368(%rsi), %rcx
	call	sma_fx_push
	movq	8(%rbx), %rdx
	movq	%rbp, %rcx
	call	sma_fx_push
	leaq	2368(%rsi), %rcx
	call	sma_fx_ready
	testl	%eax, %eax
	jne	.L11
.L1:
	movq	72(%rsp), %rbx
	movq	80(%rsp), %rsi
	movq	96(%rsp), %rbp
	addq	$104, %rsp
	ret
	.p2align 4,,10
	.p2align 3
.L11:
	movq	%rbp, %rcx
	call	sma_fx_ready
	testl	%eax, %eax
	je	.L1
	movq	%rbp, %rcx
	movl	8(%rsi), %ebx
	call	sma_fx_sum
	movl	4(%rsi), %r8d
	leaq	2368(%rsi), %rcx
	movq	%rax, %rbp
	movl	%r8d, 60(%rsp)
	call	sma_fx_sum
	movl	%ebx, 32(%rsp)
	movl	60(%rsp), %r8d
	movq	%rbp, %r9
	leaq	4464(%rsi), %rcx
	movq	%rax, %rdx
	call	crossover_fx_update
	jmp	.L1
	.section .rdata,"dr"
	.align 8
.LC0:
	.ascii "engine: malloc failed for %lld trades\12\0"
	.align 8
.LC1:
	.ascii "engine: line %lld is not a trade\12\0"
	.text
	.p2align 4
	.globl	engine_init
	.def	engine_init;	.scl	2;	.type	32;	.endef
engine_init:
	pushq	%r12
	movl	%edx, %r12d
	pushq	%rbp
	movl	%r8d, %ebp
	pushq	%rdi
	pushq	%rsi
	movl	%r9d, %esi
	pushq	%rbx
	movq	%rcx, %rbx
	addq	$16, %rcx
	subq	$32, %rsp
	movl	%r8d, -12(%rcx)
	movl	%r9d, -8(%rcx)
	movq	112(%rsp), %rdi
	movl	%edx, -16(%rcx)
	movq	$0, 4472(%rcx)
	call	vwap_init
	leaq	32(%rbx), %rcx
	call	ohlc_init
	leaq	80(%rbx), %rcx
	call	ohlc_init
	leaq	128(%rbx), %rcx
	movl	%ebp, %edx
	call	sma_init
	movl	%esi, %edx
	leaq	1176(%rbx), %rcx
	call	sma_init
	leaq	2224(%rbx), %rcx
	call	crossover_init
	leaq	2256(%rbx), %rcx
	call	vwap_fx_init
	leaq	2272(%rbx), %rcx
	call	ohlc_fx_init
	leaq	2320(%rbx), %rcx
	call	ohlc_fx_init
	leaq	2368(%rbx), %rcx
	movl	%ebp, %edx
	call	sma_fx_init
	leaq	3416(%rbx), %rcx
	movl	%esi, %edx
	call	sma_fx_init
	leaq	4464(%rbx), %rcx
	call	crossover_fx_init
	cmpl	$2, %r12d
	je	.L21
.L13:
	movl	$1, %ebp
.L12:
	addq	$32, %rsp
	movl	%ebp, %eax
	popq	%rbx
	popq	%rsi
	popq	%rdi
	popq	%rbp
	popq	%r12
	ret
	.p2align 4,,10
	.p2align 3
.L21:
	movq	24(%rdi), %rbp
	movq	%rbp, %rcx
	salq	$5, %rcx
	call	malloc
	movq	%rax, 4488(%rbx)
	movq	%rax, %rdx
	testq	%rax, %rax
	je	.L14
	xorl	%esi, %esi
	testq	%rbp, %rbp
	jg	.L15
	jmp	.L13
	.p2align 4,,10
	.p2align 3
.L17:
	cmpq	%rsi, 24(%rdi)
	jle	.L13
	movq	4488(%rbx), %rdx
.L15:
	movq	%rsi, %rax
	salq	$5, %rax
	addq	%rax, %rdx
	movq	8(%rdi), %rax
	movq	(%rax,%rsi,8), %rcx
	addq	$1, %rsi
	call	parse_trade_fx
	movl	%eax, %ebp
	testl	%eax, %eax
	jne	.L17
	movq	%rsi, %rdx
	leaq	.LC1(%rip), %rcx
	call	__mingw_printf
	movq	4488(%rbx), %rcx
	call	free
	movq	$0, 4488(%rbx)
	jmp	.L12
.L14:
	movq	%rbp, %rdx
	leaq	.LC0(%rip), %rcx
	xorl	%ebp, %ebp
	call	__mingw_printf
	jmp	.L12
	.section .rdata,"dr"
	.align 8
.LC2:
	.ascii "%lld,%lf,%lf,%lf,%lld,%15[^,],%15s\0"
	.text
	.p2align 4
	.globl	engine_step
	.def	engine_step;	.scl	2;	.type	32;	.endef
engine_step:
	subq	$216, %rsp
	movl	(%rcx), %eax
	movq	%rbx, 192(%rsp)
	movq	%rcx, %rbx
	movq	%rsi, 200(%rsp)
	movq	%r9, %rsi
	testl	%eax, %eax
	je	.L23
	cmpl	$1, %eax
	je	.L24
	movq	4488(%rcx), %rdx
	salq	$5, %r8
	movq	192(%rsp), %rbx
	movq	200(%rsp), %rsi
	addq	%r8, %rdx
	movq	8(%rdx), %rax
	movq	%rax, (%r9)
	addq	$216, %rsp
	jmp	step_fx
	.p2align 4,,10
	.p2align 3
.L24:
	movq	8(%rdx), %rax
	leaq	160(%rsp), %rdx
	movq	(%rax,%r8,8), %rcx
	call	parse_trade_fx
	testl	%eax, %eax
	jne	.L29
	movq	$0, (%rsi)
	jmp	.L22
	.p2align 4,,10
	.p2align 3
.L23:
	movq	8(%rdx), %rax
	leaq	112(%rsp), %r9
	leaq	.LC2(%rip), %rdx
	movq	(%rax,%r8,8), %rcx
	leaq	160(%rsp), %rax
	leaq	128(%rsp), %r8
	movq	%rax, 64(%rsp)
	leaq	144(%rsp), %rax
	movq	%rax, 56(%rsp)
	leaq	104(%rsp), %rax
	movq	%rax, 48(%rsp)
	leaq	136(%rsp), %rax
	movq	%rax, 40(%rsp)
	leaq	120(%rsp), %rax
	movq	%rax, 32(%rsp)
	call	__mingw_sscanf
	cmpl	$7, %eax
	je	.L26
	movq	$0, (%rsi)
	xorl	%eax, %eax
.L22:
	movq	192(%rsp), %rbx
	movq	200(%rsp), %rsi
	addq	$216, %rsp
	ret
	.p2align 4,,10
	.p2align 3
.L29:
	movq	168(%rsp), %rax
	leaq	160(%rsp), %rdx
	movq	%rbx, %rcx
	movq	%rax, (%rsi)
	call	step_fx
	jmp	.L22
	.p2align 4,,10
	.p2align 3
.L26:
	movsd	112(%rsp), %xmm1
	movsd	.LC3(%rip), %xmm0
	leaq	16(%rbx), %rcx
	movsd	120(%rsp), %xmm2
	mulsd	%xmm1, %xmm0
	addsd	.LC4(%rip), %xmm0
	cvttsd2siq	%xmm0, %rax
	movq	%rax, (%rsi)
	call	vwap_update
	movsd	112(%rsp), %xmm2
	movq	104(%rsp), %rdx
	leaq	80(%rbx), %r9
	leaq	32(%rbx), %rcx
	call	ohlc_update
	movsd	112(%rsp), %xmm1
	leaq	128(%rbx), %rcx
	call	sma_naive_push
	movsd	112(%rsp), %xmm1
	leaq	1176(%rbx), %rcx
	call	sma_naive_push
	leaq	128(%rbx), %rcx
	call	sma_ready
	testl	%eax, %eax
	je	.L22
	leaq	1176(%rbx), %rcx
	call	sma_ready
	testl	%eax, %eax
	je	.L22
	leaq	1176(%rbx), %rcx
	call	sma_naive_value
	leaq	128(%rbx), %rcx
	movsd	%xmm0, 88(%rsp)
	call	sma_naive_value
	movsd	88(%rsp), %xmm2
	leaq	2224(%rbx), %rcx
	movapd	%xmm0, %xmm1
	call	crossover_update
	jmp	.L22
	.p2align 4
	.globl	engine_free
	.def	engine_free;	.scl	2;	.type	32;	.endef
engine_free:
	pushq	%rbx
	movq	%rcx, %rbx
	subq	$32, %rsp
	movq	4488(%rcx), %rcx
	call	free
	movq	$0, 4488(%rbx)
	addq	$32, %rsp
	popq	%rbx
	ret
	.p2align 4
	.globl	engine_buys
	.def	engine_buys;	.scl	2;	.type	32;	.endef
engine_buys:
	movl	(%rcx), %eax
	testl	%eax, %eax
	jne	.L41
	movl	2244(%rcx), %eax
	ret
	.p2align 4,,10
	.p2align 3
.L41:
	movl	4476(%rcx), %eax
	ret
	.p2align 4
	.globl	engine_sells
	.def	engine_sells;	.scl	2;	.type	32;	.endef
engine_sells:
	movl	(%rcx), %eax
	testl	%eax, %eax
	jne	.L44
	movl	2248(%rcx), %eax
	ret
	.p2align 4,,10
	.p2align 3
.L44:
	movl	4480(%rcx), %eax
	ret
	.p2align 4
	.globl	engine_vwap
	.def	engine_vwap;	.scl	2;	.type	32;	.endef
engine_vwap:
	movl	(%rcx), %eax
	testl	%eax, %eax
	jne	.L47
	addq	$16, %rcx
	jmp	vwap_value
	.p2align 4,,10
	.p2align 3
.L47:
	addq	$2256, %rcx
	jmp	vwap_fx_value
	.section .rdata,"dr"
.LC5:
	.ascii "v0\0"
.LC6:
	.ascii "fast\0"
.LC7:
	.ascii "mem\0"
	.text
	.p2align 4
	.globl	engine_name
	.def	engine_name;	.scl	2;	.type	32;	.endef
engine_name:
	leaq	.LC5(%rip), %rax
	testl	%ecx, %ecx
	je	.L48
	leaq	.LC6(%rip), %rax
	cmpl	$1, %ecx
	leaq	.LC7(%rip), %rdx
	cmovne	%rdx, %rax
.L48:
	ret
	.p2align 4
	.globl	engine_parse_kind
	.def	engine_parse_kind;	.scl	2;	.type	32;	.endef
engine_parse_kind:
	pushq	%rbx
	movq	%rdx, %rbx
	subq	$32, %rsp
	cmpb	$118, (%rcx)
	jne	.L55
	cmpb	$48, 1(%rcx)
	jne	.L55
	cmpb	$0, 2(%rcx)
	jne	.L55
	movl	$0, (%rdx)
.L58:
	addq	$32, %rsp
	movl	$1, %eax
	popq	%rbx
	ret
	.p2align 4,,10
	.p2align 3
.L55:
	leaq	.LC6(%rip), %rdx
	movq	%rcx, 48(%rsp)
	call	strcmp
	movq	48(%rsp), %rcx
	testl	%eax, %eax
	je	.L61
	leaq	.LC7(%rip), %rdx
	call	strcmp
	testl	%eax, %eax
	movl	$0, %eax
	je	.L62
	addq	$32, %rsp
	popq	%rbx
	ret
	.p2align 4,,10
	.p2align 3
.L61:
	movl	$1, (%rbx)
	jmp	.L58
	.p2align 4,,10
	.p2align 3
.L62:
	movl	$2, (%rbx)
	jmp	.L58
	.section .rdata,"dr"
	.align 8
.LC3:
	.long	0
	.long	1079574528
	.align 8
.LC4:
	.long	0
	.long	1071644672
	.def	vwap_fx_update;	.scl	2;	.type	32;	.endef
	.def	ohlc_fx_update;	.scl	2;	.type	32;	.endef
	.def	sma_fx_push;	.scl	2;	.type	32;	.endef
	.def	sma_fx_ready;	.scl	2;	.type	32;	.endef
	.def	sma_fx_sum;	.scl	2;	.type	32;	.endef
	.def	crossover_fx_update;	.scl	2;	.type	32;	.endef
	.def	vwap_init;	.scl	2;	.type	32;	.endef
	.def	ohlc_init;	.scl	2;	.type	32;	.endef
	.def	sma_init;	.scl	2;	.type	32;	.endef
	.def	crossover_init;	.scl	2;	.type	32;	.endef
	.def	vwap_fx_init;	.scl	2;	.type	32;	.endef
	.def	ohlc_fx_init;	.scl	2;	.type	32;	.endef
	.def	sma_fx_init;	.scl	2;	.type	32;	.endef
	.def	crossover_fx_init;	.scl	2;	.type	32;	.endef
	.def	malloc;	.scl	2;	.type	32;	.endef
	.def	parse_trade_fx;	.scl	2;	.type	32;	.endef
	.def	free;	.scl	2;	.type	32;	.endef
	.def	vwap_update;	.scl	2;	.type	32;	.endef
	.def	ohlc_update;	.scl	2;	.type	32;	.endef
	.def	sma_naive_push;	.scl	2;	.type	32;	.endef
	.def	sma_ready;	.scl	2;	.type	32;	.endef
	.def	sma_naive_value;	.scl	2;	.type	32;	.endef
	.def	crossover_update;	.scl	2;	.type	32;	.endef
	.def	vwap_value;	.scl	2;	.type	32;	.endef
	.def	vwap_fx_value;	.scl	2;	.type	32;	.endef
	.def	strcmp;	.scl	2;	.type	32;	.endef
