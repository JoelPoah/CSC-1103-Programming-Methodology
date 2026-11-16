# Report draft: Leroi's sections (7.1 to 7.3, 8.1, and Leroi's benchmark documentation)

Draft text to paste into the group Google Doc under the matching headings.
All versions are rungs of the group's efficiency ladder (`src/`, built by `make ladder`).
All numbers are official: stress_full.csv (8,928,944 trades), `-O2 -Wall`, `taskset -c 2`,
median of 5 runs, measured with `benchmark/ladder_bench` on the laptop (2026-11-15) and the
Raspberry Pi 5 (8 GB, 2026-11-15/16). Source: `docs/leroi_stageB_C.md`.

## 7.1 Trade struct and global array

In Stage A the engine read and parsed one trade at a time while it computed the indicators. Stage B (ladder version V3) loads every trade into memory first and then runs the indicators over the array. The file is read once, and the calculation can be timed on its own.

Each trade is stored in a struct that keeps only the four fields the engine uses:

```c
typedef struct {
    long long time;     /* timestamp in microseconds */
    double    price;    /* price in USDT             */
    double    qty;      /* quantity in BTC           */
    int       is_buy;   /* 1 = buy, 0 = sell         */
} Trade;
```

The trades are kept in a global array, `Trade trades[MAX_TRADES]`, with `MAX_TRADES = 10,000,000`. The function `load_trades(path)` (`src/store/v3-v4_trades.c`) reads the whole CSV once with our fast reader from Stage A+ (Section 6) and returns the number of trades loaded, or −1 on an error. The program then times two phases separately with `now_ns()`: load (file → array) and process (VWAP, OHLC, SMA and crossover over the array).

Design choices:
- The array is global because 10 million trades × 32 bytes = 320 MB. Local variables live on the stack, which holds only a few MB, so a local array this size would crash the program. A global array lives in a separate area of memory that can hold it.
- The struct keeps only four fields. The engine never uses the trade id, quoteQty or isBestMatch, and storing them would waste memory and cache space.
- The largest fields come first. Three 8-byte fields plus one 4-byte field make 28 bytes, which the compiler pads to 32 bytes (checked with `sizeof(Trade)`). With the 8-byte fields first, the padding is just 4 bytes at the end.
- `stress_full.csv` has 8,928,944 trades, so a limit of 10 million leaves some headroom. The loader checks `n >= MAX_TRADES` before every write, so it can never write past the end of the array.
- V3 uses the same reader and the same naive SMA as V2, the streaming Stage A+ version. The only difference is when the maths happens: after loading, instead of between reads.

V3 passes `test20.csv` and gives exactly the same results as V2 and Version 0 on the large files (dev_1M: VWAP 76217.01291587, 2509 BUY / 2469 SELL; stress_full: VWAP 76718.67569936, 20386 BUY / 20176 SELL).

The table compares streaming the file (V2) with loading it into memory first (V3):

| Version | Laptop median | ns/trade | trades/s | Speedup vs V2 | Speedup vs V0 | Pi median | Pi speedup vs V2 |
|---|---|---|---|---|---|---|---|
| V2 streaming (fast parser) | 828.7 ms | 92.8 | 10.8 M | 1.00× | 3.76× | 2523.9 ms | 1.00× |
| V3 in-memory, load + process | 756.1 ms | 84.7 | 11.8 M | 1.10× | 4.12× | 2558.9 ms | 0.99× |
| V3 in-memory, process phase only | 193.4 ms | 21.7 | 46.2 M | 4.28× | 16.1× | 816.8 ms | 3.09× |

*stress_full.csv, `-O2`, `taskset -c 2`, median of 5 runs.*

The process phase on its own is 4.3× faster than the whole streaming run on the laptop and 3.1× faster on the Pi, because reading and parsing now happen outside the timed part. Loading takes 74% (laptop) and 68% (Pi) of V3's time.

For load plus process together, the two machines disagree. On the Pi, V3 is 1.4% slower than streaming, as we predicted: it does the same parsing and the same maths, and it also writes 286 MB into the array and reads it back. The Pi's `perf stat` counters agree. V3 runs the same number of instructions per trade as V2 at the same IPC (3.01 vs 3.03), but has 2.8× more level-1 cache misses from the extra memory traffic. On the laptop, V3 was 10% faster than streaming, which we did not expect. The Pi's counters cannot explain it; the laptop's larger caches and wider core are the likely reason, but we have no laptop counters to check.

