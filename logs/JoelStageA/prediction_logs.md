# Prediction logbook: Joel (Stage A)

Every entry follows predict, measure, explain:

1. Predict: write the expected value and the reason before measuring.
2. Measure: run it and record the actual numbers (median of 5).
3. Explain: say why the result matched the prediction or differed from it.

Folder layout:

```
logs/JoelStageA/
  prediction_logs.md     this logbook
  joel_laptop/           raw laptop outputs (timings, peak working set)
  joel_pi/               raw Pi 5 outputs (timings, memory, valgrind, perf stat)
    valgrind/
```

---

## SMA windows

Group 06 uses short = 35 and long = 60 for `dev_1M.csv` and `stress_full.csv`. Every
group uses short = 3 and long = 5 for `test20.csv`.

### Measurement rules (apply to every entry)

- Time with `now_ns()`; run at least 5 times and report the median.
- On the Pi: `taskset -c 2`, CPU governor `performance`, `-mcpu=cortex-a76` where relevant.
- Record both laptop (CSC1103) and Pi (CSC1104) results.
- Per version, record: time, throughput (trades/s), ns per trade, `perf stat` (cycles,
  instructions, IPC, cache misses: Pi only), and peak memory.
- Decision: Valgrind and `perf` run on the Pi only. They don't run on native Windows, and
  under WSL they would measure a Linux build, not the laptop's Windows program. Laptop
  memory is the native Windows peak working set.

### Test set-up

| | Laptop | Pi 5 |
|---|---|---|
| Device | Acer Swift SFE14-51T | Raspberry Pi 5 Model B Rev 1.1 |
| CPU | Intel Core Ultra 9 288V, 8 cores / 8 threads | Arm Cortex-A76, 4 cores, 2.40 GHz max |
| RAM | 31.6 GB | 7.9 GB (8 GB model) |
| OS | Windows 11 Home 25H2, 64-bit (build 26200) | Raspberry Pi OS 64-bit (Debian 12 bookworm), kernel `6.12.115+rpt-rpi-2712` |
| Compiler + version | gcc 15.2.0 (MSYS2 UCRT64, Rev8) | gcc 12.2.0 (Debian 12.2.0-14) |
| Flags | `-O2 -Wall` (`make ladder`); A3 also uses `-O0`, `-O1` and `-O3` | `-O2 -Wall` (`make ladder`); A3 also uses `-O0`, `-O1`, `-O3` and `-O3 -mcpu=cortex-a76` |
| CPU governor / pinning | "Best performance" power mode, plugged in; no pinning (`taskset` is Linux only) | `performance` governor; `taskset -c 2` |

Source: `benchmark/results/joel/laptop/device.txt` and `benchmark/results/joel/rpi/device.txt`.

---

## Prediction A1: How long does Version 0 take on `stress_full.csv`, laptop vs Pi?

**Date:** 2026-11-16  **Signed:** Poah Zhi Kang Joel

### 1. Predict

| | Laptop | Pi 5 |
|---|---|---|
| Total time (s) | 0.08 | 0.16 |
| ns per trade | 10 | 20 |
| Throughput (trades/s) | 100,000,000 | 50,000,000 |
| Faster machine, by about | laptop, 2× | |

*Reason:*
- The laptop CPU boosts up to 5,100 MHz and the Pi's maximum is about 2,400 MHz, so the
  laptop should be almost twice as fast.
- I guessed that 100 rows take about 1 µs. The file has about 8 million rows, so
  8,000,000 / 100 × 1 µs = 80,000 µs = 0.08 s on the laptop, and about double that
  (0.16 s) on the Pi.

### 2. Measure

*Method:* the group's bench tool, `./benchmark/ladder_bench -m joel`, run on each machine.
It builds V0 with `gcc -O2 -Wall`, checks the test20 output against
`test20_expected.txt`, does 1 warm-up run, then times 5 runs of `v0_stageA_naive_sma stress`
with `now_ns()` from launch to exit (so reading the file is included). The median is used:
ns per trade = median / 8,928,944 trades, and throughput = 8,928,944 / median. Pi:
`performance` governor, pinned with `taskset -c 2`. Laptop: "Best performance" power
mode, plugged in, no pinning. `perf stat` (Pi only) is one extra untimed run under
`perf stat -e cycles,instructions,cache-references,cache-misses`.

