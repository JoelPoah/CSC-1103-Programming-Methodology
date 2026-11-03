# Leroi — Stage B & Stage C working notes

Working documentation for my parts of the project (Group 06). Each section records
**what I built, why, how it was checked, and how it compares** with the version before it.
These notes feed the Overleaf report, the efficiency ladder (with Yong Sheng) and the
prediction logbook (`logs/predictions.md`).

---

## Measurement rules (from the project brief)

Every comparison below must follow these rules, otherwise the numbers don't count:

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
| Report must include a **trade-off analysis** and a **deployment recommendation** for a power-/memory-limited device (Pi 5, 4 GB) | Testing 3 |

Formulas:
- time per trade = total time (ns) / number of trades
- throughput = number of trades / total time (s)
- speedup_k = time of Version 0 / time of version k

> **Open question for the group:** which program is "Version 0"? (The Stage A streaming
> program `stageA/stageA_stream.c` is the natural choice.) Agree this with Yong Sheng so
> every rung on the ladder uses the same baseline.

---

## System set-up

| | Laptop | Raspberry Pi 5 |
|---|---|---|
| CPU | Intel Core Ultra X7 358H (16 threads) | _TODO_ |
| RAM | 32 GB | 4 GB (issued kit) |
| OS / kernel | Arch Linux, Linux 7.2.11 | _TODO_ |
| Compiler | gcc 16.2.1 | _TODO_ (`gcc --version`) |
| Flags | `-O2 -Wall` | _TODO_ (+ `-mcpu=cortex-a76`?) |
| CPU governor | `powersave` (left as default) | _TODO_ |


---

## 1. Trade struct + fixed-size global array (Stage B task 1) — DONE

**Files:** `stageB/engine/trades.h`, `stageB/engine/trades.c`, `stageB/stageB_inmem.c`
(commit `a4c01e4` on `leroi`).

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
- `load_trades(path)` reads the whole CSV once (via the helper `trade_io`), returns the
  number of trades, or -1 on error / too many trades.
- `stageB_inmem.c` runs in two timed phases: **LOAD** (file → array) then **PROCESS**
  (VWAP, OHLC, SMA, crossover over the array, no file reading).

### Why (design choices to defend)
- **Global, not local:** 10M × 32 B = 320 MB. Local variables live on the stack (a few MB),
  so a local array this size would crash. A global array lives in a separate memory area
  that can hold it. (The brief suggests exactly this.)
- **Only 4 fields:** trade id and quoteQty are never used by the engine, so storing them
  would waste memory and cache space.
- **Biggest fields first:** 3 × 8 B + 4 B = 28 B, padded to **32 B** (verified with
  `sizeof(Trade)`). Ordering 8-byte fields first keeps padding to the minimum 4 bytes at
  the end.
- **`MAX_TRADES = 10M`:** `stress_full.csv` has 8,928,944 trades → some headroom.
- **Bounds check** `if (n >= MAX_TRADES)` before every write → never writes past the array.
- **Loader still uses `trade_io`:** when Jaren's fast parser is ready, only the inside of
  `load_trades()` changes; the rest of Stage B is untouched.

### Correctness
| Check | Result |
|---|---|
| `test20.csv` (short 3, long 5): all 20 rows vs `test20_expected.txt` | **match** |
| `dev_1M.csv` (35/60): final VWAP / signals | 76217.01291587, 2465 BUY, 2464 SELL — **match Stage A** |
| `stress_full.csv` (35/60) | 8,928,944 trades, VWAP 76718.67569936, 20188 BUY, 20188 SELL |

### Comparison: in-memory vs re-reading the file (ladder rung)

_Preliminary single run on the laptop, not pinned — **not for the report**, only to show
the shape of the result:_

| File | Load (file → array) | Process (from memory) | Load share |
|---|---|---|---|
| `dev_1M.csv` | 332 ms | 9.1 ms | ~97% |
| `stress_full.csv` | 2812 ms | 86.1 ms | ~97% |

