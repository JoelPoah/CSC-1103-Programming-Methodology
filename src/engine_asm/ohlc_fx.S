// ohlc_fx.S - one-second OHLC bars with fixed-point prices, hand-written AArch64
//
// The same functions as engine_fx/ohlc_fx.c (see engine_fx/ohlc_fx.h).
// Calling rules: see the top of vwap_fx.S.
//
// Two things done differently from the compiler's version of ohlc_fx.c:
//   - high and low are updated with compare + select (csel), no branches
//   - time / 1,000,000 uses the multiply trick below, like the compiler,
//     but skips the fix-up for negative numbers: a timestamp is never negative

        .text

// ---- the data, private to this file ---------------------------------------
// the bar being built right now
        .equ    BAR_SEC,     0
        .equ    BAR_OPEN,    8
        .equ    BAR_HIGH,   16
        .equ    BAR_LOW,    24
        .equ    BAR_CLOSE,  32
        .equ    BAR_N,      40              // int
        .equ    BAR_ACTIVE, 44              // int: 1 once a bar has been started
// the last finished bar, ready to print (same order as above)
        .equ    FIN_SEC,    48
        .equ    FIN_OPEN,   56
        .equ    FIN_HIGH,   64
        .equ    FIN_LOW,    72
        .equ    FIN_CLOSE,  80
        .equ    FIN_N,      88              // int

        .bss
        .balign 8
ohlc_data:
        .skip   96
        .text

// void ohlc_fx_init(void)
// What:  mark that no bar has started yet
        .global ohlc_fx_init
        .type   ohlc_fx_init, %function
