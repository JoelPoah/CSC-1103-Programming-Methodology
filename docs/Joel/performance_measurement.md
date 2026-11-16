# Performance measurement on the laptop (CSC1103) and Pi 5 (CSC1104): Joel

**Member:** Poah Zhi Kang Joel  **Group:** 06  **Date measured:** 2026-11-16
**Dataset:** `data/group_data/stress_full.csv` (8,928,944 trades, 678 MB)

Every version of the group's `src/` ladder (V0 to V8) was benchmarked on
`stress_full.csv` on both machines, at every optimisation level. The fastest version on
the laptop is V7 (`fread`) at `-O2`: 818 ms, 10.9 million trades/s, 24.3× faster than
V0. The fastest on the Pi is V8 (assembly OHLC) at `-O3`: 1.47 s, 6.06 million trades/s,
7.4× faster than V0. Neither machine is faster at everything: the Pi wins V0, V5 and V6,
the laptop wins the rest. Peak memory stays at about 1 to 4 MB for the streaming versions
(V0 to V2), rises to about 274 MB once all trades are kept in memory (V3 and up), and to
about 920 MB when the whole file is also held in memory (V7).

---

## 1. System set-up

| | Laptop (CSC1103) | Raspberry Pi 5 (CSC1104) |
|---|---|---|
| Device / Pi model | Acer Swift SFE14-51T | Raspberry Pi 5 Model B Rev 1.1 (8 GB) |
| CPU, cores, max clock | Intel Core Ultra 9 288V, 8 cores / 8 threads, up to 5.1 GHz | Arm Cortex-A76, 4 cores, 2.40 GHz |
| RAM | 31.6 GB | 7.9 GB |
| OS and kernel | Windows 11 Home 25H2, 64-bit (build 26200) | Raspberry Pi OS 64-bit (Debian 12 bookworm), kernel `6.12.115+rpt-rpi-2712`, 16 KB pages |
| Compiler and version | gcc 15.2.0 (MSYS2 UCRT64, Rev8) | gcc 12.2.0 (Debian 12.2.0-14) |
| Flags | `-Wall` with `-O0`, `-O1`, `-O2`, `-O3` | `-Wall` with `-O0`, `-O1`, `-O2`, `-O3`, `-O3 -mcpu=cortex-a76` |
| CPU governor / power mode | "Best performance" power mode, plugged in | `performance` |
| Pinning | none (`taskset` is Linux only) | `taskset -c 2` (one core) |
| Git commit benchmarked | `2db1bce` | `2db1bce` |
| Python version | 3.14.7 | 3.11.2 |

Sources: `benchmark/results/joel/{laptop,rpi}/device.txt`, `logs/JoelStageA/joel_pi/system.txt`.

---

## 2. How the numbers were measured

- **Tool:** the group's `ladder_bench` (`./benchmark/ladder_bench -m joel`), run on each
  machine. It compiles every version at every flag level, then benchmarks it.
- **Runs:** 1 warm-up run (not counted), then 5 timed runs; the **median** is reported.
- **Timing:** `now_ns()`. V0 to V2 are timed from launch to exit. V3 and up run test20,
  dev_1M and stress_full in one go, so their time is their own Load + Process time for the
  `stress_full` part only.
- **Correctness check:** every build's test20 Final VWAP and BUY/SELL counts are checked
  against `test20_expected.txt`. All builds passed.
- **Throughput and ns per trade:** trades/s = 8,928,944 / median; ns per trade = median /
  8,928,944.
- **perf stat (Pi only):** one extra untimed run under
  `perf stat -e cycles,instructions,cache-references,cache-misses`. User-space counts only.
- **Memory:** measured on that same extra run. Laptop: `PeakWorkingSetSize` of the program
  itself. Pi: `ru_maxrss` from `getrusage`, which cannot go below about 19 MB (see
  section 6), so V0's true figure was also measured with `maxrss.c`. Leaks were checked
  with Valgrind Memcheck on the Pi.
