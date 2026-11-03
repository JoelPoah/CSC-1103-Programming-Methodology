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
| Report **`perf stat`**: cycles, instructions, IPC, cache misses (Pi) | General Steps 9 |
| Every version must still **pass `test20.csv`** | Stage C note |
| **No plot / `.dat` file writing** in timed versions | General Steps 8 |
| Write the **prediction before measuring** (logbook entry, dated + signed) | General Steps 10 |

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
| RAM | 32 GB | _TODO_ |
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

**Official measurements — TODO** (stress_full, median of 5, pinned with `taskset`):

| Version | Laptop time | Laptop ns/trade | Laptop trades/s | Pi time | Pi ns/trade | Pi trades/s | Speedup vs V0 (Pi) |
|---|---|---|---|---|---|---|---|
| Streaming (`stageB_manual`, reads file in loop) | | | | | | | |
| In-memory, process phase only (`stageB_inmem`) | | | | | | | |
| In-memory, load + process | | | | | | | |

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

## 3. Fixed-point prices vs double (Stage B task 3) — TODO

Brief: _"Store prices as fixed-point integers (long long) instead of double, and compare."_
Also: _"Check that price × qty cannot overflow long long (~9.2 × 10¹⁸), and explain your
choice in the report."_

To record:
- Chosen scale factors (price ×?, qty ×?) and why they fit the data
  (price ~76,000 with 2 decimals; qty with 8 decimals).
- Overflow working: single price × qty product, SMA window sum (max price × scale × 60),
  and the **VWAP running sum of price × qty over the whole day**.
- Precision: difference between fixed-point and double results (VWAP, SMA, signal counts).

| Version | Laptop process time | Pi process time | ns/trade (Pi) | Speedup | Final VWAP | BUY/SELL |
|---|---|---|---|---|---|---|
| double | | | | 1.0× | 76718.67569936 | 20188 / 20188 |
| fixed-point | | | | | | |

Trade-off discussion for report: speed vs precision vs readability.

---

## 4. Memory profiling with valgrind (Stage B task 4) — TODO

- `valgrind --leak-check=full ./stageB/stageB_inmem` (use `dev_1M.csv` — valgrind is slow).
- Record: heap usage summary, "definitely lost" bytes (should be 0), peak memory.

---

## 5. malloc array vs fixed global array (Stage C task 1) — TODO

Brief: _"Replace the fixed-size array with one allocated by malloc to fit the file, and
compare memory use."_ Also a ladder rung: _"memory allocated with malloc to fit the data
exactly."_

| Version | Memory reserved | Memory used (stress_full) | Time (laptop/Pi) | valgrind leaks |
|---|---|---|---|---|
| Fixed global array | 320 MB (always) | 286 MB | | n/a |
| malloc, sized to file | | | | |

Memory = `sizeof(Trade)` × number of trades. Also measure actual peak with
`/usr/bin/time -v` (Max resident set size) or valgrind massif.

---

## Prediction logbook candidates (my entries)

Brief: ≥ 6 group entries, at least one per stage, each dated and signed. Write the
prediction **before** running.

My entries are written in `logs/stageb_predictions.md`.

- [ ] In-memory: "process phase will be X× faster than streaming because…"
- [ ] Naive vs running-sum SMA speedup (predict before the official runs; preliminary
      result was ~2.5×, not the ~24× the operation count suggests — good "explain" material)
- [ ] Do naive and running-sum give the same signals? (They don't with `double` — rounding.)
- [x] Fixed-point vs double speed (and whether results change) — Prediction 1 written, results TODO
- [ ] malloc vs fixed array: memory saved, any speed difference
