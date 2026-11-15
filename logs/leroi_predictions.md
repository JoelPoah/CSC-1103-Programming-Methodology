# Prediction logbook: Leroi (Stage B and C)

Each entry follows predict, measure, explain. I wrote each prediction before the
measurement it predicts and filled in Result and Reason afterwards.

## Prediction 1: Fixed-point (long long) prices vs double

**Date:** 2026-11-03  **Signed:** Leroi
**Status:** prediction written before the fixed-point version was implemented.

**Expected:**

*Speed:* fixed-point will be only slightly faster than double, about **1.1–1.5×** on the
process phase of `stress_full.csv` (laptop and Pi), and **not** a big jump.
- Adding/subtracting whole numbers is a bit quicker for the CPU than adding decimals, and
  the running sums (SMA sum, VWAP sums) are updated every trade.
- But each trade still does the same amount of work (VWAP, OHLC, SMA, crossover), and the
  `Trade` struct stays 32 bytes, so the same amount of memory is read. The work per trade
  is already small (~8.5 ns on the laptop).
- Risk: if the code divides by N every trade to get the SMA, whole-number division is slow
  and could cancel out the gain. Comparing the sums directly
  (`sum_short × 60` vs `sum_long × 35`) avoids dividing at all.

*Pi vs laptop:* the gain may be a little bigger on the Pi, because its simpler CPU feels the
cost of each decimal operation more.

*Correctness:*
- `test20.csv` will still match.
- `dev_1M.csv` signal counts will **change** from the double version (2465 BUY / 2464 SELL)
  to the exact answer (2553 BUY / 2469 SELL). On 17,132 trades the two SMAs are exactly
  equal; with double, rounding leaves a tiny non-zero difference whose sign is random,
  while whole numbers give exactly 0. (The exact counts come from an earlier integer-cents
  check, so this part predicts that the fixed-point engine matches that check.)
- VWAP will match the double value to at least 6–8 decimal places.

**Result:**

Measured on the group's `src/` ladder: V4 (`double`, running-sum SMA) vs V5 (fixed-point).

| | V4 double | V5 fixed-point | speedup |
|---|---|---|---|
| Laptop, process phase (median of 5) | 92.8 ms | 70.2 ms | 1.32× |
| Laptop, load + process (median of 5) | 656.8 ms | 502.8 ms | 1.31× |
| Pi 5, process phase (median of 5) | 218.1 ms | 163.3 ms | 1.34× |
| Pi 5, load + process (median of 5) | 1962.0 ms | 1409.7 ms | 1.39× |

(Laptop measured 2026-11-15, Pi 5 2026-11-15/16, both with `benchmark/ladder_bench`,
`stress_full.csv`, `-O2`, `taskset -c 2`. V4 → V5 also changes the parser (straight to integers) and the module style,
so the process phase is the fair comparison. My first version, where only the number type
changed (`stageB_inmem` vs `stageB_fixed`, 2026-11-03), gave 79.9 vs 70.6 ms = 1.13×.)

- `test20.csv`: PASS (V5), laptop and Pi
- Pi `perf stat` (whole program): 33% fewer instructions (15.88 → 10.58 × 10⁹), IPC 3.03 → 2.80
- `dev_1M.csv` signals: 2553 BUY / 2469 SELL (V4 double: 2465 / 2464)
- `dev_1M.csv` VWAP: 76217.01291611 (V4 double: 76217.01291587; exact: 76217.01291611)
- `stress_full.csv`: 20943 BUY / 20060 SELL, VWAP 76718.67569889 (exact: 76718.67569889)

**Reason:** (2026-11-16) The speed prediction was right. Fixed-point was 1.32× faster on the
laptop and 1.34× faster on the Pi in the process phase, inside my 1.1 to 1.5×.

The gain is small because fixed-point changes the kind of arithmetic, while the amount of
work stays the same. Every trade still goes through VWAP, OHLC, SMA and the crossover, and
`TradeFx` is still 32 bytes, so the CPU reads the same amount of memory. The integer version
does need fewer instructions: the Pi's `perf stat` shows 33% fewer for V5, at a slightly
lower IPC (2.80 vs 3.03). The crossover compares `sum_short × 60` with `sum_long × 35`, so
there is no division, which avoids the risk I noted about slow whole-number division.