- **Why no perf / Valgrind on the laptop:** neither runs on native Windows, and under WSL
  they would measure a Linux build, not the Windows program that was timed.

---

## 3. Versions measured

| Version | Stage | What changes | Laptop | Pi |
|---|---|---|---|---|
| V0 `v0_stageA_naive_sma` | A | Helper library (`fgets` + `sscanf`), `double` prices, naive SMA, streams the file | ✓ | ✓ |
| V1 `v1_stageAplus_own_reader` | A+ | Own `read_trade()` through pointers, still `fgets` + `sscanf` | ✓ | ✓ |
| V2 `v2_stageAplus_fast_parser` | A+ | Digit-by-digit parser instead of `sscanf` | ✓ | ✓ |
| V3 `v3_stageB_in_memory` | B | Whole file loaded into a global array, then processed from memory | ✓ | ✓ |
| V4 `v4_stageB_running_sma` | B | Running-sum SMA instead of the naive SMA | ✓ | ✓ |
| V5 `v5_stageB_fixed_point` | B | Fixed-point `long long` prices, fixed-size global array | ✓ | ✓ |
| V6 `v6_stageC_malloc` | C | Array `malloc`'d to fit the file (file read twice: count, then fill) | ✓ | ✓ |
| V7 `v7_stageC_fread` | C | Whole file read with one `fread`, then parsed in memory | ✓ | ✓ |
| V8 `v8_stageB_asm_ohlc` | B | V5 with a hand-written AArch64 OHLC | n/a | ✓ |
| V8 `v8_stageB_asm_sma` | B | V5 with a hand-written AArch64 SMA | n/a | ✓ |

V8 is ARM64 assembly, so it cannot run on the x86 laptop.

---

## 4. Timing and throughput (every version, `-O2`)

Speedup is against V0 `-O2` on the same machine.

| Version | Laptop median | Laptop trades/s | Laptop speedup | Pi median | Pi trades/s | Pi speedup |
|---|---|---|---|---|---|---|
| V0 | 19.88 s | 449,148 | 1.00× | 10.84 s | 824,042 | 1.00× |
| V1 | 6.77 s | 1,318,586 | 2.94× | 10.82 s | 825,423 | 1.00× |
| V2 | 2.36 s | 3,783,889 | 8.42× | 2.66 s | 3,353,720 | 4.07× |
| V3 | 2.07 s | 4,306,420 | 9.59× | 2.70 s | 3,303,038 | 4.01× |
| V4 | 1.91 s | 4,676,933 | 10.41× | 2.06 s | 4,338,012 | 5.26× |
| V5 | 1.69 s | 5,286,674 | 11.77× | 1.50 s | 5,967,480 | 7.24× |
| V6 | 3.13 s | 2,850,739 | 6.35× | 1.99 s | 4,483,387 | 5.44× |
| V7 | 0.818 s | 10,916,220 | 24.30× | 2.81 s | 3,181,207 | 3.86× |
| V8 asm OHLC | n/a | n/a | n/a | 1.48 s | 6,051,011 | 7.34× |
| V8 asm SMA | n/a | n/a | n/a | 1.50 s | 5,970,769 | 7.25× |

**Fastest on each machine (any flag level):**
- Laptop: V7 `-O2`, 818.0 ms, 10,916,220 trades/s, 24.30× faster than V0.
- Pi: V8 asm OHLC `-O3`, 1,472.7 ms, 6,062,864 trades/s, 7.36× faster than V0.

### 4.1 Load vs process time (V3 and up, `-O2`)

| Version | Laptop load | Laptop process | Pi load | Pi process |
|---|---|---|---|---|
| V3 | 1,871.3 ms | 200.8 ms | 1,838.8 ms | 864.4 ms |
| V4 | 1,824.8 ms | 83.0 ms | 1,839.6 ms | 218.9 ms |
| V5 | 1,618.8 ms | 70.4 ms | 1,331.3 ms | 164.9 ms |
| V6 | 3,062.1 ms | 70.1 ms | 1,832.4 ms | 158.7 ms |
| V7 | 745.0 ms | 69.7 ms | 2,647.6 ms | 159.6 ms |
| V8 asm OHLC | n/a | n/a | 1,333.9 ms | 141.7 ms |
| V8 asm SMA | n/a | n/a | 1,331.6 ms | 163.7 ms |

