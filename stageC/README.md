# Stage C: real-time replay, signal and the cost of slow code

Everything is built from the project root with `make stageC` (or `make` for all stages).
On the Pi, add `-mcpu=cortex-a76` with `make stageC CFLAGS_C="-O2 -Wall -mcpu=cortex-a76"`.

## Brief → where it is done

| # | Brief (Stage C) | Status | Where |
|---|---|---|---|
| 1 | malloc'd array sized to the file, compare memory | done (Leroi) | `stageC_malloc.c`, `engine/trades_malloc.*`, `docs/leroi_stageB_C.md` §4 |
| 2 | replay in timestamp order: real speed ≥ 10 min, full day at 100× and 1000× | **code done**, Pi runs TODO | `stageC_replay.c` |
| 3 | LED green BUY / red SELL + coloured terminal (CSC1104) | **code done**, test on Pi with `sudo` | `stageC_replay.c --led`, `engine/display.*` |
| 4 | per-trade latency: average and worst case | **code done** (+ p50/p99 stretch goal) | `engine/latency.*`, replay summary |
| 5 | `gcc -S -O2` of the main loop, annotated | x86-64 done, **AArch64 TODO on Pi** | `make asm`, `asm/annotated_main_loop.md` |
| 6 | rival bot: P&L of V0 and fastest, money lost, sweep R 0.1→100 µs, plot | **code done**, Pi runs TODO | `stageC_rival.c`, `plot_rival.gnuplot` |
| – | race demo: V0 vs fastest side by side, lag counter + P&L, LED | **code done**, record video | two terminals, see below |
| – | every version still passes test20 | **checked** for v0, fast and mem | see "test20 check" |

## Design

```
stageC/engine/feed.*        whole CSV -> one malloc'd text buffer + line[] + ts[]   (the "exchange")
stageC/engine/fast_parse.*  CSV line -> TradeFx, digit by digit (Stage A+ approach, no sscanf)
stageC/engine/engine.*      3 versions behind one engine_step():
                              v0   = sscanf + double + NAIVE SMA          (= Version 0 maths)
                              fast = fast_parse + fixed-point + running SMA
                              mem  = parsed once at load, fixed-point only  (fastest)
stageC/engine/rival.*       rival bot P&L rules (cents, exact)
stageC/engine/latency.*     mean / worst / p50 / p99
stageC/engine/display.*     ANSI colours, status bar
stageC/stageC_replay.c      the replay (clock, lag, latency, LED, live P&L)
stageC/stageC_rival.c       offline per-trade timing of all 3 versions + R sweep
```

The engine maths reuses the Stage B modules (`stageB/engine/*`) unchanged.

### Key decisions (to defend in the report and the demo)
- **The feed holds raw text, not parsed numbers.** On a real exchange, trades arrive as
  bytes, so parsing belongs to the engine's work. That way v0 and fast pay for parsing on
  every trade. `mem` is the in-memory rung, which moves parsing into the load phase.
- **Replay clock:** trade *t* is released at `start + (τ_t − τ_first) / speed`. The
  program **busy-polls** the clock for the last 20 ms before a release, because sleeping
  can overshoot by up to 15 ms on Windows, far longer than one trade. Real low-latency
  systems do the same.
- **Three times per trade:** processing = decided − started; latency = decided −
  *scheduled* release; lag = started − release. Once the engine falls behind, lag and
  latency grow while processing time stays the same. That is the race.
- **Bursts:** many trades share one timestamp, and speed-up compresses market time even
  more. Trades released together queue behind each other, so latency is larger than
  processing time even when the program keeps up overall (e.g. mem at 1000×: processing
  ~0.13 µs, latency p50 ~11 µs on the laptop). This is a real effect worth explaining.
- **Printing, the LED and the status bar run after `t_done`**, so they are not counted as
  latency. They still take time and can cause lag, which is why per-signal lines are off
  by default above 100×.
- **The rival bot's reaction time:** `stageC_rival` uses the processing time of the
  triggering trade, which is the brief's rule. The replay shows both: *latency* (used
  live, because a trade you haven't reached yet can't be reacted to) and *processing
  time*.
- **Identical signals:** the brief assumes V0 and the fastest version raise the same
  signals. Ours don't: V0's double + naive SMA gets the sign of `d` wrong on trades where
  short SMA == long SMA (dev_1M: 400 trades differ; see `docs/leroi_stageB_C.md` §2).
  `stageC_rival` therefore uses the **exact fixed-point signals for every version**.
  Each version contributes only its own reaction times, so the money lost is purely the
  cost of speed. It also prints V0's own-signal P&L for comparison.
- **Open position:** if the day ends holding 1 BTC, that buy is not counted. P&L covers
  closed round trips only (otherwise one unmatched buy subtracts about 76k USDT).

