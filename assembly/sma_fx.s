// sma_fx.s - AArch64 (64-bit ARM) version of sma_fx.c
//
// Running-sum simple moving average with fixed-point prices.
// Drop-in replacement for sma_fx.c: same function names, same SmaFx layout,
// so the rest of the C program can link against it unchanged.
//
// Build:  gcc -c sma_fx.s -o sma_fx.o      (on a 64-bit Raspberry Pi OS)
//
// Calling convention (AAPCS64):
//   x0-x7   integer/pointer arguments, x0 = integer return value
//   d0      double return value
//   x9-x15  scratch registers (free to use, not preserved)
//   x19-x28 callee-saved (must be restored before returning)
//   x29/x30 frame pointer / link register (return address)
//
// Performance notes (the first version, without these, ran ~9% slower than
// the C build on the Pi):
//   - Fields are read straight from the struct pointer, e.g. [x0, #1032].
//     An earlier version first did "add x9, x0, #1024" and read [x9, #8];
//     that extra add delays every load by a cycle, in all six calls per trade.
//   - head is sign-extended once with ldrsw, then used as [x0, x11, lsl #3].
//     The form [x0, w11, sxtw #3] costs an extra cycle on Cortex-A72/A76.
//   - Each function starts on a 16-byte boundary (.p2align 4), like GCC does.

// ---------------------------------------------------------------------------
// Constants. THESE TWO MUST MATCH YOUR HEADERS:
//   MAX_WIN     from sma.h
//   PRICE_SCALE from fixed.h
// ---------------------------------------------------------------------------
        .equ    MAX_WIN,     128            // <-- change to match sma.h
        .equ    PRICE_SCALE, 100            // <-- change to match fixed.h

// SmaFx layout (byte offsets from the start of the struct):
//   long long win[MAX_WIN]   0
//   int       window         8*MAX_WIN
//   int       head           8*MAX_WIN + 4
//   int       count          8*MAX_WIN + 8
//   (4 bytes padding so sum is 8-byte aligned)
//   long long sum            8*MAX_WIN + 16
        .equ    O_WINDOW, MAX_WIN * 8
        .equ    O_HEAD,   O_WINDOW + 4
        .equ    O_COUNT,  O_WINDOW + 8
        .equ    O_SUM,    O_WINDOW + 16

// A 32-bit ldr/str can encode an offset of at most 16380 (and 64-bit 32760),
// which allows MAX_WIN up to 2046. Past that, fail the build loudly.
        .if     O_COUNT > 16380
        .error  "MAX_WIN too large: field offsets no longer fit in ldr/str"
        .endif

        .section .rodata
msg_fail:
        .asciz  "SMA INIT FAILED: window size out of bounds\n"

        .text

// ---------------------------------------------------------------------------
// void sma_fx_init(SmaFx *s, int window)
//   x0 = s, w1 = window
// ---------------------------------------------------------------------------
        .p2align 4
        .global sma_fx_init
        .type   sma_fx_init, %function
