// sma_fx.S - running-sum SMA with fixed-point prices, hand-written AArch64
//
// The same five functions as engine_fx/sma_fx.c (see engine_fx/sma_fx.h).
// Calling rules: see the top of vwap_fx.S.
//
// Where this beats the compiler's version of sma_fx.c:
//   C writes   head = (head + 1) % window;
//   and the compiler MUST use a divide instruction (sdiv) for the %, because
//   it cannot know that head is always smaller than window. We know it is,
//   so "head + 1" can only be 1 .. window, and wrapping is just
//   "if it reached window, go back to 0": a compare and a select (csel).
//   A divide costs many times more than a compare on the Pi's Cortex-A76.
//   The two "if" statements become csel / cinc as well, so the function has
//   no branches at all - nothing for the CPU to mispredict.

        .text

// ---- the data, private to this file ---------------------------------------
// Everything for both windows sits in one block, so ONE address in x9
// reaches all of it. id is 0 (short) or 1 (long).
        .equ    MAX_WIN_FX, 256             // must match engine_fx/sma_fx.h
        .equ    WINDOW,  0                  // int window[2]   at +0  (+4 * id)
        .equ    HEAD,    8                  // int head[2]     at +8  (+4 * id)
        .equ    COUNT,  16                  // int count[2]    at +16 (+4 * id)
        .equ    SUM,    24                  // long long sum[2] at +24 (+8 * id)
        .equ    WIN,    40                  // long long win[2][256] at +40 (+2048 * id)

        .bss
        .balign 8
sma_data:
        .skip   WIN + 2 * MAX_WIN_FX * 8
        .text

// void sma_fx_init(int id, int size)
//                  w0 = id  w1 = size
// What:  empty window with the given size (1 if size is out of range)
        .global sma_fx_init
        .type   sma_fx_init, %function
sma_fx_init:
        adrp    x9, sma_data
        add     x9, x9, :lo12:sma_data
        add     x10, x9, w0, sxtw #2        // x10 = sma_data + 4 * id  (the int arrays)
        add     x11, x9, w0, sxtw #3        // x11 = sma_data + 8 * id  (the sum array)
        mov     w12, #1                     // the fallback size
        cmp     w1, #1
        csel    w1, w12, w1, lt             // if (size < 1)          size = 1
        cmp     w1, #MAX_WIN_FX
        csel    w1, w12, w1, gt             // if (size > MAX_WIN_FX) size = 1
        str     w1,  [x10, #WINDOW]         // window[id] = size
        str     wzr, [x10, #HEAD]           // head[id]   = 0
        str     wzr, [x10, #COUNT]          // count[id]  = 0
        str     xzr, [x11, #SUM]            // sum[id]    = 0
        ret

// void sma_fx_push(int id, long long price)
//                  w0 = id  x1 = price
// What:  add a price; once the window is full, drop the oldest one
        .global sma_fx_push
        .type   sma_fx_push, %function
sma_fx_push:
        adrp    x9, sma_data
        add     x9, x9, :lo12:sma_data
        add     x10, x9, w0, sxtw #2        // x10 = sma_data + 4 * id  (the int arrays)
        add     x11, x9, w0, sxtw #3        // x11 = sma_data + 8 * id  (the sum array)
        sxtw    x12, w0                     // x12 = id as a 64-bit number
        add     x12, x9, x12, lsl #11       // x12 = sma_data + 2048 * id
        add     x12, x12, #WIN              // x12 = &win[id][0]

        ldr     w13, [x10, #WINDOW]         // w13 = window[id]
        ldr     w14, [x10, #HEAD]           // w14 = head[id]
        ldr     w15, [x10, #COUNT]          // w15 = count[id]
        ldr     x16, [x11, #SUM]            // x16 = sum[id]
        ldr     x17, [x12, w14, sxtw #3]    // x17 = win[id][head], the oldest price

        // if (count == window) sum -= oldest;     if (count < window) count++;
        cmp     w15, w13
        csel    x17, x17, xzr, eq           // keep the oldest price only if the window is full
        cinc    w15, w15, lt                // count + 1 only if count < window
        sub     x16, x16, x17               // sum -= oldest (or -= 0)
        add     x16, x16, x1                // sum += price

        str     x1,  [x12, w14, sxtw #3]    // win[id][head] = price
        str     x16, [x11, #SUM]            // store sum[id]
        str     w15, [x10, #COUNT]          // store count[id]

        // head = (head + 1) % window, WITHOUT a divide
        add     w14, w14, #1                // head + 1
        cmp     w14, w13
        csel    w14, wzr, w14, eq           // if it reached window, wrap to 0
        str     w14, [x10, #HEAD]           // store head[id]
        ret

// int sma_fx_ready(int id)
//                  w0 = id
// What:  1 if count[id] == window[id], else 0
        .global sma_fx_ready
        .type   sma_fx_ready, %function
sma_fx_ready:
        adrp    x9, sma_data
        add     x9, x9, :lo12:sma_data
        add     x10, x9, w0, sxtw #2
        ldr     w13, [x10, #WINDOW]
        ldr     w15, [x10, #COUNT]
        cmp     w15, w13
        cset    w0, eq                      // w0 = (count == window) ? 1 : 0
        ret

// long long sma_fx_sum(int id)
//                      w0 = id
// What:  the exact window sum
        .global sma_fx_sum
        .type   sma_fx_sum, %function
sma_fx_sum:
        adrp    x9, sma_data
        add     x9, x9, :lo12:sma_data
        add     x11, x9, w0, sxtw #3
        ldr     x0, [x11, #SUM]             // return sum[id]
        ret

// double sma_fx_value(int id)
//                     w0 = id
// What:  average in USDT = sum / window / PRICE_SCALE, or 0.0 until ready
// Watch: same order of divisions as the C version, so it prints the same.
        .global sma_fx_value
        .type   sma_fx_value, %function
sma_fx_value:
        adrp    x9, sma_data
        add     x9, x9, :lo12:sma_data
        add     x10, x9, w0, sxtw #2
        add     x11, x9, w0, sxtw #3
        ldr     w13, [x10, #WINDOW]
        ldr     w15, [x10, #COUNT]
        cmp     w15, w13
        b.ne    sma_not_ready               // if (count != window) return 0.0
        ldr     x16, [x11, #SUM]
        scvtf   d0, x16                     // d0 = (double)sum
        scvtf   d1, w13                     // d1 = (double)window
        fdiv    d0, d0, d1                  // d0 = sum / window
        mov     x12, #100                   // PRICE_SCALE (see engine_fx/fixed.h)
        scvtf   d1, x12
        fdiv    d0, d0, d1                  // d0 = d0 / PRICE_SCALE
        ret
sma_not_ready:
        fmov    d0, xzr                     // d0 = 0.0
        ret

        .section .note.GNU-stack,"",%progbits
