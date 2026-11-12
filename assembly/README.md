# Assembly engine (AArch64 / Raspberry Pi)

This folder contains hand-written 64-bit ARM assembly versions of two Stage B
engine files:

| Assembly file | Replaces                    | Functions |
|---------------|-----------------------------|-----------|
| `sma_fx.s`    | `stageB/engine/sma_fx.c`    | `sma_fx_init`, `sma_fx_push`, `sma_fx_ready`, `sma_fx_sum`, `sma_fx_value` |
| `ohlc_fx.s`   | `stageB/engine/ohlc_fx.c`   | `ohlc_fx_init`, `ohlc_fx_update` |
| `sma.s`       | `stageB/engine/sma.c`       | `sma_init`, `sma_push`, `sma_ready`, `sma_value` (double prices, for `stageB_inmem`) |
| `ohlc.s`      | `stageB/engine/ohlc.c`      | `ohlc_init`, `ohlc_update` (double prices, for `stageB_inmem`) |

`sma.s` / `ohlc.s` use the same techniques as the `_fx` files, but with
floating-point registers (`d0`…) and `fcsel` instead of `csel`. Time them
against `stageB_inmem` with `bash assembly/compare_inmem.sh`.

Line-by-line walkthroughs of each file: [`sma_fx.md`](sma_fx.md) and
[`ohlc_fx.md`](ohlc_fx.md).

**The experiment:** `stageB/stageB_fixed.c` is not changed at all. The `.s`
files keep the same function names, arguments and struct layouts as the C
files, so the only difference between builds is which file gets linked. Any
speed difference therefore comes from the assembly itself, not from changes
to the program.

---

## Why the assembly can be faster

A modern C compiler (`gcc -O2`) already produces good ARM code, so rewriting a
function line by line in assembly gains nothing. The assembly only wins where
it does something the compiler **cannot** or **is not allowed to** do. Each
speed-up below is one of those cases.

### `ohlc_fx.s`: one-second OHLC bars

`ohlc_fx_update` runs once per trade. Almost every call is the "same second"
case (the trade only updates the current bar), so that path is what matters.

**1. No mispredicted branches on high / low (the main gain)**

The C code says:

```c
if (price > bar->high) bar->high = price;
if (price < bar->low)  bar->low  = price;
```

The compiler turns each `if` into a branch. The CPU guesses which way a branch
will go before it knows the answer. Because prices move up and down randomly,
it often guesses wrong, and every wrong guess throws away work, costing
roughly 11 cycles on the Pi 5's Cortex-A76.

The assembly uses `csel` (conditional select) instead. It computes
max/min without any branch, so there is nothing to guess wrong:

```asm
ldp   x12, x13, [x0, #B_HIGH]   // load high and low together
cmp   x2, x12
csel  x12, x2, x12, gt          // high = max(high, price)
cmp   x2, x13
csel  x13, x2, x13, lt          // low  = min(low, price)
stp   x12, x13, [x0, #B_HIGH]   // store both together
```

*Why the compiler doesn't do this:* the branch-free version always writes
`high` and `low` back to memory, even when they didn't change. C only writes
them when the `if` is true, and compilers avoid adding memory writes that the
program didn't ask for. In assembly we know this struct is ours alone, so the
extra write is safe.

**2. Cheaper "same second?" test**

The C code computes `sec = time_us / 1000000` on every trade. The compiler
avoids a real division by multiplying with a 64-bit "magic number", but that
still takes about 7 instructions, 4 of them just to build the constant.

The assembly asks the question a different way, with no division:

```
same second  <=>  bar->sec * 1,000,000  <=  time  <  bar->sec * 1,000,000 + 1,000,000
```

```asm
msub  x12, x10, x11, x1   // x12 = time - bar->sec * 1,000,000
cmp   x12, x11            // unsigned compare: 0 <= x12 < 1,000,000 ?
b.hs  .Lcheck_slow        // no -> new second
```

