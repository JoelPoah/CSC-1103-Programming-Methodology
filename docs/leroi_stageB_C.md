# Leroi: Stage B and Stage C working notes

Working documentation for my parts of the project (Group 06). Each section records what I built, why, how it was checked, and how it compares with the version before it. These notes feed the report (`docs/leroi_report_draft.md`), the efficiency ladder (with Yong Sheng) and my prediction logbook (`logs/leroi_predictions.md`).

The official versions are the `src/` efficiency ladder (decided 2026-11-15). Joel ported my Stage B/C code into it, with the group's fast readers, so every comparison below is between two ladder rungs built by `make ladder`:

| My comparison | Rungs | What changes |
|---|---|---|
| In-memory vs re-reading the file | V2 → V3 | stream line by line → load into a global array, then process |
| Naive vs running-sum SMA | V3 → V4 | `v0-v3_sma_naive.c` → `v4_sma_running.c` |
| `double` vs fixed-point | V4 → V5 | `double` → `long long` prices (also a new parser and module style, see §3) |
| Fixed global array vs `malloc` | V5 → V6 | `v5_trades_fx_array.c` → `v6_trades_fx_malloc.c` |

My first versions (`stageB/stageB_inmem.c`, `stageB_stream.c`, `stageB_fixed.c`, `stageC/stageC_malloc.c`, using the course helper reader) are kept as development history. Their numbers are in §6 and are not for the report's tables.

## Measurement rules (from the project brief)

| Rule | Where it comes from |
|---|---|
| Benchmark on **`stress_full.csv`** (8,928,944 trades) | Ladder + performance sections |
| Time with the provided **`now_ns()`** | General Steps 5 |
| Run **at least 5 times**, report the **median** | General Steps 5 |
| Measure on **both laptop (CSC1103) and Pi 5 (CSC1104)** | Stage 0 / Testing 1 |
| Pin to one core (`taskset -c 2`) (CPU governor left at default) | Project outline |
| Report **time, time per trade (ns) and throughput (trades/s)** | Formulas 8, Testing 1 |
| Report **speedup vs Version 0** = time(V0) / time(Vk) | Formulas 8, Stage B 4 |
| Report **memory usage** for every version | Testing 1 |
| Report **`perf stat`**: cycles, instructions, IPC, cache misses (Pi) — **in every member's own write-up** | General Steps 9, Testing 1 |
| Every version must still **pass `test20.csv`** | Stage C note |
| **No plot / `.dat` file writing** in timed versions | General Steps 8 |
| Write the **prediction before measuring** (logbook entry, dated + signed) | General Steps 10 |
| Group logbook: **≥ 6 entries, at least one per stage** (0, A, A+, B, C) | Testing 2 |
| Report must include a **trade-off analysis** and a **deployment recommendation** for a power-/memory-limited device (Pi 5) | Testing 3 |

Formulas:
- time per trade = total time (ns) / number of trades
- throughput = number of trades / total time (s)
- speedup_k = time of Version 0 / time of version k