sma_fx_init:
        stp     x29, x30, [sp, #-32]!       // save frame pointer + return address
        mov     x29, sp
        str     x19, [sp, #16]              // save x19 so we can use it
        mov     x19, x0                     // x19 = s (survives the printf call)

        str     wzr, [x19, #O_HEAD]         // s->head  = 0
        str     wzr, [x19, #O_COUNT]        // s->count = 0
        str     xzr, [x19, #O_SUM]          // s->sum   = 0
        str     w1,  [x19, #O_WINDOW]       // s->window = window

        cmp     w1, #1                      // window < 1 ?
        b.lt    .Linit_bad
        cmp     w1, #MAX_WIN                // window <= MAX_WIN ?
        b.le    .Linit_done

.Linit_bad:
        adrp    x0, msg_fail                // x0 = address of the message
        add     x0, x0, :lo12:msg_fail
        bl      printf
        mov     w10, #1
        str     w10, [x19, #O_WINDOW]       // s->window = 1 (safe value)

.Linit_done:
        ldr     x19, [sp, #16]              // restore x19
        ldp     x29, x30, [sp], #32         // restore fp + lr, pop the frame
        ret
        .size   sma_fx_init, .-sma_fx_init

// ---------------------------------------------------------------------------
// void sma_fx_push(SmaFx *s, long long price)
//   x0 = s, x1 = price
//   Leaf function (calls nothing), so no stack frame is needed.
// ---------------------------------------------------------------------------
        .p2align 4
        .global sma_fx_push
        .type   sma_fx_push, %function
sma_fx_push:
        ldr     w10, [x0, #O_WINDOW]        // w10 = window
        ldrsw   x11, [x0, #O_HEAD]          // x11 = head (sign-extended for indexing)
        ldr     w12, [x0, #O_COUNT]         // w12 = count
        ldr     x13, [x0, #O_SUM]           // x13 = sum

        cmp     w12, w10                    // if (count == window)
        b.ne    1f
        ldr     x14, [x0, x11, lsl #3]      //     x14 = s->win[head]  (s + head*8)
        sub     x13, x13, x14               //     sum -= oldest price
1:
        str     x1, [x0, x11, lsl #3]       // s->win[head] = price
        add     x13, x13, x1                // sum += price
        str     x13, [x0, #O_SUM]           // sum stays in a register: no reload

        // head = (head + 1) % window, without dividing.
        // head is always in 0..window-1, so head + 1 is at most window:
        // the only wrap case is head + 1 == window, which becomes 0.
        add     w11, w11, #1                // head + 1
        cmp     w11, w10
        csel    w11, wzr, w11, eq           // w11 = (head + 1 == window) ? 0 : head + 1
        str     w11, [x0, #O_HEAD]

        cmp     w12, w10                    // if (count < window)
        b.ge    2f
        add     w12, w12, #1                //     count++
        str     w12, [x0, #O_COUNT]
2:
        ret
        .size   sma_fx_push, .-sma_fx_push

// ---------------------------------------------------------------------------
// int sma_fx_ready(const SmaFx *s)
//   x0 = s, returns w0 = 1 if count == window, else 0
// ---------------------------------------------------------------------------
        .p2align 4
        .global sma_fx_ready
        .type   sma_fx_ready, %function
sma_fx_ready:
        ldr     w10, [x0, #O_COUNT]
        ldr     w11, [x0, #O_WINDOW]
        cmp     w10, w11
        cset    w0, eq                      // w0 = (count == window) ? 1 : 0
        ret
        .size   sma_fx_ready, .-sma_fx_ready

// ---------------------------------------------------------------------------
// long long sma_fx_sum(const SmaFx *s)
//   x0 = s, returns x0 = s->sum
// ---------------------------------------------------------------------------
        .p2align 4
        .global sma_fx_sum
        .type   sma_fx_sum, %function
sma_fx_sum:
        ldr     x0, [x0, #O_SUM]
        ret
        .size   sma_fx_sum, .-sma_fx_sum

// ---------------------------------------------------------------------------
// double sma_fx_value(const SmaFx *s)
//   x0 = s, returns d0 = sum / window / PRICE_SCALE, or 0.0 if not ready
//   The ready check is done inline instead of calling sma_fx_ready.
// ---------------------------------------------------------------------------
        .p2align 4
        .global sma_fx_value
        .type   sma_fx_value, %function
sma_fx_value:
        ldr     w10, [x0, #O_COUNT]
        ldr     w11, [x0, #O_WINDOW]
        cmp     w10, w11                    // ready?
        b.eq    1f
        fmov    d0, xzr                     // return 0.0
        ret
1:
        ldr     x12, [x0, #O_SUM]
        scvtf   d0, x12                     // d0 = (double)sum
        scvtf   d1, w11                     // d1 = (double)window
        fdiv    d0, d0, d1                  // d0 = sum / window
        mov     x13, #PRICE_SCALE
        scvtf   d1, x13                     // d1 = (double)PRICE_SCALE
        fdiv    d0, d0, d1                  // d0 = sum / window / PRICE_SCALE
        ret
        .size   sma_fx_value, .-sma_fx_value

        .section .note.GNU-stack, "", %progbits