| | Laptop | Pi 5 |
|---|---|---|
| Total time (s) | 19.88 | 10.84 |
| ns per trade | 2,226.4 | 1,213.5 |
| Throughput (trades/s) | 449,148 | 824,042 |
| Faster machine, by | Pi 5, 1.83× (19,879.7 / 10,835.5 ms) | |
| Spread of the 5 runs | 19.67 to 20.15 s (std dev 0.91% of mean) | 10.83 to 10.86 s (std dev 0.14% of mean) |
| Pinning | none | `taskset -c 2` |

Pi `perf stat` on the same `-O2` build (one extra untimed run):

| cycles | instructions | IPC | cache references | cache misses |
|---|---|---|---|---|
| 25,674,997,481 | 75,861,539,605 | 2.95 | 20,233,372,224 | 936,320 (0.00%) |

That is about 8,496 instructions and 2,875 cycles per trade.

Measured vs predicted: both machines were much slower than predicted (laptop 19.88 s vs
0.08 s, about 250× off; Pi 10.84 s vs 0.16 s, about 68× off), and the faster machine was
the wrong one: the Pi beat the laptop by 1.83×, where I predicted the laptop would win by 2×.

Source: `benchmark/results/joel/laptop/ladder/logs/v0_stageA_naive_sma_O2.txt` and
`benchmark/results/joel/rpi/ladder/logs/v0_stageA_naive_sma_O2.txt`.

### 3. Explain

My guess that 100 rows take 1 µs was far off. The real time depends on how many
instructions each trade needs and how many clock cycles each instruction takes, and those
are hard to estimate without running the code. On the Pi, `perf stat` showed about 8,500
instructions and 2,900 cycles per trade, which is about 1.2 µs per trade, not the 10 to
20 ns I predicted.

The more interesting result is that the Pi beat my Acer laptop by 1.83×. The laptop's
clock is about twice as fast, but it needed more cycles per trade than the Pi: at
2,226 ns per trade it uses at least about 6,700 cycles per trade even at 3 GHz, compared
with the Pi's 2,900. So the laptop does more work per trade, and a faster clock cannot
make up for that.

A likely reason is that the two machines run different versions of `sscanf` and `strtod`
(string to double). V0's C code is the same on both, but the C library is not: the Pi
uses glibc's version, while the Windows build uses MinGW's (`__USE_MINGW_ANSI_STDIO` in
`trade_io.c`). glibc's version may convert characters to numbers more efficiently, so the
laptop spends more cycles per trade.

---

## Prediction A2: Where does Version 0 spend its time: parsing (`sscanf`) or the indicators?

**Date:** 2026-11-16  **Signed:** Poah Zhi Kang Joel

### 1. Predict

| Part of V0 | Predicted share of run time |
|---|---|
| `sscanf` parsing each line | 50% |
| Naive SMA + OHLC | 20% |
| Everything else (`fgets` reading, VWAP, signal) | 30% |

*Reason:*
- `sscanf` will be the biggest cost. V0 reads the file once, one line at a time with
  `fgets`, and `sscanf` parses all 7 fields of every line (8.9 million lines).
- The naive SMA re-adds the whole window on every trade (35 + 60 = 95 additions) and then
  divides each sum by N. OHLC divides every timestamp by 1,000,000 to find its second. I
  think the re-adding and the divisions together cost about 20%.

*How I will measure it:* `perf record` on V0 on the Pi, which shows the share of time in
each function (`sscanf` and its helpers, `fgets`, `sma_value`, the OHLC update). `perf`
only works on the Pi.

### 2. Measure

*Method:* sampling profiler, Pi only. `perf record` interrupts V0 many times a second
while it runs on `stress_full.csv` and notes which function the CPU is in (event
`cycles:Pu`, user space only). `perf report --sort symbol` then gives each function's
share of all samples. I added these shares into the three groups below. V0 is the `-O2`
build from `make ladder`, pinned with `taskset -c 2`.

