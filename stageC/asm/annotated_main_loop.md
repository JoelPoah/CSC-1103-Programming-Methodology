# Stage C task 5: annotated assembly of the main loop (fastest version)

Brief: _"Generate the assembly for your main loop (`gcc -S -O2`) and annotate what each
instruction does."_

The fastest version (`--engine mem`, same maths as `stageB/stageB_fixed.c`) does this for
every trade (`step_fx` in `stageC/engine/engine.c`):

```c
vwap_fx_update(&e->vwap_fx, t->price, t->qty);          /* 2 running sums        */
ohlc_fx_update(&e->bar_fx, t->time, t->price, ...);     /* 1-second bar          */
sma_fx_push(&e->sma_short_fx, t->price);                /* running-sum SMA x 2   */
sma_fx_push(&e->sma_long_fx,  t->price);
crossover_fx_update(&e->cross_fx, sum_s, S, sum_l, L);  /* exact integer signal  */
```

The listings below follow one trade through the code: the dispatch in `engine_step`, the
sequence in `step_fx`, then the four kernels it calls. Each `.s` file was made with:

```sh
make asm            # = gcc -S -O2 -fno-asynchronous-unwind-tables -fno-ident <file>.c
```

> **Machine:** these listings are **x86-64** from the laptop (Windows, MinGW gcc 16.1,
> Windows x64 calling convention). **The Pi listing is still TODO.** On the Pi, run
> `make asm` again. It writes `*_aarch64_O2.s` (ARM64 instructions: `ldr/str/add/sub/
> mul/sdiv/cmp/b.le/csel`...). Annotate that file the same way for CSC1104 (see the table at
> the end).

Windows x64 calling convention used below: arguments 1–4 arrive in `rcx, rdx, r8, r9`,
the 5th is on the stack at `40(%rsp)`, and the return value goes in `eax`/`rax`.
`q` = 64-bit (long long), `l` = 32-bit (int) operand.

---

## 0. The loop body: `engine_step` and `step_fx`

Each trade starts here. Only the `mem` path of `engine_step` is shown; the `v0` and `fast`
paths in `engine_x86-64_O2.s` call the parser first.

```c
case ENGINE_MEM:
    *price_cents = e->parsed[i].price;
    return step_fx(e, &e->parsed[i]);
```

`TradeFx` layout: `time` at 0, `price` at 8, `qty` at 16, `is_buy` at 24; 32 bytes in all.

```asm
engine_step:                    ; rcx = e, rdx = feed, r8 = i, r9 = price_cents
    subq    $216, %rsp          ; reserve stack space (used by the v0 / fast paths)
    movl    (%rcx), %eax        ; eax = e->kind
    movq    %rbx, 192(%rsp)     ; save rbx (callee-saved)
    movq    %rcx, %rbx          ; rbx = e (kept for the v0 / fast paths)
    movq    %rsi, 200(%rsp)     ; save rsi (callee-saved)
    movq    %r9, %rsi           ; rsi = price_cents (kept for the v0 / fast paths)
    testl   %eax, %eax          ; kind == ENGINE_V0 (0) ?
    je      .L23                ;   yes -> Version 0 path
    cmpl    $1, %eax            ; kind == ENGINE_FAST (1) ?
    je      .L24                ;   yes -> fast path
    ; ---- ENGINE_MEM ----
    movq    4488(%rcx), %rdx    ; rdx = e->parsed (start of the TradeFx array)
    salq    $5, %r8             ; r8 = i * 32 (shift, because sizeof(TradeFx) = 2^5)
    movq    192(%rsp), %rbx     ; restore rbx
    movq    200(%rsp), %rsi     ; restore rsi
    addq    %r8, %rdx           ; rdx = &e->parsed[i]
    movq    8(%rdx), %rax       ; rax = parsed[i].price
    movq    %rax, (%r9)         ; *price_cents = price
    addq    $216, %rsp          ; release the stack space
    jmp     step_fx             ; tail call: step_fx returns straight to our caller
```