For a single pass over the data, then, loading into memory does not pay off on the edge device. It pays off when the same data is used more than once, as in the Stage C replay or the race.

V2 needs 2.2 MB (laptop) / 1.6 MB (Pi) of physical memory and V3 needs 289 / 287 MB. The fixed array always reserves 320 MB, whatever the file size: stress_full fills 285.7 MB of it and dev_1M only 32 MB. Section 8.1 compares this with the `malloc` version.

## 7.2 Naive SMA vs running-sum SMA

The brief asks for the SMA to be computed twice: first naively, by adding up the whole window again at every trade, then with a running sum.

- The naive version (`src/engine/v0-v3_sma_naive.c`, used by V0 to V3) stores each new price in the circular buffer and then adds up all N prices again. That is O(N) per trade: 35 + 60 = 95 additions per trade for our windows.
- The running-sum version (`src/engine/v4_sma_running.c`, V4) updates the window sum in one step, S_new = S_old + newest price − oldest price. That is O(1) per trade: 2 additions and 2 subtractions.

Both files implement the same header, `sma.h` (`sma_init`, `sma_push`, `sma_ready`, `sma_value`), and V3 and V4 are the same program with a different SMA file linked in. The two differ only in how the sum is found, and the timed loop contains no `if` to choose between them.

| Version | Laptop median | ns/trade | trades/s | Speedup | Pi median | Pi speedup |
|---|---|---|---|---|---|---|
| V3 naive SMA | 193.4 ms | 21.7 | 46.2 M | 1.00× | 816.8 ms | 1.00× |
| V4 running-sum SMA | 92.8 ms | 10.4 | 96.2 M | 2.08× | 218.1 ms | 3.75× |

*Process phase only, stress_full.csv, `-O2`, `taskset -c 2`, median of 5 runs. Whole run: laptop 756.1 → 656.8 ms (1.15×), Pi 2558.9 → 1962.0 ms (1.30×); speedup vs Version 0 rises from 4.12× to 4.74× (laptop) and from 4.08× to 5.32× (Pi).*

The running sum is 2.1× faster on the laptop and 3.75× faster on the Pi. The operation count (95 vs 4) would suggest about 24×, but the SMA is only part of the work per trade: VWAP, OHLC and the crossover check cost the same in both versions. The naive loop also runs over just 35 and 60 doubles, a window small enough to stay in the L1 cache, so re-adding it is cheap.

The gain is larger on the Pi, as we predicted (3 to 5×). `perf stat` shows that the running sum removes 23% of the instructions (about 470 per trade) while the IPC stays at about 3. On the Pi, the naive version takes 67 ns more per trade, and running those 470 extra instructions at that IPC and 2.4 GHz takes about 65 ns, so the Pi pays for every extra addition. The laptop's wider core overlaps much of that work with the rest of each trade, and its naive version costs only 11 ns more per trade.

Both versions pass `test20.csv`, but on the larger files they give different numbers of signals:

| File | V3 naive | V4 running sum | Exact answer |
|---|---|---|---|
| dev_1M.csv | 2509 BUY / 2469 SELL | 2465 BUY / 2464 SELL | 2553 BUY / 2469 SELL |
| stress_full.csv | 20386 BUY / 20176 SELL | 20188 BUY / 20188 SELL | 20943 BUY / 20060 SELL |

The price often stays flat for many trades, so both windows hold the same prices and the two SMAs are exactly equal. This happens on 17,132 trades in dev_1M (1.7%) and 156,471 in stress_full.

On those trades s = SMA_short − SMA_long should be exactly 0. With `double`, s comes out as a tiny rounding error such as ±10⁻¹⁰ with an effectively random sign, which can create or hide BUY and SELL signals. The two methods add the numbers in a different order, so they round differently and give different counts. Neither matches the exact answer, which we found with whole-number arithmetic (Section 7.3), and `test20.csv` is too short to show the problem. This was the main reason we moved to fixed-point prices.

## 7.3 Fixed-point prices

The fixed-point version (V5, `src/v5-v8_main_stageB_fixed.c`) stores prices and quantities as whole numbers (`long long`) instead of `double`. Its modules are `src/store/trades_fx.h`, `src/engine_fx/vwap_fx`, `sma_fx`, `signal_fx` and `ohlc_fx`, with the scales in `src/engine_fx/fixed.h`.