```
perf record -o logs/JoelStageA/joel_pi/v0_perf.data \
    taskset -c 2 ./bin/v0_stageA_naive_sma stress > /dev/null
perf report -i logs/JoelStageA/joel_pi/v0_perf.data --stdio --sort symbol \
    > logs/JoelStageA/joel_pi/v0_perf_report.txt
```

| Part of V0 | Predicted | Measured (Pi, `perf`) |
|---|---|---|
| `sscanf` parsing each line | 50% | 83.8% |
| Naive SMA + OHLC | 20% | 8.6% |
| Everything else (`fgets` reading, VWAP, signal) | 30% | 7.6% |

43K samples. Raw report: `logs/JoelStageA/joel_pi/v0_perf_report.txt`. Biggest items
in each group:

- **`sscanf` (83.8%)**: the main `sscanf` loop 40.7%, converting the prices (`strtod`
  and its helpers) about 28%, converting the integers (`strtoll`) 5.6%.
- **Naive SMA + OHLC (8.6%)**: `sma_value` (re-adding the windows) 7.1%.
- **Everything else (7.6%)**: `fgets` 3.7%, file locking 1.6%, signal, VWAP and the rest
  2.4%.

### 3. Explain

`sscanf` was the biggest part, as I predicted, but at 83.8% it was far bigger than my
50%. Turning text into numbers is much more work than I expected. For every line,
`sscanf` re-reads the format string and goes through the line one character at a time,
and `strtod` uses exact big-number arithmetic for each of the 3 prices so that the
rounding is always correct. Converting the prices alone took about 28% of the run time.

The naive SMA and OHLC took only 8.6%, not 20%. The 95 additions per trade are about 200
of the 2,875 cycles each trade takes, and the windows are only 2 KB, so they stay in the
cache and never wait on memory. The divisions are only 3 per trade, which is small next
to the parsing. Arithmetic on numbers is cheap; turning text into numbers is not.

---

## Prediction A3: How much do `-O1`, `-O2` and `-O3` speed up Version 0?

**Date:** 2026-11-16  **Signed:** Poah Zhi Kang Joel

### 1. Predict

| Flags | Laptop speedup vs -O0 | Pi speedup vs -O0 |
|---|---|---|
| -O0 | 1.0× | 1.0× |
| -O1 | 1.5× | 1.5× |
| -O2 | 1.8× | 1.8× |
| -O3 | 2.0× | 1.9× |
| -O3 -mcpu=cortex-a76 (Pi) | n/a | 2.0× |

*Reason:*
- Each level turns on more optimisations than the one before, so each step should be
  faster. The biggest jump should be `-O0` to `-O1`, when variables move from memory into
  registers; `-O2` and `-O3` add less each time (1.5× to 1.8× to 2.0×).
- I expect `-O3` to help the Pi slightly less than the laptop (1.9× vs 2.0×), because by
  default gcc on the Pi builds for a generic ARM CPU. Adding `-mcpu=cortex-a76` lets gcc
  use the A76's newer instructions and order the code for its pipeline, which should
  bring the Pi up to 2.0×.

### 2. Measure

*Method:* same as A1 (`./benchmark/ladder_bench -m joel`). It builds V0 once per flag
level (`gcc <flags> -Wall`), checks test20, does 1 warm-up run, then times 5 runs on
`stress_full.csv` and takes the median. Speedup = median at `-O0` / median at that level.
`perf stat` (Pi only) is one extra untimed run per level.

| Flags | Laptop time / speedup | Pi time / speedup |
|---|---|---|
| -O0 | 21.36 s / 1.00× | 12.99 s / 1.00× |
| -O1 | 19.88 s / 1.075× | 10.78 s / 1.205× |
| -O2 | 19.88 s / 1.075× | 10.84 s / 1.199× |
| -O3 | 20.50 s / 1.042× | 10.84 s / 1.198× |
| -O3 -mcpu=cortex-a76 | n/a | 10.67 s / 1.218× |

Spread of the 5 runs (std dev as % of mean): laptop -O0 2.16%, -O1 3.29%, -O2 0.91%,
-O3 6.29%; Pi 0.14% to 0.19% at every level.

