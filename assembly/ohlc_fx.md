# How `ohlc_fx.s` works

`ohlc_fx.s` is a hand-written AArch64 (64-bit ARM) version of
`stageB/engine/ohlc_fx.c`. It builds **one-second OHLC bars**: for every
second of trading it records the **O**pen (first price), **H**igh, **L**ow and
**C**lose (last price), plus the number of trades.

It has the same two functions as the C file, with the same arguments and the
same struct layout, so the C program links against it without any changes:

| Function         | What it does                                              | Called       |
|------------------|-----------------------------------------------------------|--------------|
| `ohlc_fx_init`   | mark that no bar has started yet                          | once         |
| `ohlc_fx_update` | add one trade to the current bar, or close it and start a new one | every trade |

On the Raspberry Pi this file made the whole Stage B processing loop
**12.7 % faster** (see `README.md`).

---

## 1. ARM64 basics used in this file

| Thing                 | Meaning |
|-----------------------|---------|
| `x0` … `x30`          | 64-bit registers. `w0` … `w30` are the lower 32 bits of the same registers (used for `int`). |
| `xzr` / `wzr`         | the zero register: reads as 0. |
| `x0`–`x3`             | the function's arguments; `w0` also holds the `int` return value. |
| `x9` … `x15`          | scratch registers: free to overwrite. |
| `ldr` / `str`         | load / store one register. `[x0, #16]` means "address x0 + 16". |
| `ldp` / `stp`         | load / store a **pair** of registers from two neighbouring fields at once. |
| `mov` / `movk`        | put a constant in a register; `movk` fills in 16 more bits without touching the rest. |
| `cmp a, b`            | compare and set the flags. |
| `csel d, a, b, cond`  | `d = cond ? a : b`, **without branching**. |
| `msub d, a, b, c`     | `d = c - a * b` (multiply-subtract). |
| `sdiv d, a, b`        | `d = a / b` (signed division, rounds toward zero like C). Slow. |
| `cbz r, label`        | branch if `r` is zero. |
| `tbnz r, #bit, label` | branch if that bit of `r` is 1. Bit 63 is the sign bit. |
| `b.eq`, `b.hs`, …     | branch if equal, if higher-or-same (unsigned ≥), … |
| `ret`                 | return to the caller. |

---

## 2. The struct in memory

The C struct (from `ohlc_fx.h`):

```c
typedef struct {
    long long sec;      /* which second this bar covers */
    long long open;
    long long high;
    long long low;
    long long close;
    int       n;        /* number of trades in the second */
    int       active;   /* 1 once a bar has been started  */
} OhlcBarFx;
```

It takes exactly 48 bytes:

```
offset  0 ┌─────────────┬─────────────┐
          │ sec         │ open        │   8 + 8 bytes
       16 ├─────────────┼─────────────┤
          │ high        │ low         │   8 + 8 bytes
       32 ├─────────────┼──────┬──────┤
          │ close       │  n   │active│   8 + 4 + 4 bytes
       48 └─────────────┴──────┴──────┘
```

The fields come in neat 16-byte pairs, which is why the code can use
`ldp`/`stp` to handle two fields per instruction. The assembly repeats the
offsets as constants, because it can't read C headers:

```asm
        .equ    MICROS_PER_SEC, 1000000
        .equ    B_SEC,    0
        .equ    B_OPEN,   8
        .equ    B_HIGH,   16
        .equ    B_LOW,    24
        .equ    B_CLOSE,  32
        .equ    B_N,      40
        .equ    B_ACTIVE, 44
```

---

## 3. `ohlc_fx_init(OhlcBarFx *bar)`

**C equivalent:** set every field to 0.

```asm
ohlc_fx_init:
        stp     xzr, xzr, [x0, #B_SEC]      // sec = 0, open = 0
        stp     xzr, xzr, [x0, #B_HIGH]     // high = 0, low = 0
        stp     xzr, xzr, [x0, #B_CLOSE]    // close = 0, and n + active = 0
        ret
```

Three paired stores of zero clear all 48 bytes. The last one writes 8 bytes
of zero over `close`, and another 8 over `n` and `active` together.

---

## 4. `ohlc_fx_update(bar, time_us, price, finished)`