The reader (`read_trade_fx`) builds each number straight from its digits as a scaled whole number, so 76589.57 becomes 7658957 and no `double` is ever created. If a value has more decimals than the scale can hold exactly, the reader rejects it instead of rounding it silently. The whole processing loop (VWAP sums, OHLC, SMA running sums and the crossover) then uses only whole-number arithmetic, and values are turned back into doubles only for printing.

The crossover test no longer divides. Instead of the sign of SMA_short − SMA_long, it uses the sign of

  sum_short × L − sum_long × S  (S = 35, L = 60)

This is the original difference multiplied by S × L, which is positive, so the sign is the same. Because there is no division, the result is exact.

### 7.3.1 Choice of scaling factors

We chose the scales by checking the data in `stress_full.csv`:

| Field | Decimals in data | Range | Scale | Stored as |
|---|---|---|---|---|
| price | at most 2 | 73,027.02 – 79,500.00 | ×100 (cents) | 7,302,702 – 7,950,000 |
| qty | at most 5 | up to 18.29992 | ×100,000 | up to 1,829,992 |

Both scales store every value exactly, so no information is lost. The file prints quantities with 8 decimals, but the last 3 are always 0, because Binance's smallest step for this coin is 0.00001 BTC.

### 7.3.2 Overflow analysis

`long long` holds values up to about 9.22 × 10¹⁸. The worst cases are:

| Value | Worst case | Fits? |
|---|---|---|
| one price × qty | 7.95×10⁶ × 1.83×10⁶ ≈ 1.5×10¹³ | yes |
| VWAP Σ(price × qty), full day | 3.40×10⁹ USDT × 100 × 10⁵ ≈ 3.4×10¹⁶ (measured 3.402×10¹⁶) | yes, ~270× headroom |
| VWAP Σ qty, full day | ~44,600 BTC × 10⁵ ≈ 4.5×10⁹ | yes |
| SMA window sum (60 prices) | 60 × 7.95×10⁶ ≈ 4.8×10⁸ | yes |
| crossover sum × window | 4.8×10⁸ × 60 ≈ 2.9×10¹⁰ | yes |

The largest value is the full-day VWAP numerator, at 3.4×10¹⁶. Scaling quantity by 10⁸ to keep all 8 printed decimals would make that sum 3.4×10¹⁹, which overflows. ×10⁵ is the largest scale that is exact for this data and still safe.

### 7.3.3 Precision vs double

| Check | V4 `double` | V5 fixed-point | Exact answer |
|---|---|---|---|
| test20.csv | pass | pass | — |
| dev_1M VWAP | 76217.01291587 | 76217.01291611 | 76217.01291611 |
| dev_1M signals | 2465 BUY / 2464 SELL | 2553 BUY / 2469 SELL | 2553 / 2469 |
| stress_full VWAP | 76718.67569936 | 76718.67569889 | 76718.67569889 |
| stress_full signals | 20188 BUY / 20188 SELL | 20943 BUY / 20060 SELL | 20943 / 20060 |

We checked the exact VWAP independently with 40-digit decimal arithmetic (Python `Decimal`), and the exact signal counts with a separate whole-number check. The later fixed-point versions (V6, V7) give the same results as V5.

Fixed-point matches the exact answer everywhere and `double` does not. The double VWAP is off by only 2.4×10⁻⁷ (dev_1M) and 4.7×10⁻⁷ USDT (stress_full), rounding error that builds up over millions of additions. The signals show a larger effect. When the two SMAs are exactly equal, whole numbers give s = 0 exactly, so the brief's rule (BUY when s goes from ≤ 0 to > 0) is applied exactly as written, while double gives a tiny s of random sign.

The dev_1M reference counts from Stage A (2465 / 2464 with a running sum, 2509 / 2469 with the naive sum) are therefore rounding effects too. The correct counts are 2553 / 2469.

BUY is higher than SELL in the exact counts because of the rule's ≤ / ≥. A sequence such as s = +, 0, + produces a BUY when s returns to + after the 0, but no SELL.

The speed of the two versions:

| Phase | V4 `double` | V5 fixed-point | Speedup | Pi V4 | Pi V5 | Pi speedup |
|---|---|---|---|---|---|---|
| Process | 92.8 ms | 70.2 ms | 1.32× | 218.1 ms | 163.3 ms | 1.34× |
| Load | 564.0 ms | 432.5 ms | 1.30× | 1743.7 ms | 1246.5 ms | 1.40× |
| Total | 656.8 ms | 502.8 ms | 1.31× | 1962.0 ms | 1409.7 ms | 1.39× |
| Speedup vs Version 0 | 4.74× | 6.19× | | 5.32× | 7.40× | |

