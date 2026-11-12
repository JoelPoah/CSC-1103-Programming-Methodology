# How `sma_fx.s` works

`sma_fx.s` is a hand-written AArch64 (64-bit ARM) version of
`stageB/engine/sma_fx.c`. It computes a **simple moving average (SMA)**, the
average of the last *N* prices, using fixed-point prices (cents stored as
`long long`).

It has the same five functions as the C file, with the same arguments and the
same struct layout, so the C program links against it without any changes:

| Function       | What it does                                         | Called            |
|----------------|------------------------------------------------------|-------------------|
| `sma_fx_init`  | set up an empty window of size N                     | once per SMA      |
| `sma_fx_push`  | add a new price, drop the oldest one                 | every trade       |
| `sma_fx_ready` | has the window filled up yet?                        | every trade       |
| `sma_fx_sum`   | the exact integer sum of the window                  | every trade (after warm-up) |
| `sma_fx_value` | the average in USDT, for printing                    | only when printing |

---

## 1. The idea: circular buffer + running sum

The SMA keeps the last N prices in an array used as a **circular buffer**:
`head` points at the slot to overwrite next, and when it reaches the end it
wraps back to 0. Instead of adding up all N prices every time, it keeps a
**running sum**:

```
new sum = old sum + newest price - oldest price
```

That is one add and one subtract per trade, however big N is.

---

## 2. ARM64 basics used in this file

| Thing                 | Meaning |
|-----------------------|---------|
| `x0` … `x30`          | 64-bit registers. `w0` … `w30` are the lower 32 bits of the same registers (used for `int`). |
| `xzr` / `wzr`         | the zero register: reads as 0, writes are thrown away. |
| `x0`, `x1`, …         | function arguments arrive here; an integer return value goes back in `x0`/`w0`. |
| `d0`, `d1`            | floating-point registers; a `double` return value goes in `d0`. |
| `x9` … `x15`          | scratch registers: a function may overwrite them freely. |
| `x19` … `x28`         | callee-saved: if a function uses one, it must restore it before returning. |
| `x29`, `x30`          | frame pointer and link register (`x30` holds the return address). |
| `ldr` / `str`         | load from / store to memory. `[x0, #1032]` means "address x0 + 1032". |
| `ldrsw`               | load a 32-bit `int` and sign-extend it to 64 bits. |
| `[x0, x11, lsl #3]`   | address `x0 + x11 * 8` (`lsl #3` = shift left 3 = multiply by 8). |
| `cmp a, b`            | compare and set flags; the next conditional instruction reads them. |
| `b.ne`, `b.ge`, `b.lt`, … | branch if not-equal, greater-or-equal, less-than, … |
| `csel d, a, b, cond`  | `d = cond ? a : b`, without branching. |
| `cset d, cond`        | `d = cond ? 1 : 0`. |
| `ret`                 | return to the address in `x30`. |
| `.equ NAME, value`    | an assembler constant, like `#define`. |

---

## 3. The struct in memory

The C struct (from `sma_fx.h`):

```c
typedef struct {
    long long win[MAX_WIN];  /* circular buffer of prices   */
    int       window;        /* N                           */
    int       head;          /* next slot to overwrite      */
    int       count;         /* prices seen, capped at N    */
    long long sum;           /* exact sum of the window     */
} SmaFx;
```

With `MAX_WIN = 128`, the bytes are laid out like this:

```
offset    0 ┌──────────────────────────────┐
          │ win[0]   (8 bytes)           │
        8 │ win[1]                       │
          │ ...                          │
     1016 │ win[127]                     │
     1024 ├──────────────────────────────┤
          │ window  (4 bytes)            │  O_WINDOW = 1024
     1028 │ head    (4 bytes)            │  O_HEAD   = 1028
     1032 │ count   (4 bytes)            │  O_COUNT  = 1032
     1036 │ (padding, 4 bytes)           │  so sum starts on an 8-byte boundary
     1040 │ sum     (8 bytes)            │  O_SUM    = 1040
     1048 └──────────────────────────────┘
```

The assembly can't read C headers, so the top of the file repeats these
numbers as constants:

```asm
        .equ    MAX_WIN,     128
        .equ    PRICE_SCALE, 100
        .equ    O_WINDOW, MAX_WIN * 8       // 1024
        .equ    O_HEAD,   O_WINDOW + 4      // 1028
        .equ    O_COUNT,  O_WINDOW + 8      // 1032
        .equ    O_SUM,    O_WINDOW + 16     // 1040
```

