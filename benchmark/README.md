# Efficiency ladder benchmark

One C program builds every version of the engine, times each one on
`stress_full.csv` with `now_ns()` from `helper_function/timing.h`, and writes
logs, tables and charts. **The stage programs are not changed**: they are
compiled and run exactly as they are in the repo.

## Run it

From the project root. `stress_full.csv` must be in `data/group_data/`, which
is where every program reads it from.

```sh
gcc -O2 -Wall benchmark/src/main.c benchmark/src/versions.c benchmark/src/run.c benchmark/src/stats.c benchmark/src/ladder.c benchmark/src/python.c benchmark/src/compare.c benchmark/src/report.c benchmark/src/charts.c benchmark/src/device.c benchmark/src/final_report.c -o benchmark/ladder_bench -lm

./benchmark/ladder_bench -m andrew            # everything: C ladder, Python, then the report
./benchmark/ladder_bench -m andrew c          # C ladder only (+ assembly builds on the Pi)
./benchmark/ladder_bench -m andrew python     # Python only (run "c" first: it compares to stageA.c)
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
  and the assembly builds, and pins every run to core 2 with `taskset`.

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
- Every version that loads with the helper library (`sscanf`) needs about 40 s
  per stress run on the laptop. The Stage B/C programs also load test20 and
  dev_1M on every run.
- The laptop C ladder (20 versions × 7 runs: warm-up, 5 timed, 1 memory) takes about 2 hours.
- The Pi is 2–3× slower, plus a 4th flag set and 9 assembly builds.
- Pure Python takes minutes per run.

Plug in and leave the machine idle.

## How the code is laid out

| File | Job |
|---|---|
| `src/bench.h` | shared types; lists every file's functions |
| `src/main.c` | options, which machine, which parts to run |
| `src/versions.c` | **what is measured**: programs, flag sets, assembly swaps, Python scripts. Edit here to add a rung |
| `src/run.c` | compile, warm up, check test20, and time **one** version (`now_ns()`) |
| `src/stats.c` | median, mean, mode, min, max, standard deviation |
| `src/ladder.c` | the C ladder and its charts (+ assembly on the Pi) |
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
| `stageA` (**Version 0** at `-O2`, the baseline) | helper library (`fgets` + `sscanf`), `double`, naive SMA, streams the file | `now_ns()` around the whole program (`stageA_manual stress`) |
| `stageA_plus` | own pointer-based parser instead of `sscanf`, running-sum SMA | `now_ns()` around the whole program (`--stress`) |
| `stageB_inmem` | whole file loaded into a static array, then processed from memory | the program's own `now_ns()` lines: **Load + Process** of the stress section |
| `stageB_fixed` | static array + fixed-point `long long` prices | same (Load + Process) |
| `stageC_malloc` | `stageB_inmem` with a `malloc`'d array sized to the file | same (Load + Process) |
| `stageA_Python.py`, `stageA_Python_numpy.py` | Stage A in Python / NumPy | each script's own `bench` mode (5 runs, `perf_counter_ns`); the whole process is also timed with `now_ns()` |

* **Flag sets:** every C version is built at `-O0`, `-O1`, `-O2` and `-O3`. The Pi
  adds `-O3 -mcpu=cortex-a76`.
* **Assembly builds (Pi only):**
  * `stageB_inmem` and `stageC_malloc` get `assembly/sma.s` only,
    `assembly/ohlc.s` only, and both.
  * `stageB_fixed` gets the same three with `sma_fx.s` / `ohlc_fx.s`.
  * All are built at `-O2`, as `assembly/compare_all.sh` does.
  * Each is compared with its plain-C build on **Process time**, the only part
    the assembly changes.
* **Why Stage B/C use their own timers:** those programs always run test20 →
  dev_1M → stress_full, and can't be told otherwise without editing them. So
  their time is their own `now_ns()` Load + Process for the stress section.
  The whole-program time is in every log too.
* **Correctness:**
  * Every version's test20 result (Final VWAP + BUY/SELL) is checked against
    `test20_expected.txt` and shown as PASS/FAIL.
  * Every assembly build is checked to give the same answer as its plain-C
    build.
* **Memory:** every version's peak memory, measured on one extra, untimed run after
  the timed runs. Not on the warm-up: on Windows the first launch of a freshly
  compiled `.exe` reads its whole image in once (we saw 310 MB instead of 35 MB).
  On the Pi, perf stat counts this same run.
  * Windows: the program's peak working set.
  * Pi: its maximum resident set size.
* **Hardware counters (Pi only):** one extra, untimed run per version under
  `perf stat`: cycles, instructions, IPC, cache references and cache misses.
  * It counts the whole program. For Stage B/C that includes their test20 and
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
    ladder/charts/                ladder_speedup.png        speedup vs stageA -O2 (stageA -O2 itself left out)
                                  ladder_flags.png          time of every version, grouped by program
                                  ladder_load_vs_process.png
                                  ladder_memory.png         peak memory of every version
                                  stage_<program>.png       one per stage: -O0/-O1/-O2/-O3 (+ -mcpu on the Pi), speedup vs its -O0
    asm/logs/, asm/charts/  (Pi)  asm_speedup_vs_plain_c.png, asm_speedup_vs_stageA.png
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

Not for speed, and only a little for memory. Compare `stageC_malloc` with
`stageB_inmem`, because nothing else differs between them:

* **Speed: `malloc` is slower here.** `load_trades_malloc()` reads the whole
  file once to count the lines, then a second time to fill the array. So its
  Load time is higher. Process time is the same, because the loop runs over the
  same 32-byte `Trade` structs. See "MALLOC vs STATIC ARRAY" in `summary.txt`.
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
* **What would make the Pi faster:** a faster loader. That means our own
  parser, as in `stageA_plus`, instead of `sscanf`. Loading is almost all of
  the in-memory versions' time.