*stress_full.csv, `-O2`, `taskset -c 2`, median of 5 runs.*

V5 is the fastest version on both machines: 6.19× faster than Version 0 on the laptop and 7.40× on the Pi. Three things change from V4 to V5, so we read the numbers phase by phase.

The process phase (1.32× laptop, 1.34× Pi) is the fixed-point vs `double` comparison. Integer add and compare are slightly cheaper than double add and compare, and the SMA needs no division. Each trade still goes through the same steps and the struct is still 32 bytes, so the gain is modest. Some of it may come from the different module style, since V5's modules keep their state in structs and V4 uses Joel's Stage A modules. In an earlier test where only the number type changed, the gain was 1.13×.

The load phase (1.30× laptop, 1.40× Pi) gains from the new reader, which builds whole numbers from the digits instead of doubles. That is only possible because the values are fixed-point.

On the Pi, `perf stat` shows V5 runs 33% fewer instructions than V4, at a slightly lower IPC (2.80 vs 3.03). Every result in the precision table above is identical on the laptop and the Pi, including the `double` VWAP to all 8 decimals.

`TradeFx` is also 32 bytes, so fixed-point uses the same memory as the double version (max RSS 289.3 vs 287.9 MB on the laptop, 286.8 vs 286.9 MB on the Pi).

## 8.1 Dynamic memory with malloc

Stage C (V6) replaces the fixed 320 MB global array with an array allocated by `malloc` that is exactly the size of the file. The loader is `load_trades_fx(path, &n)` in `src/store/v6_trades_fx_malloc.c`. It returns a pointer to the array, and the caller frees it when it has finished. V6 uses the same `TradeFx` struct, reader and main program as V5, so the only change is where the array lives. The replay engine (Section 8.3) and V7 load their trades through the same interface.

```
load_trades_fx(path):
    capacity = count_lines(path)          // pass 1: count lines, no number parsing
    if capacity <= 0: return NULL         // cannot open, or empty file
    trades = malloc(capacity * sizeof(TradeFx))
    if trades == NULL: return NULL        // out of memory
    open the file
    n = 0
    while read_trade_fx(file, &t):        // pass 2: fill the array
        if n >= capacity: free(trades); return NULL   // never write past the end
        trades[n] = t; n = n + 1
    close the file
    return trades, n
```

Design choices:
- The loader counts first and calls `malloc` once. The first pass only counts lines with `fgets` and converts no numbers; we then allocate exactly n × 32 bytes and fill the array in a second pass. This is simpler to explain than growing the array with `realloc`, and it never allocates more than it needs. The cost is reading the file twice (see the speed table below).
- The `NULL` check comes straight after `malloc`. If the device ran out of memory, the program would print an error and stop instead of crashing.
- Every way out of the function frees the array first, so nothing leaks on an error.
- The fill loop still checks `n < capacity`, so it can never write past the end of the array, even if the line count were wrong.
- An empty file is reported separately, because `malloc(0)` may return `NULL`, which would look like an out-of-memory error.
- The multiplication is written as `(size_t)capacity * sizeof(TradeFx)`, so it is done in the type `malloc` takes.

The output is identical to V5 on all three files: V6 passes `test20.csv`, and dev_1M gives VWAP 76217.01291611, 2553 BUY / 2469 SELL.

Valgrind Memcheck (`valgrind --leak-check=full --show-leak-kinds=all ./bin/v6_stageC_malloc dev`, on test20 and dev_1M) reports 0 errors and "All heap blocks were freed -- no leaks are possible" on both the laptop and the Pi. It counts 11 allocations and 11 frees, 32,023,008 bytes in total on the laptop (32,027,104 on the Pi): the 32 MB dev_1M array plus small buffers for the file handles. We ran Memcheck on dev_1M rather than stress_full because valgrind makes the program 20 to 50× slower. V3, V4 and V5 are also clean (0 errors, all heap blocks freed).

Memory use of the two versions:

| Version | Run | Array needed | Reserved (Massif, `--pages-as-heap=yes`), laptop / Pi | Physical (max RSS), laptop / Pi |
|---|---|---|---|---|
| V5 fixed array | dev_1M | 32.0 MB | 325.0 / 324.6 MB | 34.5 / 33.1 MB |
| V6 `malloc` | dev_1M | 32.0 MB | 37.0 / 36.6 MB | 34.0 / 33.1 MB |
| V5 fixed array | stress_full | 285.7 MB | 325.0 / 324.6 MB | 289.3 / 286.8 MB |
| V6 `malloc` | stress_full | 285.7 MB | 290.7 / 290.3 MB | 287.7 / 287.0 MB |

