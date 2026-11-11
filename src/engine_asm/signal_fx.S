// signal_fx.S - crossover BUY / SELL signals from window sums, hand-written AArch64
//
// The same four functions as engine_fx/signal_fx.c (see engine_fx/signal_fx.h).
// Calling rules: see the top of vwap_fx.S.

        .text

        .equ    SIG_BUY,   1                // must match engine/signal.h
        .equ    SIG_SELL, -1
        .equ    SIG_NONE,  0

// ---- the data, private to this file ---------------------------------------
        .equ    PREV_DIFF, 0                // long long prev_diff
        .equ    HAVE_PREV, 8                // int have_prev
        .equ    BUYS,      12               // int buys
        .equ    SELLS,     16               // int sells

        .bss
        .balign 8
signal_data:
        .skip   24
        .text

// void crossover_fx_init(void)
// What:  no previous diff yet, zero counts
        .global crossover_fx_init
        .type   crossover_fx_init, %function
crossover_fx_init:
        adrp    x9, signal_data
        add     x9, x9, :lo12:signal_data
        str     xzr, [x9, #PREV_DIFF]
        str     wzr, [x9, #HAVE_PREV]
        str     wzr, [x9, #BUYS]
        str     wzr, [x9, #SELLS]
        ret

// int crossover_fx_update(long long sum_short, int short_win,
//                         long long sum_long,  int long_win)
//                         x0 = sum_short  w1 = short_win
//                         x2 = sum_long   w3 = long_win
// What:  diff = sum_short * long_win - sum_long * short_win
//        BUY  if prev_diff <= 0 and diff > 0
//        SELL if prev_diff >= 0 and diff < 0
        .global crossover_fx_update
        .type   crossover_fx_update, %function
crossover_fx_update:
        adrp    x9, signal_data
        add     x9, x9, :lo12:signal_data
        sxtw    x1, w1                      // widen the two ints to 64 bits
        sxtw    x3, w3
        mul     x10, x0, x3                 // x10 = sum_short * long_win
        msub    x10, x2, x1, x10            // x10 = x10 - sum_long * short_win  = diff

        ldr     x11, [x9, #PREV_DIFF]       // x11 = prev_diff
        ldr     w12, [x9, #HAVE_PREV]       // w12 = have_prev
        str     x10, [x9, #PREV_DIFF]       // prev_diff = diff   (remember for next call)
        mov     w13, #1
        str     w13, [x9, #HAVE_PREV]       // have_prev = 1

        mov     w0, #SIG_NONE               // the answer unless a test below passes
        cbz     w12, crossover_done         // no previous diff yet -> SIG_NONE

        cmp     x10, #0
        b.gt    crossover_maybe_buy         // diff > 0
        b.lt    crossover_maybe_sell        // diff < 0
        ret                                 // diff == 0 -> SIG_NONE

crossover_maybe_buy:
        cmp     x11, #0
        b.gt    crossover_done              // prev_diff > 0: was already above
        ldr     w13, [x9, #BUYS]
        add     w13, w13, #1
        str     w13, [x9, #BUYS]            // buys++
        mov     w0, #SIG_BUY
        ret

crossover_maybe_sell:
        cmp     x11, #0
        b.lt    crossover_done              // prev_diff < 0: was already below
        ldr     w13, [x9, #SELLS]
        add     w13, w13, #1
        str     w13, [x9, #SELLS]           // sells++
        mov     w0, #SIG_SELL
crossover_done:
        ret

// int crossover_fx_buys(void)
        .global crossover_fx_buys
        .type   crossover_fx_buys, %function
crossover_fx_buys:
        adrp    x9, signal_data
        add     x9, x9, :lo12:signal_data
        ldr     w0, [x9, #BUYS]
        ret

// int crossover_fx_sells(void)
        .global crossover_fx_sells
        .type   crossover_fx_sells, %function
crossover_fx_sells:
        adrp    x9, signal_data
        add     x9, x9, :lo12:signal_data
        ldr     w0, [x9, #SELLS]
        ret

        .section .note.GNU-stack,"",%progbits