Loading (reading and parsing the file) is most of the time for every in-memory version.

### 4.2 Effect of the optimisation flags

| Version | Laptop -O0 → best | Pi -O0 → best |
|---|---|---|
| V0 | 21.36 s → 19.88 s (`-O1`/`-O2`) | 12.99 s → 10.67 s (`-O3 -mcpu`) |
| V1 | 8.38 s → 5.97 s (`-O3`) | 12.93 s → 10.63 s (`-O3 -mcpu`) |
| V2 | 4.31 s → 2.32 s (`-O3`) | 7.08 s → 2.51 s (`-O3`) |
| V3 | 4.60 s → 2.07 s (`-O2`) | 7.13 s → 2.56 s (`-O3`) |
| V4 | 3.24 s → 1.91 s (`-O2`) | 4.50 s → 1.92 s (`-O3`) |
| V5 | 2.61 s → 1.69 s (`-O2`) | 3.04 s → 1.49 s (`-O3`) |
| V6 | 4.07 s → 3.13 s (`-O2`) | 3.52 s → 1.99 s (`-O2`) |
| V7 | 2.46 s → 0.818 s (`-O2`) | 6.42 s → 2.31 s (`-O3 -mcpu`) |
| V8 asm OHLC | n/a | 2.99 s → 1.47 s (`-O3`) |
| V8 asm SMA | n/a | 2.90 s → 1.50 s (`-O2`) |

- `-O0` to `-O1` gives almost all of the gain. `-O2`, `-O3` and `-mcpu=cortex-a76` change
  most versions by only a few percent.
- V0 and V1 gain the least, because most of their time is in the C library's `sscanf`,
  which the flags do not recompile.
- Odd result: on the Pi, V7 at `-O2` (2.81 s) is slower than at `-O1` (2.32 s) and `-O3`
  (2.34 s). The extra time is all in loading (2,647.6 ms against 2,133.1 ms at `-O1`), and
  IPC drops from 3.05 to 2.61.

Full tables for every flag level: `benchmark/results/joel/REPORT.md` sections 4 and 5.
Chart: `benchmark/results/joel/rpi/ladder/charts/ladder_flags.png`.

### 4.3 Run-to-run spread

Standard deviation of the 5 runs, as a percentage of the mean:

| | Typical | Largest |
|---|---|---|
| Pi | 0.02% to 0.6% | 2.74% (V5 `-O2`) |
| Laptop | 0.2% to 3.6% | 8.77% (V2 `-O1`) |

The Pi is pinned to one core with a fixed clock, so its runs are very repeatable. The
laptop is not pinned and can change clock speed and core type during a run, so its spread
is larger. Using the median keeps single slow runs from distorting the result.

---

## 5. perf stat (Pi 5, every version, `-O2`)

| Version | Cycles | Instructions | IPC | Cache references | Cache misses (%) |
|---|---|---|---|---|---|
| V0 | 25,674,997,481 | 75,861,539,605 | 2.95 | 20,233,372,224 | 936,320 (0.005%) |
| V1 | 25,611,560,857 | 76,272,454,320 | 2.98 | 20,313,341,171 | 927,128 (0.005%) |
| V2 | 6,068,222,843 | 18,391,583,815 | 3.03 | 3,177,921,504 | 531,785 (0.017%) |
| V3 | 6,788,238,382 | 20,600,595,579 | 3.03 | 3,611,948,251 | 8,218,252 (0.23%) |
| V4 | 5,063,092,632 | 15,923,827,384 | 3.15 | 2,746,205,250 | 5,893,044 (0.21%) |
| V5 | 3,566,300,848 | 10,462,029,224 | 2.93 | 2,245,662,410 | 5,892,069 (0.26%) |
| V6 | 4,542,132,591 | 12,739,346,926 | 2.80 | 2,905,528,701 | 6,366,892 (0.22%) |
| V7 | 6,872,989,113 | 17,942,375,880 | 2.61 | 2,939,761,440 | 11,372,627 (0.39%) |
| V8 asm OHLC | 3,506,589,526 | 10,421,216,013 | 2.97 | 2,228,743,271 | 5,905,955 (0.26%) |
| V8 asm SMA | 3,562,581,379 | 10,481,887,118 | 2.94 | 2,245,260,011 | 5,869,342 (0.26%) |

