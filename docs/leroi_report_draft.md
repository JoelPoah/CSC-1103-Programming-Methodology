# Report draft: Leroi's sections (7.1 to 7.3, 8.1, 8.3, 13.3, 14.2, 16.1 to 16.3, Appendix J.4)

Draft text to paste into the group Google Doc under the matching headings.
All versions are rungs of the group's efficiency ladder (`src/`, built by `make ladder`).
All numbers are official: stress_full.csv (8,928,944 trades), `-O2 -Wall`, `taskset -c 2`,
`performance` CPU governor, median of 5 runs, measured with `benchmark/ladder_bench` on the
laptop and the Raspberry Pi 5 (8 GB) on 2026-11-17. Source: `docs/leroi_stageB_C.md`.

## Proposed revised report outline (2026-11-19, for the group to agree)

This is the group's current outline (the updated table of contents) with the changes I suggest, checked against the brief's report list (p. 13) and the Testing and Enhancement marking (p. 14).

- **(new)** = a heading that is not in the current outline.
- **(renamed)** = the same heading, worded to say what it compares.
- **(moved)** = moved from somewhere else in the outline.
- Notes in *italics* say why, quoting the brief where a heading covers something that is marked.
- **[Leroi: drafted below]** = my text for that heading is further down in this file.
- Headings that I suggest removing are listed at the end of the outline.

**Title:** Tick-to-Trade on the Edge: Low-Latency Market Data Engine in C on Raspberry Pi 5 (Group 06) *(the current outline has two title lines; keep one)*

**Cover Page (new)** *(use `Cover Page Template_2026.docx`; the brief asks for "the names of group members and each member's individual contribution on the cover page")*
- Group Members and Individual Contributions

**1. Introduction**
- 1.1 Project Overview
- 1.2 Objectives
- 1.3 Group Parameters (Short/Long SMA Windows)
- 1.4 Dataset Description (test20.csv, dev_1M.csv, stress_full.csv)

**2. Design Solution**
- 2.1 Problem Definition
- 2.2 Problem Analysis
  - 2.2.1 Inputs and Outputs
  - 2.2.2 Indicators and Formulas (VWAP, SMA, Crossover, OHLC)
  - 2.2.3 Constraints (Speed, Memory, Power)
- 2.3 Pseudocode
  - 2.3.1 Main Processing Loop
  - 2.3.2 VWAP
  - 2.3.3 Circular Buffer SMA
  - 2.3.4 Crossover Signal Detection
  - 2.3.5 One-Second OHLC Bars
- 2.4 Program Structure, Modules and the Efficiency Ladder (V0 to V8) **(renamed)** *(a table of every version, what it changes and the files it links; every later section refers to "V5", "V6" and so on)*

**3. System Set-up**
- 3.1 Laptop Set-up (CSC1103)
- 3.2 Raspberry Pi 5 Set-up (CSC1104)
- 3.3 Benchmarking Methodology (Median of 5 Runs, taskset, CPU Governor)

**4. Stage 0: Getting Started on the Pi**
- 4.1 Sum Loop Timings at -O0, -O1, -O2, -O3
- 4.2 Optimiser Behaviour Observations
- 4.3 Recursive vs Iterative Sum
- 4.4 LED Integration
- 4.5 Laptop vs Pi Sum-Loop Timings (-O0, -O2) **(renamed)** *(so it is not confused with 13.4)*

**5. Stage A: Streaming Engine**
- 5.1 VWAP Implementation
- 5.2 One-Second OHLC Bars
- 5.3 Short and Long SMA (Circular Buffer)
- 5.4 BUY/SELL Crossover Signals
- 5.5 Correctness Check against test20.csv
- 5.6 Signal Plot (signals.png)

**6. Stage A+: Own Reader and Fast Parser**
- 6.1 read_trade() with Pointer Parameters
- 6.2 Custom Character-Level Parser
- 6.3 Speedup on stress_full.csv (V0 → V1 → V2)

**7. Stage B: In-Memory Engine and Efficiency Ladder**
- 7.1 Trade Struct and Global Array (In-Memory vs Re-reading the File, V2 → V3) **(renamed)** **[Leroi: drafted below]**
- 7.2 Naive SMA vs Running-Sum SMA (V3 → V4) **(renamed)** **[Leroi: drafted below]**
- 7.3 Fixed-Point Prices (V4 → V5) **(renamed)** **[Leroi: drafted below]**
  - 7.3.1 Choice of Scaling Factors
  - 7.3.2 Overflow Analysis
  - 7.3.3 Precision vs Double
- 7.4 Compiler Optimisation Flags (-O0 to -O3, -mcpu=cortex-a76)