```asm
step_fx:                        ; rcx = e, rdx = t (pointer to the TradeFx)
    subq    $104, %rsp          ; stack frame (Windows x64 needs shadow space for calls)
    movq    %rbx, 72(%rsp)      ; save rbx
    movq    %rdx, %rbx          ; rbx = t (survives the calls below)
    movq    8(%rdx), %rdx       ; argument 2 = t->price
    movq    16(%rbx), %r8       ; argument 3 = t->qty
    movq    %rsi, 80(%rsp)      ; save rsi
    movq    %rcx, %rsi          ; rsi = e (survives the calls below)
    addq    $2256, %rcx         ; argument 1 = &e->vwap_fx
    movq    %rbp, 96(%rsp)      ; save rbp
    leaq    3416(%rsi), %rbp    ; rbp = &e->sma_long_fx
    call    vwap_fx_update      ; (call 1)
    movq    8(%rbx), %r8        ; argument 3 = t->price
    movq    (%rbx), %rdx        ; argument 2 = t->time
    leaq    2320(%rsi), %r9     ; argument 4 = &e->finished_fx
    leaq    2272(%rsi), %rcx    ; argument 1 = &e->bar_fx
    call    ohlc_fx_update      ; (call 2)
    movq    8(%rbx), %rdx       ; argument 2 = t->price
    leaq    2368(%rsi), %rcx    ; argument 1 = &e->sma_short_fx
    call    sma_fx_push         ; (call 3)
    movq    8(%rbx), %rdx       ; argument 2 = t->price
    movq    %rbp, %rcx          ; argument 1 = &e->sma_long_fx
    call    sma_fx_push         ; (call 4)
    leaq    2368(%rsi), %rcx    ; argument 1 = &e->sma_short_fx
    call    sma_fx_ready        ; (call 5)
    testl   %eax, %eax          ; short window full?
    jne     .L11                ;   yes -> continue below
.L1:                            ; ---- common exit: rax already holds the signal ----
    movq    72(%rsp), %rbx      ; restore rbx
    movq    80(%rsp), %rsi      ; restore rsi
    movq    96(%rsp), %rbp      ; restore rbp
    addq    $104, %rsp          ; release the frame
    ret
.L11:
    movq    %rbp, %rcx          ; argument 1 = &e->sma_long_fx
    call    sma_fx_ready        ; (call 6)
    testl   %eax, %eax          ; long window full?
    je      .L1                 ;   no -> exit with eax = 0 = SIG_NONE
    movq    %rbp, %rcx          ; argument 1 = &e->sma_long_fx
    movl    8(%rsi), %ebx       ; ebx = e->long_win
    call    sma_fx_sum          ; (call 7) rax = long window sum
    movl    4(%rsi), %r8d       ; r8d = e->short_win
    leaq    2368(%rsi), %rcx    ; argument 1 = &e->sma_short_fx
    movq    %rax, %rbp          ; rbp = long window sum
    movl    %r8d, 60(%rsp)      ; save short_win across the call (r8 is caller-saved)
    call    sma_fx_sum          ; (call 8) rax = short window sum
    movl    %ebx, 32(%rsp)      ; argument 5 = long_win (on the stack)
    movl    60(%rsp), %r8d      ; argument 3 = short_win
    movq    %rbp, %r9           ; argument 4 = long window sum
    leaq    4464(%rsi), %rcx    ; argument 1 = &e->cross_fx
    movq    %rax, %rdx          ; argument 2 = short window sum
    call    crossover_fx_update ; (call 9) rax = the signal
    jmp     .L1                 ; restore registers and return it
```

**What it shows:**
- `sizeof(TradeFx)` is 32, a power of two, so `parsed[i]` needs only a shift and an add.
- Once both windows are full, each trade makes **9 calls**. `sma_fx_ready` and `sma_fx_sum`
  do almost nothing, but they live in another `.c` file, so gcc cannot inline them at `-O2`.
  The saving and restoring of `rbx`, `rsi`, `rbp` is also the price of those calls.

---

## 1. `vwap_fx_update`: two running sums in ONE vector add

```c
v->sum_pq += price * qty;
v->sum_q  += qty;
```

```asm
vwap_fx_update:                 ; rcx = v, rdx = price, r8 = qty
    imulq   %r8, %rdx           ; rdx = price * qty   (64-bit integer multiply)
    movdqu  (%rcx), %xmm2       ; xmm2 = {sum_pq, sum_q}: load BOTH sums (16 bytes) at once
    movq    %r8, %xmm1          ; xmm1 low half = qty
    movq    %rdx, %xmm0         ; xmm0 low half = price * qty
    punpcklqdq %xmm1, %xmm0     ; xmm0 = {price*qty, qty}: pack the two values side by side
    paddq   %xmm2, %xmm0        ; xmm0 = {sum_pq + price*qty, sum_q + qty}: 2 adds, 1 instruction
    movups  %xmm0, (%rcx)       ; store both new sums back into the struct
    ret                         ; return to step_fx
```

