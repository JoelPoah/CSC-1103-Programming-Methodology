// v8_ohlc_fx.s - AArch64 (64-bit ARM) version of engine_fx/v5-v7_ohlc_fx.c (Yong Sheng, V8)
//
// Copied unchanged from assembly/ohlc_fx.s, apart from this header.
// V8 = V7 with this file linked instead of v5-v7_ohlc_fx.c, so any speed
// difference comes from the assembly alone. assembly/sma_fx.s is NOT used:
// on the Pi it ran slower than the C sma_fx.c (assembly/out_compare_all.txt).
// AArch64 Linux only (the Raspberry Pi): "make ladder" builds V8 there.
//
//
// One-second OHLC bars with fixed-point prices.
// Drop-in replacement for ohlc_fx.c: same function names, same OhlcBarFx layout.
//
// Build:  gcc -c ohlc_fx.s -o ohlc_fx.o      (on a 64-bit Raspberry Pi OS)
//
// Calling convention (AAPCS64):
//   x0-x7   integer/pointer arguments, w0/x0 = integer return value
//   x9-x15  scratch registers (free to use, not preserved)
// Both functions are leaf functions (they call nothing), so neither needs
// a stack frame.
//
// Speed-ups over the compiler's version of ohlc_fx.c:
//   1. No division on the common path. "Is this trade still in the same
//      second?" is answered with one multiply-subtract and an unsigned
//      compare; the real division only runs when a new bar starts.
//   2. No data-dependent branches on the common path. high/low are updated
//      with csel (branch-free max/min), so random price moves can't cause
//      branch mispredictions. A C compiler won't do this itself because it
//      would mean always writing high and low back to memory.
//   3. Paired loads/stores (ldp/stp) for neighbouring fields.

// ---------------------------------------------------------------------------
// CONSTANTS - This section involve assigning the number a name (eg. line 27: assign 1000000 to MICROS_PER_SEC)
// ---------------------------------------------------------------------------
        .equ    MICROS_PER_SEC, 1000000     // <-- check against ohlc.h

// OhlcBarFx layout (48 bytes):
// Similar to creating a struct, this assigns the memory address based on the size(bytes) of datatype. 
// (eg. address of B_SEC is 1000, address of B_OPEN is 1008, ..., B_ACTIVE is 1044)
        .equ    B_SEC,    0                 // long long sec
        .equ    B_OPEN,   8                 // long long open
        .equ    B_HIGH,   16                // long long high
        .equ    B_LOW,    24                // long long low
        .equ    B_CLOSE,  32                // long long close
        .equ    B_N,      40                // int n
        .equ    B_ACTIVE, 44                // int active


//  program code below
        .text   

// ---------------------------------------------------------------------------
//  ohlc_fx_init function:
// ---------------------------------------------------------------------------
        .p2align 4                          // line the function up on a 16 bytes division (2^4), 
                                            // CPU reads in fewer chunks, provide small speed-up but it is optional
        .global ohlc_fx_init                // visible to C files to be called
        .type   ohlc_fx_init, %function     // declare function