*"dev_1M" = test20 + dev_1M; "stress_full" = all three files, peak while stress_full is loaded. MB = 10⁶ bytes. Max RSS is the median of 3 runs.*

On both machines `malloc` saves 288 MB of reserved memory on dev_1M (−89%) and 34 MB on stress_full (−11%), because the fixed array always reserves 320 MB. Physical memory is almost the same: within 1.6 MB on the laptop and 0.2 MB on the Pi. The operating system only gives real memory to the pages of the global array that are written to, so the unused part costs address space but no RAM. Valgrind's normal heap view cannot see the global array at all (its peak is 9 kB for V5), which is why Massif needs `--pages-as-heap=yes` for a fair comparison.

The speed of the two versions:

| Phase | V5 fixed array | V6 `malloc` | Difference | Pi V5 | Pi V6 | Pi difference |
|---|---|---|---|---|---|---|
| Load | 432.5 ms | 569.0 ms | +31.6% | 1246.5 ms | 1633.3 ms | +31.0% |
| Process | 70.2 ms | 69.9 ms | −0.4% | 163.3 ms | 163.2 ms | −0.1% |
| Total | 502.8 ms | 638.8 ms | +27.1% | 1409.7 ms | 1796.5 ms | +27.4% |

*stress_full.csv, `-O2`, `taskset -c 2`, median of 5 runs.*

The process phase is the same, because the loop reads the same 32-byte structs in the same order. Loading is about 31% slower on both machines because of the extra counting pass over the 678 MB file (on the Pi, `perf stat` shows 24% more instructions). Our fast parser makes the real parse quick, so reading the file a second time is a large share of the load.

`malloc` therefore makes the program slower and saves almost no physical memory. Its advantages are elsewhere: the array fits any file (the fixed array stops at 10 million trades, and stress_full already uses 89% of that), the memory can be freed after use, and the program can stop cleanly when memory runs out. Lucas's V7 avoids the double read by reading the whole file with one `fread`, at the cost of holding all of the text in memory while loading.

## Benchmark documentation: Leroi (Testing and Enhancement 1)

This is my own run of the full benchmark on `stress_full.csv` (8,928,944 trades), on my laptop (CSC1103) and on my Raspberry Pi 5 (CSC1104). It covers every version of the group's efficiency ladder, V0 to V8.

### System set-up

| | Laptop (CSC1103) | Raspberry Pi 5 (CSC1104) |
|---|---|---|
| Model | Framework Laptop 13 Pro | Raspberry Pi 5 Model B Rev 1.1 |
| CPU | Intel Core Ultra X7 358H, 16 cores, up to 4.7 GHz | Arm Cortex-A76, 4 cores, up to 2.4 GHz |
| RAM | 32 GB | 8 GB |
| OS and kernel | Arch Linux, Linux 7.2.15 (x86-64) | Debian GNU/Linux 13 (trixie), Linux 6.18.39+rpt-rpi-2712 (aarch64) |
| Memory page size | 4 KB | 16 KB |
| Compiler | gcc 16.2.1 | gcc 14.2.0 (Debian 14.2.0-19) |
| Flags | `-O0`, `-O1`, `-O2`, `-O3` (`-Wall` on all) | the same, plus `-O3 -mcpu=cortex-a76` |
| CPU governor | `powersave` (default) | `ondemand` (default); the core ran at its full 2.4 GHz throughout (V0: 25.05 × 10⁹ cycles in 10.44 s) |
| Pinning | `taskset -c 2` | `taskset -c 2` |
| Tools | valgrind 3.25.1 | perf (`linux-perf` 6.18.39), valgrind 3.24.0 |
| Date | 15 Nov 2026 | 15–16 Nov 2026 (temperature 54–59 °C, never throttled: `vcgencmd get_throttled` = `0x0`) |

Both machines ran the same source code (git commit `4e8a234`).

### Method

I used the group's benchmark program, `benchmark/ladder_bench`. It compiles every version at every optimisation level, checks the test20 answer, then runs one warm-up and 5 timed runs pinned to core 2, and I report the median of the 5 runs. V0 to V2 stream the file, so the whole program is timed with `now_ns()`. V3 and later load the file into memory first and report their own `now_ns()` times for load (file → array) and process (indicators over the array) on the stress_full section.

