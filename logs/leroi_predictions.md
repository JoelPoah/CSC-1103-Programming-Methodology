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
| Laptop, process phase (median of 5) | 89.9 ms | 72.0 ms | 1.25× |
| Laptop, load + process (median of 5) | 662.7 ms | 509.0 ms | 1.30× |
| Pi 5, process phase (median of 5) | 218.1 ms | 162.9 ms | 1.34× |
| Pi 5, load + process (median of 5) | 1958.5 ms | 1408.2 ms | 1.39× |

(Laptop and Pi 5 measured 2026-11-17 with the `performance` CPU governor, both with
`benchmark/ladder_bench`, `stress_full.csv`, `-O2`, `taskset -c 2`. V4 → V5 also changes the parser (straight to integers) and the module style,
so the process phase is the fair comparison. My first version, where only the number type
changed (`stageB_inmem` vs `stageB_fixed`, 2026-11-03), gave 79.9 vs 70.6 ms = 1.13×.
First ladder run, 2026-11-15/16 with the default governors: laptop process 92.8 vs 70.2 ms
= 1.32×, load + process 1.31×; Pi process 218.1 vs 163.3 ms = 1.34×, load + process 1.39×.
The Reason below was written from that run.)

- `test20.csv`: PASS (V5), laptop and Pi
- Pi `perf stat` (whole program): 33% fewer instructions (15.88 → 10.58 × 10⁹), IPC 3.03 → 2.80
- Laptop `perf stat` (whole program): 31% fewer instructions (16.00 → 11.02 × 10⁹), IPC 4.82 → 4.31
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
when rounded (both 76217.012916; the difference is 2.4×10⁻⁷), the low end of my 6 to 8
places. The `double` error is rounding that builds up over a million additions.

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
| Load phase, stress_full (median of 5) | 437.0 ms | 572.8 ms (+31.1%) | 1245.2 ms | 1636.4 ms (+31.4%) |
| Process phase, stress_full (median of 5) | 72.0 ms | 70.6 ms (−1.9%) | 162.9 ms | 163.7 ms (+0.5%) |

(Memory: laptop 2026-11-15, Pi 5 2026-11-16. Massif with `--pages-as-heap=yes`, single run. Max RSS is the median of
3 runs (`logs/leroi_pi/maxrss.c`). Times: 2026-11-17 rerun with the `performance` CPU governor,
`benchmark/ladder_bench`, `-O2`, `taskset -c 2`.
"dev_1M" rows = `./prog dev` (test20 + dev_1M); "stress_full" rows = full run.
MB = 10⁶ bytes. Raw output: `logs/leroi_laptop/` and `logs/leroi_pi/`.
First ladder run, 2026-11-15/16 with the default governors: load +31.6% (laptop) / +31.0% (Pi),
process −0.4% / −0.1%. The Reason below was written from that run.)

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
dev_1M writes only the first 32 MB of the array, so the other 288.0 MB never gets any RAM.
That is why `malloc` saves 288.0 MB of reserved memory on dev_1M but almost nothing in max RSS
(34.5 vs 34.0 MB on the laptop, 33.1 vs 33.1 MB on the Pi). On stress_full both versions
really use the ~286 MB the trades need. On the Pi, `malloc` even used 0.2 MB more on
stress_full: the Pi hands out memory in 16 KB pages instead of 4 KB, so small differences
round up, and 0.2 MB is noise.

The process phase was the same, as predicted (70.2 vs 69.9 ms on the laptop, 163.3 vs
163.2 ms on the Pi). The loop reads the same 32-byte structs one after another, and the CPU
does not care whether they sit in `.bss` or on the heap.

Loading was 31.6% (laptop) and 31.0% (Pi) slower instead of 10 to 20%. My estimate assumed parsing takes about 97% of
the time, as it did with `sscanf`, and with that slow reader my first version really was
only 7.7% slower. The ladder uses Lucas's fast parser, so the real reading pass is now
cheap, while the counting pass still has to read all 678 MB of text with `fgets`. That fixed
extra cost (about 136 ms on the laptop, 387 ms on the Pi) is a much bigger share of a fast
load than of a slow one. The Pi's `perf stat` agrees, with 24% more instructions and 47%
more cache misses for V6.

Memcheck was clean, as predicted: every `malloc` has a matching `free`, including on the
error paths. There was not even any "still reachable" memory, because the reader closes
each file with `fclose`, which frees its buffer. The fixed-array version showed only the
file buffers in the heap summary (13 kB on the laptop, 17 kB on the Pi), because a global
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
vs V3 (the same, loaded into the global array first). Laptop and Pi 5 2026-11-17,
`benchmark/ladder_bench`, `performance` CPU governor.