A safety check stops the build if `MAX_WIN` gets so large that these offsets
no longer fit inside a load/store instruction:

```asm
        .if     O_COUNT > 16380
        .error  "MAX_WIN too large: field offsets no longer fit in ldr/str"
        .endif
```

---

## 4. Function by function

### `sma_fx_init(SmaFx *s, int window)`

**C equivalent:**
```c
s->head = 0; s->count = 0; s->sum = 0; s->window = window;
if (window < 1 || window > MAX_WIN) {
    printf("SMA INIT FAILED: window size out of bounds\n");
    s->window = 1;
}
```

This is the only function that calls another function (`printf`), so it is
the only one that needs a **stack frame**:

```asm
sma_fx_init:
        stp     x29, x30, [sp, #-32]!       // 1. reserve 32 bytes of stack and save fp + return address
        mov     x29, sp
        str     x19, [sp, #16]              // 2. save x19, because we're about to use it
        mov     x19, x0                     // 3. x19 = s
```

1. `printf` will overwrite `x30` (the return address), so it is saved first.
2. `printf` may also overwrite `x0`–`x18`. The pointer `s` is needed again
   after the call, so it is kept in `x19`, which `printf` must preserve. Using
   `x19` means saving our caller's value of it first.

Then the fields are cleared and the window size is checked:

```asm
        str     wzr, [x19, #O_HEAD]         // head  = 0
        str     wzr, [x19, #O_COUNT]        // count = 0
        str     xzr, [x19, #O_SUM]          // sum   = 0   (64-bit zero)
        str     w1,  [x19, #O_WINDOW]       // window = window argument

        cmp     w1, #1
        b.lt    .Linit_bad                  // window < 1        -> bad
        cmp     w1, #MAX_WIN
        b.le    .Linit_done                 // window <= MAX_WIN -> fine, skip the error
```

On a bad size it prints the message and uses a safe window of 1, so that
`sma_fx_push` never works with a window of 0:

```asm
.Linit_bad:
        adrp    x0, msg_fail                // x0 = address of the message string
        add     x0, x0, :lo12:msg_fail      //   (built in two steps: page + offset)
        bl      printf                      // call printf; bl saves the return address in x30
        mov     w10, #1
        str     w10, [x19, #O_WINDOW]       // window = 1
```

Finally, everything that was saved is restored, in reverse order:

```asm
.Linit_done:
        ldr     x19, [sp, #16]              // restore the caller's x19
        ldp     x29, x30, [sp], #32         // restore fp + return address, free the 32 bytes
        ret
```

---

### `sma_fx_push(SmaFx *s, long long price)`: the hot path

**C equivalent:**
```c
if (s->count == s->window) s->sum -= s->win[s->head];   /* drop oldest   */
s->win[s->head] = price;                                /* store newest  */
s->sum += price;
s->head = (s->head + 1) % s->window;                    /* move forward  */
if (s->count < s->window) s->count++;
```

Arguments: `x0 = s`, `x1 = price`. It calls nothing, so it needs no stack
frame and uses only scratch registers.

**Step 1: load all the fields into registers**

```asm
        ldr     w10, [x0, #O_WINDOW]        // w10 = window
        ldrsw   x11, [x0, #O_HEAD]          // x11 = head, widened to 64 bits
        ldr     w12, [x0, #O_COUNT]         // w12 = count
        ldr     x13, [x0, #O_SUM]           // x13 = sum
```

`head` is loaded with `ldrsw` (sign-extend to 64 bits) because it will be
used as an array index, and addresses are 64-bit.

**Step 2: if the window is full, subtract the oldest price**

```asm
        cmp     w12, w10                    // count == window ?
        b.ne    1f                          // no -> skip to label 1 (forward)
        ldr     x14, [x0, x11, lsl #3]      // x14 = win[head]   (address s + head*8)
        sub     x13, x13, x14               // sum -= oldest price
1:
```

When the buffer is full, `win[head]` is the **oldest** price, which is exactly
the slot about to be overwritten.

**Step 3: store the new price and update the sum**

```asm
        str     x1, [x0, x11, lsl #3]       // win[head] = price
        add     x13, x13, x1                // sum += price
        str     x13, [x0, #O_SUM]
```