Some of the 1.32× comes from something other than the number type. V5 keeps its state in
structs, while V4 uses Joel's Stage A modules, and my first version, where only the number
type changed, gave 1.13×. Fixed-point on its own is worth about 1.1 to 1.3×.

The gain was bigger on the Pi by only a hair (1.34 vs 1.32), and my reason for expecting it
was weak. The Cortex-A76 adds doubles quickly, so `double` maths is not what makes the Pi
slow; on both CPUs the time goes on loading data and running the same steps for every trade.

The whole run got 1.31× (laptop) and 1.39× (Pi) faster, more than I predicted for the
program as a whole. The extra comes from the load phase. Lucas's reader turns the digits
straight into integers, which is cheaper than building a `double`, and that only works
because the prices are fixed-point.

Correctness was as predicted. The signal counts moved to the exact answer (2553 / 2469)
because on the 17,132 flat-price trades whole numbers give d = 0 exactly, while `double`
leaves a tiny error of random sign. The VWAP matched the `double` value to 6 decimal places
(76217.012916…) and differed in the 7th, the low end of my 6 to 8 places. The `double`
error is rounding from a million additions building up.

## Prediction 2 (Stage C): malloc array sized to the file vs fixed global array

**Date:** 2026-11-03  **Signed:** Leroi
**Status:** prediction written before the malloc version was implemented.

**Planned design:** read the file once just to count the trades, `malloc` exactly
`n × sizeof(Trade)` bytes (32 B each), check for `NULL`, read the file again to fill the
array, and `free` it at the end. The `Trade` struct and the engine loop stay the same; only
where the array lives changes (heap instead of a 320 MB global array).

**Expected:**

*Memory reserved (virtual size; Massif with `--pages-as-heap=yes`):*
- Fixed array: always **320 MB** (10,000,000 × 32 B), whatever the file size.
- malloc: exactly what the file needs, **~32 MB** for `dev_1M.csv` and **~286 MB** for
  `stress_full.csv`. That is a big saving on dev_1M (~288 MB) but only ~34 MB on stress_full.

*Memory actually used (peak physical memory, max RSS from `/usr/bin/time -v`):*
- **About the same** for both versions, ~32 MB + a few MB on dev_1M and ~290 MB on
  stress_full.
- The OS only gives real memory to the parts of the global array that the program writes
  to. The empty part of the fixed array is reserved but never handed physical memory, so it
  costs almost nothing. I expect the malloc saving to show up in the reserved size, not in
  the physical memory used.

*Speed:*
- **Process phase:** the same within ~3%. The trades sit one after another in memory in
  both cases and the loop does the same work, so the CPU reads the same bytes in the same
  order.
- **Load phase:** about **10–20% slower**, because the file is read twice. The counting
  pass only looks for newline characters and does not convert any numbers, so it should be
  much cheaper than the real parsing pass (parsing is ~97% of the run time).

*valgrind Memcheck (`--leak-check=full`, on `dev_1M.csv`):*
- **0 errors** and **0 bytes "definitely lost"**, because the array is freed at the end.
- If anything is reported, I expect it to be "still reachable" memory from the file
  handling in the helper library, not from my array.
- The fixed-array version will show almost nothing in the heap summary, because a global
  array is not heap memory.

*Correctness:* the signals, VWAP and OHLC will be identical to `stageB_inmem` on all three
files, and `test20.csv` will still pass.

*Pi vs laptop:* the same memory numbers on both machines, since the struct size is the same.
Both versions fit easily in the Pi 5's 4 GB. The real advantage of malloc on an edge device
is that it works for any file size: the fixed array fails above 10 million trades and wastes
reserved space on small files.

**Result:**

Measured on the group's `src/` ladder: V5 (fixed-point, fixed global array) vs V6 (the same,
with my `malloc` loader). Both use the 32-byte `TradeFx` struct.

| | V5 fixed global array | V6 malloc (sized to file) | Pi V5 | Pi V6 |
|---|---|---|---|---|
| `dev_1M`: reserved (Massif peak) | 325.0 MB | 37.0 MB | 324.6 MB | 36.6 MB |
| `dev_1M`: max RSS | 34.5 MB | 34.0 MB | 33.1 MB | 33.1 MB |
| `stress_full`: reserved (Massif peak) | 325.0 MB | 290.7 MB | 324.6 MB | 290.3 MB |
| `stress_full`: max RSS | 289.3 MB | 287.7 MB | 286.8 MB | 287.0 MB |
| Memcheck errors / definitely lost (`dev_1M`) | 0 errors / 0 B | 0 errors / 0 B ("All heap blocks were freed") | 0 errors / 0 B | 0 errors / 0 B ("All heap blocks were freed") |
| Load phase, stress_full (median of 5) | 432.5 ms | 569.0 ms (+31.6%) | 1246.5 ms | 1633.3 ms (+31.0%) |
| Process phase, stress_full (median of 5) | 70.2 ms | 69.9 ms (−0.4%) | 163.3 ms | 163.2 ms (−0.1%) |