| | V2 streaming | V3 process only | V3 load + process |
|---|---|---|---|
| Laptop time (median of 5) | 826.7 ms | 194.0 ms | 771.2 ms |
| Laptop speedup vs streaming | 1.00× | 4.26× | 1.07× |
| Pi 5 time (median of 5) | 2518.7 ms | 817.3 ms | 2556.9 ms |
| Pi 5 speedup vs streaming | 1.00× | 3.08× | 0.99× |
| Laptop max RSS (stress_full run) | 2.2 MB | — | 289.0 MB |
| Pi 5 max RSS (stress_full run) | 1.6 MB | — | 286.9 MB |

(Max RSS: 2026-11-15/16. First ladder run, 2026-11-15/16 with the default governors: laptop
828.7 / 193.4 / 756.1 ms (process 4.28×, load + process 1.10×), Pi 2523.9 / 816.8 / 2558.9 ms
(3.09×, 0.99×). The Reason below was written from that run.)

- Output identical: yes, on laptop and Pi (both: stress_full VWAP 76718.67569936, 20386 BUY / 20176
  SELL; the ladder's V2/V3 use the naive SMA, so the counts differ from the first measurement)
- On the laptop, with the fast parser, load + process was faster than streaming (1.07×),
  where I predicted the same time or slower. Loading is 75% of V3's time (it was ~97% with
  `sscanf`).
- On the Pi, load + process was 1.5% slower than streaming (0.99×), as predicted. Loading
  is 68% of V3's time, and the Pi takes 3.01 to 4.21× as long as the laptop (predicted 2 to
  4×).
- Pi `perf stat`: V3 has the same instructions per trade and IPC as V2 (3.01 vs 3.03), but
  2.48× more cache misses per trade (1.47 vs 0.59: 14.61 M misses over 9.93 M trades, since V3
  also runs test20 and dev_1M, vs 5.30 M over 8.93 M).
- Laptop `perf stat` (measured in the rerun): V3 has the same instructions per trade as V2
  (1845 vs 1833), but a 9% higher IPC (4.63 vs 4.23), so 8% fewer cycles per trade (398 vs 434).

**Reason:** (2026-11-16) I wrote this prediction for the helper reader (`sscanf`), and for
those programs it was right. With the ladder's fast parser, two parts came out differently.

With `sscanf`, the process phase was 37.8× faster than streaming, in line with my 30 to
35×, because parsing was about 97% of the work. The ladder's V2 already uses Jaren's fast
parser, so parsing is now only about 70% of the time (74% on the laptop, 68% on the Pi), and
taking it out of the timed part saves much less: 4.28× on the laptop and 3.09× on the Pi.

On the Pi, load plus process was 0.99×, as predicted. V3 does the same parsing and the same
maths as V2 and also writes 286 MB into the array and reads it back. The Pi's `perf stat`
shows the same instructions per trade and the same IPC (3.01 vs 3.03), with 2.50× more cache
misses per trade, and that extra memory traffic accounts for the whole 1.4%.

On the laptop, load plus process was 1.10× faster, which I did not predict and cannot fully
explain. The Pi's counters show that V3 does the same work as V2. My best guess is that the
laptop's larger caches and much wider out-of-order core run two simple loops (parse
everything, then compute everything) better than one loop that switches between parsing and
maths on every trade. Checking it would need `perf stat` for V2 and V3 on the laptop.

