## Project Overview: Tick-to-Trade on the Edge
The objective is to design, implement, and benchmark a low-latency market data engine in standard C on both a personal laptop and a Raspberry Pi 5. Operating as a low-cost "signal box" for high-frequency trading (HFT), the engine must read real cryptocurrency market trades, compute trading indicators, and raise BUY or SELL signals on the Pi's onboard status LED faster than the market moves.

## Dataset and Group Parameters
The dataset consists of one day of cryptocurrency trades from Binance's public market data. The data is split into three files:
*   **test20.csv**: 20 consecutive trades used exclusively for correctness checking. This file uniformly uses a short window of 3 and a long window of 5 for all users.
*   **dev_1M.csv**: 1 million consecutive trades utilized for development and debugging.
*   **stress_full.csv**: The full day's data (several million trades) utilized for final benchmarking.
*   **Group 6 Configuration**: For `dev_1M.csv` and `stress_full.csv`, the algorithmic windows are explicitly set to a **Short SMA window of 35 trades** and a **Long SMA window of 60 trades**.

Each trade row contains seven comma-separated fields: trade Id, price, quantity (qty), quoteQty, timestamp in microseconds (time), isBuyerMaker (determines buy/sell side), and isBestMatch.

## Implementation Stages

**Stage 0: Environment and Baseline Benchmarks**
*   Compile and run a C program calculating the sum of 1 to $100,000,000$ on both the laptop and Pi. 
*   Time the loop utilizing the provided `now_ns()` function at optimization levels `-O0`, `-O1`, `-O2`, and `-O3` to measure speedups.
*   Compare recursive versus iterative versions of a sum up to $10,000$, timing both to analyze stack usage.
*   Control the Pi's green LED around the timed loops using `led_init()`, `led_on()`, `led_off()`, and `led_cleanup()` to visually indicate execution.
*   Maintain a prediction logbook to forecast computational results prior to measuring them.

**Stage A: Streaming Engine**
*   Read trades sequentially using the provided helper library.
*   Compute the Volume-Weighted Average Price (VWAP) tracking the running sum of $p_t \times q_t$ and the sum of $q_t$.
*   Compute one-second OHLC (Open, High, Low, Close) bars using integer division on timestamps (`time / 1,000,000`) to reset variables every second.
*   Compute a short (35 trades) and long (60 trades) Simple Moving Average (SMA) utilizing an array as a circular window.
*   Raise a BUY signal when the short SMA rises above the long SMA, and a SELL signal when it falls below. 
*   Export roughly 2,000 trades to a text file (`signals.dat`) and plot them using `gnuplot` (`signals.png`) to visually verify crossovers.

**Stage A+: Custom Reader and Fast Parser**
*   Replace the helper library with a custom parsing function passing data via pointer parameters.
*   Replace `sscanf` with a manual string parser that walks the character array to convert digits, measuring the speedup on `stress_full.csv`.

**Stage B: In-Memory Engine and Efficiency Ladder**
*   Define a `Trade` struct and load all trades into a fixed-size global array to bypass file I/O bottlenecks.
*   Upgrade the SMA calculation from a naive window recalculation to a running-sum method ($S_{new} = S_{old} + price_{new} - price_{old}$).
*   Convert price variables from `double` to fixed-point integers (`long long`) utilizing a scale factor (e.g., multiplying by 100 or 1,000,000) while preventing overflow.
*   Generate an efficiency ladder tracking cumulative speedups against the Stage 0 baseline program.

**Stage C: Real-Time Replay, Latency, and the Rival Bot**
*   Replace the fixed-size global array with a dynamically allocated array using `malloc`.
*   Replay trades according to their timestamp intervals at 1x speed (for 10 minutes), 100x speed, and 1000x speed. 
*   Display real-time signals via the Pi's hardware LED (green for BUY, red for SELL) and via ANSI color codes in the terminal.
*   Measure the processing latency from trade release to signal decision, recording the average and worst-case times.
*   Simulate trading against an algorithmic Rival Bot that reacts to signals in $R$ microseconds (defaulting to 1 $\mu s$).
*   If the engine's reaction time is faster than $R$, the trade executes at the triggering price; if slower, the rival takes the price and the engine executes at the next trade's price.
*   Calculate Profit & Loss (P&L) based on a 1 BTC position size and plot the total money lost to slow code against varying values of $R$ (0.1 $\mu s$ to 100 $\mu s$).

## Advanced Enhancements
*   **Hand-written Assembly:** Rewrite a core function (like the VWAP sum) in AArch64 assembly or NEON SIMD to compete with the compiler's `-O3` output.
*   **Struct-of-Arrays:** Restructure memory to store each field in isolated arrays rather than an array of structs, analyzing cache line efficiency.
*   **Percentile Latency:** Sort processing times to calculate the median (p50) and 99th percentile (p99) latency.

## Operational Deliverables
*   **Codebase:** The complete source code, custom Makefiles, and assembly files allowing a standard `make` build on the Pi.
*   **Race Demonstration:** A side-by-side terminal execution running the baseline version against the highly optimized final version, showcasing live lag counters, active P&L differentials against the Rival Bot, and LED triggers.
*   **Project Documentation:** A thorough design breakdown featuring algorithmic pseudocode, the efficiency ladder graph, detailed assembly annotations (`gcc -S -O2`), perf hardware counter logs (cycles, cache misses, IPC), and an analysis of the compromises between precision and processing speed. For rigorous benchmarking, the execution must be pinned to a single core using `taskset` with the CPU governor set to performance.