**Takeaway:** about 97% of the run time is reading/parsing the file; the actual maths over
8.9M trades takes < 0.1 s (~10 ns per trade). This is the argument for (a) processing from
memory and (b) Jaren's fast parser.

**Official measurements** (stress_full, 8,928,944 trades, `-O2`, `taskset -c 2`, 5 runs,
median; laptop measured 2026-11-03):

Streaming = `stageB/stageB_stream.c`: the same engine modules, reading with `next_trade()`
while processing, with no `.dat` and no gnuplot. Joel's `stageB_manual` cannot be timed by
the brief's rules, because it writes `signals.dat`, runs gnuplot and does not use `now_ns()`.

| Version | Laptop 5 runs (ms, sorted) | Laptop median | ns/trade | trades/s | Speedup vs streaming | Pi median | Pi ns/trade | Pi trades/s | Speedup vs V0 (Pi) |
|---|---|---|---|---|---|---|---|---|---|
| Streaming (`stageB_stream`) | 2982.6, 2986.0, 2988.1, 3000.3, 3018.1 | **2988.1 ms** | 334.7 | 2.99 M | 1.00× | _TODO_ | | | _TODO (V0)_ |
| In-memory, process phase only (`stageB_inmem`) | 77.7, 77.8, 79.0, 85.2, 87.7 | **79.0 ms** | 8.9 | 113.0 M | **37.8×** | _TODO_ | | | |
| In-memory, load + process | 2954.9, 2956.5, 2976.8, 2988.4, 2990.0 | **2976.8 ms** | 333.4 | 3.00 M | **1.004×** | _TODO_ | | | |

- Output is identical for both (stress_full: VWAP 76718.67569936, 20188 BUY / 20188 SELL).
- Load + process is the median of the 5 per-run sums (load median alone: 2891.6 ms).
- **What it shows:** processing from memory is about 38× faster than streaming, but only
  because reading and parsing are moved outside the timed part. Load + process together
  costs the same as streaming (0.4% difference, within run-to-run noise). In-memory pays
  off only when the same data is processed more than once (e.g. the replay, or several
  strategies on one load). Parsing is 97% of the work, which is why Jaren's fast parser is
  the bigger win for a single pass.

Memory: in-memory version uses a fixed **320 MB** array regardless of file size
(286 MB actually filled for stress_full: 8,928,944 × 32 B).

---

## 2. Naive SMA vs running-sum SMA (Stage B task 2) — DONE on laptop, Pi timing TODO

Brief: _"Compute the SMA twice: first naively, by re-adding the whole window at every
trade, then with a running sum. Compare the times."_

**Files:** `stageB/engine/sma_naive.h`, `stageB/engine/sma_naive.c`, `#ifdef NAIVE_SMA`
block in `stageB/stageB_inmem.c`.

### What
- Running-sum version (already existed): `stageB/engine/sma.c` — O(1) per trade,
  `S_new = S_old + p_new − p_oldest`, circular buffer.
- Naive version (new): `sma_naive_push()` only stores the price in the same circular
  buffer; `sma_naive_value()` re-adds all N prices every trade — O(N) per trade.
- Both builds come from **one source file**; `-DNAIVE_SMA` picks the naive code.

Build:
```sh
SRC="stageB/stageB_inmem.c stageB/engine/trades.c stageB/engine/vwap.c stageB/engine/ohlc.c \
     stageB/engine/sma.c stageB/engine/signal.c stageB/engine/report.c helper_function/trade_io.c"
gcc -O2 -Wall $SRC -o stageB/stageB_inmem                                         # running sum
gcc -O2 -Wall -DNAIVE_SMA $SRC stageB/engine/sma_naive.c -o stageB/stageB_inmem_naive  # naive
```

### Why (design choices to defend)
- **Reuse the `Sma` struct, `sma_init()` and `sma_ready()`:** the two versions share the
  same data and only differ in how the sum is found, so the timing difference is caused by
  the algorithm and nothing else ("change one thing at a time").
- **`#ifdef NAIVE_SMA` instead of a second copy of the program:** no duplicated code, and
  no `if` inside the timed loop (which would slow both versions slightly). The compiler
  builds one version or the other.