`sum` stays in register `x13` the whole time. The compiler's version has to
reload `sum` from memory after writing `win[head]`, because it can't be sure
that `win[head]` and `sum` aren't the same memory (aliasing).

**Step 4: move head forward, without dividing**

```asm
        add     w11, w11, #1                // head + 1
        cmp     w11, w10                    // reached window?
        csel    w11, wzr, w11, eq           // yes -> 0, no -> head + 1
        str     w11, [x0, #O_HEAD]
```

The C code uses `% window`, which needs a slow divide instruction. Because
`head` is always between `0` and `window - 1`, `head + 1` can only ever be
*equal* to `window`, never larger, so "wrap to 0 when equal" gives the same
result with a single `csel`.

**Step 5: count up until the window is full**

```asm
        cmp     w12, w10                    // count < window ?
        b.ge    2f                          // no -> done
        add     w12, w12, #1                // count++
        str     w12, [x0, #O_COUNT]
2:
        ret
```

After the first N trades this branch is always taken, so the CPU predicts it
perfectly.

---

### `sma_fx_ready(const SmaFx *s)`

**C:** `return s->count == s->window;`

```asm
        ldr     w10, [x0, #O_COUNT]
        ldr     w11, [x0, #O_WINDOW]
        cmp     w10, w11
        cset    w0, eq                      // w0 = (count == window) ? 1 : 0
        ret
```

`cset` turns the comparison straight into 1 or 0, with no branch.

---

### `sma_fx_sum(const SmaFx *s)`

**C:** `return s->sum;`

```asm
        ldr     x0, [x0, #O_SUM]            // load sum into the return register
        ret
```

The crossover check uses this exact integer sum, so the main loop never has
to divide.

---

### `sma_fx_value(const SmaFx *s)`

**C:** `return ready ? (double)s->sum / s->window / PRICE_SCALE : 0.0;`

Only used when printing. It returns a `double`, so the result goes in `d0`.

```asm
        ldr     w10, [x0, #O_COUNT]
        ldr     w11, [x0, #O_WINDOW]
        cmp     w10, w11                    // ready?
        b.eq    1f
        fmov    d0, xzr                     // not ready -> return 0.0
        ret
1:
        ldr     x12, [x0, #O_SUM]
        scvtf   d0, x12                     // d0 = (double)sum      (signed int -> float)
        scvtf   d1, w11                     // d1 = (double)window
        fdiv    d0, d0, d1                  // d0 = sum / window
        mov     x13, #PRICE_SCALE
        scvtf   d1, x13                     // d1 = 100.0
        fdiv    d0, d0, d1                  // d0 = sum / window / 100
        ret
```

The two divisions are kept in the same order as the C code, so the printed
value is identical to the last digit.

---

## 5. Worked example

Window N = 3, prices in cents: 1000, 2000, 3000, 4000 ($10, $20, $30, $40).

| Push | count == window? | oldest dropped | `win` after   | sum  | head | count | ready | value |
|------|------------------|----------------|---------------|------|------|-------|-------|-------|
| init | –                | –              | `[ -, -, - ]` | 0    | 0    | 0     | 0     | 0.0   |
| 1000 | no               | –              | `[1000, -, -]`| 1000 | 1    | 1     | 0     | 0.0   |
| 2000 | no               | –              | `[1000,2000,-]`| 3000| 2    | 2     | 0     | 0.0   |
| 3000 | no               | –              | `[1000,2000,3000]`| 6000 | **0** (2+1 = 3 = N, wraps) | 3 | 1 | 6000/3/100 = **20.0** |
| 4000 | yes              | `win[0]` = 1000| `[4000,2000,3000]`| 6000 − 1000 + 4000 = 9000 | 1 | 3 | 1 | 9000/3/100 = **30.0** |

Check: the average of $20, $30 and $40 is $30. Correct.

---

## 6. Things to keep in mind

- **Link either `sma_fx.c` or `sma_fx.s`, never both**, or the linker reports
  duplicate symbols.
- **If `MAX_WIN` or `PRICE_SCALE` changes in the C headers**, change the
  `.equ` lines at the top of this file too.
- **Speed (measured on the Pi):** about the same as the C version (+1.3 %).
  The first version was 9 % slower because of small addressing details (an
  extra `add` before every load, `sxtw` inside the address, 4-byte
  alignment), which shows that fewer instructions doesn't automatically mean
  faster. See `README.md` for the full results.
