// sma.s - AArch64 (64-bit ARM) version of sma.c (double prices)
//
// Running-sum simple moving average, used by stageB_inmem.c / stageB_stream.c.
// Drop-in replacement for sma.c: same function names, same Sma layout.
// This is the double-precision twin of sma_fx.s; see sma_fx.md for a
// line-by-line explanation of the same technique.
//
// Build:  gcc -c sma.s -o sma.o      (on a 64-bit Raspberry Pi OS)
//
// Calling convention (AAPCS64):
//   Integer/pointer arguments in x0, x1, ...; floating-point arguments in
//   d0, d1, ... (counted separately). A double return value goes in d0.
//     sma_push(Sma *s, double price)  ->  s = x0, price = d0
//   x9-x15 and d1-d7 are scratch; x19-x28 are callee-saved.
//
// Results are bit-for-bit identical to sma.c: the floating-point operations
// happen in exactly the same order (sum -= oldest; sum += price; sum / window).

// ---------------------------------------------------------------------------
// Constant. MUST MATCH sma.h.
// ---------------------------------------------------------------------------
        .equ    MAX_WIN, 128                // <-- change to match sma.h

// Sma layout (byte offsets from the start of the struct):
//   double win[MAX_WIN]   0
//   int    window         8*MAX_WIN
//   int    head           8*MAX_WIN + 4
//   int    count          8*MAX_WIN + 8
//   (4 bytes padding so sum is 8-byte aligned)
//   double sum            8*MAX_WIN + 16
        .equ    O_WINDOW, MAX_WIN * 8
        .equ    O_HEAD,   O_WINDOW + 4
        .equ    O_COUNT,  O_WINDOW + 8
        .equ    O_SUM,    O_WINDOW + 16

// 32-bit ldr/str offsets go up to 16380, which allows MAX_WIN up to 2046.
        .if     O_COUNT > 16380
        .error  "MAX_WIN too large: field offsets no longer fit in ldr/str"
        .endif

        .section .rodata
msg_fail:
        .asciz  "SMA INIT FAILED: window size out of bounds\n"

        .text

// ---------------------------------------------------------------------------
// void sma_init(Sma *s, int window)
//   x0 = s, w1 = window
//   Same behaviour as sma.c: on a bad window it prints the message and
//   leaves s->window unset (sma.c does not set it either).
// ---------------------------------------------------------------------------
        .p2align 4
        .global sma_init
        .type   sma_init, %function
sma_init:
        str     wzr, [x0, #O_HEAD]          // s->head  = 0
        str     wzr, [x0, #O_COUNT]         // s->count = 0
        str     xzr, [x0, #O_SUM]           // s->sum   = 0.0 (all-zero bits)

        cmp     w1, #1                      // window < 1 ?
        b.lt    .Linit_bad
        cmp     w1, #MAX_WIN                // window > MAX_WIN ?
        b.gt    .Linit_bad
        str     w1, [x0, #O_WINDOW]         // valid: s->window = window
        ret

.Linit_bad:
        adrp    x0, msg_fail                // printf(msg_fail)
        add     x0, x0, :lo12:msg_fail
        b       printf                      // tail call: printf returns straight to our caller
        .size   sma_init, .-sma_init

// ---------------------------------------------------------------------------
// void sma_push(Sma *s, double price)
//   x0 = s, d0 = price
//   Leaf function, no stack frame.
// ---------------------------------------------------------------------------
        .p2align 4
        .global sma_push
        .type   sma_push, %function
sma_push:
        ldr     w10, [x0, #O_WINDOW]        // w10 = window
        ldrsw   x11, [x0, #O_HEAD]          // x11 = head (sign-extended for indexing)
        ldr     w12, [x0, #O_COUNT]         // w12 = count
        ldr     d1,  [x0, #O_SUM]           // d1  = sum

        cmp     w12, w10                    // if (count == window)
        b.ne    1f
        ldr     d2, [x0, x11, lsl #3]       //     d2 = s->win[head]  (oldest price)
        fsub    d1, d1, d2                  //     sum -= oldest price
1:
        str     d0, [x0, x11, lsl #3]       // s->win[head] = price
        fadd    d1, d1, d0                  // sum += price (kept in a register: no reload)
        str     d1, [x0, #O_SUM]

        // head = (head + 1) % window, without dividing:
        // head + 1 is at most window, so wrap to 0 only when equal.
        add     w11, w11, #1
        cmp     w11, w10
        csel    w11, wzr, w11, eq
        str     w11, [x0, #O_HEAD]

        cmp     w12, w10                    // if (count < window)
        b.ge    2f
        add     w12, w12, #1                //     count++
        str     w12, [x0, #O_COUNT]
2:
        ret
        .size   sma_push, .-sma_push

// ---------------------------------------------------------------------------
// int sma_ready(const Sma *s)
//   x0 = s, returns w0 = 1 if count == window, else 0
// ---------------------------------------------------------------------------
        .p2align 4
        .global sma_ready
        .type   sma_ready, %function
sma_ready:
        ldr     w10, [x0, #O_COUNT]
        ldr     w11, [x0, #O_WINDOW]
        cmp     w10, w11
        cset    w0, eq                      // w0 = (count == window) ? 1 : 0
        ret
        .size   sma_ready, .-sma_ready

// ---------------------------------------------------------------------------
// double sma_value(const Sma *s)
//   x0 = s, returns d0 = sum / window, or 0.0 if not ready
//   (stageB_inmem calls this on every trade, unlike the fixed-point build)
// ---------------------------------------------------------------------------
        .p2align 4
        .global sma_value
        .type   sma_value, %function
sma_value:
        ldr     w10, [x0, #O_COUNT]
        ldr     w11, [x0, #O_WINDOW]
        cmp     w10, w11                    // ready?
        b.ne    1f
        ldr     d0, [x0, #O_SUM]
        scvtf   d1, w11                     // d1 = (double)window
        fdiv    d0, d0, d1                  // d0 = sum / window
        ret
1:
        movi    d0, #0                      // return 0.0
        ret
        .size   sma_value, .-sma_value

        .section .note.GNU-stack, "", %progbits
