// vwap_fx.S - running VWAP with fixed-point numbers, hand-written AArch64
//
// The same three functions as engine_fx/vwap_fx.c (see engine_fx/vwap_fx.h),
// so the C main program calls them without knowing they are assembly.
//
// AArch64 calling rules used in every file of this folder:
//   - whole-number arguments arrive in x0, x1, x2, x3 (w0.. = the low 32 bits)
//   - the result goes back in x0 (whole number) or d0 (double)
//   - x9 .. x17 are scratch registers a function may change freely
//   - "ret" jumps back to the caller (the return address is in x30)
// None of these functions calls another function or uses the stack.

        .text

// ---- the two running sums, private to this file --------------------------
// Kept side by side so ONE instruction (ldp / stp) loads or stores both.
        .bss
        .balign 8
vwap_sums:
        .skip   8                       // +0  sum_pq : running sum of price * qty
        .skip   8                       // +8  sum_q  : running sum of qty
        .text

// void vwap_fx_init(void)
// What:  set both sums to zero before the first trade
        .global vwap_fx_init
        .type   vwap_fx_init, %function
vwap_fx_init:
        adrp    x9, vwap_sums               // x9 = address of vwap_sums (top bits)
        add     x9, x9, :lo12:vwap_sums     //      ... plus the low 12 bits
        stp     xzr, xzr, [x9]              // sum_pq = 0, sum_q = 0 (xzr is always 0)
        ret

// void vwap_fx_update(long long price, long long qty)
//                     x0 = price       x1 = qty
// What:  sum_pq += price * qty;  sum_q += qty;
        .global vwap_fx_update
        .type   vwap_fx_update, %function
vwap_fx_update:
        adrp    x9, vwap_sums
        add     x9, x9, :lo12:vwap_sums
        ldp     x10, x11, [x9]              // x10 = sum_pq, x11 = sum_q
        madd    x10, x0, x1, x10            // x10 = price * qty + sum_pq (multiply-add)
        add     x11, x11, x1                // x11 = sum_q + qty
        stp     x10, x11, [x9]              // store both sums back
        ret

// double vwap_fx_value(void)
// What:  VWAP in USDT = sum_pq / sum_q / PRICE_SCALE, or 0.0 if sum_q <= 0
// Watch: the divisions are done in the same order as the C version, so the
//        printed value is identical down to the last digit.
        .global vwap_fx_value
        .type   vwap_fx_value, %function
vwap_fx_value:
        adrp    x9, vwap_sums
        add     x9, x9, :lo12:vwap_sums
        ldp     x10, x11, [x9]              // x10 = sum_pq, x11 = sum_q
        cmp     x11, #0
        b.le    vwap_no_quantity            // if (sum_q <= 0) return 0.0
        scvtf   d0, x10                     // d0 = (double)sum_pq
        scvtf   d1, x11                     // d1 = (double)sum_q
        fdiv    d0, d0, d1                  // d0 = sum_pq / sum_q
        mov     x12, #100                   // PRICE_SCALE (see engine_fx/fixed.h)
        scvtf   d1, x12
        fdiv    d0, d0, d1                  // d0 = d0 / PRICE_SCALE
        ret
vwap_no_quantity:
        fmov    d0, xzr                     // d0 = 0.0
        ret

        .section .note.GNU-stack,"",%progbits   // this code does not need an executable stack