- **Didn't modify `sma.c`:** that's the shared engine module; the naive code is a separate
  file used only for this comparison.
- Expected cost: naive does 35 + 60 = **95 additions per trade** for the SMAs vs **2 adds +
  2 subtracts** for the running sum.

### Correctness
| Check | Running sum | Naive |
|---|---|---|
| `test20.csv` 20 rows vs expected | match | **match** |
| `dev_1M.csv` final VWAP | 76217.01291587 | 76217.01291587 |
| `dev_1M.csv` signals | 2465 BUY / 2464 SELL | **2509 BUY / 2469 SELL** |
| `stress_full.csv` signals | 20188 BUY / 20188 SELL | **20386 BUY / 20176 SELL** |
| Exact answer (integer cents, see below) dev_1M | 2553 BUY / 2469 SELL | |
| Exact answer stress_full | 20943 BUY / 20060 SELL | |

### Important finding: signal counts depend on floating-point rounding
- In `dev_1M.csv` there are **17,132 trades where the short and long SMA are exactly
  equal** (156,471 in `stress_full.csv`) — the price often stays flat for many trades
  (e.g. 76589.57 repeated), so both windows hold the same price.
- Mathematically d = SMA_short − SMA_long = 0 there. With `double`, d comes out as a tiny
  rounding error like ±0.0000000001 instead, and **its sign is random-ish**, so it can
  create or hide BUY/SELL signals.
- The running sum and the naive sum add the numbers in a different order, so their
  rounding errors differ → **different signal counts**. Neither matches the exact answer.
- The exact answer was found with a separate check that stores prices as integer cents
  and compares `sum_short × 60` with `sum_long × 35` (pure integer maths, no rounding).
  That check also gives the correct `test20.csv` result (1 BUY, 0 SELL).
- `test20.csv` doesn't catch this because it is too short to build up rounding error.
- **Consequences:**
  - The "reference" dev_1M numbers (2465 / 2464) are themselves a rounding artefact
    of the `double` running sum — tell Joel (test20 gatekeeper) and the group.
  - This is a strong argument for **fixed-point prices** (section 3): integer sums are
    exact, so running sum and naive sum will agree, and should give 2553 / 2469.
  - Good material for the report's "fixed-point precision vs double" trade-off and for a
    logbook entry.
- (Side note: BUY > SELL in the exact count because the brief's rule turns the pattern
  "+, 0, +" into a BUY with no matching SELL — the rule uses `≤ 0` / `> 0`.)

### Comparison

`stress_full.csv` (8,928,944 trades), **process phase only**, `-O2`, pinned with
`taskset -c 2`, 5 runs, median reported.

**Laptop (official):**

| Version | 5 runs (ms, sorted) | Median | ns/trade | Trades/s | Speedup |
|---|---|---|---|---|---|
| Naive SMA | 195.3, 197.0, 200.4, 206.0, 207.6 | **200.4 ms** | 22.4 | 44.6 M | 1.0× |
| Running-sum SMA | 74.2, 74.5, 76.2, 77.2, 77.8 | **76.2 ms** | 8.5 | 117.1 M | **2.63×** |

**Pi 5 — TODO** (same method):

| Version | 5 runs (ms, sorted) | Median | ns/trade | Trades/s | Speedup |
|---|---|---|---|---|---|
| Naive SMA | | | | | 1.0× |
| Running-sum SMA | | | | | |

Why only ~2.6× and not ~24× (95 vs 4 operations)? The SMA is only part of each trade's
work — VWAP, OHLC and the crossover check cost the same in both versions. Also the
naive loop over 35/60 doubles is very cache-friendly (the window is tiny and stays in L1).

Compare the **process phase** (data already in memory) so file reading doesn't hide the
difference.

---

## 3. Fixed-point prices vs double (Stage B task 3) — DONE on laptop, Pi timing TODO

Brief: _"Store prices as fixed-point integers (long long) instead of double, and compare."_
Also: _"Check that price × qty cannot overflow long long (~9.2 × 10¹⁸), and explain your
choice in the report."_