**C equivalent:**
```c
long long sec = time_us / 1000000;
if (!bar->active || sec != bar->sec) {
    if (bar->active) { *finished = *bar; closed = 1; }   /* close old bar */
    /* start a new bar: open = high = low = close = price, n = 1 */
    return closed;
}
if (price > bar->high) bar->high = price;
if (price < bar->low)  bar->low  = price;
bar->close = price;
bar->n++;
return 0;
```

Arguments: `x0 = bar`, `x1 = time_us`, `x2 = price`, `x3 = finished`.
Returns `w0 = 1` if a bar was closed (copied into `*finished`), else `0`.

The function has three paths:

```
                       ┌──────────────┐
                       │ load active, │
                       │ sec, 1e6     │
                       └──────┬───────┘
                 active == 0? │
            ┌─────yes─────────┤
            ▼                 ▼ no
     .Lfirst_bar      same second? (fast check)
            │           │yes            │no / unsure
            │           ▼               ▼
            │      .Lsame_bar      .Lcheck_slow
            │      (COMMON PATH)    exact division
            │      update bar        │       │
            │      return 0          │same   │different
            │                        ▼       ▼
            │                 .Lsame_bar   copy bar -> *finished
            │                                │
            └──────────────► .Lstart_bar ◄───┘
                             start a new bar, return closed
```

Almost every trade lands in the same second as the one before, so the
**common path** is what decides the speed. It is placed directly after the
checks so it runs straight through with no jumps.

### Step 1: setup

```asm
        mov     x11, #(MICROS_PER_SEC & 0xffff)          // low 16 bits:  0x4240
        movk    x11, #(MICROS_PER_SEC >> 16), lsl #16    // next 16 bits: 0x000F
                                                         // x11 = 0xF4240 = 1,000,000
        ldr     w9,  [x0, #B_ACTIVE]
        ldr     x10, [x0, #B_SEC]
        cbz     w9, .Lfirst_bar             // no bar yet -> start the first one
```

An ARM instruction can only hold a 16-bit constant, so 1,000,000 is built in
two pieces with `mov` + `movk`.

### Step 2: "is this trade still in the same second?" without dividing

The C code divides on every trade: `sec = time_us / 1000000`. Division is one
of the slowest instructions on the CPU. The assembly asks the question a
different way:

```
time is in second S   <=>   S × 1,000,000  <=  time  <  S × 1,000,000 + 1,000,000
```

```asm
        tbnz    x10, #63, .Lcheck_slow      // bar->sec negative? -> use the exact path
        msub    x12, x10, x11, x1           // x12 = time - bar->sec * 1,000,000
        cmp     x12, x11
        b.hs    .Lcheck_slow                // x12 >= 1,000,000 (unsigned) -> different second
```

**The unsigned trick:** `x12` is the distance from the start of the current
second. Two things can go wrong: the trade is a second or more later
(`x12 ≥ 1,000,000`), or it is *earlier* (`x12` negative). `b.hs` compares
*unsigned*, and a negative number viewed as unsigned is enormous (−1 becomes
18,446,744,073,709,551,615), so **one** comparison catches both cases.

**The `tbnz` line:** for negative times, C's division rounds toward zero, so
the range rule above doesn't hold. Bit 63 is the sign bit, so `tbnz` sends any
negative second down the exact division path. Real timestamps are never
negative, so this branch is never taken, but it keeps the file exactly
equivalent to the C code.

### Step 3: the common path, updating the current bar

```asm
.Lsame_bar:
        ldp     x12, x13, [x0, #B_HIGH]     // x12 = high, x13 = low (one instruction)
        cmp     x2, x12
        csel    x12, x2, x12, gt            // high = (price > high) ? price : high
        cmp     x2, x13
        csel    x13, x2, x13, lt            // low  = (price < low)  ? price : low
        stp     x12, x13, [x0, #B_HIGH]     // write both back (one instruction)
        ldr     w12, [x0, #B_N]
        add     w12, w12, #1
        str     x2,  [x0, #B_CLOSE]         // close = price
        str     w12, [x0, #B_N]             // n++
        mov     w0, #0                      // return 0: no bar closed
        ret
```

**This is where most of the speed-up comes from.** The C compiler turns
`if (price > high)` into a *branch*. The CPU guesses which way each branch
goes before it knows the answer, and because prices move up and down
randomly, it guesses wrong often. Every wrong guess throws away roughly 11
cycles of work on the Pi 5's Cortex-A76.