The Pi took 3.05 to 4.22× as long as the laptop. The 4.22× (V3's naive process phase) is a
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
ladder's own laptop number (2.16× in the 2026-11-17 rerun, 2.08× in the first run); the
2.63× in the status line came from my first version (`stageB_inmem` with and without
`-DNAIVE_SMA`).

| | V3 naive | V4 running sum | Speedup |
|---|---|---|---|
| Laptop, process phase (median of 5) | 194.0 ms | 89.9 ms | 2.16× |
| Pi 5, process phase (median of 5) | 817.3 ms | 218.1 ms | 3.75× |

(Laptop and Pi 5: 2026-11-17, `benchmark/ladder_bench`, `performance` CPU governor, `-O2`,
`taskset -c 2`. Pi `perf stat`: 23% fewer instructions with the running sum (20.55 → 15.88 ×
10⁹), IPC 3.01 → 3.03. Whole run 2556.9 → 1958.5 ms, 1.31×. First ladder run, 2026-11-15/16
with the default governors: laptop 193.4 → 92.8 ms (2.08×), Pi 816.8 → 218.1 ms (3.75×),
whole run on the Pi 1.30×. The Reason below was written from that run.)

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
Laptop: 2026-11-17, `-O2`, from `benchmark/ladder_bench` logs (the first run, 2026-11-15,
gave the same answers).

| Version | Laptop VWAP / BUY / SELL (stress_full) | Pi 5 VWAP / BUY / SELL (stress_full) | Same? |
|---|---|---|---|
| V4 (double, running sum) | 76718.67569936 / 20188 / 20188 | 76718.67569936 / 20188 / 20188 | yes |
| V3 (double, naive) | 76718.67569936 / 20386 / 20176 | 76718.67569936 / 20386 / 20176 | yes |
| V5 (fixed-point) | 76718.67569889 / 20943 / 20060 | 76718.67569889 / 20943 / 20060 | yes |
| V6 (fixed-point, malloc) | 76718.67569889 / 20943 / 20060 | 76718.67569889 / 20943 / 20060 | yes |
| test20, every version | PASS | PASS (all 50 builds, V0 to V8, every flag set) | yes |

(Pi 5: gcc 14.2.0, `-O2`, 2026-11-17; the first run, 2026-11-15/16 with 45 builds, gave the
same answers. dev_1M also identical for every version. The
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
far below the 8th decimal place of the VWAP that the program prints, so the effect I
predicted is real but never reaches a printed digit.

`test20.csv` passed on the Pi for every version at every optimisation level (all 45 builds).

## Prediction 6 (stretch goal): Struct-of-Arrays vs array of structs

**Date:** 2026-11-19  **Signed:** Leroi
**Status:** written before the Struct-of-Arrays version is written or run. Nothing in it has
been measured yet.

**What is compared:** V5 (one global array of `TradeFx` structs) against a new version that
is V5 with each field in its own global array (`soa_time[]`, `soa_price[]`, `soa_qty[]`,
`soa_is_buy[]`). The reader, the fixed-point modules and the loop's work stay the same; only
the memory layout changes. Measured on `stress_full.csv`, `-O2`, `taskset -c 2`,
`performance` governor, median of 5, laptop and Pi.

**Expected:**

*Process phase: a small gain, about 0–10% faster* (laptop 72.0 ms → about 65–72 ms;
Pi 162.9 ms → about 147–163 ms).
- `TradeFx` is 32 bytes, so a 64-byte cache line holds 2 trades. The loop reads `time`,
  `price` and `qty` (24 bytes) and never reads `is_buy` (only the test20 table prints it).
  With separate arrays it reads 24 bytes per trade instead of 32, so 25% fewer cache lines
  (0.375 instead of 0.5 per trade).
- But I don't think memory is what limits this loop. V5 reads the array at only about
  1.75 GB/s on the Pi (286 MB in 163 ms) and 4 GB/s on the laptop, and it reads it in order,
  so the hardware prefetcher can load the next lines early. Each trade takes about
  44 cycles on the Pi, mostly spent on the work itself (the calls to the VWAP, OHLC, SMA
  and crossover functions in other `.c` files, and the updates inside them). Fewer bytes per trade should help only a little.
- It could even come out the same or slightly slower: the loop now follows three arrays
  instead of one, which needs more registers and three address streams.

*Pi vs laptop:* about the same small gain on both. If one gains more, it will be the Pi,
because its memory system and caches are smaller.

*Load phase and whole run: no real change (within ±3%).* Loading is 88% (Pi) and 86%
(laptop) of V5's run and is limited by reading and parsing the text, which does not change.
So even a 10% faster process phase moves the whole run by only about 1%.

*Memory: clearly less, and this I can predict exactly.*
- Reserved (global arrays): 3 × 80 MB + 40 MB = **280 MB** instead of 320 MB.
- Physical (max RSS) on stress_full: the trades need 28 bytes each instead of 32, so about
  **250 MB of arrays instead of 286 MB**. Max RSS about 251 MB (Pi) and 254 MB (laptop),
  against 286.8 / 289.3 MB for V5. The 4 bytes saved per trade are the padding at the end
  of every struct.

*perf stat:* about the same number of instructions (±3%). Somewhat fewer level-1 cache
misses on the Pi (about 5–15% fewer for the whole program), because the process phase loads
fewer lines; the load phase, which is most of the run, does not change.

*Correctness:* exactly the same answers as V5, because only where the numbers are stored
changes: `test20.csv` passes, dev_1M VWAP 76217.01291611 with 2553 BUY / 2469 SELL,
stress_full VWAP 76718.67569889 with 20943 BUY / 20060 SELL, the same on the laptop and the
Pi.

**Result:**

Laptop, 2026-11-19: V5 and V5-SoA in the same session, one warm-up run each, then 5 runs
alternating the two, `stress_full.csv`, `-O2`, `taskset -c 2`, `performance` governor.
Raw logs: `logs/leroi_laptop/soa_2026-11-19/`.

| | V5 (array of structs) | V5-SoA | Change | Predicted |
|---|---|---|---|---|
| Process phase (median of 5) | 68.6 ms | 66.8 ms | 1.03× faster (−2.7%) | 0–10% faster |
| Load phase (median of 5) | 434.4 ms | 433.8 ms | −0.1% | ±3% |
| Whole stress_full run, load + process (median of 5) | 502.9 ms | 500.4 ms | −0.5% | ±3% |
| Reserved (`.bss`, from `size`) | 320.0 MB | 280.0 MB | −40.0 MB | 280 MB |
| Max RSS, full run (median of 3) | 289.5 MB | 259.2 MB | −30.2 MB | about 254 MB |
| Instructions (`perf stat`, whole program) | 11.02 × 10⁹ | 11.08 × 10⁹ | +0.5% | ±3% |
| Cycles | 2.59 × 10⁹ | 2.55 × 10⁹ | −1.3% | |
| L3 references | 25.55 M | 24.19 M | −5.3% | |
| L3 misses | 21.67 M | 19.91 M | −8.1% | (predicted for the Pi's L1: 5–15% fewer) |

- The 5 process times varied by only 0.3–0.4% (standard deviation ÷ mean) for each
  version, so the 2.7% gain is larger than the run-to-run noise.
- One SoA load time was slower (486.2 ms in run 4); the median is not affected.
- Output identical to V5 on all three files (test20 passes; dev_1M VWAP 76217.01291611,
  2553 BUY / 2469 SELL; stress_full VWAP 76718.67569889, 20943 BUY / 20060 SELL).
- Max RSS is 5 MB above my prediction. Possible cause (not checked): this laptop has
  transparent huge pages set to `always`, so the memory may be given out in 2 MB blocks, and
  each of the four arrays then ends in a partly used block, where V5's single array has one.
- V5's times in this session are 0.6–4.7% faster than in the official 2026-11-17 run
  (process / load / total 72.0 / 437.0 / 509.0 ms), which is why both versions were timed
  together here and compared only with each other.
- Pi: TODO (not measured yet).

**Reason:** (2026-11-19, laptop only; the Pi part is still to be measured) Most of the
prediction was right. The process phase got 2.7% faster, inside the 0–10% I predicted, and
the load phase and the whole run did not change.

The speedup comes from moving fewer cache lines, as I expected. The number of instructions
stayed the same (+0.5%), so the CPU did the same work. What changed is the memory traffic:
on stress_full the process phase now reads 214 MB instead of 286 MB (3.35 M cache lines
instead of 4.46 M), and the load phase writes 250 MB instead of 286 MB (3.91 M lines
instead of 4.46 M). That is about 1.7 M fewer lines, and `perf stat` counted 1.76 M fewer
L3 misses, so the counters match the cache-line count.

The gain is small for the reason I gave: memory was not what limited the loop. V5 reads its
array in order at only about 4 GB/s, so the hardware prefetcher already fetches the next
lines before they are needed. Most of the 7.7 ns per trade is the work itself (the calls
into the VWAP, OHLC, SMA and crossover functions), so 25% fewer bytes saves only 2.7% of the
time. The whole run moved by only 0.5% because loading is 86% of it, and loading is limited
by reading and parsing the text, which did not change.

The memory saving was clear, as predicted: 40 MB less reserved (280 MB, exactly as
calculated) and 30 MB less physical memory, because the four arrays drop the 4 bytes of
padding at the end of every struct. I was 5 MB too low on the max RSS (259 MB, not 254 MB).
My prediction assumed the RAM used is just the data plus V5's small overhead. A possible
reason it is higher, which I have not checked, is that this laptop's transparent huge pages
are set to `always`, so memory may be given out in 2 MB blocks, and four arrays end in four
partly used blocks where V5's one array ends in one.

My cache-miss prediction was for the Pi's L1 cache, so it is still to be checked. On the
laptop, perf counts the L3 cache, and those misses fell by 8.1%.

Conclusion so far: Struct-of-Arrays is worth it here for the memory (30 MB less) more than
for the speed (2.7% on the process phase, 0.5% on the whole run).

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
2553 / 2469. The 2026-11-17 rerun gave the same counts, and both new V8 builds also give
2553 / 2469.

`test20.csv` passes for every version because 20 trades are too few for rounding error to
matter. Passing a check on a small file does not prove a version is correct on the full
data.