Pi `perf stat` at each level:

| Flags | cycles | instructions | IPC |
|---|---|---|---|
| -O0 | 30,813,404,886 | 90,549,236,014 | 2.94 |
| -O1 | 25,453,449,708 | 76,333,983,380 | 3.00 |
| -O2 | 25,674,997,481 | 75,861,539,605 | 2.95 |
| -O3 | 25,683,709,667 | 75,879,480,979 | 2.95 |
| -O3 -mcpu=cortex-a76 | 25,261,255,390 | 75,834,922,523 | 3.00 |

Measured vs predicted: every speedup was far smaller than predicted (best was 1.075× on the
laptop and 1.218× on the Pi, against 2.0×). As predicted, `-O0` to `-O1` was the biggest
step and `-mcpu=cortex-a76` was the best Pi build. Not as predicted: `-O2` and `-O3` gave
no further gain over `-O1` on either machine (the laptop's `-O3` median was slightly
slower, but its runs also had the widest spread, 6.29%).

Source: `benchmark/results/joel/{laptop,rpi}/ladder/logs/v0_stageA_naive_sma_O*.txt`.

### 3. Explain

The pattern I predicted was right, but the size of every step was not. As predicted,
`-O0` to `-O1` gave the biggest jump, `-O2` and `-O3` added less (in fact nothing), and
`-mcpu=cortex-a76` was the fastest Pi build. On the Pi, `-O1` ran about 14 billion fewer
instructions than `-O0` at the same IPC (about 3), which fits my reason: at `-O0` every
variable is loaded from and stored back to memory, and `-O1` keeps them in registers.

Each higher level recompiles V0's own code more aggressively, but the C library's
`sscanf`, `strtod` and `fgets` are already compiled and are not recompiled at all. A2
showed these take about 89% of the run time, so the flags only ever work on the other
11%. `-O1` already removed most of the waste from that 11%, which left little for `-O2`,
`-O3` or `-mcpu=cortex-a76` (1.5%) to improve.

I also predicted the flags would help the Pi slightly less than the laptop. It was the
other way round: 1.2× on the Pi against 1.075× on the laptop. A likely reason is that the
laptop's library parsing is slower (A1), so V0's own code is an even smaller share of the
laptop's time. The laptop's `-O3` being slower than `-O2` is not meaningful, because its
runs had a 6.29% spread.

---

## Prediction A4: How much memory does Version 0 use, and does it grow with the file?

**Date:** 2026-11-16  **Signed:** Poah Zhi Kang Joel

### 1. Predict

Rough count of what V0 keeps in memory while it runs:

| What | Where | Rough size |
|---|---|---|
| Two SMA windows, `double win[2][128]` | `src/engine/v0-v3_sma_naive.c` (global) | 2 × 128 × 8 = 2,048 bytes |
| SMA bookkeeping: `window`, `head`, `count` (2 ints each) | `src/engine/v0-v3_sma_naive.c` (global) | 24 bytes |
| VWAP running sums (2 doubles) | `src/engine/vwap.c` (global) | 16 bytes |
| Current and finished OHLC bar (2 × second + 4 prices + count) | `src/engine/ohlc.c` (global) | about 90 bytes |
| Signal state: previous SMAs, BUY/SELL counts | `src/engine/signal.c` (global) | about 30 bytes |
| Current trade's fields, file pointer, counters | `helper_function/trade_io.c` (global) | about 70 bytes |
| Line being parsed: `char line[256]` + two 16-byte strings | `helper_function/trade_io.c` (stack) | 288 bytes |
| Total of V0's own data | | about 2.6 KB |

| | test20 | dev_1M | stress_full |
|---|---|---|---|
| Peak physical memory (max RSS), Pi | about 0.003 MB (2.6 KB) | about 0.003 MB (2.6 KB) | about 0.003 MB (2.6 KB) |
| Peak working set, laptop (native Windows) | about 0.003 MB (2.6 KB) | about 0.003 MB (2.6 KB) | about 0.003 MB (2.6 KB) |