Throughput is 8,928,944 trades ÷ time, and speedup is the time of Version 0 (`-O2`) ÷ the time of the version, on the same machine. Memory is the maximum resident set size (peak physical memory), measured with a small `wait4()` wrapper as the median of 3 runs, with MB = 10⁶ bytes. The hardware counters come from `perf stat -e cycles,instructions,cache-references,cache-misses`, one extra untimed run per version, on the Pi only.

All 45 Pi builds and all 32 laptop builds pass `test20.csv`, and every version gives the same stress_full answer on both machines.

### Timing and throughput for every version (`-O2`)

| Version | What changed | Laptop time | Laptop trades/s | Laptop speedup | Pi time | Pi ns/trade | Pi trades/s | Pi speedup | Pi ÷ laptop |
|---|---|---|---|---|---|---|---|---|---|
| V0 | Version 0: helper reader (`sscanf`), `double`, naive SMA | 3111.9 ms | 2.87 M | 1.00× | 10439.0 ms | 1169.1 | 0.86 M | 1.00× | 3.35× |
| V1 | own reader through pointers, still `sscanf` | 3081.7 ms | 2.90 M | 1.01× | 10492.4 ms | 1175.1 | 0.85 M | 0.99× | 3.40× |
| V2 | digit-by-digit parser | 828.7 ms | 10.77 M | 3.76× | 2523.9 ms | 282.7 | 3.54 M | 4.14× | 3.05× |
| V3 | all trades in a global array | 756.1 ms | 11.81 M | 4.12× | 2558.9 ms | 286.6 | 3.49 M | 4.08× | 3.38× |
| V4 | running-sum SMA | 656.8 ms | 13.59 M | 4.74× | 1962.0 ms | 219.7 | 4.55 M | 5.32× | 2.99× |
| **V5** | **fixed-point prices** | **502.8 ms** | **17.76 M** | **6.19×** | **1409.7 ms** | **157.9** | **6.33 M** | **7.40×** | 2.80× |
| V6 | `malloc`'d array (file read twice) | 638.8 ms | 13.98 M | 4.87× | 1796.5 ms | 201.2 | 4.97 M | 5.81× | 2.81× |
| V7 | whole file read with one `fread` | 700.0 ms | 12.76 M | 4.45× | 2557.2 ms | 286.4 | 3.49 M | 4.08× | 3.65× |
| V8 | V7 + hand-written AArch64 OHLC | — | — | — | 2539.0 ms | 284.4 | 3.52 M | 4.11× | — |

*Median of 5 runs, stress_full.csv, `taskset -c 2`. Laptop ns/trade = time ÷ 8,928,944 (e.g. V5: 56.3 ns). V8 is assembly for the Pi's CPU, so it runs on the Pi only.*

The 5 runs of each version were very close. On the Pi, the standard deviation was 0.1 to 0.4% of the mean for every version. On the laptop it was 0.1 to 0.7%, except for V2 and V3, where one run each was slower (941 ms and 803 ms); a single slow run does not move the median.

Load and process phases (V3 and later, `-O2`):

| Version | Laptop load | Laptop process | Load share | Pi load | Pi process | Load share |
|---|---|---|---|---|---|---|
| V3 | 562.4 ms | 193.4 ms | 74% | 1742.3 ms | 816.8 ms | 68% |
| V4 | 564.0 ms | 92.8 ms | 86% | 1743.7 ms | 218.1 ms | 89% |
| V5 | 432.5 ms | 70.2 ms | 86% | 1246.5 ms | 163.3 ms | 88% |
| V6 | 569.0 ms | 69.9 ms | 89% | 1633.3 ms | 163.2 ms | 91% |
| V7 | 629.9 ms | 70.1 ms | 90% | 2392.8 ms | 163.5 ms | 94% |
| V8 | — | — | — | 2393.9 ms | 145.1 ms | 94% |

### Every optimisation level

Median time on stress_full, with the speedup against Version 0 at `-O2` in brackets.

Laptop:

| Version | `-O0` | `-O1` | `-O2` | `-O3` |
|---|---|---|---|---|
| V0 | 4420.4 ms (0.70×) | 3197.9 ms (0.97×) | 3111.9 ms (1.00×) | 3141.3 ms (0.99×) |
| V1 | 4490.9 ms (0.69×) | 3129.2 ms (0.99×) | 3081.7 ms (1.01×) | 3075.2 ms (1.01×) |
| V2 | 2920.1 ms (1.07×) | 963.9 ms (3.23×) | 828.7 ms (3.76×) | 783.7 ms (3.97×) |
| V3 | 2942.3 ms (1.06×) | 927.2 ms (3.36×) | 756.1 ms (4.12×) | 708.2 ms (4.39×) |
| V4 | 1506.5 ms (2.07×) | 731.4 ms (4.25×) | 656.8 ms (4.74×) | 607.3 ms (5.12×) |
| V5 | 1050.8 ms (2.96×) | 510.1 ms (6.10×) | 502.8 ms (6.19×) | **499.5 ms (6.23×)** |
| V6 | 1034.8 ms (3.01×) | 641.3 ms (4.85×) | 638.8 ms (4.87×) | 633.9 ms (4.91×) |
| V7 | 2019.2 ms (1.54×) | 850.6 ms (3.66×) | 700.0 ms (4.45×) | 693.3 ms (4.49×) |

Raspberry Pi 5:

| Version | `-O0` | `-O1` | `-O2` | `-O3` | `-O3 -mcpu=cortex-a76` |
|---|---|---|---|---|---|
| V0 | 12794.2 ms (0.82×) | 10740.2 ms (0.97×) | 10439.0 ms (1.00×) | 10476.1 ms (1.00×) | 10468.5 ms (1.00×) |
| V1 | 12844.9 ms (0.81×) | 10577.2 ms (0.99×) | 10492.4 ms (0.99×) | 10484.5 ms (1.00×) | 10459.7 ms (1.00×) |
| V2 | 7083.5 ms (1.47×) | 3011.1 ms (3.47×) | 2523.9 ms (4.14×) | 2336.7 ms (4.47×) | 2363.7 ms (4.42×) |
| V3 | 7085.6 ms (1.47×) | 3074.8 ms (3.40×) | 2558.9 ms (4.08×) | 2385.0 ms (4.38×) | 2409.9 ms (4.33×) |
| V4 | 4447.5 ms (2.35×) | 2346.6 ms (4.45×) | 1962.0 ms (5.32×) | 1788.6 ms (5.84×) | 1814.0 ms (5.75×) |
| V5 | 2999.1 ms (3.48×) | 1511.5 ms (6.91×) | 1409.7 ms (7.40×) | **1402.6 ms (7.44×)** | 1412.2 ms (7.39×) |
| V6 | 3382.6 ms (3.09×) | 1905.3 ms (5.48×) | 1796.5 ms (5.81×) | 1788.0 ms (5.84×) | 1798.2 ms (5.81×) |
| V7 | 6432.7 ms (1.62×) | 2316.9 ms (4.51×) | 2557.2 ms (4.08×) | 2282.9 ms (4.57×) | 2298.0 ms (4.54×) |
| V8 | 6391.4 ms (1.63×) | 2281.0 ms (4.58×) | 2539.0 ms (4.11×) | 2264.2 ms (4.61×) | 2277.8 ms (4.58×) |

### `perf stat` results (Raspberry Pi 5, `-O2`)

| Version | Cycles | Instructions | IPC | Cache references | Cache misses | Miss rate |
|---|---|---|---|---|---|---|
| V0 | 25.05 × 10⁹ | 79.81 × 10⁹ | 3.19 | 20.17 × 10⁹ | 6.06 M | 0.03% |
| V1 | 25.16 × 10⁹ | 80.19 × 10⁹ | 3.19 | 20.28 × 10⁹ | 6.23 M | 0.03% |
| V2 | 6.04 × 10⁹ | 18.28 × 10⁹ | 3.03 | 3.16 × 10⁹ | 5.23 M | 0.17% |
| V3 | 6.84 × 10⁹ | 20.55 × 10⁹ | 3.01 | 3.62 × 10⁹ | 14.54 M | 0.40% |
| V4 | 5.24 × 10⁹ | 15.88 × 10⁹ | 3.03 | 2.76 × 10⁹ | 12.41 M | 0.45% |
| V5 | 3.78 × 10⁹ | 10.58 × 10⁹ | 2.80 | 2.48 × 10⁹ | 12.18 M | 0.49% |
| V6 | 4.85 × 10⁹ | 13.17 × 10⁹ | 2.72 | 3.32 × 10⁹ | 17.86 M | 0.54% |
| V7 | 6.85 × 10⁹ | 18.27 × 10⁹ | 2.67 | 3.22 × 10⁹ | 22.32 M | 0.69% |
| V8 | 6.80 × 10⁹ | 18.22 × 10⁹ | 2.68 | 3.20 × 10⁹ | 21.96 M | 0.69% |