**8. Stage C: Real-Time Replay and Signal**
- 8.1 Dynamic Memory with malloc (V6) **(renamed)** **[Leroi: drafted below]**
- 8.2 Whole-File Loading with fread (V7) **(new)** *(V7 had no heading, but it is the `mem` engine used in the replay and the rival bot; it is also slower than V6 on the Pi and uses 965 MB. Lucas's part)*
- 8.3 Memory Usage Comparison (Fixed Array vs malloc, V5 vs V6) **(renamed)** **[Leroi: drafted below]** *(was 8.2; brief Stage C 1: "replace the fixed-size array with one allocated by malloc … and compare memory use". V7's memory goes in 8.2)*
- 8.4 Replay Engine (1×, 100×, 1000×)
- 8.5 LED and Coloured Terminal Signal Display
- 8.6 Latency Measurement Method **(renamed)** *(was "Processing Latency (Average and Worst Case)"; the results table goes in 10.4 only, so the numbers appear once)*
- 8.7 Annotated Assembly of the Main Loop (gcc -S -O2) *(required: "your annotated assembly listing". The current listing, `stageC/asm/annotated_main_loop.md`, is x86-64 from a Windows laptop; it should be regenerated and annotated as AArch64 on the Pi)*

**9. Performance Measurement (Official Results)** *(the group's one official set of numbers, laptop and Pi; each member's own write-up is in Appendix J. The system set-up is in 3.1 and 3.2 and the method in 3.3, so they are not repeated here)*
- 9.1 Timing and Throughput for Every Version (Laptop and Pi) *(time, ns/trade, trades per second, load vs process phase)*
- 9.2 perf stat Results (Cycles, Instructions, IPC, Cache Misses) *(Pi, and the laptop as well, since ours runs Linux)*
- 9.3 Memory Usage for Every Version (Max RSS) *(the fixed-array vs malloc detail is in 8.3)*
- 9.4 Agreement Between Members' Runs **(new, optional)** *(one short table: each member's Pi median for a few versions, to show the official numbers are repeatable)*

  *The group needs to choose whose run is the official set. My `ladder_bench` run (2026-11-17, `performance` governor, every version V0 to V8 at every flag level, laptop and Pi, with perf on both) is complete and could be used; the speedups in 10.2 must come from the same run.*

**10. Performance Discovery**
- 10.1 Correctness Check of Every Version (test20.csv) **(new)** *(the brief: "Every version on your ladder must still pass the test20.csv check". This can also be a "passes test20" column in 10.2 instead of its own heading)*
- 10.2 Speedup Ladder Table
- 10.3 Speedup Ladder Graph
- 10.4 Average and Worst-Case Latency at Each Replay Speed *(was 10.3; the one latency table; the method is in 8.6)*

**11. Rival Bot: The Cost of Slow Code**
- 11.1 Simulation Method
- 11.2 P&L of Version 0 vs Fastest Version
- 11.3 Money Lost to Slow Code
- 11.4 Money Lost vs Rival Reaction Time (0.1 to 100 µs) and Where Each Version Starts Losing **(renamed)** *(the brief asks for the graph and to "explain the point where each version starts losing")*
- 11.5 Race Demonstration (Lag Counter and Running P&L) **(renamed)** *(the brief: "explain in your own words why one keeps up and the other does not")*

**12. Prediction Logbook** *(at least 6 entries, at least one per stage, each dated and signed)*
- 12.1 Logbook Format
- 12.2 Stage 0 Entries
- 12.3 Stage A Entries
- 12.4 Stage A+ Entries
- 12.5 Stage B Entries
- 12.6 Stage C Entries

**13. Analysis and Discussion**
- 13.1 Comparison Across Versions
- 13.2 What Each Change Taught Us About the Hardware
- 13.3 Trade-offs **[Leroi: drafted below]**
  - 13.3.1 Speed vs Code Readability
  - 13.3.2 Fixed-Point Precision vs Double
  - 13.3.3 Memory vs Speed **(new)** *(the third trade-off that the recommendation in 16 relies on)*
- 13.4 Laptop vs Pi Differences

**14. Stretch Goals**
- 14.1 Hand-written AArch64 Assembly (V8) vs the Compiler **(renamed)** *(was "Hand-written Assembly / NEON SIMD"; we did not use NEON. The brief: "beat the compiler's -O3 version or explain precisely why the compiler already won". We have both: our OHLC made the process phase 1.14× faster, and the compiler's SMA beat ours)*
- 14.2 Struct-of-Arrays (V5-SoA vs V5) **[Leroi: drafted below]** *(done 2026-11-19; compared with V5 only, not a rung of the ladder)*
- 14.3 C versus Python (and NumPy) *(was 14.4)*

**15. Interesting Aspects of Our Solution**
- 15.1 Assumptions Made
- 15.2 Optimisations Considered *(also the place for ideas we did not build: p50/p99 latency, a streaming fixed-point version)*

**16. Deployment Recommendation**
- 16.1 Recommended Version for a Power- and Memory-Limited Edge Device **[Leroi: drafted below]**
- 16.2 Justification **[Leroi: drafted below]**
- 16.3 When a Different Version Is Better **(new)** **[Leroi: drafted below]**

**17. Conclusion**

**References**

**Appendices** *(the brief: "all source code as appendices")*
- Appendix A: Source Code – Stage 0
- Appendix B: Source Code – Stage A
- Appendix C: Source Code – Stage A+
- Appendix D: Source Code – Stage B and the Efficiency Ladder (`src/`) **(renamed)** *(the ladder spans every stage and is the code behind all our numbers)*
- Appendix E: Source Code – Stage C (including the live GUI, `stageC/stageC_gui.c`)
- Appendix F: Makefile and README
- Appendix G: Assembly Files (`src/engine_asm/`, `assembly/`) **(new)**
- Appendix H: Source Code – Benchmark Tool (`benchmark/`) **(new)** *(the program behind every timing in the report)*
- Appendix I: Source Code – Python Version (`Python/`) **(new)**
- Appendix J: Per-Member Benchmark Documentation **(new)** *(Testing and Enhancement 1, 10%: "each member runs and documents the full benchmark on stress_full.csv on the Laptop (CSC1103) and Pi (CSC1104), one set of documentation per member")*
  - J.1 Joel
  - J.2 Yong Sheng
  - J.3 Jaren
  - J.4 Leroi **[Leroi: drafted below]**
  - J.5 Lucas

  Each member uses the same headings (the four items the brief lists), laptop and Pi, so the sets are easy to compare:
  - J.x.1 System Set-up (Pi model, OS, compiler version, flags, CPU governor)
  - J.x.2 Timing and Throughput for Every Version
  - J.x.3 perf stat Results (Cycles, Instructions, IPC, Cache Misses)
  - J.x.4 Memory Usage for Every Version
  - J.x.5 Summary *(optional: what that member's run shows)*

**Suggested for removal from the current outline**
- **9.1 Laptop Results / 9.2 Raspberry Pi 5 Results** (and their sub-headings): replaced by one official set split by measurement (9.1 to 9.3), with both machines in each table. The per-member sets are in Appendix J.
- **14.3 Percentile Latency (p50, p99)**: there is no code for it in the repo, and `stageC/README.md` says p50/p99 are not reported. Keep it only if Lucas does it before the deadline; otherwise mention it in 15.2. (Struct-of-Arrays was on this list; it is now done, see 14.2.)
- **Appendix G: Raw Benchmark Logs and perf Outputs**: the brief asks for the logs in the zip folder ("all printed results, benchmark logs and perf outputs"), not in the PDF. Point to the folder, or include one short sample per machine.

---

## 7.1 Trade Struct and Global Array (In-Memory vs Re-reading the File, V2 → V3)

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
| V2 streaming (fast parser) | 826.7 ms | 92.6 | 10.8 M | 1.00× | 3.70× | 2518.7 ms | 1.00× |
| V3 in-memory, load + process | 771.2 ms | 86.4 | 11.6 M | 1.07× | 3.97× | 2556.9 ms | 0.99× |
| V3 in-memory, process phase only | 194.0 ms | 21.7 | 46.0 M | 4.26× | 15.77× | 817.3 ms | 3.08× |

*stress_full.csv, `-O2`, `taskset -c 2`, median of 5 runs.*

The process phase on its own is 4.26× faster than the whole streaming run on the laptop and 3.08× faster on the Pi, because reading and parsing now happen outside the timed part. Loading takes 75% (laptop) and 68% (Pi) of V3's time.

For load plus process together, the two machines disagree. On the Pi, V3 is 1.5% slower than streaming, as we predicted: it does the same parsing and the same maths, and it also writes 286 MB into the array and reads it back. The Pi's `perf stat` counters agree. V3 runs the same number of instructions per trade as V2 at the same IPC (3.01 vs 3.03), but has 2.48× more level-1 cache misses per trade (1.47 vs 0.59) from the extra memory traffic.

On the laptop, V3 was 7.2% faster than streaming, which we did not expect. The laptop's counters show why. V3 also runs the same number of instructions per trade as V2 there, but at a 9% higher IPC (4.63 vs 4.23 instructions per cycle), so it needs 8% fewer cycles per trade. The laptop's wide out-of-order core runs two simple loops, each doing one job (parse everything, then compute everything), faster per instruction than one loop that switches between parsing and maths on every trade. The Pi's narrower core gets no such gain from the split, so only the extra memory traffic is left.

For a single pass over the data, then, loading into memory does not pay off on the edge device. It pays off when the same data is used more than once, as in the Stage C replay or the race.

V2 needs 2.2 MB (laptop) / 1.6 MB (Pi) of physical memory and V3 needs 289.0 / 286.9 MB. The fixed array always reserves 320 MB, whatever the file size: stress_full fills 285.7 MB of it and dev_1M only 32.0 MB. Section 8.3 compares this with the `malloc` version.

## 7.2 Naive SMA vs Running-Sum SMA (V3 → V4)

The brief asks for the SMA to be computed twice: first naively, by adding up the whole window again at every trade, then with a running sum.

- The naive version (`src/engine/v0-v3_sma_naive.c`, used by V0 to V3) stores each new price in the circular buffer and then adds up all N prices again. That is O(N) per trade: 35 + 60 = 95 additions per trade for our windows.
- The running-sum version (`src/engine/v4_sma_running.c`, V4) updates the window sum in one step, S_new = S_old + newest price − oldest price. That is O(1) per trade: 2 additions and 2 subtractions.

Both files implement the same header, `sma.h` (`sma_init`, `sma_push`, `sma_ready`, `sma_value`), and V3 and V4 are the same program with a different SMA file linked in. The two differ only in how the sum is found, and the timed loop contains no `if` to choose between them.

| Version | Laptop median | ns/trade | trades/s | Speedup | Pi median | Pi speedup |
|---|---|---|---|---|---|---|
| V3 naive SMA | 194.0 ms | 21.7 | 46.0 M | 1.00× | 817.3 ms | 1.00× |
| V4 running-sum SMA | 89.9 ms | 10.1 | 99.4 M | 2.16× | 218.1 ms | 3.75× |

*Process phase only, stress_full.csv, `-O2`, `taskset -c 2`, median of 5 runs. Whole run: laptop 771.2 → 662.7 ms (1.16×), Pi 2556.9 → 1958.5 ms (1.31×); speedup vs Version 0 rises from 3.97× to 4.62× (laptop) and from 4.09× to 5.34× (Pi).*

The running sum is 2.16× faster on the laptop and 3.75× faster on the Pi. The operation count (95 vs 4) would suggest about 24×, but the SMA is only part of the work per trade: VWAP, OHLC and the crossover check cost the same in both versions. The naive loop also runs over just 35 and 60 doubles, a window small enough to stay in the L1 cache, so re-adding it is cheap.

The gain is larger on the Pi, as we predicted (3 to 5×). `perf stat` shows that the running sum removes 23% of the instructions (about 471 per trade) while the IPC stays at about 3. On the Pi, the naive version takes 67 ns more per trade, and running those 471 extra instructions at that IPC and 2.4 GHz takes about 65 ns, so the Pi pays for every extra addition. On the laptop, the naive version costs only 12 ns more per trade. Its counters show it needs about half as many extra instructions (233 per trade) and runs them at a higher IPC and almost twice the clock speed.

Both versions pass `test20.csv`, but on the larger files they give different numbers of signals:

| File | V3 naive | V4 running sum | Exact answer |
|---|---|---|---|
| dev_1M.csv | 2509 BUY / 2469 SELL | 2465 BUY / 2464 SELL | 2553 BUY / 2469 SELL |
| stress_full.csv | 20386 BUY / 20176 SELL | 20188 BUY / 20188 SELL | 20943 BUY / 20060 SELL |

The price often stays flat for many trades, so both windows hold the same prices and the two SMAs are exactly equal. This happens on 17,132 trades in dev_1M (1.7%) and 156,471 in stress_full.

On those trades s = SMA_short − SMA_long should be exactly 0. With `double`, s comes out as a tiny rounding error such as ±10⁻¹⁰ with an effectively random sign, which can create or hide BUY and SELL signals. The two methods add the numbers in a different order, so they round differently and give different counts. Neither matches the exact answer, which we found with whole-number arithmetic (Section 7.3), and `test20.csv` is too short to show the problem. This was the main reason we moved to fixed-point prices.

## 7.3 Fixed-Point Prices (V4 → V5)

The fixed-point version (V5, `src/v5-v8_main_stageB_fixed.c`) stores prices and quantities as whole numbers (`long long`) instead of `double`. Its modules are `src/store/trades_fx.h`, `src/engine_fx/vwap_fx`, `sma_fx`, `signal_fx` and `ohlc_fx`, with the scales in `src/engine_fx/fixed.h`.

The reader (`read_trade_fx`) builds each number straight from its digits as a scaled whole number, so 76589.57 becomes 7658957 and no `double` is ever created. If a value has more decimals than the scale can hold exactly, the reader rejects it instead of rounding it silently. The whole processing loop (VWAP sums, OHLC, SMA running sums and the crossover) then uses only whole-number arithmetic, and values are turned back into doubles only for printing.

The crossover test no longer divides. Instead of the sign of SMA_short − SMA_long, it uses the sign of

  sum_short × L − sum_long × S  (S = 35, L = 60)

This is the original difference multiplied by S × L, which is positive, so the sign is the same. Because there is no division, the result is exact.

### 7.3.1 Choice of Scaling Factors

We chose the scales by checking the data in `stress_full.csv`:

| Field | Decimals in data | Range | Scale | Stored as |
|---|---|---|---|---|
| price | at most 2 | 73,027.02 to 79,500.00 | ×100 (cents) | 7,302,702 to 7,950,000 |
| qty | at most 5 | up to 18.29992 | ×100,000 | up to 1,829,992 |

Both scales store every value exactly, so no information is lost. The file prints quantities with 8 decimals, but the last 3 are always 0, because Binance's smallest step for this coin is 0.00001 BTC.

### 7.3.2 Overflow Analysis

`long long` holds values up to about 9.22 × 10¹⁸. The worst cases are:

| Value | Worst case | Fits? |
|---|---|---|
| one price × qty | 7.95×10⁶ × 1.83×10⁶ ≈ 1.5×10¹³ | yes |
| VWAP Σ(price × qty), full day | 3.40×10⁹ USDT × 100 × 10⁵ ≈ 3.4×10¹⁶ (measured 3.402×10¹⁶) | yes, ~270× headroom |
| VWAP Σ qty, full day | ~44,600 BTC × 10⁵ ≈ 4.5×10⁹ | yes |
| SMA window sum (60 prices) | 60 × 7.95×10⁶ ≈ 4.8×10⁸ | yes |
| crossover sum × window | 4.8×10⁸ × 60 ≈ 2.9×10¹⁰ | yes |

The largest value is the full-day VWAP numerator, at 3.4×10¹⁶. Scaling quantity by 10⁸ to keep all 8 printed decimals would make that sum 3.4×10¹⁹, which overflows. ×10⁵ is the largest scale that is exact for this data and still safe.

### 7.3.3 Precision vs Double

| Check | V4 `double` | V5 fixed-point | Exact answer |
|---|---|---|---|
| test20.csv | pass | pass | — |
| dev_1M VWAP | 76217.01291587 | 76217.01291611 | 76217.01291611 |
| dev_1M signals | 2465 BUY / 2464 SELL | 2553 BUY / 2469 SELL | 2553 / 2469 |
| stress_full VWAP | 76718.67569936 | 76718.67569889 | 76718.67569889 |
| stress_full signals | 20188 BUY / 20188 SELL | 20943 BUY / 20060 SELL | 20943 / 20060 |

We checked the exact VWAP independently with 40-digit decimal arithmetic (Python `Decimal`), and the exact signal counts with a separate whole-number check. The later fixed-point versions (V6, V7, V8) give the same results as V5.

Fixed-point matches the exact answer everywhere and `double` does not. The double VWAP is off by only 2.4×10⁻⁷ (dev_1M) and 4.7×10⁻⁷ USDT (stress_full), rounding error that builds up over millions of additions. The signals show a larger effect. When the two SMAs are exactly equal, whole numbers give s = 0 exactly, so the brief's rule (BUY when s goes from ≤ 0 to > 0) is applied exactly as written, while double gives a tiny s of random sign.

The dev_1M reference counts from Stage A (2465 / 2464 with a running sum, 2509 / 2469 with the naive sum) are therefore rounding effects too. The correct counts are 2553 / 2469.

BUY is higher than SELL in the exact counts because of how the rule treats s = 0. Without zeros, BUY and SELL simply alternate. A sequence such as s = +, 0, + (the short SMA touches the long one from above and moves away again) produces a BUY when s returns to + after the 0, but no SELL, and the mirror case −, 0, − produces a SELL but no BUY. In dev_1M the first case happens 240 times and the second 157 times, which accounts for 83 of the 84 extra BUYs; the last one comes from whether the day starts and ends on a BUY.

The speed of the two versions:

| Phase | V4 `double` | V5 fixed-point | Speedup | Pi V4 | Pi V5 | Pi speedup |
|---|---|---|---|---|---|---|
| Process | 89.9 ms | 72.0 ms | 1.25× | 218.1 ms | 162.9 ms | 1.34× |
| Load | 572.3 ms | 437.0 ms | 1.31× | 1740.4 ms | 1245.2 ms | 1.40× |
| Total | 662.7 ms | 509.0 ms | 1.30× | 1958.5 ms | 1408.2 ms | 1.39× |
| Speedup vs Version 0 | 4.62× | 6.01× | | 5.34× | 7.42× | |

*stress_full.csv, `-O2`, `taskset -c 2`, median of 5 runs.*

V5 is the fastest version on the laptop (6.01× faster than Version 0) and the fastest C version on the Pi (7.42×); only V8, which replaces one C function with hand-written assembly, is faster on the Pi (7.54×). Three things change from V4 to V5, so we read the numbers phase by phase.

The process phase (1.25× laptop, 1.34× Pi) is the fixed-point vs `double` comparison. Integer add and compare are slightly cheaper than double add and compare, and the SMA needs no division. Each trade still goes through the same steps and the struct is still 32 bytes, so the gain is modest. Some of it may come from the different module style, since V5's modules keep their state in structs and V4 uses Joel's Stage A modules. In an earlier test where only the number type changed, the gain was 1.13×.

The load phase (1.31× laptop, 1.40× Pi) gains from the new reader, which builds whole numbers from the digits instead of doubles. That is only possible because the values are fixed-point.

`perf stat` shows V5 runs about a third fewer instructions than V4 on both machines (33% fewer on the Pi, 31% on the laptop), at a slightly lower IPC (Pi 2.80 vs 3.03, laptop 4.31 vs 4.82). Every result in the precision table above is identical on the laptop and the Pi, including the `double` VWAP to all 8 decimals.

`TradeFx` is also 32 bytes, so fixed-point uses the same memory as the double version (max RSS 289.3 vs 287.9 MB on the laptop, 286.8 vs 286.9 MB on the Pi).

## 8.1 Dynamic Memory with malloc (V6)

Stage C (V6) replaces the fixed 320 MB global array with an array allocated by `malloc` that is exactly the size of the file. The loader is `load_trades_fx(path, &n)` in `src/store/v6_trades_fx_malloc.c`. It returns a pointer to the array, and the caller frees it when it has finished. V6 uses the same `TradeFx` struct, reader and main program as V5, so the only change is where the array lives. The replay engine (Section 8.4) and V7 load their trades through the same interface.

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

## 8.3 Memory Usage Comparison (Fixed Array vs malloc, V5 vs V6)

Memory use of V5 and V6:

| Version | Run | Array needed | Reserved (Massif, `--pages-as-heap=yes`), laptop / Pi | Physical (max RSS), laptop / Pi |
|---|---|---|---|---|
| V5 fixed array | dev_1M | 32.0 MB | 325.0 / 324.6 MB | 34.5 / 33.1 MB |
| V6 `malloc` | dev_1M | 32.0 MB | 37.0 / 36.6 MB | 34.0 / 33.1 MB |
| V5 fixed array | stress_full | 285.7 MB | 325.0 / 324.6 MB | 289.3 / 286.8 MB |
| V6 `malloc` | stress_full | 285.7 MB | 290.7 / 290.3 MB | 287.7 / 287.0 MB |

*"dev_1M" = test20 + dev_1M; "stress_full" = all three files, peak while stress_full is loaded. MB = 10⁶ bytes. Max RSS is the median of 3 runs.*

On both machines `malloc` saves 288.0 MB of reserved memory on dev_1M (−89%) and 34.3 MB on stress_full (−11%), because the fixed array always reserves 320 MB. Physical memory is almost the same: within 1.6 MB on the laptop and 0.2 MB on the Pi. The operating system only gives real memory to the pages of the global array that are written to, so the unused part costs address space but no RAM. Valgrind's normal heap view cannot see the global array at all (its peak for V5 is 9 kB on the laptop and 13 kB on the Pi), which is why Massif needs `--pages-as-heap=yes` for a fair comparison.

The speed of the two versions:

| Phase | V5 fixed array | V6 `malloc` | Difference | Pi V5 | Pi V6 | Pi difference |
|---|---|---|---|---|---|---|
| Load | 437.0 ms | 572.8 ms | +31.1% | 1245.2 ms | 1636.4 ms | +31.4% |
| Process | 72.0 ms | 70.6 ms | −1.9% | 162.9 ms | 163.7 ms | +0.5% |
| Total | 509.0 ms | 643.3 ms | +26.4% | 1408.2 ms | 1799.6 ms | +27.8% |

*stress_full.csv, `-O2`, `taskset -c 2`, median of 5 runs.*

The process phase is the same within run-to-run noise, because the loop reads the same 32-byte structs in the same order. Loading is 31.1% slower on the laptop and 31.4% on the Pi because of the extra counting pass over the 678 MB file (`perf stat` shows 24% more instructions on the Pi and 21% more on the laptop). Our fast parser makes the real parse quick, so reading the file a second time is a large share of the load.

`malloc` therefore makes the program slower and saves almost no physical memory. What it gains is that the array fits any file (the fixed array stops at 10 million trades, and stress_full already uses 89% of that), the memory can be freed after use, and the program can stop cleanly when memory runs out. Lucas's V7 avoids the double read by reading the whole file with one `fread`, at the cost of holding all of the text in memory while loading (Section 8.2).

## 13.3 Trade-offs

Each rung bought speed, memory or precision, and the cost differed from rung to rung. Some made the code harder to follow, fixed-point ties the engine to this day's data, and the assembly and `-mcpu` builds only run on the Pi. All times below are `stress_full.csv`, `-O2`, `taskset -c 2`, `performance` governor, median of 5 runs, from Leroi's run on 17 Nov 2026.

### 13.3.1 Speed vs Code Readability

Each row is one step, measured against the rung before it. Code lines count the `.c` and `.s` files each version links, without comments or blank lines (counted with `gcc -fpreprocessed -E -P`).

| Change | Step | Pi speedup of this step | Laptop | Code lines | What it costs to read and maintain |
|---|---|---|---|---|---|
| Own reader through pointers | V0 → V1 | 0.99× | 0.99× | reader 79 → 32 | Nothing: the interface is clearer, but `sscanf` still does the work. |
| Digit-by-digit parser | V1 → V2 | 4.17× | 3.73× | reader 32 → 86 | The parser handles each field's format itself (digits, the decimal point, `True`/`False`), which `sscanf` did in one line. |
| Load into a global array | V2 → V3 | 0.99× | 1.07× | + 33 (loader) | Simple code, but 320 MB is always reserved and the program stops at 10 million trades. |
| Running-sum SMA | V3 → V4 | 1.31× (process 3.75×) | 1.16× (process 2.16×) | SMA 39 → 42 | Almost free to read. With `double`, though, rounding errors build up in the sum and change the signal counts (Section 7.2). |
| Fixed-point prices | V4 → V5 | 1.39× | 1.30× | whole program 413 → 387 | Fewer lines, but more ideas to keep in mind: units (cents, ×10⁵), scales chosen from the data, an overflow check, conversion back for printing. |
| `malloc` sized to the file | V5 → V6 | 0.78× | 0.79× | loader 34 → 65 | A pointer, a `NULL` check, a `free` on every error path, and the file is read twice. |
| Whole file in one `fread` | V5 → V7 | 0.55× | 0.73× | loader 34 → 102 | A 678 MB text buffer and hand-written line splitting; peak memory 965 MB. |
| Assembly OHLC | V5 → V8 | 1.016× | Pi only | OHLC 28 lines of C → 68 of assembly | Runs on the Pi only, and the AArch64 calling rules must be followed by hand. |
| Assembly SMA | V5 → V8 | 0.99× | Pi only | SMA 36 lines of C → 105 of assembly | The same costs, and it is slower than the compiler's version. |
| `-O2` → `-O3` | V5 | 1.007× | 1.004× | none | None, but the gain is within run-to-run noise. |
| `-O3` → `-O3 -mcpu=cortex-a76` | V5 | 0.995× | Pi only | none | A Pi-only build, with no gain. |

What the table shows:

- The biggest gains came from doing less work, all in plain C. Together the parser (4.17×), the running sum (1.31×) and fixed-point (1.39×) take the Pi from 10.45 s to 1.41 s. Each one cuts the instructions the CPU has to run (Pi: V1 80.2, V2 18.3, V4 15.9 and V5 10.6 × 10⁹), while IPC stays at about 3.
- The low-level changes gave almost nothing. Hand-written assembly gained 1.6% at best and lost 1.2% for the SMA, `-O3` gained 0.7%, and `-mcpu=cortex-a76` gained nothing. Each of them makes the code harder to read or ties it to one machine, for a gain no bigger than the run-to-run noise. Our assembly OHLC did make the process phase 1.14× faster, but loading is 90% of the run, so the whole run gained only 1.6%. For the SMA, the compiler's code was already faster than ours.
- Moving the data into memory costs time when the data is only used once. Loading into a global array (V3), `malloc` (V6) and `fread` (V7) do not make a single pass over the file any faster on the Pi, because reading the file takes 68 to 94% of the run. They pay off when the same trades are used again, as in the Stage C replay.
- The total code size hardly changes: every version is 330 to 455 lines. What grows is how much each version asks the reader to understand, from pointers in V1 and character-by-character parsing in V2 to scaled whole numbers in V5, dynamic memory in V6 and assembly in V8.

### 13.3.2 Fixed-Point Precision vs Double

Section 7.3 has the full comparison; the table sums it up.

| | `double` (V0 to V4) | Fixed-point (V5 to V8) |
|---|---|---|
| stress_full signals | 20386 / 20176 (naive), 20188 / 20188 (running sum) | 20943 / 20060 (exact) |
| stress_full VWAP | off by 4.7 × 10⁻⁷ USDT | exact |
| Same answer on laptop and Pi | yes | yes |
| Process phase (Pi) | 218.1 ms (V4) | 162.9 ms (V5), 1.34× faster |
| Works for any data | yes | only if every price has ≤ 2 decimals and every quantity ≤ 5 decimals, and the day's sums stay below 9.2 × 10¹⁸ |
| Ideas the reader must follow | none beyond ordinary arithmetic | units, scale factors, overflow, conversion for printing |

The error in the VWAP does not matter for a trading signal, since it is in the 7th decimal place. The error in the signals does. On stress_full, `double` misses 2.7% (naive sum) to 3.6% (running sum) of the BUY signals, and which ones it misses depends only on the order of the additions. Here fixed-point is exact and also faster, so it costs no speed. It also made the rival bot comparison fair: Version 0 and the fastest version have to raise the same signals, and with `double` the naive and running-sum versions do not.

What fixed-point gives up is generality, because we chose the scales from this day's data. A coin with more decimal places, or a day with far more volume, would need new scales and a new overflow check. Our reader rejects a value with too many decimals instead of rounding it, so this would show up as an error and not as a wrong answer. With `double`, none of this analysis is needed.

### 13.3.3 Memory vs Speed

| Version | Peak memory (Pi) | Whole run (Pi) | Limit |
|---|---|---|---|
| V0 to V2 (streaming) | 1.6 MB | 10.45 s / 10.51 s / 2.52 s | none: one trade at a time |
| V3 to V5 (global array) | 287 MB, 320 MB reserved | 2.56 s / 1.96 s / 1.41 s | 10 million trades |
| V6 (`malloc`) | 287 MB, only what the file needs is reserved | 1.80 s | free RAM |
| V7 (`fread`) | 964 MB | 2.56 s | free RAM, and the file's text is held as well |

Streaming needs about 180× less memory than any in-memory version, and V2 takes only 1.8× as long as the fastest version. Holding the trades in memory pays off when they are used more than once: in the replay, in the race, or to re-run the indicators with other windows. At 32 bytes per trade, a 4 GB Pi with about 3 GB free could hold about 90 million trades, roughly ten days at stress_full's rate. That is an estimate; we did not measure it.

## 14.2 Struct-of-Arrays (V5-SoA vs V5)

The brief's stretch goal is to "store each field in its own array instead of an array of structs, and explain any speedup using cache lines". V5-SoA is V5 with that one change. The reader, the fixed-point modules and the work per trade are the same, so the comparison measures the memory layout alone. V5-SoA is not a rung of the efficiency ladder and is compared only with V5.

V5 keeps one global array of 32-byte `TradeFx` structs. V5-SoA keeps four global arrays, one per field, and the main program reaches them through a struct of pointers (`src/store/trades_soa.h`):

```c
typedef struct {
    long long *time;     /* timestamps in microseconds */
    long long *price;    /* prices x PRICE_SCALE       */
    long long *qty;      /* quantities x QTY_SCALE     */
    int       *is_buy;   /* 1 = buy, 0 = sell          */
    long long  n;        /* number of trades loaded    */
} TradesSoa;
```

The loader (`src/store/soa_trades_fx.c`) reads each line with the same `read_trade_fx()` as V5 and stores the four fields in the four arrays. The main program (`src/soa_main_stageB_fixed.c`) is a copy of V5's in which only the way the loop reads a trade changes, from `t->price` to `price[i]`. We copied it instead of editing V5's main because that file also builds V6, V7 and both V8s, and their measured code had to stay the same.

A cache line is 64 bytes and holds 2 `TradeFx` structs. The processing loop uses `time`, `price` and `qty` (24 bytes per trade). It never reads `is_buy`, which only the test20 table prints. So in V5, every line the loop loads also carries 8 bytes it does not use: `is_buy` and 4 bytes of padding. With one array per field, a line holds 8 values of a single field and the loop never touches the `is_buy` array. It loads 0.375 lines per trade instead of 0.5, which is 25% fewer.

We timed both versions on the laptop in the same session on 19 Nov 2026: one warm-up run each, then 5 runs alternating V5 and V5-SoA, on stress_full.csv with `-O2`, `taskset -c 2` and the `performance` governor. The table gives the medians.

| | V5 (array of structs) | V5-SoA | Change |
|---|---|---|---|
| Process phase | 68.6 ms (7.69 ns/trade) | 66.8 ms (7.48 ns/trade) | 1.03× faster (−2.7%) |
| Load phase | 434.4 ms | 433.8 ms | −0.1% |
| Whole stress_full run | 502.9 ms | 500.4 ms | −0.5% |
| Reserved memory (`.bss`) | 320.0 MB | 280.0 MB | −40.0 MB |
| Peak physical memory (max RSS) | 289.5 MB | 259.2 MB | −30.2 MB |
| Instructions (`perf stat`) | 11.02 × 10⁹ | 11.08 × 10⁹ | +0.5% |
| L3 cache misses (`perf stat`) | 21.67 M | 19.91 M | −8.1% |

Raspberry Pi 5: TODO (not measured yet).

V5-SoA gives exactly the same output as V5 on all three files. The process times varied by only 0.3 to 0.4% between runs, so the 2.7% gain is real, though small.

The counters agree with the cache-line count. On stress_full the process phase now reads 214 MB instead of 286 MB (3.35 M lines instead of 4.46 M), and the load phase writes 250 MB instead of 286 MB (3.91 M lines instead of 4.46 M). That is about 1.7 M fewer lines, and `perf stat` counts 1.76 M fewer L3 misses. The instruction count is unchanged, so the whole gain comes from moving fewer cache lines.

The gain is much smaller than the 25% cut in bytes because memory was not what limited the loop. V5 reads its array in order at only about 4 GB/s, and the CPU's hardware prefetcher loads the next cache lines before they are needed. Most of each trade's 7.7 ns goes on the work itself: the calls into the VWAP, OHLC, SMA and crossover functions and the updates inside them. Fewer cache lines only cut the time spent waiting for memory, which was a small part of the total. The whole run improves by only 0.5%, because loading the file is 86% of it and does not change.

The memory saving is larger, and certain. V5-SoA reserves 40 MB less and uses 30 MB less RAM, because it no longer stores the 4 bytes of padding at the end of every struct. Peak memory is about 5 MB above the 250 MB of data. We did not check why; one possible cause is that this laptop's transparent huge pages are set to `always`, so memory may be handed out in 2 MB blocks, and each of the four arrays then ends in a partly used block.

For this engine, Struct-of-Arrays saves memory and a little processing time. In return it needs a second main program and a loader that splits every trade across four arrays, and one struct per trade is easier to read and to pass to a function. On a memory-limited device the 30 MB matters more than the 2.7%.

## 16.1 Recommended Version for a Power- and Memory-Limited Edge Device

We recommend V6, built with plain `-O2` and no assembly, for the Raspberry Pi 5 signal box. V6 has fixed-point prices, the running-sum SMA and a `malloc`'d array sized to the file.

## 16.2 Justification

1. Only the fixed-point versions (V5 to V8) give the exact signals, and they give the same answers on the laptop and the Pi. Speed is worth nothing in a trading engine if the signals are wrong.
2. Once the trades are loaded, V6 is as fast per trade as the fastest C version: it processes them in 163.7 ms on the Pi (18.3 ns per trade), the same as V5 (162.9 ms) within noise. In a signal box the per-trade time decides whether a signal beats the rival, and V6's extra 391 ms of loading is paid once, at start-up. The Stage C results show why the per-trade time matters: at R = 1 µs, Version 0 wins none of the 37,063 signals against the rival and the fixed-point engine wins 37,060, a difference of +1,799.30 USDT over the day (Section 11). At 1000× replay (Section 10.4), Version 0's average latency is 220 µs and the fixed-point engine's is 5.84 µs.
3. A full day needs 287 MB of RAM, about 7% of the 4 GB Pi the course issues. Unlike V5, V6 reserves only what the file needs (dev_1M: 37 MB instead of 325 MB) and has no fixed limit of 10 million trades, of which stress_full already uses 89%. It gives the memory back with `free` and stops with a clear error if `malloc` fails. Valgrind reports no leaks or errors. V7 is ruled out because it needs 964 MB and is slower on the Pi.
4. We did not measure power, but V6 should use less of it. At a fixed clock, the CPU's energy for a job grows roughly with the number of cycles it is busy. V6 needs 4.83 × 10⁹ cycles against Version 0's 25.05 × 10⁹, 5.2× fewer, even though V6's count also includes test20 and dev_1M. It finishes the same work in about a fifth of the time, and the CPU can idle for the rest.
5. Every member can explain V6's plain, portable C, and it builds the same on the laptop and the Pi. We left out the changes whose gain is within noise: assembly (+1.6%, Pi only, two to three times as many lines), `-O3` (+0.7%) and `-mcpu=cortex-a76` (no gain).

## 16.3 When a Different Version Is Better

V5 is the better choice when the largest day is known in advance and start-up time matters. It takes 22% less time end to end (1.41 s against 1.80 s) and uses the same RAM.

With a live feed, trades arrive one at a time from the network and there is no file to load. The best design would then stream like V2 (1.6 MB of memory) through V6's fixed-point kernels. We did not build this streaming fixed-point version; it would be the next rung to add.

---

## Appendix J.4: Leroi (Per-Member Benchmark Documentation)

This is my own run of the full benchmark on `stress_full.csv` (8,928,944 trades), on my laptop (CSC1103) and on my Raspberry Pi 5 (CSC1104). It covers every version of the group's efficiency ladder, V0 to V8.

### J.4.1 System Set-up

| | Laptop (CSC1103) | Raspberry Pi 5 (CSC1104) |
|---|---|---|
| Model | Framework Laptop 13 Pro | Raspberry Pi 5 Model B Rev 1.1 |
| CPU | Intel Core Ultra X7 358H, 16 cores, up to 4.7 GHz | Arm Cortex-A76, 4 cores, up to 2.4 GHz |
| RAM | 32 GB | 8 GB |
| OS and kernel | Arch Linux, Linux 7.2.15 (x86-64) | Debian GNU/Linux 13 (trixie), Linux 6.18.60+rpt-rpi-2712 (aarch64) |
| Memory page size | 4 KB | 16 KB |
| Compiler | gcc 16.2.1 | gcc 14.2.0 (Debian 14.2.0-19) |
| Flags | `-O0`, `-O1`, `-O2`, `-O3` (`-Wall` on all) | the same, plus `-O3 -mcpu=cortex-a76` |
| CPU governor | `performance` (power profile `performance`, on AC power) | `performance` (V0: 25.05 × 10⁹ cycles in 10.45 s = the full 2.4 GHz) |
| Pinning | `taskset -c 2` | `taskset -c 2` |
| Tools | perf 7.3.1, valgrind 3.25.1 | perf (`linux-perf` 6.18), valgrind 3.24.0 |
| Date | 17 Nov 2026 | 17 Nov 2026 (temperature 46 to 59 °C, never throttled: `vcgencmd get_throttled` = `0x0`) |

Both machines ran the same source code (git commit `114564d`).

#### Method

I used the group's benchmark program, `benchmark/ladder_bench`. It compiles every version at every optimisation level, checks the test20 answer, then runs one warm-up and 5 timed runs pinned to core 2, and I report the median of the 5 runs. V0 to V2 stream the file, so the whole program is timed with `now_ns()`. V3 and later load the file into memory first and report their own `now_ns()` times for load (file → array) and process (indicators over the array) on the stress_full section.

Throughput is 8,928,944 trades ÷ time, and speedup is the time of Version 0 (`-O2`) ÷ the time of the version, on the same machine. Memory is the maximum resident set size (peak physical memory), measured with a small `wait4()` wrapper as the median of 3 runs, with MB = 10⁶ bytes. The hardware counters come from `perf stat -e cycles,instructions,cache-references,cache-misses`, one extra untimed run per version, on both machines.

Both machines used the `performance` CPU governor, as the brief asks. An earlier run on 15 and 16 Nov with the default governors (laptop `powersave`, Pi `ondemand`) gave medians within 2.0% (laptop) and 0.9% (Pi) of these, so the governor made almost no difference here.

All 50 Pi builds and all 32 laptop builds pass `test20.csv`, and every version gives the same stress_full answer on both machines.

### J.4.2 Timing and Throughput for Every Version (`-O2`)

| Version | What changed | Laptop time | Laptop trades/s | Laptop speedup | Pi time | Pi ns/trade | Pi trades/s | Pi speedup | Pi ÷ laptop |
|---|---|---|---|---|---|---|---|---|---|
| V0 | Version 0: helper reader (`sscanf`), `double`, naive SMA | 3058.6 ms | 2.92 M | 1.00× | 10448.9 ms | 1170.2 | 0.85 M | 1.00× | 3.42× |
| V1 | own reader through pointers, still `sscanf` | 3081.2 ms | 2.90 M | 0.99× | 10508.2 ms | 1176.9 | 0.85 M | 0.99× | 3.41× |
| V2 | digit-by-digit parser | 826.7 ms | 10.80 M | 3.70× | 2518.7 ms | 282.1 | 3.55 M | 4.15× | 3.05× |
| V3 | all trades in a global array | 771.2 ms | 11.58 M | 3.97× | 2556.9 ms | 286.4 | 3.49 M | 4.09× | 3.32× |
| V4 | running-sum SMA | 662.7 ms | 13.47 M | 4.62× | 1958.5 ms | 219.3 | 4.56 M | 5.34× | 2.96× |
| **V5** | **fixed-point prices** | **509.0 ms** | **17.54 M** | **6.01×** | 1408.2 ms | 157.7 | 6.34 M | 7.42× | 2.77× |
| V6 | `malloc`'d array (file read twice) | 643.3 ms | 13.88 M | 4.75× | 1799.6 ms | 201.5 | 4.96 M | 5.81× | 2.80× |
| V7 | whole file read with one `fread` | 699.7 ms | 12.76 M | 4.37× | 2561.2 ms | 286.8 | 3.49 M | 4.08× | 3.66× |
| **V8 asm OHLC** | **V5 + hand-written AArch64 OHLC** | — | — | — | **1385.9 ms** | **155.2** | **6.44 M** | **7.54×** | — |
| V8 asm SMA | V5 + hand-written AArch64 SMA | — | — | — | 1425.6 ms | 159.7 | 6.26 M | 7.33× | — |

*Median of 5 runs, stress_full.csv, `taskset -c 2`. Laptop ns/trade = time ÷ 8,928,944 (e.g. V5: 57.0 ns). V8 is assembly for the Pi's CPU, so it runs on the Pi only.*

The 5 runs of each version were very close. On the Pi, the standard deviation was 0.03 to 0.40% of the mean for every version. On the laptop it was 0.21 to 0.66%, except for V4 (0.97%), where one run was slower (677.1 ms); a single slow run does not move the median.

Load and process phases (V3 and later, `-O2`):

| Version | Laptop load | Laptop process | Load share | Pi load | Pi process | Load share |
|---|---|---|---|---|---|---|
| V3 | 578.1 ms | 194.0 ms | 75% | 1740.1 ms | 817.3 ms | 68% |
| V4 | 572.3 ms | 89.9 ms | 86% | 1740.4 ms | 218.1 ms | 89% |
| V5 | 437.0 ms | 72.0 ms | 86% | 1245.2 ms | 162.9 ms | 88% |
| V6 | 572.8 ms | 70.6 ms | 89% | 1636.4 ms | 163.7 ms | 91% |
| V7 | 629.5 ms | 70.2 ms | 90% | 2397.0 ms | 163.8 ms | 94% |
| V8 asm OHLC | — | — | — | 1242.5 ms | 143.5 ms | 90% |
| V8 asm SMA | — | — | — | 1247.7 ms | 177.8 ms | 88% |

#### Every optimisation level

Median time on stress_full, with the speedup against Version 0 at `-O2` in brackets.

Laptop:

| Version | `-O0` | `-O1` | `-O2` | `-O3` |
|---|---|---|---|---|
| V0 | 4407.6 ms (0.69×) | 3152.7 ms (0.97×) | 3058.6 ms (1.00×) | 3097.1 ms (0.99×) |
| V1 | 4480.8 ms (0.68×) | 3149.0 ms (0.97×) | 3081.2 ms (0.99×) | 3082.1 ms (0.99×) |
| V2 | 2928.3 ms (1.04×) | 964.4 ms (3.17×) | 826.7 ms (3.70×) | 782.9 ms (3.91×) |
| V3 | 2949.1 ms (1.04×) | 915.2 ms (3.34×) | 771.2 ms (3.97×) | 706.2 ms (4.33×) |
| V4 | 1510.3 ms (2.03×) | 744.3 ms (4.11×) | 662.7 ms (4.62×) | 613.6 ms (4.98×) |
| V5 | 1052.7 ms (2.91×) | 514.9 ms (5.94×) | 509.0 ms (6.01×) | **506.9 ms (6.03×)** |
| V6 | 1042.3 ms (2.93×) | 650.2 ms (4.70×) | 643.3 ms (4.75×) | 639.6 ms (4.78×) |
| V7 | 2030.2 ms (1.51×) | 853.9 ms (3.58×) | 699.7 ms (4.37×) | 694.9 ms (4.40×) |

Raspberry Pi 5:

| Version | `-O0` | `-O1` | `-O2` | `-O3` | `-O3 -mcpu=cortex-a76` |
|---|---|---|---|---|---|
| V0 | 12793.8 ms (0.82×) | 10737.3 ms (0.97×) | 10448.9 ms (1.00×) | 10493.9 ms (1.00×) | 10466.0 ms (1.00×) |
| V1 | 12857.3 ms (0.81×) | 10604.5 ms (0.99×) | 10508.2 ms (0.99×) | 10486.5 ms (1.00×) | 10474.2 ms (1.00×) |
| V2 | 7079.3 ms (1.48×) | 3011.8 ms (3.47×) | 2518.7 ms (4.15×) | 2335.7 ms (4.47×) | 2360.7 ms (4.43×) |
| V3 | 7076.4 ms (1.48×) | 3074.8 ms (3.40×) | 2556.9 ms (4.09×) | 2388.8 ms (4.37×) | 2406.9 ms (4.34×) |
| V4 | 4443.5 ms (2.35×) | 2346.0 ms (4.45×) | 1958.5 ms (5.34×) | 1785.1 ms (5.85×) | 1810.6 ms (5.77×) |
| V5 | 2996.4 ms (3.49×) | 1498.5 ms (6.97×) | 1408.2 ms (7.42×) | 1397.9 ms (7.47×) | 1404.7 ms (7.44×) |
| V6 | 3376.1 ms (3.10×) | 1890.9 ms (5.53×) | 1799.6 ms (5.81×) | 1785.1 ms (5.85×) | 1797.4 ms (5.81×) |
| V7 | 6431.4 ms (1.62×) | 2311.8 ms (4.52×) | 2561.2 ms (4.08×) | 2282.9 ms (4.58×) | 2293.4 ms (4.56×) |
| V8 asm OHLC | 2951.3 ms (3.54×) | 1478.8 ms (7.07×) | 1385.9 ms (7.54×) | **1378.3 ms (7.58×)** | 1383.7 ms (7.55×) |
| V8 asm SMA | 2849.8 ms (3.67×) | 1512.7 ms (6.91×) | 1425.6 ms (7.33×) | 1416.2 ms (7.38×) | 1423.5 ms (7.34×) |

### J.4.3 perf stat Results

#### Raspberry Pi 5 (`-O2`)

| Version | Cycles | Instructions | IPC | Cache references | Cache misses | Miss rate |
|---|---|---|---|---|---|---|
| V0 | 25.05 × 10⁹ | 79.81 × 10⁹ | 3.19 | 20.17 × 10⁹ | 6.16 M | 0.03% |
| V1 | 25.16 × 10⁹ | 80.19 × 10⁹ | 3.19 | 20.27 × 10⁹ | 5.89 M | 0.03% |
| V2 | 6.04 × 10⁹ | 18.28 × 10⁹ | 3.03 | 3.16 × 10⁹ | 5.30 M | 0.17% |
| V3 | 6.82 × 10⁹ | 20.55 × 10⁹ | 3.01 | 3.62 × 10⁹ | 14.61 M | 0.40% |
| V4 | 5.23 × 10⁹ | 15.88 × 10⁹ | 3.03 | 2.76 × 10⁹ | 12.33 M | 0.45% |
| V5 | 3.77 × 10⁹ | 10.58 × 10⁹ | 2.80 | 2.47 × 10⁹ | 12.21 M | 0.49% |
| V6 | 4.83 × 10⁹ | 13.17 × 10⁹ | 2.73 | 3.32 × 10⁹ | 18.21 M | 0.55% |
| V7 | 6.84 × 10⁹ | 18.26 × 10⁹ | 2.67 | 3.22 × 10⁹ | 21.98 M | 0.68% |
| V8 asm OHLC | 3.71 × 10⁹ | 10.54 × 10⁹ | 2.84 | 2.46 × 10⁹ | 12.22 M | 0.50% |
| V8 asm SMA | 3.81 × 10⁹ | 10.62 × 10⁹ | 2.79 | 2.48 × 10⁹ | 12.25 M | 0.49% |

*One extra, untimed run of the whole program, user space and kernel. V0 to V2 read stress_full only; V3 and later also run test20 and dev_1M first, about 11% more trades. On the Cortex-A76, perf's generic cache events count level-1 data cache accesses and refills.*

#### Laptop (`-O2`)

| Version | Cycles | Instructions | IPC | L3 references | L3 misses | Miss rate |
|---|---|---|---|---|---|---|
| V0 | 13.78 × 10⁹ | 77.01 × 10⁹ | 5.59 | 10.68 M | 10.63 M | 99.5% |
| V1 | 13.78 × 10⁹ | 77.49 × 10⁹ | 5.62 | 10.70 M | 10.61 M | 99.2% |
| V2 | 3.87 × 10⁹ | 16.37 × 10⁹ | 4.23 | 10.70 M | 9.48 M | 88.6% |
| V3 | 3.96 × 10⁹ | 18.32 × 10⁹ | 4.63 | 25.42 M | 19.90 M | 78.3% |
| V4 | 3.32 × 10⁹ | 16.00 × 10⁹ | 4.82 | 25.43 M | 21.75 M | 85.5% |
| V5 | 2.56 × 10⁹ | 11.02 × 10⁹ | 4.31 | 25.49 M | 21.44 M | 84.1% |
| V6 | 3.28 × 10⁹ | 13.36 × 10⁹ | 4.08 | 37.34 M | 31.37 M | 84.0% |
| V7 | 3.61 × 10⁹ | 16.35 × 10⁹ | 4.52 | 68.42 M | 52.95 M | 77.4% |

*Same method as on the Pi. The laptop's CPU has two kinds of core, so perf reports the counts for the performance cores (`cpu_core`) that core 2 belongs to; `ladder_bench` does not read these, so they were copied from its raw perf files. On this Intel CPU the generic cache events count the L3 (last-level) cache, so the cache columns cannot be compared with the Pi's L1 numbers. Most data that reaches L3 is being read for the first time, which is why the miss rates look high.*

### J.4.4 Memory Usage for Every Version (`-O2`)

| Version | Where the trades are kept | Laptop max RSS | Pi max RSS |
|---|---|---|---|
| V0, V1, V2 | one trade at a time (streaming) | 2.2 MB | 1.6 MB |
| V3, V4 | global `Trade` array (`.bss` = 320 MB) | 289.0 / 287.9 MB | 286.9 / 286.9 MB |
| V5 | global `TradeFx` array (`.bss` = 320 MB) | 289.3 MB | 286.8 MB |
| V6 | `malloc`'d array, sized to the file | 287.7 MB | 287.0 MB |
| V7 | `malloc`'d array + the whole 678 MB file text while loading | 965.4 MB | 964.4 MB |
| V8 asm OHLC, V8 asm SMA | as V5 | — | 286.9 MB |

*Peak physical memory of the full run (test20, dev_1M and stress_full), median of 3 runs.*

The fixed global array reserves 320 MB of address space whatever the file size, but the operating system only gives it real memory for the parts that are written. Valgrind Massif (`--pages-as-heap=yes`) shows the difference: on dev_1M, V5 reserves 325.0 MB (laptop) / 324.6 MB (Pi) while V6 reserves only 37.0 / 36.6 MB, and both use about 33 to 35 MB of physical memory (Section 8.3). Valgrind Memcheck reports 0 errors and no leaks for V3, V4, V5 and V6 on both machines.

### J.4.5 Summary: What My Benchmark Shows

- On the laptop, V5 (fixed-point, global array) is the fastest version: 506.9 ms at `-O3`, 6.03× faster than Version 0 (6.01× at the ladder's standard `-O2`).
- On the Pi, V8 asm OHLC is the fastest: 1378.3 ms at `-O3`, 7.58× faster than Version 0 (7.54× at `-O2`). It is V5 with Yong Sheng's hand-written assembly OHLC, which makes the process phase 1.14× faster (143.5 vs 162.9 ms at `-O2`); the total gains only 1.6%, because loading is 90% of the run. The fastest version in plain C is V5 (1397.9 ms at `-O3`, 7.47×). The assembly SMA is slower than the compiler's C (process 0.92×).
- The Pi takes 2.77 to 3.66× as long as the laptop for a whole run. In the process phase alone, the gap is largest for the naive SMA's `double` additions (V3: 4.21×) and smallest for fixed-point (V5: 2.26×).
- Reading the file is the bottleneck. The biggest single gain is the parser (V1 → V2: 3.73× on the laptop, 4.17× on the Pi): with `sscanf`, V1 needs 80.19 × 10⁹ instructions for the day's data on the Pi, and V2's digit-by-digit parser needs 18.28 × 10⁹. Even in the in-memory versions, loading takes 68 to 94% of the run time.
- IPC stays between 2.67 and 3.19 on the Pi and between 4.08 and 5.62 on the laptop, so each rung's gain comes mainly from running fewer instructions. The laptop also runs at about twice the clock (4.5 vs 2.4 GHz), which together with its higher IPC makes it about 3× faster per trade.
- `-O0` → `-O2` makes V2 3.54× faster on the laptop and 2.81× on the Pi, but V0 and V1 only 1.44× and 1.45× (laptop) and 1.22× (Pi). Most of their time is spent inside `sscanf`, which is already compiled into the C library, so our flags do not affect it. `-O3` adds a few percent. On the Pi, `-O3 -mcpu=cortex-a76` is between 0.3% faster and 1.4% slower than plain `-O3`, so it brings no gain.
- With this data, run time limits the Pi and memory does not. Streaming needs at most 2.2 MB, the in-memory versions about 287 MB, and V7 under 1 GB, all well below the Pi's 8 GB.
- On the Pi, V7 runs slower at `-O2` than at `-O1` or `-O3` (2561.2 ms vs 2311.8 / 2282.9 ms). The cause is in Lucas's loader, and I did not investigate it.

Figures to include (made by `ladder_bench`):
- `benchmark/results/leroi/rpi/ladder/charts/ladder_speedup.png` (speedup against Version 0, Pi), and the laptop version in `benchmark/results/leroi/laptop/`;
- `benchmark/results/leroi/compare/charts/laptop_vs_rpi_all_versions.png` (laptop vs Pi for every version);
- `benchmark/results/leroi/rpi/ladder/charts/ladder_memory.png` and `ladder_load_vs_process.png`;
- `benchmark/results/leroi/rpi/ladder/charts/asm_vs_c_process.png` (process time, C vs each V8).

All raw logs (every run, gcc output, program output, perf output) are in `benchmark/results/leroi/laptop/` and `benchmark/results/leroi/rpi/`, and the valgrind and memory logs are in `logs/leroi_laptop/` and `logs/leroi_pi/`.