Version 0 is `bin/v0_stageA_naive_sma` at `-O2` (Joel's Stage A: helper reader with `fgets` + `sscanf`, `double`, naive SMA, streaming), as agreed by the group.

## How the numbers were taken

Time, throughput and speedup come from Yong Sheng's `./benchmark/ladder_bench -m leroi c`. It compiles every rung, checks test20 (PASS/FAIL), runs 1 warm-up and 5 timed runs pinned to core 2, and reports the median. I use the `-O2` builds, which are what `make ladder` builds; the other flag sets belong to Yong Sheng's section. Raw results are in `benchmark/results/leroi/<machine>/` (`summary.txt`, `ladder/logs/<version>_O2.txt`).
- V0 to V2 stream the file, so they are timed as a whole program with `now_ns()`.
- V3 and later time themselves with `now_ns()`: Load (file → array) plus Process (VWAP, OHLC, SMA and crossover over the array) for the stress_full section. They always run test20 and dev_1M first, which is not counted.

Memory (max RSS) and valgrind come from my script `logs/leroi_pi/run_leroi_pi.sh`, with laptop results in `logs/leroi_laptop/` and Pi results in `logs/leroi_pi/`.
- Max RSS uses `logs/leroi_pi/maxrss.c` (fork, `execvp`, `wait4`: the same counter as `/usr/bin/time -v`), median of 3. MB = 10⁶ bytes.
- `ladder_bench` also prints a peak memory, in MiB (2²⁰ bytes), but it can never go below about 15.6 MiB. It measures from a forked copy of itself, and Linux keeps the copy's memory high-water mark across `exec`, so it shows ~15.7 MiB for V0 to V2, which really use 2.2 MB. Its numbers for V3 and later agree with mine (e.g. V5 275.8 MiB = 289.2 MB).
- Memcheck (`--leak-check=full --show-leak-kinds=all`) and Massif run on `./prog dev` (test20 + dev_1M), because valgrind is 20 to 50× slower. Massif `--pages-as-heap=yes` was also run on the full stress_full run for V5 and V6 (`STRESS_MASSIF=1`), on both machines.

`ladder_bench` runs `perf stat` on the Pi only: one extra untimed run of the whole program, so V3 and later include their test20 + dev_1M part, about 11% more trades.

The Pi run went from 2026-11-15 23:44 to 2026-11-16 00:13. I started it from the laptop over SSH inside `tmux` (`~/run_all.sh` on the Pi; the log is copied to `logs/leroi_pi/run_all.txt`). The Pi was idle and on mains power at 54 to 59 °C, and `vcgencmd get_throttled` read `0x0` (never throttled) before, between and after the two parts.

## System set-up

| | Laptop | Raspberry Pi 5 |
|---|---|---|
| Machine | Framework Laptop 13 Pro | Raspberry Pi 5 Model B Rev 1.1 (hostname `abcpi`) |
| CPU | Intel Core Ultra X7 358H, 16 cores, up to 4.7 GHz | Arm Cortex-A76 ×4, up to 2.4 GHz |
| RAM | 32 GB (30.9 GB usable) | **8 GB** (7.9 GiB; + 2 GiB swap, unused) |
| OS / kernel | Arch Linux, Linux 7.2.15 | Debian 13 (trixie), Linux 6.18.39+rpt-rpi-2712 (aarch64) |
| Page size | 4 KB | 16 KB |
| Compiler | gcc 16.2.1 | gcc 14.2.0 (Debian 14.2.0-19) |
| Flags | `-O2 -Wall` | `-O2 -Wall` (same as the laptop) |
| CPU governor | `powersave` (left as default) | `ondemand` (left as default) |
| Pinning | `taskset -c 2` | `taskset -c 2` |
| Tools | valgrind 3.25.1 | valgrind 3.24.0, perf (`linux-perf` 6.18.39), `kernel.perf_event_paranoid=1` |
| Measured | 2026-11-15, commit `4e8a234` | 2026-11-15/16, commit `4e8a234` (same code) |

The Pi ran at full clock: V0's 25.05 × 10⁹ cycles in 10.44 s = 2.40 GHz, so the `ondemand` governor kept core 2 at its maximum during the runs.

## The ladder at a glance (`-O2`, stress_full)

Laptop (2026-11-15, `ladder_bench`, 5 runs, median). Speedup vs V0 = 3111.9 ms / time.

| Version | What changed | Total | ns/trade | trades/s | Speedup vs V0 | Load | Process | Max RSS |
|---|---|---|---|---|---|---|---|---|
| V0 `v0_stageA_naive_sma` | baseline: helper reader, `double`, naive SMA, streaming | 3111.9 ms | 348.5 | 2.87 M | 1.00× | — | — | 2.2 MB |
| V1 `v1_stageAplus_own_reader` | own `read_trade()`, still `sscanf` | 3081.7 ms | 345.1 | 2.90 M | 1.01× | — | — | 2.2 MB |
| V2 `v2_stageAplus_fast_parser` | digit-by-digit parser (Jaren) | 828.7 ms | 92.8 | 10.77 M | 3.76× | — | — | 2.2 MB |
| **V3** `v3_stageB_in_memory` | **global array, load then process** | 756.1 ms | 84.7 | 11.81 M | 4.12× | 562.4 ms | 193.4 ms | 289.0 MB |
| **V4** `v4_stageB_running_sma` | **running-sum SMA** | 656.8 ms | 73.6 | 13.59 M | 4.74× | 564.0 ms | 92.8 ms | 287.9 MB |
| **V5** `v5_stageB_fixed_point` | **fixed-point prices** | 502.8 ms | 56.3 | 17.76 M | **6.19×** | 432.5 ms | 70.2 ms | 289.3 MB |
| **V6** `v6_stageC_malloc` | **`malloc`'d array (file read twice)** | 638.8 ms | 71.5 | 13.98 M | 4.87× | 569.0 ms | 69.9 ms | 287.7 MB |
| V7 `v7_stageC_fread` | one `fread` of the whole file (Lucas) | 700.0 ms | 78.4 | 12.76 M | 4.45× | 629.9 ms | 70.1 ms | 965.4 MB |

Bold rows are my rungs. Every version passes test20. Runs varied by 0.2 to 0.6% (standard deviation), except V2 (one run of 941 ms) and V3 (one run of 803 ms); the median ignores them.

Pi 5 (2026-11-15/16, `ladder_bench`, 5 runs, median). Speedup vs V0 = 10439.0 ms / time. The last column is how many times longer the Pi takes than the laptop for the same version.

| Version | Total | ns/trade | trades/s | Speedup vs V0 | Load | Process | Max RSS | Pi ÷ laptop |
|---|---|---|---|---|---|---|---|---|
| V0 | 10439.0 ms | 1169.1 | 0.86 M | 1.00× | — | — | 1.6 MB | 3.35× |
| V1 | 10492.4 ms | 1175.1 | 0.85 M | 0.99× | — | — | 1.6 MB | 3.40× |
| V2 | 2523.9 ms | 282.7 | 3.54 M | 4.14× | — | — | 1.6 MB | 3.05× |
| **V3** | 2558.9 ms | 286.6 | 3.49 M | 4.08× | 1742.3 ms | 816.8 ms | 286.9 MB | 3.38× |
| **V4** | 1962.0 ms | 219.7 | 4.55 M | 5.32× | 1743.7 ms | 218.1 ms | 286.9 MB | 2.99× |
| **V5** | 1409.7 ms | 157.9 | 6.33 M | **7.40×** | 1246.5 ms | 163.3 ms | 286.8 MB | 2.80× |
| **V6** | 1796.5 ms | 201.2 | 4.97 M | 5.81× | 1633.3 ms | 163.2 ms | 287.0 MB | 2.81× |
| V7 | 2557.2 ms | 286.4 | 3.49 M | 4.08× | 2392.8 ms | 163.5 ms | 964.4 MB | 3.65× |
| V8 (AArch64 OHLC, Pi only) | 2539.0 ms | 284.4 | 3.52 M | 4.11× | 2393.9 ms | 145.1 ms | 964.4 MB | — |

Every version passes test20 and gives the same stress_full answer as on the laptop. The Pi's runs were very steady, with a standard deviation of 0.07 to 0.45% of the mean for every `-O2` version.

- V5 is the fastest version on both machines (Pi: 7.40× vs V0 at `-O2`, 7.44× at `-O3`).
- The Pi takes 2.3 to 4.2× as long as the laptop. The gap is biggest for work that is heavy on `double` additions (V3's naive process phase: 4.22×) and smallest for the integer fixed-point process phase (2.33×).
- V7 (Lucas's single `fread`) is slower than my V6 on both machines (Pi 2557.2 vs 1796.5 ms, laptop 700.0 vs 638.8 ms at `-O2`; V6 is also faster at every other flag set on the Pi). Reading the whole 678 MB file into memory first costs more than reading it twice with `fgets`. That is Lucas's rung, so I only note it here.

## 1. Trade struct and fixed-size global array (Stage B task 1): V3

Files: `src/store/trades.h`, `src/store/v3-v4_trades.c`, main program `src/v3-v4_main_stageB_inmem.c`. (First written as `stageB/engine/trades.*`.)

### What
- `Trade` struct with only the fields the engine uses:
  ```c
  typedef struct {
      long long time;     /* timestamp in microseconds */
      double    price;    /* price in USDT             */
      double    qty;      /* quantity in BTC           */
      int       is_buy;   /* 1 = buy, 0 = sell         */
  } Trade;
  ```
- Global array `Trade trades[MAX_TRADES]` with `MAX_TRADES = 10,000,000`.
- `load_trades(path)` reads the whole CSV once with Jaren's fast `read_trade()` (the V2 reader) and returns the number of trades, or -1 on an error or too many trades.
- The main program runs in two timed phases: Load (file → array), then Process (VWAP, OHLC, SMA and crossover over the array, with no file reading).

### Why (design choices to defend)
- The array is global because 10M × 32 B = 320 MB. Local variables live on the stack (a few MB), so a local array this size would crash. A global array lives in a separate memory area (`.bss`) that can hold it, which is what the brief suggests. `size bin/v3_stageB_in_memory` shows `bss = 320,002,272` bytes.
- Only 4 fields: the engine never uses trade id, quoteQty or isBestMatch, so storing them would waste memory and cache space.
- Biggest fields first: 3 × 8 B + 4 B = 28 B, padded to 32 B (`sizeof(Trade)`). With the 8-byte fields first, the padding is the minimum 4 bytes at the end.
- `MAX_TRADES = 10M`: `stress_full.csv` has 8,928,944 trades (89% of the limit).
- The bounds check `if (n >= MAX_TRADES)` runs before every write, so the loader never writes past the array.
- V3 keeps V2's reader and V2's naive SMA, so V2 → V3 changes only when the maths happens (after loading instead of between reads).

### Correctness (laptop and Pi: identical)
| Check | V2 (streaming) | V3 (in-memory) |
|---|---|---|
| `test20.csv` | PASS | PASS |
| `dev_1M.csv` VWAP / signals | 76217.01291587, 2509 BUY / 2469 SELL | 76217.01291587, 2509 / 2469 |
| `stress_full.csv` VWAP / signals | 76718.67569936, 20386 / 20176 | 76718.67569936, 20386 / 20176 |

The results are identical, as expected: same reader, same modules, same order of additions (and the same as V0). The Pi gives exactly the same numbers.

### Comparison: in-memory vs re-reading the file (V2 → V3)

stress_full, `-O2`, `taskset -c 2`, 5 runs, median.

| Version | Laptop runs (ms, sorted) | Laptop median | ns/trade | trades/s | Speedup vs V2 | Speedup vs V0 | Pi median | Pi speedup vs V2 |
|---|---|---|---|---|---|---|---|---|
| V2 streaming | 823.4, 827.1, 828.7, 836.8, 941.3 | **828.7 ms** | 92.8 | 10.77 M | 1.00× | 3.76× | **2523.9 ms** | 1.00× |
| V3 load + process | 753.4, 754.0, 756.1, 759.0, 802.6 | **756.1 ms** | 84.7 | 11.81 M | **1.10×** | 4.12× | **2558.9 ms** | **0.99×** |
| V3 process only | 192.8, 193.1, 193.4, 193.7, 194.1 | **193.4 ms** | 21.7 | 46.2 M | **4.28×** | 16.1× | **816.8 ms** | **3.09×** |
| V3 load only | | 562.4 ms | 63.0 | 15.9 M | | | 1742.3 ms | |

Pi runs (ms, sorted): V2 2517.0, 2519.2, 2523.9, 2527.7, 2540.9; V3 total 2556.3, 2557.9, 2558.9, 2559.8, 2560.7; V3 process 816.2 to 816.9.

`perf stat` on the Pi (whole program; V3's count also includes test20 + dev_1M, 11.2% more trades than V2's stress_full-only run):

| | V2 | V3 | V3 ÷ V2 |
|---|---|---|---|
| instructions | 18.28 × 10⁹ | 20.55 × 10⁹ | 1.124 (≈ the 1.112 extra trades) |
| IPC | 3.03 | 3.01 | same |
| cache misses | 5.2 M (0.17%) | 14.5 M (0.40%) | 2.8× |

What it shows:
- Processing from memory is 4.3× (laptop) / 3.1× (Pi) faster than the whole streaming run, because reading and parsing move outside the timed part. With the fast parser, loading is 74% (laptop) / 68% (Pi) of V3's time, down from ~97% with the helper's `sscanf` (§6).
- For load plus process together, the machines disagree. On the laptop V3 is 9.6% faster than streaming (756 vs 829 ms), which Prediction 3 did not expect ("the same, or up to 5% slower"). On the Pi it is 1.4% slower (2559 vs 2524 ms), which is what Prediction 3 expected.
- The Pi does not support my guess for the laptop (two short loops, each doing one job, keep their code and data in the caches). On the Pi, V3 runs the same number of instructions per trade as V2 at the same IPC and has 2.8× more cache misses, from writing 286 MB into the array and reading it back, and that extra memory traffic is the whole difference. Why the laptop gains is still open. Its larger caches and more aggressive out-of-order core are the likely reason, but I have no laptop `perf` numbers to show it.
- With the helper reader the two were equal on the laptop (§6: 1.00×), because `sscanf` was so slow that any such effect was lost in it.
- For a single pass, loading into memory does not pay off on the Pi. It pays off when the same data is processed more than once (the replay, or several strategies on one load).
- V2 needs 2.2 MB (laptop) / 1.6 MB (Pi) of physical memory and V3 needs 289.0 / 286.9 MB, about 130 to 180× more.

## 2. Naive SMA vs running-sum SMA (Stage B task 2): V3 → V4

Brief: _"Compute the SMA twice: first naively, by re-adding the whole window at every trade, then with a running sum. Compare the times."_

Files: `src/engine/sma.h`, naive `src/engine/v0-v3_sma_naive.c`, running sum `src/engine/v4_sma_running.c`. (First written as `stageB/engine/sma_naive.c` with a `-DNAIVE_SMA` switch; the ladder uses separate files instead.)

### What
- One header (`sma.h`: `sma_init`, `sma_push`, `sma_ready`, `sma_value`) and two `.c` files. The Makefile links exactly one of them. Both keep the last N prices in a circular array.
- Naive: `sma_push` only stores the price, and `sma_value` re-adds all N prices every trade, which is O(N) per trade.
- Running sum: `sma_push` also updates `sum = sum + newest − oldest`, and `sma_value` is one division, which is O(1) per trade.

### Why (design choices to defend)
- V3 and V4 are the same program with a different SMA file linked, so the time difference comes from the algorithm alone ("change one thing at a time"). No `if` inside the timed loop picks between them.
- Expected cost: the naive version does 35 + 60 = 95 additions per trade for the two SMAs, against 2 adds and 2 subtracts for the running sum.

### Correctness
| Check | V3 naive | V4 running sum | Exact (integer check) |
|---|---|---|---|
| `test20.csv` | PASS | PASS | 1 BUY / 0 SELL |
| `dev_1M.csv` signals | 2509 BUY / 2469 SELL | **2465 BUY / 2464 SELL** | 2553 / 2469 |
| `stress_full.csv` signals | 20386 BUY / 20176 SELL | **20188 BUY / 20188 SELL** | 20943 / 20060 |
| VWAP (both files) | identical | identical | — |

### Signal counts depend on floating-point rounding
In `dev_1M.csv` there are 17,132 trades where the short and long SMA are exactly equal (156,471 in `stress_full.csv`). The price often stays flat for many trades (e.g. 76589.57 repeated), so both windows hold the same price.

Mathematically d = SMA_short − SMA_long = 0 there. With `double`, d comes out as a tiny rounding error like ±0.0000000001 instead, its sign is close to random, and it can create or hide BUY/SELL signals. The running sum and the naive sum add the numbers in a different order, so their rounding errors differ and they give different signal counts. Neither matches the exact answer.

The exact answer came from a separate check that stores prices as integer cents and compares `sum_short × 60` with `sum_long × 35` (pure integer maths, no rounding). That check also gives the correct `test20.csv` result (1 BUY, 0 SELL), and the fixed-point rungs V5 to V7 give exactly these counts. `test20.csv` doesn't catch the problem because it is too short to build up rounding error.

So the old "reference" dev_1M numbers (2465 / 2464) are themselves a rounding artefact of the `double` running sum, and this is a strong argument for fixed-point prices (§3).

BUY > SELL in the exact count because the brief's rule turns the pattern "+, 0, +" into a BUY with no matching SELL; the rule uses `≤ 0` / `> 0`.

### Comparison

stress_full, process phase (the data is already in memory, so reading does not hide the difference), `-O2`, `taskset -c 2`, 5 runs, median.

| Version | Laptop runs (ms, sorted) | Laptop median | ns/trade | trades/s | Speedup | Pi median | Pi speedup |
|---|---|---|---|---|---|---|---|
| V3 naive SMA | 192.8, 193.1, 193.4, 193.7, 194.1 | **193.4 ms** | 21.7 | 46.2 M | 1.00× | **816.8 ms** (91.5 ns/trade) | 1.00× |
| V4 running-sum SMA | 91.7, 92.5, 92.8, 94.4, 94.4 | **92.8 ms** | 10.4 | 96.2 M | **2.08×** | **218.1 ms** (24.4 ns/trade) | **3.75×** |

Pi runs (ms): V3 816.2 to 816.9, V4 218.0 to 218.3 (all five within 1 ms).

Whole run (load + process): laptop 756.1 → 656.8 ms, 1.15× (vs V0: 4.12× → 4.74×); Pi 2558.9 → 1962.0 ms, 1.30× (vs V0: 4.08× → 5.32×). The load phase is the same in both versions (laptop 562.4 vs 564.0 ms, Pi 1742.3 vs 1743.7 ms), as it should be.

`perf stat` on the Pi (whole program, the same files for both): instructions 20.55 × 10⁹ → 15.88 × 10⁹ (−23%, about 470 fewer per trade), IPC 3.01 → 3.03, cache misses 14.5 M → 12.4 M.

The gain is about 2 to 4× rather than the ~24× the operation count (95 vs 4) suggests, because the SMA is only part of each trade's work: VWAP, OHLC and the crossover check cost the same in both versions. The naive loop over 35/60 doubles is also very cache-friendly, since the window is tiny and stays in the L1 cache.

The gain is bigger on the Pi (3.75× vs 2.08×), which Prediction 4 expected (3 to 5×).
- On the Pi, the extra time per trade (91.5 − 24.4 = 67 ns) matches running the ~470 extra instructions at the Pi's normal speed (IPC 3 at 2.4 GHz ≈ 65 ns), so the naive SMA costs exactly its extra additions.
- The laptop's naive SMA costs only 11 ns per trade more. Its much wider core overlaps more of that work with everything else, so the extra additions are partly hidden. The Pi's naive process phase is 4.2× slower than the laptop's, but its running-sum one only 2.3×.
- My first version (§6, struct-based modules) gave 2.63× on the laptop. Its running-sum process phase was faster (76 ms vs 93 ms here) while the naive one was about the same, so the ratio differs. The ladder's V4 uses Joel's Stage A modules (module-level `static` variables).

## 3. Fixed-point prices vs double (Stage B task 3): V4 → V5

Brief: _"Store prices as fixed-point integers (long long) instead of double, and compare."_ Also: _"Check that price × qty cannot overflow long long (~9.2 × 10¹⁸), and explain your choice in the report."_

Files: `src/engine_fx/fixed.h` (scales and overflow working), `src/store/trades_fx.h`, `src/store/v5_trades_fx_array.c`, `src/engine_fx/vwap_fx.*`, `sma_fx.*`, `signal_fx.*`, `v5-v7_ohlc_fx.c`, main program `src/v5-v8_main_stageB_fixed.c`. Lucas's `src/reader/v5-v8_read_trade_fx.c` parses each line straight into fixed-point. (First written as `stageB/stageB_fixed.c` + `stageB/engine/*_fx.*`.)

### What
- `TradeFx` struct: `time`, `price` and `qty` are all `long long`, plus `is_buy`, still 32 bytes.
- The reader builds each number from its digits as a scaled integer (`76589.57` → `7658957`), so no `double` is ever created. It rejects a value with more decimals than the scale can hold exactly, instead of silently rounding it.
- The whole processing loop (VWAP sums, OHLC, SMA running sum, crossover) uses only whole-number maths. Doubles appear only when printing.
- The crossover compares `sum_short × L − sum_long × S` instead of `SMA_short − SMA_long`. The sign is the same (both sides multiplied by S × L > 0), and the comparison needs no division, so it is exact.

### What else changes from V4 to V5 (be careful in the report)
V4 → V5 changes three things at once:
1. Number type: `double` → `long long` (the thing being compared).
2. Parser: V4 parses to `double` with Jaren's reader; V5 parses straight to integers with Lucas's reader. This changes the load phase.
3. Module style: V4 uses Joel's Stage A modules (module-level `static` variables); V5 uses my `_fx` modules (state in structs passed by pointer).

So the fair "fixed vs double" number is the process phase, and my first version (§6: `stageB_inmem` vs `stageB_fixed`, same module style, same reader, only the number type changed) is the cleaner controlled test.

### Choice of scale factors (measured on `stress_full.csv`)
| Field | Decimals in data | Range | Scale | Stored as |
|---|---|---|---|---|
| price | at most 2 | 73,027.02 – 79,500.00 | ×100 (cents) | 7,302,702 – 7,950,000 |
| qty | at most 5 | up to 18.29992 | ×100,000 | up to 1,829,992 |

Both scales store every value exactly, so no information is lost. The file shows 8 decimals for qty, but the last 3 are always 0 (Binance's step size is 0.00001 BTC).

### Overflow working (long long max ≈ 9.22 × 10¹⁸)
| Quantity | Worst case | Fits? |
|---|---|---|
| one price × qty | 7.95×10⁶ × 1.83×10⁶ = 1.5×10¹³ | yes |
| VWAP Σ(price × qty) over full day | 3.40×10⁹ USDT × 100 × 10⁵ = **3.4×10¹⁶** (measured: 3.402×10¹⁶) | yes, ~270× headroom |
| VWAP Σ qty over full day | ~44,600 BTC × 10⁵ = 4.5×10⁹ | yes |
| SMA window sum (60 prices) | 60 × 7.95×10⁶ = 4.8×10⁸ | yes |
| crossover `sum × window` | 4.8×10⁸ × 60 = 2.9×10¹⁰ | yes |

Why not ×10⁸ for qty, to keep all 8 printed decimals? The day's VWAP sum would be 3.40×10⁹ × 100 × 10⁸ = 3.4×10¹⁹, which overflows. ×10⁵ is the largest scale that is both exact for this data and safe; the brief's own example warns about this case.

### Correctness / precision (laptop and Pi: identical)
| Check | V4 `double` | V5 fixed-point | Exact answer |
|---|---|---|---|
| `test20.csv` | PASS | **PASS** | — |
| `dev_1M` final VWAP | 76217.01291587 | **76217.01291611** | 76217.01291611 |
| `dev_1M` signals | 2465 BUY / 2464 SELL | **2553 BUY / 2469 SELL** | 2553 / 2469 |
| `stress_full` final VWAP | 76718.67569936 | **76718.67569889** | 76718.67569889 |
| `stress_full` signals | 20188 BUY / 20188 SELL | **20943 BUY / 20060 SELL** | 20943 / 20060 |

- The exact VWAP was computed independently with Python `Decimal` (40-digit precision), and the exact signal counts come from the integer-cents check (§2). V6 and V7 give the same as V5. Every number in this table is the same on the Pi, including the `double` VWAP to all 8 decimals, even though the Pi build uses a fused multiply-add for `sum_pq += price * qty` (`objdump` shows `fmadd` in `vwap_update`; the laptop's x86-64 build has none). The rounding difference is ~10⁻¹⁶ per step, far below the printed digits.
- Fixed-point matches the exact answer everywhere and double does not. The double VWAP is off by 2.4×10⁻⁷ (dev_1M) and 4.7×10⁻⁷ (stress_full) USDT, a tiny error that comes from rounding building up over millions of additions.
- The signals show the big difference. On the ~1.7% of trades where both SMAs are exactly equal, double's rounding gives a tiny `d` of random sign, which creates or hides signals. Whole numbers give `d = 0` exactly, so the brief's `≤ 0` / `> 0` rule is applied as written.

### Comparison: speed

stress_full, `-O2`, `taskset -c 2`, 5 runs, median.

| Phase | V4 `double` (laptop) | V5 fixed-point (laptop) | Speedup (laptop) | V4 Pi | V5 Pi | Speedup (Pi) |
|---|---|---|---|---|---|---|
| **Process** (the number-type comparison) | 92.8 ms (10.4 ns/trade) | **70.2 ms** (7.9 ns/trade) | **1.32×** | 218.1 ms (24.4 ns/trade) | **163.3 ms** (18.3 ns/trade) | **1.34×** |
| Load (parser change) | 564.0 ms | 432.5 ms | 1.30× | 1743.7 ms | 1246.5 ms | 1.40× |
| Total | 656.8 ms | **502.8 ms** | 1.31× | 1962.0 ms | **1409.7 ms** | 1.39× |
| Speedup vs V0 | 4.74× | **6.19×** | | 5.32× | **7.40×** | |

V5 runs (total, ms, sorted): laptop 499.8, 501.7, 502.8, 505.8, 506.7 (process 70.0 to 70.4); Pi 1408.6, 1408.9, 1409.7, 1410.3, 1414.2 (process 162.8 to 163.4).

`perf stat` on the Pi (whole program): instructions 15.88 × 10⁹ → 10.58 × 10⁹ (−33%), IPC 3.03 → 2.80, cycles 5.24 × 10⁹ → 3.78 × 10⁹ (−28%), cache misses about the same (12.4 M → 12.2 M).

- Process 1.32× (laptop) / 1.34× (Pi): integer add/compare is a little cheaper than `double` add/compare, the SMA needs no division, and the struct is the same 32 bytes, so the memory traffic is the same. Part of the gain may come from the different module style (see above); the controlled first version gave 1.13× on the laptop. Prediction 1 expected 1.1 to 1.5× and "maybe a little bigger on the Pi", and both came true, the second only just (1.34 vs 1.32).
- Load 1.30× / 1.40×: likely because building an integer from the digits is simpler than building a `double` (not measured separately). This is Lucas's parser, not my change, but it only works because the numbers are fixed-point.
- V5 runs a third fewer instructions at a slightly lower IPC: each instruction waits a little longer, but there are far fewer of them.
- V5 is the fastest version on both machines (`-O2`: laptop 6.19×, Pi 7.40× vs V0; `-O3`: 6.23× / 7.44×).
- `TradeFx` is 32 bytes like `Trade`, so max RSS is the same (laptop 289.3 vs 287.9 MB, Pi 286.8 vs 286.9 MB).

### Trade-offs (for the report)
- Precision: fixed-point is exact for this data, because prices and quantities have a fixed number of decimals. Double gives a slightly wrong VWAP and noticeably different signal counts. The answers are identical on the laptop and the Pi.
- Speed: 1.32× (laptop) / 1.34× (Pi) on the process phase, 1.31× / 1.39× end to end.
- Readability and effort: fixed-point needs more code (separate `_fx` modules) and units to keep track of (cents, ×10⁵). The scales have to be chosen from the data and checked for overflow, and values must be converted back for printing. Double is simpler to write and read.
- Risk: fixed-point breaks if the data changes, for example a coin with more decimals, or a much larger day's volume that overflows. The V5 reader at least rejects a value with too many decimals instead of rounding it silently. Double never overflows here.

## 4. malloc array vs fixed global array, with valgrind (Stage C task 1): V5 → V6

Brief: _"Replace the fixed-size array with one allocated by malloc to fit the file, and compare memory use."_ Also a ladder rung: _"memory allocated with malloc to fit the data exactly."_

Files: `src/store/v6_trades_fx_malloc.c`, with the same `trades_fx.h` interface as V5, so the main program does not change. (First written as `stageC/engine/trades_malloc.*` + `stageC/stageC_malloc.c`, with the `double` `Trade` struct.) Lucas's V7 and the replay keep the same interface.

### What
`load_trades_fx(path, &n)`:
1. `count_lines(path)`: read the file once with `fgets`, counting lines, with no number parsing.
2. `malloc(capacity × sizeof(TradeFx))`, then check for `NULL`.
3. Read the file again with `read_trade_fx` and fill the array.
4. Return the array; the caller calls `free_trades_fx()` (= `free`) when finished.

### Design choices (to defend)
- Count first, then call `malloc` once, so exactly `n × 32 B` is allocated. This is simpler to explain than growing the array with `realloc`, and it never over-allocates. The cost is reading the file twice. I chose this before measuring (Prediction 2).
- V6 uses the same `TradeFx` struct and the same parser as V5, so the two differ only in where the array lives and the comparison is fair (both fixed-point).
- Every error path frees what it allocated, and the `NULL` check comes straight after `malloc`. The fill loop still checks `n < capacity`, so it can never write past the end, even if the line count were wrong.
- An empty file is rejected, because `malloc(0)` may return `NULL` and would look like an out-of-memory error.
- `(size_t)capacity * sizeof(TradeFx)` does the multiplication in the type `malloc` takes. (8.9M × 32 B is far from overflowing anyway.)

### Correctness (laptop and Pi)
- The output is identical to V5 on all three files, on both machines: test20 PASS; dev_1M 76217.01291611, 2553 / 2469; stress_full 76718.67569889, 20943 / 20060.
- valgrind Memcheck (`--leak-check=full --show-leak-kinds=all ./bin/v6_stageC_malloc dev`) reports 0 errors and "All heap blocks were freed -- no leaks are possible" on both machines: 11 allocs / 11 frees, 32,023,008 bytes on the laptop and 32,027,104 on the Pi (the 32 MB dev_1M array, test20's tiny array and the file buffers; the C library's buffers differ slightly).
- Memcheck is also clean for V3, V4 and V5 on both machines (0 errors, all heap blocks freed; 5 allocs / 5 frees, 13,232 B laptop / 17,328 B Pi). That is only the file buffers, because their array is not on the heap.
- valgrind works on the Pi's 16 KB-page kernel (valgrind 3.24.0).

### Results: memory (laptop 2026-11-15, Pi 2026-11-16)

"dev" = `./prog dev` (test20 + dev_1M). "full" = `./prog` (all three files; the peak comes while stress_full is loaded). MB = 10⁶ bytes.

| Version | Run | Array actually needed | `.bss` (`size`) | Reserved (Massif `--pages-as-heap=yes` peak) | Heap-only Massif peak | Physical (max RSS, median of 3) | Memcheck errors | Definitely lost |
|---|---|---|---|---|---|---|---|---|
| V5 fixed global array | dev | 32.0 MB | 320.0 MB | **325.0 MB** | 0.009 MB | **34.5 MB** | 0 | 0 B |
| V6 `malloc`, sized to file | dev | 32.0 MB | 8 B | **37.0 MB** | 32.0 MB | **34.0 MB** | 0 | 0 B |
| V5 fixed global array | full | 285.7 MB | 320.0 MB | **325.0 MB** | — | **289.3 MB** | — | — |
| V6 `malloc`, sized to file | full | 285.7 MB | 8 B | **290.7 MB** | — | **287.7 MB** | — | — |

Array actually needed = `sizeof(TradeFx)` (32 B) × number of trades.

Pi 5 (`logs/leroi_pi/`, same method):

| Version | Run | `.bss` | Reserved (Massif pages) | Heap-only Massif | Max RSS (median of 3) | Memcheck errors | Definitely lost |
|---|---|---|---|---|---|---|---|
| V5 fixed global array | dev | 320.0 MB | **324.6 MB** | 0.013 MB | **33.1 MB** | 0 | 0 B |
| V6 `malloc` | dev | 8 B | **36.6 MB** | 32.0 MB | **33.1 MB** | 0 | 0 B |
| V5 fixed global array | full | 320.0 MB | **324.6 MB** | — | **286.8 MB** | — | — |
| V6 `malloc` | full | 8 B | **290.3 MB** | — | **287.0 MB** | — | — |

What it shows:
- `malloc` saves 288 MB of reserved memory on dev_1M (325.0 → 37.0 MB, −89%) and 34 MB on stress_full (325.0 → 290.7 MB, −11%). The fixed array reserves 320 MB whatever the file size.
- The Pi shows the same savings (−288 MB on dev_1M, −34 MB on stress_full). Its reserved figures are ~0.4 MB lower than the laptop's because the program code and libraries are a little smaller on ARM.
- Physical memory is almost the same: on the laptop, 0.5 MB (dev) and 1.6 MB (full) less with `malloc`; on the Pi, equal on dev and 0.2 MB more on full (within the 16 KB page size). The OS only gives real memory to the pages of the global array that are written, so the unused part of the fixed array costs reserved address space but no RAM. Prediction 2 expected this, and it held on both machines.
- Plain valgrind (heap only) cannot see the global array. For V5 its peak is 9 kB, while V6 shows the 32 MB heap block, which is why `--pages-as-heap=yes` is needed for a fair comparison.

### Results: speed (V5 vs V6, stress_full, `-O2`, `taskset -c 2`, 5 runs, median)

| Phase | V5 static (laptop) | V6 `malloc` (laptop) | Difference | V5 Pi | V6 Pi | Difference (Pi) |
|---|---|---|---|---|---|---|
| Load | 432.5 ms | 569.0 ms | **+31.6%** (+136.5 ms) | 1246.5 ms | 1633.3 ms | **+31.0%** (+386.8 ms) |
| Process | 70.2 ms | 69.9 ms | −0.4% (noise) | 163.3 ms | 163.2 ms | −0.1% (noise) |
| Total | 502.8 ms | 638.8 ms | **+27.1%** | 1409.7 ms | 1796.5 ms | **+27.4%** |
| Speedup vs V0 | 6.19× | 4.87× | | 7.40× | 5.81× | |

Pi runs (total, ms, sorted): V6 1795.1, 1796.2, 1796.5, 1807.6, 1810.3. `perf stat` on the Pi: instructions 10.58 × 10⁹ → 13.17 × 10⁹ (+24%, the counting pass), cache misses 12.2 M → 17.9 M.

- The process phase is the same, as predicted: the loop reads the same 32-byte structs in the same order, whether they are in `.bss` or on the heap.
- Loading is 31.6% (laptop) / 31.0% (Pi) slower, more than my predicted 10 to 20%, and almost the same share on both machines. The counting pass (`fgets` over 678 MB) costs ~136 ms on the laptop and ~387 ms on the Pi. With the old `sscanf` reader it added only 7.7% to a 3 s load (§6); the fast parser makes the real parse so quick that reading the file a second time is a big share.
- On both machines, then, `malloc` costs speed and saves almost no physical memory. Its benefits are that the array fits any file (no 10M-trade limit; stress_full is at 89% of it), the memory can be freed, and the program can stop cleanly when memory runs out. Lucas's V7 removes the double read (one `fread`), but holds the whole 678 MB text while loading (max RSS 965 MB).

## 5. Per-member benchmark write-up (Testing and Enhancement 1): done (laptop + Pi)

Brief: each member documents the full `stress_full.csv` benchmark on laptop and Pi, one set per member, including:
- system set-up (Pi model, OS, compiler version, flags, CPU governor): see the table above;
- time and throughput (trades/s) for every version: "The ladder at a glance" above (`-O2`), and every flag set in `benchmark/results/leroi/<machine>/summary.txt`;
- `perf stat`: cycles, instructions, IPC, cache misses (Pi);
- memory usage for each version: max RSS in the ladder table, reserved memory below.

The write-up itself is the last section of `docs/leroi_report_draft.md`.

### Memory, every version (`-O2`)

Max RSS: median of 3, MB = 10⁶ bytes. Reserved = Massif `--pages-as-heap=yes` peak (full run).

| Version | `.bss` | Reserved (laptop / Pi) | Max RSS full run (laptop / Pi) | Max RSS dev run (laptop / Pi) |
|---|---|---|---|---|
| V0–V2 (streaming) | small | — | 2.2 / 1.6 MB | — |
| V3 / V4 (`double`, global array) | 320.0 MB | ~325 MB (same `.bss` as V5; not run under Massif) | 289.0 / 286.9 MB, 287.9 / 286.9 MB | 35.5 / 33.1 MB, 35.6 / 33.1 MB |
| V5 (fixed-point, global array) | 320.0 MB | 325.0 / 324.6 MB | 289.3 / 286.8 MB | 34.5 / 33.1 MB |
| V6 (`malloc`) | 8 B | 290.7 / 290.3 MB | 287.7 / 287.0 MB | 34.0 / 33.1 MB |
| V7 (`fread` + `malloc`) | — | — | 965.4 / 964.4 MB | 109.8 / 109.1 MB |
| V8 (V7 + AArch64 OHLC, Pi only) | — | — | — / 964.4 MB | — / 109.1 MB |

Streaming uses 130× (laptop) to 180× (Pi) less physical memory than any in-memory version. Even V7, the largest at 0.96 GB, fits easily in the Pi's 8 GB. For the edge-device recommendation, this means memory is not the Pi's limit for this data; run time is.

### Pi 5: `perf stat` (`-O2`)

From `benchmark/results/leroi/rpi/ladder/logs/<version>_O2.txt`: one extra untimed run of the whole program, so V3 and later include their test20 and dev_1M part (about 11% more trades than V0 to V2). On the Cortex-A76, perf's generic "cache references / misses" count level-1 data cache accesses and refills.

| Version | cycles | instructions | IPC | cache references | cache misses | miss rate |
|---|---|---|---|---|---|---|
| V0 | 25.05 × 10⁹ | 79.81 × 10⁹ | 3.19 | 20.17 × 10⁹ | 6.06 M | 0.03% |
| V1 | 25.16 × 10⁹ | 80.19 × 10⁹ | 3.19 | 20.28 × 10⁹ | 6.23 M | 0.03% |
| V2 | 6.04 × 10⁹ | 18.28 × 10⁹ | 3.03 | 3.16 × 10⁹ | 5.23 M | 0.17% |
| **V3** | 6.84 × 10⁹ | 20.55 × 10⁹ | 3.01 | 3.62 × 10⁹ | 14.54 M | 0.40% |
| **V4** | 5.24 × 10⁹ | 15.88 × 10⁹ | 3.03 | 2.76 × 10⁹ | 12.41 M | 0.45% |
| **V5** | 3.78 × 10⁹ | 10.58 × 10⁹ | 2.80 | 2.48 × 10⁹ | 12.18 M | 0.49% |
| **V6** | 4.85 × 10⁹ | 13.17 × 10⁹ | 2.72 | 3.32 × 10⁹ | 17.86 M | 0.54% |
| V7 | 6.85 × 10⁹ | 18.27 × 10⁹ | 2.67 | 3.22 × 10⁹ | 22.32 M | 0.69% |
| V8 | 6.80 × 10⁹ | 18.22 × 10⁹ | 2.68 | 3.20 × 10⁹ | 21.96 M | 0.69% |

What the counters say:
- IPC stays between 2.7 and 3.2 for every version, so each speedup comes from running fewer instructions. `sscanf` (V0/V1) needs ~80 × 10⁹ instructions, and the digit parser (V2) cuts that to 18 × 10⁹.
- V2 → V3: instructions per trade and IPC unchanged, cache misses 2.8× higher (writing and re-reading the 286 MB array). That is why in-memory is 1.4% slower on the Pi (§1).
- V3 → V4: 23% fewer instructions (the 95 additions per trade are gone), same IPC (§2).
- V4 → V5: 33% fewer instructions, IPC a little lower at 2.80 (§3).
- V5 → V6: 24% more instructions and 47% more cache misses, from the extra counting pass over the file (§4).
- Miss rates stay below 0.7%. The data is read in order, so the CPU's prefetcher keeps almost everything in the L1 cache, even for the 286 MB array.

## 6. Development history: my first versions (helper reader), superseded

Before the ladder existed I measured my own programs, all reading with the course helper (`fgets` + `sscanf`): `stageB_stream`, `stageB_inmem` (+ `-DNAIVE_SMA`), `stageB_fixed`, `stageC_malloc`. They were never run on the Pi. Laptop, stress_full, `-O2`, `taskset -c 2`, 5 runs, median, 2026-11-03 (V0 comparison 2026-11-13). These numbers are not for the report's tables; I quote them only where they explain a ladder result.

| Comparison | First version (helper reader) | Ladder (fast readers) |
|---|---|---|
| In-memory process vs streaming | 37.8× (79.0 vs 2988.1 ms) | 4.28× (V3 vs V2; Pi 3.09×) |
| In-memory load + process vs streaming | 1.00× (2976.8 vs 2988.1 ms) | 1.10× (Pi 0.99×) |
| Load share of in-memory run | ~97% | 74% (V3; Pi 68%) |
| Naive vs running SMA (process) | 2.63× (200.4 vs 76.2 ms) | 2.08× (V3 vs V4; Pi 3.75×) |
| `double` vs fixed-point (process) | 1.13× (79.9 vs 70.6 ms), same modules and reader | 1.32× (V4 vs V5; Pi 1.34×) |
| `malloc` vs static, load | +7.7% (3218.7 vs 2988.7 ms) | +31.6% (V6 vs V5; Pi +31.0%) |
| `malloc` vs static, max RSS (full) | 287.7 vs 289.0 MB | 287.7 vs 289.3 MB |
| Massif reserved, dev / full | 37.0 / 290.7 MB vs 325.0 / 325.0 MB | identical |

With `sscanf`, parsing was ~97% of every run, so only a faster parser could speed up a single pass (Version 0 and `stageB_stream` both took ~2.97 s).

## Prediction logbook (my entries)

Brief: ≥ 6 group entries, at least one per stage, each dated and signed, with the prediction written before running. My entries are in `logs/leroi_predictions.md`. All five were written before measuring and their predictions are unchanged; the results come from the ladder.

- [x] Prediction 1, fixed-point vs double: Result and Reason written (laptop 1.32×, Pi 1.34× process; ~1.13× from the number type alone)
- [x] Prediction 2 (Stage C), malloc vs fixed array: Result and Reason written (memory as predicted; load +31%, more than predicted because the fast parser makes the second pass a big share)
- [x] Prediction 3, in-memory vs streaming: Result and Reason written (Pi 0.99× as predicted; laptop 1.10× not fully explained)
- [x] Prediction 4, naive vs running SMA on the Pi: Result and Reason written (3.75×, inside the predicted 3 to 5×)
- [x] Prediction 5, laptop vs Pi answers: Result and Reason written (identical; the Pi build does use `fmadd`, too small to change a printed digit)