The unsigned compare checks both bounds at once: if `x12` is negative, it
looks like a huge unsigned number and fails the test. The real division only
runs on the rare path, when a new bar starts.

**3. Paired memory operations**

Neighbouring fields are read and written in pairs with `ldp` / `stp`
(load / store pair). Starting a new bar uses 3 stores instead of 8: `n = 1`
and `active = 1` sit next to each other, so a single 64-bit store of
`0x0000000100000001` sets both.

**4. Fast path laid out first**

The common case runs straight through with no jumps. The rare "new bar" code
is placed after it, so the CPU never has to jump over it.

### `sma_fx.s`: running-sum moving average

`sma_fx_push` runs twice per trade (short and long SMA).

**1. No hardware division for the circular buffer**

C: `head = (head + 1) % window;`. Here `window` is a variable, not a constant,
so the compiler must use a real `sdiv` (divide) instruction, one of the
slowest integer instructions on the CPU (roughly 5–12 cycles on the Pi 5's
Cortex-A76, versus 1 for an add). The
assembly uses the fact that `head + 1` can never be larger than `window`, so
the only wrap-around case is `head + 1 == window`:

```asm
add   w11, w11, #1
cmp   w11, w10
csel  w11, wzr, w11, eq     // head = (head + 1 == window) ? 0 : head + 1
```

**2. Keeping `sum` in a register (fewer memory accesses)**

The compiler's version stores `sum`, then writes the new price into
`win[head]`, then **loads `sum` again**. It has to, because it can't prove
that `win[head]` and `sum` are different memory locations: if `head` were out
of range, `win[head]` could overlap `sum` (this is called *aliasing*). The
assembly knows `head` is always in range, so it keeps `sum` in a register:

| `sma_fx_push` (full window) | Loads | Stores | Divides |
|-----------------------------|-------|--------|---------|
| Compiler (`-O2`)            | 6     | 4      | 1       |
| Assembly                    | 5     | 3      | 0       |

**3. Smaller details:** `sma_fx_ready` uses `cset` (no branch), and constants
are encoded straight into instructions instead of being loaded from memory.

**Lesson learned: fewer instructions is not automatically faster.** The first
version of `sma_fx.s` had all of the above and still ran **~9 % slower** than
the C build on the Pi (see *Measured results*). Three details the compiler
gets right were missing:

| First version                                | Problem                                                                 | Fixed version |
|----------------------------------------------|-------------------------------------------------------------------------|---------------|
| `add x9, x0, #1024` then `ldr w10, [x9, #8]` | every load waits for the `add`, in all 6 SMA calls per trade             | `ldr w10, [x0, #1032]` (offset encoded directly) |
| `ldr x14, [x0, w11, sxtw #3]`                | sign-extending inside the address costs an extra cycle on Cortex-A72/A76 | `ldrsw x11, ...` once, then `[x0, x11, lsl #3]` |
| `.align 2` (4-byte function start)           | functions can straddle instruction-fetch blocks                         | `.p2align 4` (16-byte, like GCC) |

---

## What was *not* converted, and why

`vwap_fx_update` was left in C on purpose. The compiler already produces 5
instructions (`ldp`, `madd`, `add`, `stp`, `ret`), which is optimal, so
assembly would gain nothing. It is a useful **control case**: it shows that
hand-written assembly only helps where the compiler is held back.

---

## Correctness

Both files were tested in an AArch64 emulator (Unicorn) against the real
compiled C functions, comparing every byte of the structs and every return
value after every call:

| File        | Test                                                                 | Result |
|-------------|----------------------------------------------------------------------|--------|
| `sma_fx.s`  | 13 window sizes (including invalid ones), 3,000 random pushes each   | 156,065 checks, 0 failures |
| `ohlc_fx.s` | 3 scenarios (real timestamps, negative times, large jumps), 50,000 trades each | 450,000 checks, 0 failures |