V0 to V2 counters cover only `stress_full`. V3 and up also process test20 and dev_1M in
the same run, so their counters include those (small) files as well.

**What the counters show:**
- The speedups come from running fewer instructions, not from a higher IPC. IPC stays
  between 2.6 and 3.2 for every version. Instructions fall from 75.9 billion (V0) to 18.4
  billion once `sscanf` is replaced (V2), and to about 10.5 billion with fixed-point prices
  (V5).
- Cache misses are tiny for the streaming versions (under 1 million), because they only
  keep one line at a time. Keeping every trade in memory (V3 and up) raises them to about
  6 to 8 million, and holding the whole file as well (V7) to 11.4 million. Even then fewer
  than 0.4% of references miss.

**Laptop:** not measured (see section 2).

---

## 6. Memory usage (every version)

| Version | Laptop peak working set | Pi max RSS (`ladder_bench`) | Pi max RSS (`maxrss`) |
|---|---|---|---|
| V0 | 4.3 MB | 19.0 MB | 1,072 kB (about 1.1 MB) |
| V1 | 4.3 MB | 19.0 MB | not measured |
| V2 | 4.3 MB | 19.0 MB | not measured |
| V3 | 276.8 MB | 273.6 MB | not needed |
| V4 | 276.8 MB | 273.6 MB | not needed |
| V5 | 276.8 MB | 273.5 MB | not needed |
| V6 | 276.3 MB | 273.7 MB | not needed |
| V7 | 922.7 MB | 919.7 MB | not needed |
| V8 asm OHLC | n/a | 273.6 MB | not needed |
| V8 asm SMA | n/a | 273.5 MB | not needed |

"MB" here is 1024 × 1024 bytes (MiB), as `ladder_bench` reports it. Values are at `-O2`;
they are the same at every flag level.

**Note on the Pi figures:** `ladder_bench` reports the largest of all the processes it
starts (the shell, `taskset`, `perf` and the program), so its Pi figure cannot go below
about 19 MB. For V0 to V2, which use far less, the 19.0 MB is the measuring tools, not the
program. Measured on its own with `maxrss.c`, V0 used 1,072 kB (median of 3 runs) on
test20, dev and stress alike (`logs/JoelStageA/joel_pi/maxrss.txt`). V3 and later use
about 273 MB, well above that floor, so their `ladder_bench` figures are accurate.

**Valgrind Memcheck (Pi, V0, test20 and dev):** 0 errors; 4 allocations, 4 frees,
13,232 bytes, all freed ("no leaks are possible"). V0's static data (`.bss`) is 2,312
bytes.

**What the memory numbers show:**
- The streaming versions (V0 to V2) use the same small amount whatever the file size,
  because they keep only one line at a time.
- Keeping every trade in memory (V3 to V6, V8) costs about 274 MB: about 32 bytes per
  trade for 8.93 million trades. The static array (V5) and the `malloc`'d array (V6) use
  the same memory.
- V7 adds about 646 MB on top of that, which is the whole 678 MB file held in one buffer
  by `fread`.
- The laptop is about 3 MB higher than the Pi for every version. This fixed difference is
  the cost of running any program on each OS, not the program's own data.

---

## 7. Laptop vs Pi

Same version at `-O2` on both machines:

| Version | Laptop | Pi | Faster machine |
|---|---|---|---|
| V0 | 19.88 s | 10.84 s | Pi, 1.83× |
| V1 | 6.77 s | 10.82 s | laptop, 1.60× |
| V2 | 2.36 s | 2.66 s | laptop, 1.13× |
| V3 | 2.07 s | 2.70 s | laptop, 1.30× |
| V4 | 1.91 s | 2.06 s | laptop, 1.08× |
| V5 | 1.69 s | 1.50 s | Pi, 1.13× |
| V6 | 3.13 s | 1.99 s | Pi, 1.57× |
| V7 | 0.818 s | 2.81 s | laptop, 3.43× |

Fastest version on each machine: the laptop's V7 (818.0 ms) is 1.80× faster than the Pi's
V8 asm OHLC (1,472.7 ms).

- **V0 vs V1:** V1 uses the same `sscanf` format as V0 but through its own reader. On the
  Pi it changes nothing (1.00×), but on the laptop it is 2.94× faster. V0's helper
  (`trade_io.c`) defines `__USE_MINGW_ANSI_STDIO`, which makes the Windows build use
  MinGW's own `sscanf`; V1's reader does not, so it uses the Windows C library's version
  instead. The likely reason the Pi wins V0 but loses V1 is that MinGW's `sscanf` is much
  slower (not confirmed, as `perf` cannot run on the laptop).
- **V6:** reading the file twice (count, then fill) costs much more on the laptop (load
  3,062 ms against 1,619 ms for V5) than on the Pi (1,832 against 1,331 ms).
- **V7:** one large `fread` makes loading very fast on the laptop (745 ms) but not on the
  Pi (2,648 ms).

Source: `benchmark/results/joel/compare/compare.txt` and its charts.

---

## 8. C vs Python (optional)

| | Laptop | Pi |
|---|---|---|
| `stageA_Python.py` | 23.70 s | 55.30 s |
| `stageA_Python_numpy.py` | failed (numpy not installed) | 22.06 s |
| C V0 `-O2` | 19.88 s | 10.84 s |
| Fastest C | V7 `-O2`, 0.818 s (29.0× faster than the fastest Python) | V8 asm OHLC `-O3`, 1.47 s (15.0× faster than the fastest Python) |

Source: `benchmark/results/joel/{laptop,rpi}/python/summary.txt`.

---

## 9. Limitations

- No `perf stat` on the laptop (Windows), so the laptop has no cycles, IPC or cache data.
- The laptop is not pinned to one core. Its CPU has two core types and can change clock
  speed during a run, which explains its larger spread.
- V8 is ARM64 assembly and only runs on the Pi.
- The Pi memory figure from `ladder_bench` has a ~19 MB floor; only V0's true figure was
  measured separately. V1 and V2 are expected to be similar but were not measured.
- For V3 and up, perf stat and memory cover the whole run (test20 + dev_1M + stress_full),
  while their times cover stress_full only.
- perf counts user space only, so time in the kernel (reading the file) is not counted.
- One run of the benchmark on one machine of each kind.
- `stageA_Python_numpy.py` did not run on the laptop because numpy was not installed.

---

## 10. Raw data

| What | Where |
|---|---|
| Group benchmark report | `benchmark/results/joel/REPORT.md` |
| Per-version logs (every run) | `benchmark/results/joel/{laptop,rpi}/ladder/logs/` |
| All results as CSV | `benchmark/results/joel/{laptop,rpi}/c_results.csv` |
| perf stat raw output (Pi) | `benchmark/results/joel/rpi/ladder/logs/raw/*_perf.txt` |
| Charts | `benchmark/results/joel/{laptop,rpi}/ladder/charts/` |
| Laptop vs Pi comparison | `benchmark/results/joel/compare/compare.txt` |
| Stage A logbook (V0 deep dive) | `logs/JoelStageA/prediction_logs.md` |
| Pi memory, perf record, Valgrind | `logs/JoelStageA/joel_pi/` |
| Laptop memory | `logs/JoelStageA/joel_laptop/` |