ohlc_fx_init:
        // stp (store pair: writes two registers), xzr (register of zero) set B_SEC, ..., B_ACTIVE as zeroed
        // x0 holds the address of the bar struct, xzr is 8 byte, hence, 
        // B_SEC will be zeroed and the next 8 byte will be zeroed which is B_OPEN
        stp     xzr, xzr, [x0, #B_SEC]      // sec = 0, open = 0
        stp     xzr, xzr, [x0, #B_HIGH]     // high = 0, low = 0
        stp     xzr, xzr, [x0, #B_CLOSE]    // close = 0, n = 0 and active = 0
        ret
        .size   ohlc_fx_init, .-ohlc_fx_init  // record how long the function is

// ---------------------------------------------------------------------------
//   ohlc_fx_update function: 
//   !!! TAKE NOTE OF THESE REGISTER USAGE !!!
//   x0 = bar
//   x1 = time_us
//   x2 = price
//   x3 = finished
//   returns w0 = 1 if the previous bar was closed and copied to *finished
//
//   Register use:
//     w9  = bar->active, later sec     
//     x10 = bar->sec
//     x11 = 1,000,000
//     x12, x13 = temporary
//     w14 = closed (return value on the new-bar path)
// ---------------------------------------------------------------------------
        .p2align 4
        .global ohlc_fx_update
        .type   ohlc_fx_update, %function

ohlc_fx_update:
        mov     x11, #(MICROS_PER_SEC & 0xffff)         // 1,000,000 needs 20 bits, one instruction can only carry 16-bit, there is 4 bit missing.
                                                        // bitwise mask, as 0xffff is 16 bit, this keep the bottom 16 bit (16,960) and toss the remainder
        movk    x11, #(MICROS_PER_SEC >> 16), lsl #16   // movk (move and keep) stores the remaining missing top 4 bit (983,040) by 'logical shift left'
                                                        // thus, x11 = 983,040 + 16,690 = 1,000,000 
        ldr     w9,  [x0, #B_ACTIVE]
        ldr     x10, [x0, #B_SEC]
        cbz     w9, .Lfirst_bar         // compare and branch if zero. (jumps to branch .Lfirst_bar if no bar exist yet)



        // Same second?  sec == bar->sec  <=>  bar->sec*1e6 <= time < bar->sec*1e6 + 1e6
        // x12 = time - bar->sec*1e6; one UNSIGNED compare checks both bounds,
        // because a negative difference looks like a huge unsigned number.
        // That equivalence needs bar->sec >= 0 (C rounds negative times toward
        // zero), so a negative second takes the exact division path instead.

        // slight speed up compared to c program
        tbnz    x10, #63, .Lcheck_slow      // tbnz: test bit and branch if not zero. x10 holds bar->sec
                                            // the sign bit is bit 63, 1 (negative) or 0 (positive)
                                            // if negative jump to .Lcheck_slow (to perform the division)
                                            // division is slowest basic operation, hence, avoid if possible to improve the speed.
        // this is for fast check
        msub    x12, x10, x11, x1           // multiply then subtract: x12 = time(x1) - bar->sec(x10) * 1,000,000(x11)
                                            // how many microsec the trade is past the start of the bar's second
        cmp     x12, x11                    // compare x12 and x11 (1,000,000)
        b.hs    .Lcheck_slow                // branch if higher or same, jump to .Lcheck_slow if 1,000,000 or higher.


        // ---- frequent path: same second, update the current bar ----
.Lsame_bar:
        ldp     x12, x13, [x0, #B_HIGH]     // load pair: grabs high and low together, since they sit next to each other in the struct (offsets 16 and 24)
                                            // x12 = high, x13 = low
        //compare high
        cmp     x2, x12                     // compare price and high
        csel    x12, x2, x12, gt            // if price is greater than high then high = max(high, price), set the new high
        
        //compare low 
        cmp     x2, x13                     // compare price and low
        csel    x13, x2, x13, lt            // if price is lesser than low then low  = min(low, price)

        stp     x12, x13, [x0, #B_HIGH]     // store pair: write the new? high and low back into the bar
        ldr     w12, [x0, #B_N]             // load n (trade count) into w12 (w is for 4 bytes data)
        add     w12, w12, #1                // increment n by 1
        str     x2,  [x0, #B_CLOSE]         // close = price (the latest trade is set as the close)
        str     w12, [x0, #B_N]             // store (n + 1) into the bar
        mov     w0, #0                      // return 0: no bar closed
        ret


        // ---- rare path: work out the second exactly ----
.Lcheck_slow:  // when the fast check shows not equal or bar->sec is negative
        sdiv    x9, x1, x11                 // signed division: sec = time_us / MICROS_PER_SEC
                                            // rounds towards zero
        cmp     x9, x10
        b.eq    .Lsame_bar                  // jumps to .Lsame_bar if equivant to be updated
                                            // (only possible for negative-times edge case)

        // second changed: *finished = *bar (copy all 48 bytes, 16 at a time)
        ldp     x12, x13, [x0, #0]          // load pair from bar (x0), bytes 0-15: x12 = sec, x13 = open
        stp     x12, x13, [x3, #0]          // store pair into finished (x3), bytes 0-15
        ldp     x12, x13, [x0, #16]         // bytes 16-31: x12 = high, x13 = low
        stp     x12, x13, [x3, #16]         // store pair into finished
        ldp     x12, x13, [x0, #32]         // bytes 32-47: x12 = close, x13 = n + active (4bytes + 4bytes)
        stp     x12, x13, [x3, #32]         // store pair into finished
        mov     w14, #1                     // closed = 1: finished bar was copied
        b       .Lstart_bar                 // jumps to .Lstart_bar, skipping .Lfirst_bar


.Lfirst_bar:  // no bar exist yet, (bar->active = 0)
        sdiv    x9, x1, x11                 // signed division: sec = time_us / MICROS_PER_SEC
        mov     w14, #0                     // closed = 0: no previous bar was closed, proceed to .Lstart_bar


.Lstart_bar:  // start a new bar with this trade
        stp     x9, x2, [x0, #B_SEC]        // store pair: x9 = sec, open = price
        stp     x2, x2, [x0, #B_HIGH]       // store pair: high = low = price (first trade of the second)
        mov     x12, #0x100000001           // low word n = 1, high word active = 1
                                            // x12 holds two 4-byte 1s. ARM stores the lower half at the lower address (little-endian),
                                            // so the lower half lands on n (byte 40) and the upper half on active (byte 44)
        stp     x2, x12, [x0, #B_CLOSE]     // store pair: close = price, n = 1, active = 1
        mov     w0, w14                     // return value = closed (1=bar finished, 0=first bar)
        ret
        .size   ohlc_fx_update, .-ohlc_fx_update // housekeeping: records the function's size in bytes (current position "." minus the start of the function)

        .section .note.GNU-stack, "", %progbits  // tells the linker this code does not need an executable stack (does nothing at run time)