`compare_fx.sh` and `compare_inmem.sh` also check on the Pi that every
assembly build prints exactly the same output as the C build before timing
them.

---

## Final results: all 8 builds (Raspberry Pi 5)

Measured with `bash assembly/compare_all.sh`: all eight builds in one
session, each run pinned to core 2 (`taskset -c 2`), **median of 5 runs**,
Process time on `stress_full.csv` (~8.9 million trades). This is the same
method as `logs/leroi_predictions.md`. Raw output: `out_compare_all.txt`.

| Build               | Engine      | SMA        | OHLC        | Median (ms) | vs `stageB_inmem` | vs `stageB_fixed` |
|---------------------|-------------|------------|-------------|-------------|-------------------|-------------------|
| `stageB_inmem`      | double      | C          | C           | 187.63      | —                 | +16.1 %           |
| `stageB_inmem_sma`  | double      | `sma.s`    | C           | 180.30      | −3.9 %            | +11.6 %           |
| `stageB_inmem_ohlc` | double      | C          | `ohlc.s`    | 168.06      | −10.4 %           | +4.0 %            |
| `stageB_inmem_asm`  | double      | `sma.s`    | `ohlc.s`    | 159.18      | −15.2 %           | −1.5 %            |
| `stageB_fixed`      | fixed-point | C          | C           | 161.63      | −13.9 %           | —                 |
| `stageB_fixed_sma`  | fixed-point | `sma_fx.s` | C           | 164.32      | −12.4 %           | +1.7 %            |
| **`stageB_fixed_ohlc`** | **fixed-point** | **C**  | **`ohlc_fx.s`** | **143.07** | **−23.7 %**   | **−11.5 %**       |
| `stageB_fixed_asm`  | fixed-point | `sma_fx.s` | `ohlc_fx.s` | 146.95      | −21.7 %           | −9.1 %            |

`dev_1M.csv` gives the same percentages to within about 1 %. The runs were
steady: within each build, min and max are less than 1 % apart (except one
slow `stageB_fixed_sma` run, which the median ignores).

**What the numbers show**

- **Fixed-point is 1.16× faster than double in plain C** (187.63 / 161.63 ms).
  Leroi predicted 1.1–1.5× and measured 1.13× on the laptop. See *Why the
  fixed-point build is faster* below.
- **`ohlc` assembly saves the same time in both engines:** about 19 ms
  (19.6 ms double, 18.6 ms fixed-point), i.e. about 2.1 ns per trade. That
  fits its cause: it removes the same mispredicted high/low branches,
  whatever the number type.
- **`sma` assembly helps the double engine (−3.9 %) but not fixed-point
  (+1.7 %).** Keeping `sum` in a register avoids a store-then-reload before
  each floating-point add. With integer adds, that reload barely mattered.
- **The fastest build is `stageB_fixed_ohlc`** (143.07 ms): fixed-point C
  with only `ohlc_fx.s` in assembly. It is **1.31× faster** than the plain
  double build and **1.13× faster** than the plain fixed-point build.
  Adding `sma_fx.s` on top makes it slower (146.95 ms).
- **Pi 5 vs laptop:** the plain-C builds take about 2.3× longer on the Pi
  than on Leroi's laptop (187.6 vs 79.9 ms double, 161.6 vs 70.6 ms
  fixed-point), within his predicted "2–4× longer".

## Earlier measurements (`dev_1M.csv`, 20 unpinned runs, min Process time)

These runs came before the final method above. They are kept because they
show how `sma_fx.s` was found to be slow and then fixed.

First version of `sma_fx.s` (before the fixes above):

| Build              | SMA      | OHLC     | min (ms) | vs all-C |
|--------------------|----------|----------|----------|----------|
| `stageB_fixed`     | C        | C        | 18.084   | —        |
| `stageB_fixed_sma` | assembly | C        | 19.662   | +8.7 % (slower) |
| `stageB_fixed_asm` | assembly | assembly | 17.327   | −4.2 % (faster) |