**What it shows:** the compiler noticed that `sum_pq` and `sum_q` sit next to each
other in memory (`VwapFx` = 2 x 8 bytes) and used one SSE2 instruction (`paddq`) to do
both additions together: automatic SIMD. This only works because the sums are
`long long`; with `double` it would be `addsd` x 2 (or `addpd`).

---

## 2. `sma_fx_push`: running-sum SMA with a circular buffer

```c
if (s->count == s->window) s->sum -= s->win[s->head];   /* oldest leaves  */
s->win[s->head] = price;                                 /* newest joins   */
s->sum += price;
s->head = (s->head + 1) % s->window;                     /* wrap around    */
if (s->count < s->window) s->count++;
```

`SmaFx` layout: `win[128]` at offset 0 (1024 bytes), `window` at 1024, `head` at 1028,
`count` at 1032, `sum` at 1040.

```asm
sma_fx_push:                    ; rcx = s, rdx = price
    movslq  1028(%rcx), %r9     ; r9  = s->head (int, sign-extended to 64 bits for indexing)
    movl    1024(%rcx), %r10d   ; r10 = s->window
    movq    %rdx, %r8           ; r8  = price (free up rdx: idiv needs it)
    pushq   %rbx                ; save rbx (callee-saved) because we use it below
    movl    1032(%rcx), %r11d   ; r11 = s->count
    movq    1040(%rcx), %rbx    ; rbx = s->sum
    leal    1(%r9), %eax        ; eax = head + 1
    cltd                        ; sign-extend eax into edx:eax (needed before idiv)
    idivl   %r10d               ; eax = (head+1) / window, edx = (head+1) % window   <-- SLOW
    cmpl    %r10d, %r11d        ; compare count with window
    je      .L13                ; window full -> take the "drop the oldest" path
    ; ---- window not full yet (only the first N trades) ----
    movq    %r8, (%rcx,%r9,8)   ; win[head] = price   (address = s + head*8)
    addq    %rbx, %r8           ; r8 = sum + price
    movq    %r8, 1040(%rcx)     ; s->sum = sum + price
    movl    %edx, 1028(%rcx)    ; s->head = (head+1) % window
    cmpl    %r10d, %r11d        ; count < window ?
    jge     .L9                 ;   no -> done
    addl    $1, %r11d           ; count + 1
    movl    %r11d, 1032(%rcx)   ; s->count = count + 1
.L9:
    popq    %rbx                ; restore rbx
    ret
.L13:                           ; ---- window full: the path taken for almost every trade ----
    subq    (%rcx,%r9,8), %rbx  ; sum -= win[head]     (oldest price leaves; memory operand)
    movq    %r8, (%rcx,%r9,8)   ; win[head] = price    (newest price overwrites it)
    addq    %r8, %rbx           ; sum += price
    movl    %edx, 1028(%rcx)    ; s->head = (head+1) % window
    movq    %rbx, 1040(%rcx)    ; s->sum = sum
    popq    %rbx                ; restore rbx
    ret
```

**What it shows:**
- The steady-state path (`.L13`) is just **one subtract, one store, one add**, exactly the
  `S_t = S_{t-1} + p_t - p_{t-N}` formula. No loop at all, unlike the naive SMA, whose
  assembly has a loop running N times.
- The compiler moved the `count == window` test after the division and merged the two
  `if`s into one branch (`je .L13`). Once the window is full, that branch is always
  taken, so the CPU's branch predictor gets it right every time.
- **`idivl` is the most expensive instruction here.** The `%` operator for wrapping
  `head` becomes a real hardware integer divide, which takes many times longer than an
  add (roughly 10 to 30 cycles, depending on the CPU). It runs twice per trade
  (short and long SMA). **Possible optimisation (not applied, for the report):**
  `if (++s->head == s->window) s->head = 0;` gives the same result with a compare
  instead of a divide.

---

## 3. `crossover_fx_update`: the exact integer BUY/SELL decision

```c
long long d = sum_short * long_win - sum_long * short_win;
if (have_prev) {
    if (prev_d <= 0 && d > 0)      { sig = BUY;  buys++;  }
    else if (prev_d >= 0 && d < 0) { sig = SELL; sells++; }
}
prev_d = d; have_prev = 1; return sig;
```