*One extra, untimed run of the whole program. V0 to V2 read stress_full only; V3 and later also run test20 and dev_1M first, about 11% more trades. On the Cortex-A76, perf's generic cache events count level-1 data cache accesses and refills. The brief notes that perf works only on the Pi, so there are no laptop counters.*

### Memory usage for every version (`-O2`)

| Version | Where the trades are kept | Laptop max RSS | Pi max RSS |
|---|---|---|---|
| V0, V1, V2 | one trade at a time (streaming) | 2.2 MB | 1.6 MB |
| V3, V4 | global `Trade` array (`.bss` = 320 MB) | 289.0 / 287.9 MB | 286.9 / 286.9 MB |
| V5 | global `TradeFx` array (`.bss` = 320 MB) | 289.3 MB | 286.8 MB |
| V6 | `malloc`'d array, sized to the file | 287.7 MB | 287.0 MB |
| V7 | `malloc`'d array + the whole 678 MB file text while loading | 965.4 MB | 964.4 MB |
| V8 | as V7 | — | 964.4 MB |

*Peak physical memory of the full run (test20, dev_1M and stress_full), median of 3 runs.*

The fixed global array reserves 320 MB of address space whatever the file size, but the operating system only gives it real memory for the parts that are written. Valgrind Massif (`--pages-as-heap=yes`) shows the difference: on dev_1M, V5 reserves 325.0 MB (laptop) / 324.6 MB (Pi) while V6 reserves only 37.0 / 36.6 MB, and both use about 33 to 35 MB of physical memory (Section 8.1). Valgrind Memcheck reports 0 errors and no leaks for V3, V4, V5 and V6 on both machines.

### What the benchmark shows

- V5 (fixed-point, global array) is the fastest version on both machines: 499.5 ms on the laptop (6.23× faster than Version 0) and 1402.6 ms on the Pi (7.44×), both at `-O3`. At the ladder's standard `-O2` it is 6.19× and 7.40×.
- The Pi takes 2.8 to 3.7× as long as the laptop for a whole run. In the process phase alone, the gap is largest for the naive SMA's `double` additions (V3: 4.2×) and smallest for fixed-point (V5: 2.3×).
- Reading the file is the bottleneck. The biggest single gain is the parser (V1 → V2: 3.7× on the laptop, 4.2× on the Pi): `sscanf` needs about 80 × 10⁹ instructions for the day's data, and the digit-by-digit parser needs 18 × 10⁹. Even in the in-memory versions, loading takes 68 to 94% of the run time.
- On the Pi, IPC stays between 2.7 and 3.2 for every version, so each rung's gain comes from running fewer instructions.
- `-O0` → `-O2` makes V2 3.5× faster on the laptop and 2.8× on the Pi, but V0 and V1 only about 1.4× (laptop) and 1.2× (Pi). Most of their time is spent inside `sscanf`, which is already compiled into the C library, so our flags do not affect it. `-O3` adds a few percent. On the Pi, `-O3 -mcpu=cortex-a76` is between 0.2% faster and 1.4% slower than plain `-O3`, so it brings no gain.
- Memory does not limit the Pi with this data. Streaming needs at most 2.2 MB, the in-memory versions about 287 MB, and V7 and V8 under 1 GB, against the Pi's 8 GB. Run time is what limits it.
- On the Pi, V7 and V8 run slower at `-O2` than at `-O1` or `-O3` (2557 ms vs 2317 / 2283 ms for V7). The cause is in Lucas's loader, and I did not investigate it.

Figures to include (made by `ladder_bench`):
- `benchmark/results/leroi/rpi/ladder/charts/ladder_speedup.png` (speedup against Version 0, Pi), and the laptop version in `benchmark/results/leroi/laptop/`;
- `benchmark/results/leroi/compare/charts/laptop_vs_rpi_all_versions.png` (laptop vs Pi for every version);
- `benchmark/results/leroi/rpi/ladder/charts/ladder_memory.png` and `ladder_load_vs_process.png`.

All raw logs (every run, gcc output, program output, perf output) are in `benchmark/results/leroi/laptop/` and `benchmark/results/leroi/rpi/`, and the valgrind and memory logs are in `logs/leroi_laptop/` and `logs/leroi_pi/`.