`csel` picks one of two values **without branching**, so there is nothing to
guess. The catch is that `high` and `low` are always written back, even when
they didn't change. A C compiler is not allowed to add memory writes the
program didn't ask for, so it can't make this change itself. In assembly we
know the bar belongs only to us, so the extra write is harmless.

### Step 4: the rare path, a new second

Reached only when the fast check says "not the same second" (or the second
is negative). It now divides once, exactly like the C code:

```asm
.Lcheck_slow:
        sdiv    x9, x1, x11                 // sec = time_us / 1,000,000
        cmp     x9, x10
        b.eq    .Lsame_bar                  // same second after all (negative times only)
```

If the second really changed, the finished bar is copied out, 16 bytes at a
time:

```asm
        ldp     x12, x13, [x0, #0]          // sec, open
        stp     x12, x13, [x3, #0]
        ldp     x12, x13, [x0, #16]         // high, low
        stp     x12, x13, [x3, #16]
        ldp     x12, x13, [x0, #32]         // close, then n + active as one 8-byte value
        stp     x12, x13, [x3, #32]
        mov     w14, #1                     // closed = 1
        b       .Lstart_bar
```

### Step 5: the very first trade

```asm
.Lfirst_bar:
        sdiv    x9, x1, x11                 // sec = time_us / 1,000,000
        mov     w14, #0                     // closed = 0 (there was no previous bar)
```

### Step 6: start a new bar

Both the "first trade" and "second changed" paths end here:

```asm
.Lstart_bar:
        stp     x9, x2, [x0, #B_SEC]        // sec = new second, open = price
        stp     x2, x2, [x0, #B_HIGH]       // high = low = price
        mov     x12, #0x100000001           // n = 1 AND active = 1, in one register
        stp     x2, x12, [x0, #B_CLOSE]     // close = price, then n and active
        mov     w0, w14                     // return closed (0 or 1)
        ret
```

**Setting `n` and `active` together:** they are two 4-byte `int`s next to
each other at offsets 40 and 44. ARM is *little-endian*: when a 64-bit value
is stored, its low 32 bits go to the lower address. So storing
`0x00000001_00000001` writes `1` to `n` (offset 40) and `1` to `active`
(offset 44) at once. The whole new bar takes 3 stores instead of 7.

---

## 5. Worked example

Four trades (times in microseconds, prices in cents):

| Trade | time_us   | price | Path taken | Why | Bar after (sec, O, H, L, C, n) | Returns |
|-------|-----------|-------|------------|-----|-------------------------------|---------|
| 1 | 5,000,000 | 100 | first bar   | `active = 0` | (5, 100, 100, 100, 100, 1) | 0 |
| 2 | 5,400,000 | 120 | **same bar** | 5,400,000 − 5×1,000,000 = 400,000 < 1,000,000 | (5, 100, **120**, 100, 120, 2) | 0 |
| 3 | 5,900,000 |  90 | **same bar** | 900,000 < 1,000,000 | (5, 100, 120, **90**, 90, 3) | 0 |
| 4 | 6,100,000 | 110 | new second  | 1,100,000 ≥ 1,000,000 → slow path, 6,100,000 / 1,000,000 = 6 ≠ 5 | (6, 110, 110, 110, 110, 1) | **1** |

After trade 4, `*finished` holds the completed bar for second 5:
`(5, 100, 120, 90, 90, 3)`. The C program prints it as one OHLC row.

Only trades 1 and 4 needed a division. In real data, most trades take the
"same bar" path, which has no division and no unpredictable branches.

---

## 6. Things to keep in mind

- **Link either `ohlc_fx.c` or `ohlc_fx.s`, never both**, or the linker
  reports duplicate symbols.
- **If `MICROS_PER_SEC` or the `OhlcBarFx` struct changes in the C
  headers**, update the `.equ` lines at the top of this file too.
- **The last bar is never closed by this function**, the same as in the C
  version. The caller prints the final bar after the loop.
- **Correctness:** tested in an ARM64 emulator against the C version
  (450,000 checks, 0 failures, including negative times and large time
  jumps), and on the Pi the program's output is identical to the C build.