(Laptop 2026-11-15, Pi 5 2026-11-16. Massif with `--pages-as-heap=yes`, single run. Max RSS is the median of
3 runs (`logs/leroi_pi/maxrss.c`). Times from `benchmark/ladder_bench`, `-O2`, `taskset -c 2`.
"dev_1M" rows = `./prog dev` (test20 + dev_1M); "stress_full" rows = full run.
MB = 10⁶ bytes. Raw output: `logs/leroi_laptop/`.)

- `test20.csv`: PASS (V6)
- Signals / VWAP match V5: yes, identical output on all three files (laptop and Pi)
- Memcheck "still reachable" from file handling: none (11 allocs, 11 frees), on both machines
- The Pi 5 measured here has 8 GB of RAM (7.9 GiB), not the 4 GB assumed above; both
  versions fit easily either way.
- (My first version, `stageC_malloc` vs `stageB_inmem` with `double` and the helper reader,
  2026-11-03: the same memory numbers, and load +7.7%.)

**Reason:** (2026-11-16) The memory part was right on both machines, but I underestimated
the load time.

The global array reserves 320 MB of addresses as soon as the program starts (it is in
`.bss`), so Massif with `--pages-as-heap=yes` always shows about 325 MB. The operating
system only hands out real memory for a page when the program first writes to it, though.
dev_1M writes only the first 32 MB of the array, so the other 288 MB never gets any RAM.
That is why `malloc` saves 288 MB of reserved memory on dev_1M but almost nothing in max RSS
(34.5 vs 34.0 MB on the laptop, 33.1 vs 33.1 MB on the Pi). On stress_full both versions
really use the ~286 MB the trades need. On the Pi, `malloc` even used 0.2 MB more on
stress_full: the Pi hands out memory in 16 KB pages instead of 4 KB, so small differences
round up, and 0.2 MB is noise.

The process phase was the same, as predicted (70.2 vs 69.9 ms on the laptop, 163.3 vs
163.2 ms on the Pi). The loop reads the same 32-byte structs one after another, and the CPU
does not care whether they sit in `.bss` or on the heap.

Loading was 31% slower instead of 10 to 20%. My estimate assumed parsing takes about 97% of
the time, as it did with `sscanf`, and with that slow reader my first version really was
only 7.7% slower. The ladder uses Lucas's fast parser, so the real reading pass is now
cheap, while the counting pass still has to read all 678 MB of text with `fgets`. That fixed
extra cost (about 136 ms on the laptop, 387 ms on the Pi) is a much bigger share of a fast
load than of a slow one. The Pi's `perf stat` agrees, with 24% more instructions and 47%
more cache misses for V6. How much a second read costs depends on how fast the real parse
is.

Memcheck was clean, as predicted: every `malloc` has a matching `free`, including on the
error paths. There was not even any "still reachable" memory, because the reader closes
each file with `fclose`, which frees its buffer. The fixed-array version showed only the
file buffers in the heap summary (13 KB on the laptop, 17 KB on the Pi), because a global
array is not heap memory.

The Pi turned out to have 8 GB, not 4 GB, so memory is even less of a limit than I assumed.
The benefit of `malloc` is still the one I predicted: it fits any file size, while the fixed
array stops at 10 million trades, and stress_full already uses 89% of that.

## Prediction 3: Processing from memory vs re-reading the file (streaming)

**Date:** 2026-11-03  **Signed:** Leroi
**Status:** written before the official (median of 5, pinned) runs. Already known
beforehand: one preliminary, unpinned laptop run of `stageB_inmem` on `stress_full.csv`
gave load 2812 ms and process 86 ms.

**Compared:** a streaming version (reads and processes one trade at a time with
`next_trade()`) vs `stageB_inmem` (load everything into the array first, then process).
Same engine modules, double prices, running-sum SMA, `-O2`, `stress_full.csv`.