**Files:** `stageB/engine/fixed.h` (scales + overflow working), `trades_fx.h/.c`,
`vwap_fx.h/.c`, `ohlc_fx.h/.c`, `sma_fx.h/.c`, `signal_fx.h/.c`, and the program
`stageB/stageB_fixed.c`. Reuses the shared `report.c` (printing) and `signal.c`
(`signal_name`) unchanged.

Build:
```sh
gcc -O2 -Wall stageB/stageB_fixed.c stageB/engine/trades_fx.c stageB/engine/vwap_fx.c \
    stageB/engine/ohlc_fx.c stageB/engine/sma_fx.c stageB/engine/signal_fx.c \
    stageB/engine/signal.c stageB/engine/report.c helper_function/trade_io.c \
    -o stageB/stageB_fixed
```

### What
- `TradeFx` struct: `time`, `price`, `qty` all `long long`, plus `is_buy` — still 32 bytes.
- Loader converts each double from the helper library **once** with
  `to_fixed(value, scale) = (long long)(value × scale + 0.5)`.
- The whole processing loop (VWAP sums, OHLC, SMA running sum, crossover) uses only
  whole-number maths. Doubles appear **only when printing**.
- Crossover compares `sum_short × L − sum_long × S` instead of `SMA_short − SMA_long`:
  same sign (multiplied both sides by S × L > 0), but **no division and exact**.

### Choice of scale factors (measured on `stress_full.csv`)
| Field | Decimals in data | Range | Scale | Stored as |
|---|---|---|---|---|
| price | at most 2 | 73,027.02 – 79,500.00 | ×100 (cents) | 7,302,702 – 7,950,000 |
| qty | at most 5 | up to 18.29992 | ×100,000 | up to 1,829,992 |