The first `sma_fx.s` was a loss, which led to the fixes above.

After the fixes (four builds, so each file can be measured on its own):

| Build               | SMA      | OHLC     | min (ms) | avg (ms) | vs all-C (min) |
|---------------------|----------|----------|----------|----------|----------------|
| `stageB_fixed`      | C        | C        | 18.101   | 18.240   | —              |
| `stageB_fixed_sma`  | assembly | C        | 18.335   | 18.466   | +1.3 %         |
| `stageB_fixed_ohlc` | C        | assembly | **15.802** | **15.980** | **−12.7 %** |
| `stageB_fixed_asm`  | assembly | assembly | 16.277   | 16.529   | −10.1 %        |

**Conclusions**

- **`ohlc_fx.s`: −12.7 %.** A real, repeatable gain. It comes from the two
  things the compiler can't do here: branch-free high/low with `csel`, and no
  division on the same-second path.
- **`sma_fx.s`: no gain (+1.3 %, about the same as C).** The fixes removed
  the 9 % slowdown, but the C compiler's version was already about as good as
  hand-written code. The `sdiv` that the assembly removes is not on the
  critical path: the CPU runs it in parallel with other work, so removing it
  saves almost nothing.
- **Fastest build: assembly OHLC + C SMA** (`stageB_fixed_ohlc`). Adding
  `sma_fx.s` on top makes it slightly slower (16.28 vs 15.80 ms).
- **Takeaway:** hand-written assembly paid off only where the compiler was
  held back by a C language rule (it may not add memory writes, so it can't
  remove the high/low branches). Where the compiler had no such limit
  (`sma_fx`, `vwap_fx`), it already matched hand-written code.

The assembly only replaces part of each trade's work (reading the trade array,
VWAP and the crossover check are unchanged), so the overall gain stays modest.
Load time is unaffected.

### Double-precision engine: `stageB_inmem` (`bash assembly/compare_inmem.sh`)

| Build                    | SMA      | OHLC     | min (ms) | vs `stageB_inmem` C |
|--------------------------|----------|----------|----------|---------------------|
| `stageB_inmem`           | C        | C        | 21.033   | —                   |
| `stageB_inmem_sma`       | `sma.s`  | C        | 20.225   | −3.8 %              |
| `stageB_inmem_ohlc`      | C        | `ohlc.s` | 18.745   | −10.9 %             |
| **`stageB_inmem_asm`**   | `sma.s`  | `ohlc.s` | **17.731** | **−15.7 %**       |
| `stageB_fixed` (reference) | C (fixed-point) | C (fixed-point) | 18.125 | −13.8 % |

- **The in-memory (double) build is ~16 % slower than the fixed-point build**
  in plain C. It does floating-point maths and calls `sma_value` (a
  division) twice per trade, where the fixed-point build compares integer
  sums with no division.
- **`ohlc.s` gives about the same gain as `ohlc_fx.s`** (−10.9 % vs −12.7 %):
  branch-free high/low works the same way with `fcsel`.
- **`sma.s` helps here (−3.8 %), although `sma_fx.s` did not.** A likely
  explanation: the compiler's version stores `sum` and reloads it before the
  next `fadd`. A floating-point add takes about twice as long as an integer
  add, so that store-then-reload costs more of each trade's time. With
  integers, the same reload barely mattered.
- **The two gains add up** (0.81 + 2.29 ≈ 3.30 ms saved together). With both
  assembly files, the double build becomes slightly faster than the all-C
  fixed-point build.

### Why the fixed-point build is faster than the double build

In plain C, `stageB_fixed` takes 18.13 ms and `stageB_inmem` takes 21.03 ms
(+16 %). Despite the name, **both programs are in-memory**: each loads the
whole CSV into an array, then processes it. The difference is the **number
type**: integers in cents (`long long`) versus `double`.