`CrossoverFx` layout: `prev_d` at 0, `have_prev` at 8, `buys` at 12, `sells` at 16.

```asm
crossover_fx_update:            ; rcx = x, rdx = sum_short, r8d = short_win,
                                ; r9 = sum_long, 40(%rsp) = long_win (5th argument)
    movslq  40(%rsp), %rax      ; rax = long_win (int -> 64 bits)
    movslq  %r8d, %r8           ; r8  = short_win (int -> 64 bits)
    imulq   %r9, %r8            ; r8  = sum_long * short_win
    imulq   %rdx, %rax          ; rax = sum_short * long_win
    movl    8(%rcx), %edx       ; edx = x->have_prev
    subq    %r8, %rax           ; rax = d = sum_short*L - sum_long*S   (no division!)
    testl   %edx, %edx          ; have_prev == 0 ?
    je      .L4                 ;   yes -> no signal possible, go store d
    movq    (%rcx), %rdx        ; rdx = prev_d
    testq   %rdx, %rdx          ; set flags from prev_d
    jle     .L14                ; prev_d <= 0 -> go check for BUY
.L5:                            ; prev_d >= 0 here: check for SELL
    testq   %rax, %rax          ; set flags from d
    js      .L15                ; d < 0 (sign bit set) -> SELL
    xorl    %edx, %edx          ; sig = SIG_NONE (0)
.L4:
    movq    %rax, (%rcx)        ; x->prev_d = d
    movl    %edx, %eax          ; return value = sig
    movl    $1, 8(%rcx)         ; x->have_prev = 1
    ret
.L14:                           ; prev_d <= 0
    testq   %rax, %rax          ; d > 0 ?
    jle     .L6                 ;   no -> maybe still a SELL if prev_d == 0
    movl    $1, %edx            ; sig = SIG_BUY (1)
    movq    %rax, (%rcx)        ; x->prev_d = d
    addl    $1, 12(%rcx)        ; x->buys++   (add straight into memory)
    movl    %edx, %eax          ; return SIG_BUY
    movl    $1, 8(%rcx)         ; x->have_prev = 1
    ret
.L15:                           ; SELL
    addl    $1, 16(%rcx)        ; x->sells++
    movl    $-1, %edx           ; sig = SIG_SELL (-1)
    jmp     .L4                 ; store d and return
.L6:                            ; prev_d <= 0 and d <= 0
    testq   %rdx, %rdx          ; was prev_d exactly 0?
    je      .L5                 ;   yes -> prev_d >= 0 also holds, check SELL
    xorl    %edx, %edx          ; otherwise no signal
    jmp     .L4
```

**What it shows:**
- Comparing `SMA_short - SMA_long` became **two integer multiplies and one subtract**.
  There is no `div` and no floating point, so `d == 0` is exact when the averages are
  equal. That is why the fixed-point version gets the exact signal count and the double
  version does not.
- `jle`, `js` and `je` test the sign of `d` / `prev_d` straight from the CPU flags.
- Most trades give no signal, so the common path (`testq`, `js` not taken, `xorl`, store,
  `ret`) is short and well predicted. A signal (~0.5% of trades) is a mispredicted branch,
  which costs about 15–20 cycles.

---

## 4. `ohlc_fx_update`: one-second bars, and a division with no divide

```c
long long sec = time_us / MICROS_PER_SEC;
if (!bar->active || sec != bar->sec) { /* copy to *finished, start a new bar */ }
if (price > bar->high) bar->high = price;
if (price < bar->low)  bar->low = price;
bar->close = price;
bar->n++;
```

`OhlcBarFx` layout: `sec` at 0, `open` 8, `high` 16, `low` 24, `close` 32, `n` 40, `active` 44.