**Expected:**

*Speed (laptop):*
- The in-memory **process phase alone** will be about **30–35× faster** than the whole
  streaming run (~2.9 s vs ~85 ms). Reading and parsing the text with `sscanf` is about 97%
  of the work, and the process phase skips it completely.
- **Load + process together** will take about the **same time as streaming, or up to 5%
  longer**. The file still has to be read and parsed once. On top of that, the in-memory
  version writes 286 MB into the array and then reads it back. So for one pass over the
  data, keeping it in memory does not save time overall. It pays off only when the same
  data is processed again (e.g. several runs or the replay).

*Speed (Pi 5):* the same pattern, with every time about **2–4× longer** than on the laptop.
The share spent parsing should stay around 95% or more.

*Memory:* streaming needs almost nothing (one trade at a time, max RSS of a few MB). The
in-memory version needs ~290 MB of physical memory for stress_full.

*Correctness:* both give identical output (same modules, same order of additions):
VWAP 76718.67569936, 20188 BUY / 20188 SELL on stress_full.

**Result:**

First measurement (the programs named above, helper reader, laptop, 2026-11-03,
stress_full, `-O2`, `taskset -c 2`, 5 runs, median):

| | Streaming (`stageB_stream`) | In-memory: process only | In-memory: load + process |
|---|---|---|---|
| Laptop time (median of 5) | 2988.1 ms | 79.0 ms | 2976.8 ms |
| Laptop speedup vs streaming | 1.00× | 37.8× | 1.004× |
| Laptop max RSS (stress_full run) | 1.9 MB | — | 289.0 MB |

Official measurement on the group's `src/` ladder: V2 (streaming, fast parser, naive SMA)
vs V3 (the same, loaded into the global array first). Laptop 2026-11-15, `benchmark/ladder_bench`.

| | V2 streaming | V3 process only | V3 load + process |
|---|---|---|---|
| Laptop time (median of 5) | 828.7 ms | 193.4 ms | 756.1 ms |
| Laptop speedup vs streaming | 1.00× | 4.28× | 1.10× |
| Pi 5 time (median of 5) | 2523.9 ms | 816.8 ms | 2558.9 ms |
| Pi 5 speedup vs streaming | 1.00× | 3.09× | 0.99× |
| Laptop max RSS (stress_full run) | 2.2 MB | — | 289.0 MB |
| Pi 5 max RSS (stress_full run) | 1.6 MB | — | 286.9 MB |

(Pi 5: 2026-11-15/16, same method.)

- Output identical: yes, on laptop and Pi (both: stress_full VWAP 76718.67569936, 20386 BUY / 20176
  SELL; the ladder's V2/V3 use the naive SMA, so the counts differ from the first measurement)
- On the laptop, with the fast parser, load + process was faster than streaming (1.10×),
  where I predicted the same time or slower. Loading is 74% of V3's time (it was ~97% with
  `sscanf`).
- On the Pi, load + process was 1.4% slower than streaming (0.99×), as predicted. Loading
  is 68% of V3's time, and the Pi takes 3.0 to 4.2× as long as the laptop (predicted 2 to
  4×).
- Pi `perf stat`: V3 has the same instructions per trade and IPC as V2 (3.01 vs 3.03), but
  2.8× more cache misses (14.5 M vs 5.2 M).

**Reason:** (2026-11-16) I wrote this prediction for the helper reader (`sscanf`), and for
those programs it was right. With the ladder's fast parser, two parts came out differently.

With `sscanf`, the process phase was 37.8× faster than streaming, in line with my 30 to
35×, because parsing was about 97% of the work. The ladder's V2 already uses Jaren's fast
parser, so parsing is now only about 70% of the time (74% on the laptop, 68% on the Pi), and
taking it out of the timed part saves much less: 4.28× on the laptop and 3.09× on the Pi.

On the Pi, load plus process was 0.99×, as predicted. V3 does the same parsing and the same
maths as V2 and also writes 286 MB into the array and reads it back. The Pi's `perf stat`
shows the same instructions per trade and the same IPC (3.01 vs 3.03), with 2.8× more cache
misses, and that extra memory traffic accounts for the whole 1.4%.