**1. Division on every trade (likely the biggest factor).**
`stageB_inmem` needs the actual averages to compare the two SMAs, so it calls
`sma_value` twice per trade, and each call is a floating-point division:

```c
if (have_short) short_value = sma_value(&short_sma);   /* fdiv */
if (have_long)  long_value  = sma_value(&long_sma);    /* fdiv */
signal = crossover_update(&cross, short_value, long_value);
```

`stageB_fixed` avoids division completely. Instead of checking
`short_sum / 35 > long_sum / 60`, it multiplies both sides out and checks
`short_sum × 60 > long_sum × 35`:

```c
signal = crossover_fx_update(&cross, sma_fx_sum(&short_sma), short_win,
                             sma_fx_sum(&long_sma), long_win);
/* d = sum_short * long_win - sum_long * short_win  -> 2 integer multiplies */
```

On the Pi 5's Cortex-A76, a floating-point division takes roughly 7–15
cycles and an integer multiply about 2.

**2. Floating-point adds are slower than integer adds.**
The running sums (two SMAs and VWAP) are updated every trade, and each update
has to wait for the previous trade's result. On the Cortex-A76 an integer
`add` takes 1 cycle and a floating-point `fadd` about 2. This also explains
why keeping `sum` in a register helped `sma.s` (double, −3.8 %) but not
`sma_fx.s` (integer, +1.3 %): the slower floating-point adds made that wait
matter more.

*(Cycle counts are approximate figures for the Cortex-A76; the exact values
depend on the operands.)*

**3. Not a factor: memory or loading.** `Trade` and `TradeFx` are both 32
bytes, so the same amount of data is read, and loading the CSV is timed
separately from these numbers.

| | `stageB_inmem` (double) | `stageB_fixed` (integer) |
|---|---|---|
| Crossover check | 2 floating-point divisions per trade | 2 integer multiplies, no division |
| Running sums | `fadd` / `fsub`, ~2 cycles each | `add` / `sub`, 1 cycle each |
| Results | can carry tiny rounding errors | exact |

The fixed-point version is both **faster and exact**. Avoiding the division
in the crossover check is likely the bigger reason, but this has not been
measured separately. Changing `stageB_inmem` to compare sums the way
`stageB_fixed` does would show how much it accounts for.

## How to measure it

On the Raspberry Pi:

```bash
bash assembly/compare_all.sh         # ALL 8 builds (double + fixed-point, C + asm), 5 runs
bash assembly/compare_fx.sh          # fixed-point builds only
bash assembly/compare_inmem.sh       # double builds only
bash assembly/compare_all.sh 10      # more runs = steadier numbers
```

`compare_all.sh` is the one to use for the report: it times every build in
the same session and ends with a summary table showing each build's median
against **both** pure-C builds (`stageB_inmem` and `stageB_fixed`).

Each script builds its versions, checks their outputs match the C build, and
reports the **median** / min / average / max *Process time* over interleaved
runs. Timing runs are pinned to CPU core 2 with `taskset -c 2`. Median of 5
pinned runs is the same method as `logs/leroi_predictions.md`, so the numbers
can be compared directly with the laptop results there.

*(The earlier results in this file were measured before the scripts used
pinning and the median: 20 unpinned runs, compared by the minimum.)*

| Build               | SMA      | OHLC     | Shows                         |
|---------------------|----------|----------|-------------------------------|
| `stageB_fixed`      | C        | C        | baseline                      |
| `stageB_fixed_sma`  | assembly | C        | effect of `sma_fx.s` alone    |
| `stageB_fixed_ohlc` | C        | assembly | effect of `ohlc_fx.s` alone   |
| `stageB_fixed_asm`  | assembly | assembly | both together                 |

Compare `dev_1M` (and `stress_full` once that file is in
`data/group_data/`). `test20` is too small and is dominated by printing.

## Building by hand

