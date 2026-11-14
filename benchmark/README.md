# Efficiency ladder benchmark

One C program builds every version of the `src/` ladder, times each one on
`stress_full.csv` with `now_ns()` from `helper_function/timing.h`, and writes
logs, tables and charts.

**It measures the programs that `make ladder` builds** (V0 to V7 in `bin/`, plus
V8 on the Pi). Each version's name and source files in `src/versions.c` are
copied from its Makefile rule, and the benchmark compiles it again at every
optimisation level, so its `-O2` build is the build `make` gives. The ladder
programs are not changed. If a ladder rule in the Makefile changes, change the
matching entry in `src/versions.c`.

## Run it

From the project root. `stress_full.csv` must be in `data/group_data/`, which
is where every program reads it from.

```sh
make benchmark        # or the gcc line below
gcc -O2 -Wall benchmark/src/main.c benchmark/src/versions.c benchmark/src/run.c benchmark/src/stats.c benchmark/src/ladder.c benchmark/src/python.c benchmark/src/compare.c benchmark/src/report.c benchmark/src/charts.c benchmark/src/device.c benchmark/src/final_report.c -o benchmark/ladder_bench -lm

./benchmark/ladder_bench -m andrew            # everything: C ladder, Python, then the report
./benchmark/ladder_bench -m andrew c          # C ladder only (+ V8 on the Pi)
./benchmark/ladder_bench -m andrew python     # Python only (run "c" first: it compares to V0)
./benchmark/ladder_bench -m andrew report     # REPORT.md + laptop-vs-Pi charts from saved results (no timing)
```

Options:
- `-m NAME` (**required**): whose results these are. The brief asks for one
  set of documentation per member, so each member's results go to
  `benchmark/results/NAME/` and nobody overwrites anyone else. Use the **same
  name** on the laptop and on the Pi.
- `-n 5` sets the number of timed runs per C version (default 5). One extra
  warm-up run is never counted.
- `-p py` sets the Python command. The default is `py` on Windows and
  `python3` on the Pi.

**The machine is detected automatically** when the tool is compiled:
- Windows → `laptop`.
- 64-bit ARM Linux → `rpi`. This build adds the `-mcpu=cortex-a76` flag set
  and V8 (the assembly OHLC version), and pins every run to core 2 with `taskset`.

Build the tool on each machine; don't copy the program across.

**Steps for the report:**
1. Run it on the laptop: `./benchmark/ladder_bench -m NAME`.
2. On the Pi:
   - set the CPU governor to `performance`
   - `sudo apt install gnuplot python3-numpy`
   - build the tool and run it
3. Put `benchmark/results/NAME/laptop/` and `benchmark/results/NAME/rpi/` in one
   copy of the repo. `benchmark/results/` is git-ignored, so each member's
   results stay off GitHub. Copy the Pi folder to the laptop instead, e.g. from
   the laptop: `scp -r pi@<pi-address>:<repo>/benchmark/results/NAME/rpi benchmark/results/NAME/`
4. Run `./benchmark/ladder_bench -m NAME report` there. It writes `benchmark/results/NAME/REPORT.md`:
   both machines' specs side by side, every table, the key results and every chart.
   Open it in VS Code (Ctrl+Shift+V) or on GitHub.

**How long it takes:**
- Only V0 and V1 still parse with `sscanf`; they need tens of seconds per
  stress run on the laptop. V2 and up use our own parser and are much quicker.
  V3 and up also load test20 and dev_1M on every run.
- The laptop C ladder is 32 versions (8 programs × 4 flag sets) × 7 runs each
  (warm-up, 5 timed, 1 memory).
- The Pi is 2–3× slower and runs 45 versions (V8 and a 5th flag set are added).
- Pure Python takes minutes per run.

Plug in and leave the machine idle.

## How the code is laid out

| File | Job |
|---|---|
| `src/bench.h` | shared types; lists every file's functions |
| `src/main.c` | options, which machine, which parts to run |
| `src/versions.c` | **what is measured**: the ladder versions (as in the Makefile), flag sets, Python scripts. Edit here to add a rung |
| `src/run.c` | compile, warm up, check test20, and time **one** version (`now_ns()`) |
| `src/stats.c` | median, mean, mode, min, max, standard deviation |
| `src/ladder.c` | the C ladder and its charts (+ V8 on the Pi) |
| `src/python.c` | the two Python versions vs C |
| `src/compare.c` | laptop vs Pi, fastest C vs fastest Python |
| `src/report.c` | per-version logs, `summary.txt`, the results `.csv` |
| `src/charts.c` | writes each chart's numbers to a `.dat` file and calls gnuplot |
| `src/device.c` | the machine's specs (device, CPU, cores, RAM, OS, compiler, governor) → `device.txt` |
| `src/final_report.c` | both machines' specs and results → `benchmark/results/<member>/REPORT.md` |
| `charts/*.gnuplot` | how the charts look (`style.gnuplot`), bars, grouped bars |

## What is measured

| Version | Rung (what changed) | How the time is taken |
|---|---|---|
| `v0_stageA_naive_sma` (**Version 0** at `-O2`, the baseline) | helper library (`fgets` + `sscanf`), `double`, naive SMA, streams the file | `now_ns()` around the whole program (`stress`) |
| `v1_stageAplus_own_reader` | own `read_trade()` through pointers, still `fgets` + `sscanf` | `now_ns()` around the whole program (`--stress`) |
| `v2_stageAplus_fast_parser` | digit-by-digit parser instead of `sscanf` | same (`--stress`) |
| `v3_stageB_in_memory` | whole file loaded into a global array (fast parser), then processed from memory | the program's own `now_ns()` lines: **Load + Process** of the stress section |
| `v4_stageB_running_sma` | running-sum SMA instead of the naive SMA | same (Load + Process) |
| `v5_stageB_fixed_point` | fixed-point `long long` prices, parsed straight to fixed-point; fixed-size global array | same (Load + Process) |
| `v6_stageC_malloc` | array `malloc`'d to fit the file (file read twice: count, then fill) | same (Load + Process) |
| `v7_stageC_fread` | whole file read with one `fread`, then parsed in memory | same (Load + Process) |
| `v8_stageC_asm_ohlc` (Pi only) | V7 with the hand-written AArch64 OHLC in place of the C one | same (Load + Process) |
| `stageA_Python.py`, `stageA_Python_numpy.py` | Stage A in Python / NumPy | each script's own `bench` mode (5 runs, `perf_counter_ns`); the whole process is also timed with `now_ns()` |