## How to run

Paths below assume the CSVs are in `data/`. Pass whatever path your copy uses.

```sh
make stageC

# test20 check (windows 3/5): every engine must print exactly 1 BUY at trade 7, 0 SELL
./stageC/stageC_replay data/test20.csv --short 3 --long 5 --speed 0 --signals --engine v0
./stageC/stageC_replay data/test20.csv --short 3 --long 5 --speed 0 --signals --engine fast
./stageC/stageC_replay data/test20.csv --short 3 --long 5 --speed 0 --signals --engine mem

# Stage C 2+3+4 on the Pi (sudo for the LED, pinned to core 2):
sudo taskset -c 2 ./stageC/stageC_replay data/stress_full.csv --speed 1 --minutes 10 --led   # real speed, 10 min
sudo taskset -c 2 ./stageC/stageC_replay data/stress_full.csv --speed 100  --led             # ~14.4 min
sudo taskset -c 2 ./stageC/stageC_replay data/stress_full.csv --speed 1000 --led             # ~1.5 min
#   repeat the 1000x / 100x runs with --engine v0 for the latency table

# Stage C 6: rival bot + sweep -> stageC/rival_sweep.dat -> stageC/rival_sweep.png
sudo taskset -c 2 ./stageC/stageC_rival data/stress_full.csv
gnuplot stageC/plot_rival.gnuplot

# Stage C 5: assembly (on the Pi this writes *_aarch64_O2.s)
make asm

# Race demo: two terminals side by side, same speed. Pick a speed where v0 falls
# behind on the Pi (try 1000, then 5000 / 10000), e.g.
taskset -c 2 ./stageC/stageC_replay data/stress_full.csv --engine v0  --speed 5000
sudo taskset -c 3 ./stageC/stageC_replay data/stress_full.csv --engine mem --speed 5000 --led
```

Options: `--engine v0|fast|mem`, `--speed X` (0 = no clock), `--minutes M`, `--from-min M`
(start later in the day, e.g. at a busy period), `--short/--long`, `--rival R_us`, `--led`,
`--signals/--quiet`. Ctrl-C stops cleanly and switches the LED off.

## Preliminary laptop results (Windows 11, MinGW gcc 16.1, -O2, NOT pinned, single runs; do not use in the report)

dev_1M.csv, windows 35/60:

| engine | processing avg | p50 | p99 | signals | VWAP |
|---|---|---|---|---|---|
| v0 | ~4.6 µs | 3.5 µs | 12 µs | 2509 BUY / 2469 SELL | 76217.01291587 |
| fast (old sscanf placeholder; re-measure with the real parser) | ~4.1 µs | 3.3 µs | 11.5 µs | 2553 / 2469 | 76217.01291611 |
| mem | ~0.12 µs | 0.1 µs | 0.2 µs | 2553 / 2469 (exact) | 76217.01291611 |

- Replay 1000×: **v0 fell up to 1.1 s behind the clock**; mem's max lag was 2.7 ms.
- Rival at R = 1 µs: mem won 99.9% of signals, v0 0%; money lost ≈ 220 USDT on dev_1M.
  In the sweep, v0 only starts winning above R ≈ 3 µs and mem above R ≈ 0.12 µs. Each
  version "starts losing" when R drops below its typical per-trade processing time.
- Windows' clock (QPC) ticks every 100 ns, so the mem times are rounded to 100 ns. The
  Pi's `clock_gettime` is finer.
- On the laptop, v0 calls MinGW's `__mingw_sscanf` (selected by `__USE_MINGW_ANSI_STDIO`, as
  in `trade_io.c`), which is far slower than glibc's `sscanf` (Leroi measured ~335 ns/trade
  streaming on Linux). Expect much smaller v0 numbers on the Pi.
- Unpinned runs of the same engine varied by up to 2× (v0 p50 3.5 µs to 7.9 µs), so these
  numbers only show the rough shape.

## TODO for the group
1. **Verify the fast parser** (adapted from Jaren's Stage A+ parser to give integers
   directly). It compiles but has **not been run yet**. Run
   `./stageC/test_parser data/test20.csv data/dev_1M.csv data/stress_full.csv` (must print
   `PASS`), then the three test20 commands above.
2. Run everything above **on the Pi** (5 runs, median, `taskset`, `performance`
   governor) and fill in the latency table: avg + worst at real speed, 100×, 1000×.
3. `make asm` on the Pi and annotate the AArch64 listing (reference table at the end of
   `asm/annotated_main_loop.md`).
4. Record the race and the LED for the demo video.
5. Prediction-logbook entry for Stage C (e.g. "at 1000×, will v0 keep up on the Pi?"),
   written **before** running.