```asm
ohlc_fx_update:                 ; rcx = bar, rdx = time_us, r8 = price, r9 = finished
    movabsq $4835703278458516699, %rax ; rax = 2^82 / 1,000,000 (the "magic number")
    movq    %rdx, %r10          ; r10 = time_us (copy, imul overwrites rdx)
    imulq   %rdx                ; rdx:rax = rax * time_us; rdx is the high 64 bits
    movl    44(%rcx), %eax      ; eax = bar->active
    sarq    $63, %r10           ; r10 = 0 if time_us >= 0, -1 if negative
    sarq    $18, %rdx           ; rdx = high half >> 18, i.e. product >> 82 = time_us / 10^6
    subq    %r10, %rdx          ; correct the rounding for negative values; rdx = sec
    testl   %eax, %eax          ; is there a bar yet?
    je      .L4                 ;   no -> start one (eax = 0 is also the return value)
    cmpq    %rdx, (%rcx)        ; bar->sec == sec ?
    je      .L5                 ;   yes -> same second, the common case
    ; ---- a new second: copy the 48-byte bar to *finished, 16 bytes at a time ----
    movdqu  (%rcx), %xmm0       ; load bytes 0-15
    movl    $1, %eax            ; return value = 1 (a bar was closed)
    movups  %xmm0, (%r9)        ; store them in *finished
    movdqu  16(%rcx), %xmm0     ; bytes 16-31
    movups  %xmm0, 16(%r9)
    movdqu  32(%rcx), %xmm0     ; bytes 32-47
    movups  %xmm0, 32(%r9)
.L4:                            ; ---- start a new bar ----
    movq    %rdx, (%rcx)        ; bar->sec = sec
    movabsq $4294967297, %rdx   ; rdx = 0x0000000100000001 (two 32-bit ones)
    movq    %r8, 32(%rcx)       ; close = price
    movq    %r8, 24(%rcx)       ; low   = price
    movq    %r8, 16(%rcx)       ; high  = price
    movq    %r8, 8(%rcx)        ; open  = price
    movq    %rdx, 40(%rcx)      ; n = 1 AND active = 1, in one 8-byte store
    ret
.L5:                            ; ---- same second ----
    cmpq    %r8, 16(%rcx)       ; compare bar->high with price
    jge     .L7                 ;   high >= price -> keep it
    movq    %r8, 16(%rcx)       ; high = price
.L7:
    cmpq    %r8, 24(%rcx)       ; compare bar->low with price
    jle     .L8                 ;   low <= price -> keep it
    movq    %r8, 24(%rcx)       ; low = price
.L8:
    addl    $1, 40(%rcx)        ; bar->n++
    xorl    %eax, %eax          ; return value = 0 (no bar closed)
    movq    %r8, 32(%rcx)       ; close = price
    ret
```

**What it shows:** the divisor is a constant, so gcc replaces the slow `idiv` with a
multiply by a pre-computed "magic number" and a shift. `% window` in `sma_fx_push` cannot
get this treatment because `window` is only known at run time.

---

## Summary for the report

| Kernel | Instructions on the common path | Expensive parts |
|---|---|---|
| `vwap_fx_update` | 7 (1 `imul`, 1 SIMD `paddq`) | none: auto-vectorised |
| `sma_fx_push` (x2) | ~13 each | **`idivl`** for `% window` (roughly 10–30 cycles) |
| `crossover_fx_update` | ~13 | 2 `imul`; mispredicted branch on a signal |
| `ohlc_fx_update` | ~17 in the same second | none: the divide by 1,000,000 became a multiply + shift |

Why it is not even faster: each step is a separate function in a separate `.c` file, so
gcc cannot inline them into the loop at `-O2` (every trade does 9 `call`/`ret` pairs).
Building with `-flto` (link-time optimisation), or using `static inline` functions in the
headers, would let gcc inline them. That is a possible extra rung on the ladder.

## AArch64 (Pi) quick reference for annotating `*_aarch64_O2.s`

| AArch64 | x86-64 equivalent | Meaning |
|---|---|---|
| `x0`–`x7` | `rcx, rdx, r8, r9`, stack | arguments 1–8, return value in `x0`/`w0` |
| `ldr x1, [x0, 1040]` | `movq 1040(%rcx), %rbx` | load 64 bits from x0 + 1040 |
| `str x1, [x0, x2, lsl 3]` | `movq %r8, (%rcx,%r9,8)` | store to x0 + x2*8 (array element) |
| `add / sub / mul` | `addq / subq / imulq` | integer arithmetic (3 operands on ARM) |
| `sdiv` + `msub` | `idivl` | ARM has no remainder instruction: `a % b = a - (a/b)*b` |
| `cmp` + `b.le / b.gt / b.eq` | `cmp` + `jle / jg / je` | compare and branch |
| `csel / cset` | `sete` / `cmov` | pick a value without branching |
| `ret` | `ret` | return (to the address in `x30`) |