* **Flag sets:** every C version is built at `-O0`, `-O1`, `-O2` and `-O3`. The Pi
  adds `-O3 -mcpu=cortex-a76`. `-O2` is what `make ladder` uses.
* **Assembly (Pi only):** V8 is V7 with `src/engine_asm/v8_ohlc_fx.s` in place
  of the C OHLC. It is compared with V7 at the same flag set on **Process
  time**, the only part the assembly changes.
* **Why V3 and up use their own timers:** those programs always run test20 →
  dev_1M → stress_full, and can't be told otherwise without editing them. So
  their time is their own `now_ns()` Load + Process for the stress section.
  The whole-program time is in every log too.
* **Correctness:**
  * Every version's test20 result (Final VWAP + BUY/SELL) is checked against
    `test20_expected.txt` and shown as PASS/FAIL.
  * V8 is checked to give the same answer as V7.
* **Memory:** every version's peak memory, measured on one extra, untimed run after
  the timed runs. Not on the warm-up: on Windows the first launch of a freshly
  compiled `.exe` reads its whole image in once (we saw 310 MB instead of 35 MB).
  On the Pi, perf stat counts this same run.
  * Windows: the program's peak working set.
  * Pi: its maximum resident set size.
* **Hardware counters (Pi only):** one extra, untimed run per version under
  `perf stat`: cycles, instructions, IPC, cache references and cache misses.
  * It counts the whole program. For V3 and up that includes their test20 and
    dev_1M part.
  * If perf reports nothing, run
    `sudo sysctl kernel.perf_event_paranoid=1` first.
* **Statistics** per version:
  * Every run, then the median, mean, mode, min, max and standard deviation,
    plus ns/trade and trades/s. Every comparison uses the median.
  * Mode: run times are first rounded to 1 ms (C) or 100 ms (Python). When no
    rounded value repeats, the log says "none".

## Output

```
benchmark/results/<member>/REPORT.md      THE REPORT: specs of both machines, all tables, key results, all charts
benchmark/results/<member>/<laptop|rpi>/
    device.txt                    this machine's specs (device, CPU, cores, RAM, OS, gcc, governor, Python)
    summary.txt                   efficiency ladder table, fastest version, malloc vs static
    c_results.csv                 the same numbers, machine-readable (read by "report")
    ladder/logs/<version>.txt     one per version: every run, statistics, speedup, peak memory, perf stat
    ladder/logs/raw/              gcc output, the program's output, the test20 output, perf stat output
    ladder/charts/                ladder_speedup.png        speedup vs V0 -O2 (V0 -O2 itself left out)
                                  ladder_flags.png          time of every version, grouped by program
                                  ladder_load_vs_process.png
                                  ladder_memory.png         peak memory of every version
                                  stage_<program>.png       one per version: -O0/-O1/-O2/-O3 (+ -mcpu on the Pi), speedup vs its -O0
    python/                       summary.txt, logs/, charts/python_vs_c_time.png
benchmark/results/<member>/compare/       compare.txt, charts/laptop_vs_rpi_all_versions.png,
                                  charts/fastest_laptop_vs_rpi.png, charts/fastest_c_vs_fastest_python.png
```
Every chart's numbers are in `charts/data/` next to it. Built programs go to
`bin/bench/<machine>/`.

**Git-ignored:** `benchmark/results/` (everyone's results and reports),
`benchmark/ladder_bench(.exe)` and `bin/`. The brief wants "all printed results,
benchmark logs and perf outputs" in the submitted zip, so copy the final
`benchmark/results/` folder into the zip by hand. To commit one member's
results anyway, use `git add -f benchmark/results/NAME`.

To try the tool out in minutes, add `-DLADDER_QUICK` to the gcc line. It then
times `dev_1M.csv` instead. Never use those numbers in the report.

## Does `malloc` help on the Pi?

Not for speed, and only a little for memory. Compare `v6_stageC_malloc` with
`v5_stageB_fixed_point`, because nothing else differs between them:

* **Speed: `malloc` is slower here.** V6's loader reads the whole
  file once to count the lines, then a second time to fill the array. So its
  Load time is higher. Process time is about the same, because the loop runs over the
  same 32-byte `TradeFx` structs. See "MALLOC vs STATIC ARRAY" in `summary.txt`.
* **Memory: about the same in practice.**
  * The static array reserves 10 M × 32 B = 320 MB. The OS only gives real
    memory to the pages that are written to, and for stress_full (8.93 M
    trades) both versions touch about 286 MB.
  * The Pi 5 has 4 GB, so either fits. The measured peak memory of each
    version is in the report's "Memory and hardware counters" table.
* **What `malloc` really gives:**
  * The array is the right size for any file, with no 10 M-trade limit.
    stress_full is already at 89 % of that limit.
  * The memory can be freed.
  * The program can stop cleanly when memory runs out.
* **What makes loading faster:** reading the file once. V7 does that with one
  `fread` and parses the text in memory. Loading is almost all of the
  in-memory versions' time.