```bash
gcc -O2 -Wall stageB/stageB_fixed.c stageB/engine/trades_fx.c \
    stageB/engine/vwap_fx.c assembly/ohlc_fx.s assembly/sma_fx.s \
    stageB/engine/signal_fx.c stageB/engine/signal.c stageB/engine/report.c \
    helper_function/trade_io.c -o stageB/stageB_fixed_asm
```

Link each engine file from **either** the `.c` **or** the `.s` file, never
both, or the linker reports duplicate symbols. The `.s` files only build on
64-bit ARM (e.g. Raspberry Pi OS 64-bit).

**If you edit the C headers:** the assembly cannot read `#include` files, so
`MAX_WIN`, `PRICE_SCALE`, `MICROS_PER_SEC` and the struct field offsets are
copied by hand at the top of each `.s` file. If you change them in the
headers, update the `.s` files too. `sma_fx.s` stops the build with an error
if `MAX_WIN` grows past 2046.

---

## Summary

**Goal.** Find out whether rewriting parts of the Stage B engine in AArch64
assembly makes it faster on the Raspberry Pi 5, while leaving the C programs
(`stageB_inmem.c` and `stageB_fixed.c`) completely unchanged.

**What was done.**

- The SMA and OHLC modules of both engines were rewritten in assembly:
  `sma.s` / `ohlc.s` (double, for `stageB_inmem`) and `sma_fx.s` /
  `ohlc_fx.s` (fixed-point, for `stageB_fixed`). They keep the same function
  names and struct layouts, so they drop in at link time.
- VWAP was kept in C as a control, because the compiler's code for it is
  already optimal.
- All four assembly files were checked in an emulator against the C versions
  (1,128,087 checks, 0 failures). On the Pi, every build's output was
  confirmed identical to the C build of its engine before timing.

**Results** (`stress_full.csv`, median of 5 pinned runs, all 8 builds in one
session):

| Build                              | Time          | vs plain double | vs plain fixed-point |
|------------------------------------|---------------|-----------------|----------------------|
| Plain C, double (`stageB_inmem`)   | 187.63 ms     | —               | +16.1 %              |
| Double + both asm                  | 159.18 ms     | −15.2 %         | −1.5 %               |
| Plain C, fixed-point (`stageB_fixed`) | 161.63 ms  | −13.9 %         | —                    |
| **Fixed-point + `ohlc_fx.s`**      | **143.07 ms** | **−23.7 %**     | **−11.5 %**          |
| Fixed-point + both asm             | 146.95 ms     | −21.7 %         | −9.1 %               |

**Key findings.**

1. **The number type matters as much as the assembly.** Fixed-point C is
   1.16× faster than double C, most likely because it compares integer sums
   instead of dividing to get the SMAs on every trade (not yet measured
   separately).
2. **Assembly helped only where the compiler was held back.** The OHLC
   assembly saves about 19 ms (~2.1 ns per trade) in both engines because it
   uses branch-free `csel`/`fcsel` for high/low. A C compiler is not allowed
   to do this, since it would add memory writes the program never asked for.
3. **Where the compiler had no such limit, it matched hand-written code.**
   `sma_fx.s` was no faster than C, and VWAP was already optimal. `sma.s`
   gained 3.9 % only because floating-point adds made the compiler's extra
   reload of `sum` cost more.
4. **Fewer instructions is not the same as faster.** The first `sma_fx.s`
   used fewer instructions than the compiler but ran 9 % slower, because of
   addressing details (an extra `add` before every load, slow index
   extension, poor alignment). Measuring on the real hardware, not counting
   instructions, is what found this.
5. **Best configuration:** fixed-point C with only `ohlc_fx.s` in assembly,
   1.31× faster than the plain double build.

**Conclusion.** The biggest wins came from design choices in C (integers,
and no division in the crossover check) and from one targeted piece of
assembly: the OHLC high/low update, where a C language rule stops the
compiler from using branch-free instructions. For straightforward code,
`gcc -O2` already produces code as good as hand-written assembly.