ohlc_fx_init:
        adrp    x9, ohlc_data
        add     x9, x9, :lo12:ohlc_data
        stp     xzr, xzr, [x9, #BAR_SEC]    // sec = 0, open = 0
        stp     xzr, xzr, [x9, #BAR_HIGH]   // high = 0, low = 0
        stp     xzr, xzr, [x9, #BAR_CLOSE]  // close = 0, n = 0 and active = 0
        ret

// int ohlc_fx_update(long long time_us, long long price)
//                    x0 = time_us       x1 = price
// What:    put one trade into the current bar, or start a new bar
// Returns: 1 if a bar was closed by this trade (copied to FIN_*), else 0
        .global ohlc_fx_update
        .type   ohlc_fx_update, %function
ohlc_fx_update:
        adrp    x9, ohlc_data
        add     x9, x9, :lo12:ohlc_data

        // x10 = time_us / 1,000,000 without a divide instruction:
        // multiply by 2^82 / 1,000,000 (rounded up), keep the top 64 bits of
        // the 128-bit answer (smulh), then shift right by 18 more (64 + 18 = 82).
        // Exact for every time_us from 0 up to the largest long long.
        mov     x10, #0x34DB
        movk    x10, #0xD7B6, lsl #16
        movk    x10, #0xDE82, lsl #32
        movk    x10, #0x431B, lsl #48       // x10 = 0x431BDE82D7B634DB
        smulh   x10, x0, x10
        asr     x10, x10, #18               // x10 = current second

        ldr     w11, [x9, #BAR_ACTIVE]
        mov     w0, #0                      // "no bar closed" unless changed below
        cbz     w11, ohlc_start_bar         // no bar yet -> start the first one

        ldr     x12, [x9, #BAR_SEC]
        cmp     x10, x12
        b.ne    ohlc_finish_bar             // a new second -> close the old bar

        // ---- same second: update high, low, close and the count ----
        ldp     x13, x14, [x9, #BAR_HIGH]   // x13 = high, x14 = low
        ldr     w15, [x9, #BAR_N]
        cmp     x1, x13
        csel    x13, x1, x13, gt            // high = (price > high) ? price : high
        cmp     x1, x14
        csel    x14, x1, x14, lt            // low  = (price < low)  ? price : low
        add     w15, w15, #1                // n++
        stp     x13, x14, [x9, #BAR_HIGH]
        str     x1,  [x9, #BAR_CLOSE]       // close = price
        str     w15, [x9, #BAR_N]
        ret                                 // w0 is still 0

ohlc_finish_bar:
        // ---- copy the current bar to the finished bar, two values at a time ----
        ldp     x13, x14, [x9, #BAR_SEC]    // sec, open
        stp     x13, x14, [x9, #FIN_SEC]
        ldp     x13, x14, [x9, #BAR_HIGH]   // high, low
        stp     x13, x14, [x9, #FIN_HIGH]
        ldr     x13, [x9, #BAR_CLOSE]
        str     x13, [x9, #FIN_CLOSE]
        ldr     w13, [x9, #BAR_N]
        str     w13, [x9, #FIN_N]
        mov     w0, #1                      // a bar was closed

ohlc_start_bar:
        // ---- start a new bar: open = high = low = close = price, n = 1 ----
        mov     w11, #1
        stp     x10, x1, [x9, #BAR_SEC]     // sec, open
        stp     x1,  x1, [x9, #BAR_HIGH]    // high, low
        str     x1,  [x9, #BAR_CLOSE]
        str     w11, [x9, #BAR_N]           // n = 1
        str     w11, [x9, #BAR_ACTIVE]      // active = 1
        ret                                 // w0 = 1 if we came through ohlc_finish_bar

// int ohlc_fx_flush(void)
// What:    finish the last bar after the final trade
// Returns: 1 if a bar was active (copied to FIN_*), else 0
        .global ohlc_fx_flush
        .type   ohlc_fx_flush, %function
ohlc_fx_flush:
        adrp    x9, ohlc_data
        add     x9, x9, :lo12:ohlc_data
        ldr     w0, [x9, #BAR_ACTIVE]
        cbz     w0, ohlc_flush_done         // no bar -> return 0
        ldp     x13, x14, [x9, #BAR_SEC]
        stp     x13, x14, [x9, #FIN_SEC]
        ldp     x13, x14, [x9, #BAR_HIGH]
        stp     x13, x14, [x9, #FIN_HIGH]
        ldr     x13, [x9, #BAR_CLOSE]
        str     x13, [x9, #FIN_CLOSE]
        ldr     w13, [x9, #BAR_N]
        str     w13, [x9, #FIN_N]
        str     wzr, [x9, #BAR_ACTIVE]      // active = 0
        mov     w0, #1
ohlc_flush_done:
        ret

// ---- the most recently finished bar: one load each ----
        .global ohlc_fx_finished_sec
        .type   ohlc_fx_finished_sec, %function
ohlc_fx_finished_sec:
        adrp    x9, ohlc_data
        add     x9, x9, :lo12:ohlc_data
        ldr     x0, [x9, #FIN_SEC]
        ret

        .global ohlc_fx_finished_open
        .type   ohlc_fx_finished_open, %function
ohlc_fx_finished_open:
        adrp    x9, ohlc_data
        add     x9, x9, :lo12:ohlc_data
        ldr     x0, [x9, #FIN_OPEN]
        ret

        .global ohlc_fx_finished_high
        .type   ohlc_fx_finished_high, %function
ohlc_fx_finished_high:
        adrp    x9, ohlc_data
        add     x9, x9, :lo12:ohlc_data
        ldr     x0, [x9, #FIN_HIGH]
        ret

        .global ohlc_fx_finished_low
        .type   ohlc_fx_finished_low, %function
ohlc_fx_finished_low:
        adrp    x9, ohlc_data
        add     x9, x9, :lo12:ohlc_data
        ldr     x0, [x9, #FIN_LOW]
        ret

        .global ohlc_fx_finished_close
        .type   ohlc_fx_finished_close, %function
ohlc_fx_finished_close:
        adrp    x9, ohlc_data
        add     x9, x9, :lo12:ohlc_data
        ldr     x0, [x9, #FIN_CLOSE]
        ret

        .global ohlc_fx_finished_n
        .type   ohlc_fx_finished_n, %function
ohlc_fx_finished_n:
        adrp    x9, ohlc_data
        add     x9, x9, :lo12:ohlc_data
        ldr     w0, [x9, #FIN_N]
        ret

        .section .note.GNU-stack,"",%progbits