Both scales store **every value exactly** (no information lost). The file shows 8 decimals
for qty but the last 3 are always 0 (Binance's step size is 0.00001 BTC).

### Overflow working (long long max ≈ 9.22 × 10¹⁸)
| Quantity | Worst case | Fits? |
|---|---|---|
| one price × qty | 7.95×10⁶ × 1.83×10⁶ = 1.5×10¹³ | yes |
| VWAP Σ(price × qty) over full day | 3.40×10⁹ USDT × 100 × 10⁵ = **3.4×10¹⁶** (measured: 3.402×10¹⁶) | yes, ~270× headroom |
| VWAP Σ qty over full day | ~44,600 BTC × 10⁵ = 4.5×10⁹ | yes |
| SMA window sum (60 prices) | 60 × 7.95×10⁶ = 4.8×10⁸ | yes |
| crossover `sum × window` | 4.8×10⁸ × 60 = 2.9×10¹⁰ | yes |

**Why not ×10⁸ for qty (all 8 printed decimals)?** The day's VWAP sum would be
3.40×10⁹ × 100 × 10⁸ = **3.4×10¹⁹ → overflows**. ×10⁵ is the largest scale that is both
exact for this data and safe. (The brief's own example warns about exactly this.)

### Correctness / precision
| Check | double (`stageB_inmem`) | fixed-point (`stageB_fixed`) | Exact answer |
|---|---|---|---|
| `test20.csv` rows + OHLC | match | **match** | — |
| `dev_1M` final VWAP | 76217.01291587 | **76217.01291611** | 76217.01291611 |
| `dev_1M` signals | 2465 BUY / 2464 SELL | **2553 BUY / 2469 SELL** | 2553 / 2469 |
| `stress_full` final VWAP | 76718.67569936 | **76718.67569889** | 76718.67569889 |
| `stress_full` signals | 20188 BUY / 20188 SELL | **20943 BUY / 20060 SELL** | 20943 / 20060 |

- Exact VWAP computed independently with Python `Decimal` (40-digit precision); exact
  signal counts from the earlier integer-cents check (section 2).
- **Fixed-point matches the exact answer everywhere; double does not.** The double VWAP is
  off by 2.4×10⁻⁷ (dev_1M) and 4.7×10⁻⁷ (stress_full) USDT — tiny, but it comes from
  rounding error building up over millions of additions.
- The big difference is the **signals**: on the ~1.7% of trades where both SMAs are
  exactly equal, double's rounding gives a random-sign tiny `d`, creating/hiding signals.
  Whole numbers give `d = 0` exactly, so the brief's `≤ 0` / `> 0` rule is applied as
  written.

### Comparison: speed

`stress_full.csv` (8,928,944 trades), **process phase only**, `-O2`, `taskset -c 2`,
5 runs, median. Both runs taken back to back.

**Laptop:**

| Version | 5 runs (ms, sorted) | Median | ns/trade | Trades/s | Speedup |
|---|---|---|---|---|---|
| double (`stageB_inmem`) | 76.0, 77.9, 79.9, 80.5, 88.7 | **79.9 ms** | 8.9 | 111.8 M | 1.0× |
| fixed-point (`stageB_fixed`) | 70.2, 70.5, 70.6, 70.7, 72.2 | **70.6 ms** | 7.9 | 126.5 M | **1.13×** |

**Pi 5 — TODO** (same method):

| Version | 5 runs (ms, sorted) | Median | ns/trade | Trades/s | Speedup |
|---|---|---|---|---|---|
| double | | | | | 1.0× |
| fixed-point | | | | | |

Why only ~1.13×: the work per trade is the same (same steps, same 32-byte struct, so the
same memory traffic); integer add/compare is only slightly cheaper than double add/compare
on a modern CPU. The fixed-point runs were also more consistent (70.2–72.2 ms vs
76.0–88.7 ms). Memory: `TradeFx` is 32 bytes like `Trade`, so no memory difference.

### Trade-offs (for the report)
- **Precision:** fixed-point is exact for this data (prices and quantities have a fixed
  number of decimals); double gives slightly wrong VWAP and noticeably different signal
  counts.
- **Speed:** small gain (~13% on laptop).
- **Readability / effort:** more code (separate `_fx` modules), units to keep track of
  (cents, ×10⁵), scales must be chosen from the data and checked for overflow, and values
  must be converted back for printing. Double is simpler to write and read.
- **Risk:** fixed-point silently breaks if the data changes (e.g. a coin with more
  decimals, or a much larger day's volume → overflow). Double never overflows here.

## 4. malloc array vs fixed global array, with valgrind (Stage C task 1) — BUILT, measurements TODO

Brief: _"Replace the fixed-size array with one allocated by malloc to fit the file, and
compare memory use."_ Also a ladder rung: _"memory allocated with malloc to fit the data
exactly."_

### What was built (2026-11-03)
- `stageC/engine/trades_malloc.h/.c`: `count_lines(path)` and
  `load_trades_malloc(path, &n)`, which returns a `malloc`'d `Trade *` (the caller
  calls `free`).
- `stageC/stageC_malloc.c`: a copy of `stageB_inmem.c` (double, running-sum SMA). The
  only change is that the array is a `malloc`'d pointer passed into `process()`, not the
  global `trades[]`. It frees the array after each file. `./stageC/stageC_malloc dev`
  skips stress_full, because valgrind is too slow for it.
- **For Lucas (replay):** call `load_trades_malloc`, loop over `list[0..n-1]`, then
  `free(list)`.

Build (from project root):
```sh
gcc -O2 -Wall stageC/stageC_malloc.c stageC/engine/trades_malloc.c stageB/engine/vwap.c \
    stageB/engine/ohlc.c stageB/engine/sma.c stageB/engine/signal.c stageB/engine/report.c \
    helper_function/trade_io.c -o stageC/stageC_malloc
```

**Design choices (to defend):**
- **Count first, then `malloc` once.** The first pass counts lines with `fgets` and no
  `sscanf`, so no numbers are converted. Then exactly `n × 32 B` is allocated and a second
  pass fills it. This is simpler to explain than growing the array with `realloc`, and it
  never over-allocates. The cost is reading the file twice. This was chosen before
  measuring and matches the Prediction 2 design.
- **Same `Trade` struct as Stage B**, included from `stageB/engine/trades.h`. The two
  versions then differ only in where the array lives, so the comparison is fair.
  `trades.c` is not linked, so the 320 MB global array does not exist in this program.
- **Every error path frees what it allocated**, and the `NULL` check comes straight after
  `malloc`. The fill loop still checks `n < capacity`, so it can never write past the
  end, even if the line count were wrong.
- **An empty file is rejected**, because `malloc(0)` may return `NULL` and would look like
  an out-of-memory error.
- `(size_t)capacity * sizeof(Trade)`: the multiplication is done in the type `malloc`
  takes. (8.9M × 32 B is far from overflowing anyway.)

**Correctness (laptop, 2026-11-03):**
- Output is **identical** to `stageB_inmem` on all three files, apart from the timing
  lines. test20 matches `test20_expected.txt`. dev_1M gives VWAP 76217.01291587,
  2465 BUY / 2464 SELL.
- **valgrind Memcheck** (`valgrind --leak-check=full --show-leak-kinds=all
  ./stageC/stageC_malloc dev`, i.e. test20 + dev_1M): **0 errors**, "All heap blocks were
  freed -- no leaks are possible". Heap usage: 11 allocs, 11 frees, 32,023,008 bytes in
  total (the 32 MB dev_1M array plus small blocks for the helper's file handles).

**Preliminary timings** (single run, not pinned; do not copy into the report):
stress_full load 3005 ms (count + malloc + fill), process 85.6 ms. Stage B's earlier
preliminary run gave load 2812 ms, process 86.1 ms.

**Where valgrind fits:** the brief never names valgrind. It is the tool that produces the
evidence for this comparison (and for "memory usage for each version" in the per-member
benchmark write-up). It used to be listed as a separate Stage B step; it belongs here.

### What to measure
| Tool | Command (use `dev_1M.csv` — valgrind is 20–50× slower) | Gives |
|---|---|---|
| Memcheck | `valgrind --leak-check=full ./prog` | invalid reads/writes, uninitialised values, leaks ("definitely lost" must be 0) |
| Massif | `valgrind --tool=massif --pages-as-heap=yes ./prog` then `ms_print massif.out.<pid>` | peak memory over the run |
| (simpler) | `/usr/bin/time -v ./prog` → "Maximum resident set size" | peak physical memory |

Things to explain in the report:
- The global `trades[]` array is **not heap memory**, so valgrind's normal heap summary
  shows almost nothing for Stage B. Massif needs `--pages-as-heap=yes` to count it. The
  `malloc` version **does** appear in the heap summary.
- The OS only gives physical memory for the parts of an array that are actually touched.
  The fixed array reserves 320 MB but `dev_1M.csv` only fills ~32 MB, so the real saving
  from `malloc` may be smaller than "320 MB → exact size". Measure, don't assume.
- Pi 5 has **4 GB** RAM — relevant for the "memory-limited edge device" recommendation.

### Results: memory on the laptop (2026-11-03). Pi TODO

(No timing needed here: Stage C 1 asks only for memory use. The `stageC_malloc` time and
throughput go in the per-member benchmark write-up, section 5.)

"dev" = `./prog dev` (test20 + dev_1M). "stress" = `./prog` (all three files; the peak
comes while stress_full is loaded). Both programs were built with `-O2 -Wall`. MB = 10⁶ bytes.

| Version | Run | Array actually needed | Reserved (Massif `--pages-as-heap=yes` peak) | Heap-only Massif peak | Physical (max RSS, median of 5) | Memcheck errors | Definitely lost |
|---|---|---|---|---|---|---|---|
| Fixed global array (`stageB_inmem`) | dev | 32.0 MB | **325.0 MB** | 0.009 MB | **34.7 MB** | 0 | 0 B |
| malloc, sized to file (`stageC_malloc`) | dev | 32.0 MB | **37.0 MB** | 32.0 MB | **34.0 MB** | 0 | 0 B |
| Fixed global array (`stageB_inmem`) | stress | 285.7 MB | **325.0 MB** | — | **289.0 MB** | — | — |
| malloc, sized to file (`stageC_malloc`) | stress | 285.7 MB | **290.7 MB** | — | **287.7 MB** | — | — |

Array actually needed = `sizeof(Trade)` (32 B) × number of trades.

**What it shows:**
- **Reserved memory:** `malloc` saves **288 MB on dev_1M** (325.0 → 37.0 MB, −89%) and
  **34 MB on stress_full** (325.0 → 290.7 MB, −11%). The fixed array reserves 320 MB
  whatever the file size. `size stageB/stageB_inmem` shows `bss = 320,000,104` bytes,
  against `bss = 80` for `stageC_malloc`.
- **Physical memory is almost the same**, only about 0.7 MB (dev) and 1.3 MB (stress) less
  with `malloc`. The OS only gives real memory to the pages of the global array that are written,
  so the unused part of the fixed array costs reserved address space but no RAM.
- **Plain valgrind (heap only) cannot see the global array.** For `stageB_inmem` its peak
  is 8.7 kB, while `stageC_malloc` shows the 32 MB heap block. That is why
  `--pages-as-heap=yes` is needed for a fair comparison.
- **Memcheck is clean for both** (dev run, `--leak-check=full --show-leak-kinds=all`):
  0 errors, "All heap blocks were freed". Fixed array: 5 allocs / 5 frees, 13,232 B (only
  the helper's file buffers). malloc: 11 allocs / 11 frees, 32,023,008 B.

**Method notes:**
- Max RSS was read with `wait4()` (fork, exec, wait) through a tiny C wrapper. This is the
  same counter that `/usr/bin/time -v` prints as "Maximum resident set size", but GNU
  `time` is not installed on the laptop. On the Pi: `sudo apt install time`, then
  `/usr/bin/time -v ./prog dev`.
  - A first attempt used a Python wrapper, but it can never report less than ~9 MB (the
    child is a copy of Python until `exec`). It gave the same numbers for the big versions
    but 9 MB for streaming, which really uses 1.9 MB. All numbers here are from the C
    wrapper.
- Massif peak = the largest `mem_heap_B` in `massif.out` (or `ms_print massif.out.<pid>`).
  With `--pages-as-heap=yes` it also counts the program code and libraries (~4–5 MB),
  which is why the reserved figure is a little above the array size.
- Massif and Memcheck are single runs, because they count bytes and give the same result
  every time. Max RSS is the median of 5 runs.

---

## 5. Per-member benchmark write-up (Testing & Enhancement 1) — laptop DONE (my versions), Pi TODO

Brief: each member documents the full `stress_full.csv` benchmark on laptop and Pi, **one
set per member**, including:
- system set-up (Pi model, OS, compiler version, flags, CPU governor) — see table above;
- **time and throughput (trades/s) for every version**;
- **`perf stat`**: cycles, instructions, IPC, cache misses (Pi only — `perf` doesn't work
  on the laptop);
- **memory usage for each version**.

(README assigns `perf stat` to Yong Sheng only; the brief requires it in every member's
own write-up.)

### Laptop (2026-11-03): time and throughput

stress_full (8,928,944 trades), `-O2 -Wall`, `taskset -c 2`, 5 runs back to back, one
version after another, median. Total = load + process (the median of the per-run sums).
trades/s = 8,928,944 ÷ time.

| Version | Program | Total time | ns/trade | trades/s | Process phase | Process ns/trade | Process trades/s |
|---|---|---|---|---|---|---|---|
| Version 0 (TBD by group) | | | | | | | |
| Streaming, double, running SMA | `stageB_stream` | 2988.1 ms | 334.7 | 2.99 M | — (mixed with reading) | — | — |
| In-memory, double, naive SMA | `stageB_inmem_naive` | 3122.9 ms | 349.7 | 2.86 M | 202.6 ms | 22.7 | 44.1 M |
| In-memory, double, running SMA | `stageB_inmem` | 2976.8 ms | 333.4 | 3.00 M | 79.0 ms | 8.9 | 113.0 M |
| In-memory, fixed-point | `stageB_fixed` | 3007.4 ms | 336.8 | 2.97 M | 71.9 ms | 8.1 | 124.1 M |
| malloc, double, running SMA | `stageC_malloc` | 3319.8 ms | 371.8 | 2.69 M | 85.3 ms | 9.6 | 104.6 M |

- **`stageC_malloc` re-check:** it ran last and had a wide spread (total 3209–3573 ms), so
  it was re-run 5 times, alternating with `stageB_inmem`:
  - inmem: load 2988.7, process 80.9, total 3069.2 ms
  - malloc: load 3218.7, process 81.0, total 3299.8 ms

  The process phase is the same. Load is +230 ms (+7.7%), the cost of the extra counting
  pass. The 85.3 ms above was noise from the running order.
- Runs vary by 1–2% between sets (inmem process: 79.0 here, 80.9 in the re-check, 76.2 in
  section 2), so differences below ~3% should be treated as noise.
- Section 2's naive/running numbers (200.4 / 76.2 ms) came from an earlier set and agree
  with these.

### Laptop (2026-11-03): memory

Reserved = Massif `--pages-as-heap=yes` peak (includes ~4–5 MB of code and libraries).
Physical = max RSS, median of 5 (C wrapper, see section 4). Full run (peak while
stress_full is loaded). MB = 10⁶ bytes.

| Version | Program | `.bss` (from `size`) | Reserved | Physical (max RSS) |
|---|---|---|---|---|
| Streaming | `stageB_stream` | 80 B | 5.0 MB | 1.9 MB |
| In-memory, naive SMA | `stageB_inmem_naive` | 320.0 MB | 325.0 MB | 288.5 MB |
| In-memory, running SMA | `stageB_inmem` | 320.0 MB | 325.0 MB | 289.0 MB |
| In-memory, fixed-point | `stageB_fixed` | 320.0 MB | 325.0 MB | 289.2 MB |
| malloc | `stageC_malloc` | 80 B | 290.7 MB | 287.7 MB |

- `stageB_fixed` uses the same 32-byte struct size (`long long` price/qty instead of
  `double`), so its memory matches the double version.
- Streaming uses about 150× less physical memory than any in-memory version. This matters
  for the edge-device recommendation (Pi 5: 4 GB).

### Pi 5: TODO

| Version | Total time | trades/s | Process phase | cycles | instructions | IPC | cache misses | Reserved | Max RSS |
|---|---|---|---|---|---|---|---|---|---|
| Version 0 (TBD by group) | | | | | | | | | |
| `stageB_stream` | | | | | | | | | |
| `stageB_inmem_naive` | | | | | | | | | |
| `stageB_inmem` | | | | | | | | | |
| `stageB_fixed` | | | | | | | | | |
| `stageC_malloc` | | | | | | | | | |

Still missing from "every version": teammates' versions (fast parser, compiler-flag builds,
Version 0) once they exist, and `stageA_stream` / `stageB_manual`. Those two cannot be timed
as they are, because they write `.dat` files, run gnuplot and do not use `now_ns()`.

---

## Prediction logbook candidates (my entries)

Brief: ≥ 6 group entries, at least one per stage, each dated and signed. Write the
prediction **before** running.

My entries are written in `logs/leroi_predictions.md`.

- [x] In-memory vs streaming: Prediction 3 written 2026-11-03 (before official runs); laptop results filled in, Pi + Reason TODO
- [x] Naive vs running-sum SMA: Prediction 4 written 2026-11-03. It covers the Pi only, because the laptop result (2.63×) was already measured. Pi + Reason TODO
- [x] ~~Do naive and running-sum give the same signals?~~ Already observed before a prediction could be written (they don't), so it is recorded under "Findings" instead. Replaced by Prediction 5: do the laptop and the Pi give the same answers? Pi + Reason TODO
- [x] Fixed-point vs double speed (and whether results change) — Prediction 1 written; laptop results filled in, Pi + Reason TODO
- [x] **Stage C** — malloc vs fixed array: memory saved, any speed difference — Prediction 2 written 2026-11-03 (before building); Result + Reason TODO