This row is taken straight from the "Total of V0's own data" in the rough-size table
(about 2.6 KB), the same for every file and both machines. It counts V0's own data only,
not the fixed cost of the C program itself (code, C library, stack), which I did not
estimate. The row was filled in after the measurements had been run.

*Grows with file size?* No. The same for all three files.

*`.bss` size of `bin/v0_stageA_naive_sma`:* about 2.3 KB (the global SMA windows, VWAP
sums, OHLC bar, signal state and trade fields above).

*Valgrind Memcheck:* 0 errors, no leaks. Heap allocations made by V0's own code: 0, because
V0 never calls `malloc`. Allocations made inside the C library (`fopen`, `printf`): a few
small ones (file and output buffers, a few KB), all freed by exit. Filled in after
measuring.

*Reason:*
- V0 streams the file: it reads one line, updates VWAP, OHLC, SMA and the signal, then
  moves on to the next line. It never stores all the trades. The biggest thing it keeps
  is the last 128 prices for each SMA window (2 KB), so memory stays the same whether
  the file has 20 trades or 8.9 million.
- V0 does not need `malloc`: every array has a fixed size known at compile time, so it
  sits in `.bss` (globals) or on the stack.
- No leaks: `close_trades()` calls `fclose`, which frees what `fopen` set up.
- V0's own data is only about 2.6 KB, so the peak memory should be almost all the fixed
  cost of running any C program (its code, the C library and the stack), not V0's data.

### 2. Measure

*Method:*
- **Peak memory:** `maxrss.c` on the Pi and `joel_laptop/peakws.c` on the laptop. Each
  starts V0, waits for it to finish, and reads its peak memory. 3 runs per dataset,
  middle value kept.
- **Section sizes:** `size bin/v0_stageA_naive_sma` on the Pi.
- **Leaks and memory errors:** Valgrind Memcheck on the Pi, on test20 and dev.

| | test20 | dev_1M | stress_full |
|---|---|---|---|
| Peak memory, Pi | 1,072 kB | 1,072 kB | 1,072 kB |
| Peak memory, laptop | 4,440 kB | 4,444 kB | 4,440 kB |

| Pi only | |
|---|---|
| `.bss` / `.data` / `.text` | 2,312 / 720 / 9,346 bytes |
| Memcheck errors | 0 (test20 and dev) |
| Heap usage | 4 allocs / 4 frees / 13,232 bytes, all freed (test20 and dev) |

Raw outputs: `joel_pi/` (`maxrss.txt`, `size_bss.txt`, `valgrind/`) and
`joel_laptop/peakws.txt`.

### 3. Explain

My memory figure (2.6 KB) was far too low because it only counted V0's own data. The
measured peak (about 1.1 MB on the Pi and 4.3 MB on the laptop) is mostly the fixed cost
of running any program: the program's code, the C library loaded into memory, and the
stack. The OS also counts memory in whole pages (16 KB each on the Pi), so even tiny data
takes up a full page. The laptop is higher because Windows loads more system libraries
into every program. V0's own data was about what I predicted: `.bss` was 2,312 bytes
against my 2.3 KB.

The part I got right is that memory does not grow with the file. Peak memory was the same
for test20, dev and stress, and the heap usage was the same for test20 and dev, because V0
streams the file one line at a time and never stores the trades.

---

## Lessons from Stage A

- What surprised me: the Pi beat my laptop by 1.83× even though the laptop's clock is
  about twice as fast. Also, `sscanf` took 84% of the run time, while the naive SMA I
  expected to be slow took under 9%, and the `-O` flags barely helped (1.2× at best).
- What this taught me about the hardware: clock speed alone does not decide speed; what
  matters is how many cycles each trade needs. The same C code can need very different
  amounts of work on two machines because their C libraries differ. Arithmetic on numbers
  is cheap and stays in the cache; turning text into numbers is the expensive part.
- What Stage A+ should fix first, and why: replace `sscanf`/`strtod` with my own parser for
  this CSV format. Parsing is 84% of the time and is library code the compiler cannot
  optimise, so it is the only change that can give a big speedup. Speeding up the SMA would
  save at most about 9%.