On the laptop, load plus process was 1.10× faster, which I did not predict and cannot fully
explain. The Pi's counters show that V3 does the same work as V2. My best guess is that the
laptop's larger caches and much wider out-of-order core run two simple loops (parse
everything, then compute everything) better than one loop that switches between parsing and
maths on every trade. Checking it would need `perf stat` for V2 and V3 on the laptop.

The Pi took 3.0 to 4.2× as long as the laptop. The 4.2× (V3's naive process phase) is a
little above my 2 to 4×; Prediction 4 explains why the naive SMA hits the Pi harder. Memory
was as predicted: 2.2 MB (laptop) and 1.6 MB (Pi) for streaming, and about 287 to 289 MB in
memory.

The conclusion still holds, most clearly on the Pi, which is the edge device. For one pass
over the data, loading it into memory first does not save time. It pays off only when the
same data is used again, as in the replay or when several strategies run on one load.

## Prediction 4: Naive vs running-sum SMA on the Pi 5

**Date:** 2026-11-03  **Signed:** Leroi
**Status:** written before the Pi runs. The laptop result was already measured
(process phase, stress_full, median of 5, pinned): naive 200.4 ms, running sum 76.2 ms,
**2.63×**. So this entry predicts only the Pi.

**Expected:**
- The Pi speedup will be **larger than the laptop's, about 3–5×**.
- The naive version does 95 additions per trade for the SMAs, against 4 for the running
  sum. The laptop's CPU is wide and can run many additions at once, which hides much of the
  extra work. The Pi 5's Cortex-A76 is a simpler core, so the extra additions should cost
  relatively more.
- It will still be nowhere near the ~24× the operation count suggests. VWAP, OHLC and the
  crossover check cost the same in both versions, and the window (35/60 doubles) always
  stays in the L1 cache.

**Result:**

Measured on the group's `src/` ladder: V3 (naive) vs V4 (running sum), process phase,
stress_full, `-O2`, `taskset -c 2`. The laptop baseline for the Pi prediction is now the
ladder's own laptop number (2.08×, 2026-11-15); the 2.63× in the status line came from my
first version (`stageB_inmem` with and without `-DNAIVE_SMA`).

| | V3 naive | V4 running sum | Speedup |
|---|---|---|---|
| Laptop, process phase (median of 5) | 193.4 ms | 92.8 ms | 2.08× |
| Pi 5, process phase (median of 5) | 816.8 ms | 218.1 ms | 3.75× |

(Pi 5: 2026-11-15/16, `benchmark/ladder_bench`, `-O2`, `taskset -c 2`. Pi `perf stat`: 23%
fewer instructions with the running sum (20.55 → 15.88 × 10⁹), IPC 3.01 → 3.03. Whole run
2558.9 → 1962.0 ms, 1.30×.)

**Reason:** (2026-11-16) The prediction was right: 3.75× on the Pi, inside 3 to 5× and well
above the laptop's 2.08×. The laptop baseline is now 2.08×, not 2.63×, because the ladder's
V4 uses Joel's Stage A modules; the Pi gain is larger than either figure.

The Pi's `perf stat` shows that the running sum removes about 470 instructions per trade
(23% of the whole program) while the IPC stays at about 3. On the Pi, the naive version takes
67 ns more per trade (91.5 vs 24.4 ns), and running 470 extra instructions at IPC 3 and
2.4 GHz takes about 65 ns, so the Pi pays the full price for every extra addition.

On the laptop, the naive version costs only 11 ns more per trade for the same extra
instructions. Its much wider core runs them out of order alongside the rest of each trade's
work, so most of the cost is hidden. That matches the reason I gave: the laptop's wide CPU
hides the extra additions, and the Pi's simpler core feels them.

The speedup is still far from the ~24× that the operation count (95 vs 4) suggests, for the
reasons I gave. VWAP, OHLC and the crossover cost the same in both versions, and the 35- and
60-price windows always stay in the L1 cache, so each extra addition is cheap.

This rung matters more on the edge device than on the laptop: the whole run gets 1.30×
faster on the Pi and only 1.15× faster on the laptop.

## Prediction 5: Do the laptop and the Pi give exactly the same answers?

**Date:** 2026-11-03  **Signed:** Leroi
**Status:** written before any of my programs has been run on the Pi.

**Expected:**
- **Fixed-point (`stageB_fixed`): identical** on both, to the last digit. Whole-number
  arithmetic has no rounding, so the CPU makes no difference.
- **Double versions, signal counts: identical.** The SMA sums only add and subtract, and
  both CPUs follow the same IEEE 754 rules for double, so each addition rounds the same way.
- **Double versions, VWAP: might differ in the last printed digits.** On ARM, gcc can join
  `sum += p * q` into one "fused multiply-add" instruction that rounds once instead of
  twice. The laptop build does not use FMA by default. If that happens, `sum_pq` drifts
  slightly, but the signals do not change because they do not use VWAP.
- `test20.csv` will pass on the Pi for every version.

**Result:**

Checked on the group's `src/` ladder (the same three kinds of version as named above).
Laptop: 2026-11-15, `-O2`, from `benchmark/ladder_bench` logs.

| Version | Laptop VWAP / BUY / SELL (stress_full) | Pi 5 VWAP / BUY / SELL (stress_full) | Same? |
|---|---|---|---|
| V4 (double, running sum) | 76718.67569936 / 20188 / 20188 | 76718.67569936 / 20188 / 20188 | yes |
| V3 (double, naive) | 76718.67569936 / 20386 / 20176 | 76718.67569936 / 20386 / 20176 | yes |
| V5 (fixed-point) | 76718.67569889 / 20943 / 20060 | 76718.67569889 / 20943 / 20060 | yes |
| V6 (fixed-point, malloc) | 76718.67569889 / 20943 / 20060 | 76718.67569889 / 20943 / 20060 | yes |
| test20, every version | PASS | PASS (all 45 builds, V0–V8, every flag set) | yes |

(Pi 5: gcc 14.2.0, `-O2`, 2026-11-15/16. dev_1M also identical for every version. The
`double` VWAP matches to all 8 printed decimals, so if gcc used fused multiply-add on the
Pi, it did not change any printed digit.)

**Reason:** (2026-11-16) Every answer was identical on the two machines, so the prediction
was right, apart from the VWAP warning, which turned out to be too small to see.

Whole-number addition and multiplication are exact, so the fixed-point versions give the
same answer on any CPU. For the `double` signal counts, IEEE 754 requires every addition,
subtraction and division to give the correctly rounded result, so x86 and ARM produce
bit-for-bit the same SMA sums when the code does the operations in the same order. The same
sums carry the same tiny rounding errors, so the same signals appear and disappear.

For the `double` VWAP, I checked the machine code. On the Pi, gcc 14 turned
`sum_pq += price * qty` into one `fmadd` (fused multiply-add) instruction, while the
laptop's x86-64 build has no FMA instruction at all. The Pi therefore rounds that line once
instead of twice, and its `sum_pq` really is slightly different. Each rounding difference is
about 10⁻¹⁶ of the value, though, and even after millions of additions the difference stays
far below the 8th decimal place of the VWAP that the program prints. The effect I predicted
exists but does not change any printed digit.

`test20.csv` passed on the Pi for every version at every optimisation level (all 45 builds).

## Findings for the report

### Fixed-point fixes a real precision bug that `test20.csv` can't catch

**Date:** 2026-11-03  **Noted by:** Leroi

Every `double` version gives a different signal count on `dev_1M.csv`, and none of them is
the right one:

  | Version | BUY | SELL | VWAP |
  |---|---|---|---|
  | Stage A (`stageA_stream.c`, double, running sum) | 2465 | 2464 | 76217.01291587 |
  | Stage B naive SMA (double) | 2509 | 2469 | 76217.01291587 |
  | **Stage B fixed-point** | **2553** | **2469** | **76217.01291611** |
  | Exact (integer / Python Decimal check) | 2553 | 2469 | 76217.01291611 |

On 17,132 trades in `dev_1M.csv` (156,471 in `stress_full.csv`) the price is flat, so the
short and long SMA are exactly equal. With `double`, rounding leaves a tiny non-zero
difference of effectively random sign, so signals appear or disappear, and the running sum
and the naive sum add in a different order, so they round differently. Fixed-point
(whole-number) sums are exact, so `d = 0` exactly and the brief's `≤ 0` / `> 0` rule is
applied as written.

The group's `src/` ladder confirmed this on 2026-11-15: V0 to V3 (double, naive) give
2509 / 2469, V4 (double, running sum) gives 2465 / 2464, and V5 to V7 (fixed-point) give
2553 / 2469.

`test20.csv` passes for every version because 20 trades are too few for rounding error to
matter. Passing a check on a small file does not prove a version is correct on the full
data.
