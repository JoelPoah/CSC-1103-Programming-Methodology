# Stage C: real-time replay, signal and the cost of slow code

Everything is built from the project root with `make stageC` (or `make` for all stages).
On the Pi, add `-mcpu=cortex-a76` with `make stageC CFLAGS_C="-O2 -Wall -mcpu=cortex-a76"`.

## Brief → where it is done

| # | Brief (Stage C) | Where |
|---|---|---|
| 1 | malloc'd array sized to the file, compare memory | `stageC_malloc.c`, `engine/trades_malloc.*` (Leroi), `docs/leroi_stageB_C.md` §4 |
| 2 | replay in timestamp order: real speed ≥ 10 min, full day at 100× and 1000× | `stageC_replay.c` |
| 3 | LED green BUY / red SELL + coloured terminal (CSC1104) | `stageC_replay.c --led` |
| 4 | per-trade latency: average and worst case | replay summary |
| 5 | `gcc -S -O2` of the main loop, annotated | `make asm` (Yong Sheng) |
| 6 | rival bot: P&L of V0 and fastest, money lost, sweep R 0.1→100 µs, plot | `stageC_rival.c`, `plot_rival.gnuplot` |
| – | race demo: V0 vs fastest side by side, lag counter + P&L, LED | two terminals, see below |
| – | every version still passes test20 | see "How to run" |

## Design: Stage C's engine is built on Stage B's

Stage C has its own engine folder, built the same way Stage B's was built from Stage A:
Stage B's modules are copied into `stageC/engine/` (each file says so at the top, the
code is unchanged), and Stage C adds what the brief asks for in Stage C next to them.
Everything Stage C runs, and everything `make asm` compiles, is in `stageC/engine/`.

```
stageC/engine/  -- copied from stageB/engine/ (unchanged) --
  fixed.h                         PRICE_SCALE (cents), QTY_SCALE
  vwap.*     vwap_fx.*            running VWAP (double / fixed-point)
  ohlc.*     ohlc_fx.*            one-second OHLC bars
  sma.*      sma_naive.*  sma_fx.*   running-sum SMA, naive SMA (Version 0), fixed-point SMA
  signal.*   signal_fx.*          short/long SMA crossover -> BUY / SELL

stageC/engine/  -- added in Stage C --
  trades_fx.h                     Stage B's TradeFx, now in a malloc'd array sized to the file
  trades_fx_malloc.c              load_trades_fx(): count lines, malloc, fill with fgets (as ladder V6)
  trades_fx_fread.c               load_trades_fx(): whole file with one fread           (as ladder V7)
  read_trade_fx.*                 line -> TradeFx in cents without sscanf
  ohlc_fx.s                       Yong Sheng's AArch64 OHLC, drop-in for ohlc_fx.c   (as ladder V8)
  stream_engine.*                 Version 0 (v0): helper library next_trade() (fgets + sscanf,
                                  one line from the file per trade) + vwap, ohlc, NAIVE sma, signal
  fixed_engine.*                  fastest (mem): TradeFx array + vwap_fx, ohlc_fx, sma_fx, signal_fx
  rival.*                         rival bot P&L rules (cents, exact)
  trades_malloc.*                 Leroi's malloc'd double array (memory comparison, stageC_malloc.c)

stageC/stageC_replay.c            the replay: clock, lag, latency, LED, coloured signals, live P&L
stageC/stageC_rival.c             per-trade timing of v0 and mem, rival bot P&L, R sweep
```

`stream_engine` and `fixed_engine` add no new maths: they only call the modules above,
so v0 and mem compute exactly what the group's Version 0 and fastest version compute
(same signals and VWAP on test20 and dev_1M).

### Variants (same idea as the src/ ladder)

The replay and the rival bot are built in variants from the same source files; only
the files linked for `mem` change (see the Makefile), so any difference in load time,
memory or reaction time comes from those files alone:

| Program | `mem` links | Same as | Built |
|---|---|---|---|
| `stageC_replay`, `stageC_rival` | `trades_fx_fread.c` + `ohlc_fx.c` | ladder V7 | everywhere (default) |
| `stageC_replay_malloc`, `stageC_rival_malloc` | `trades_fx_malloc.c` + `ohlc_fx.c` | ladder V6 | everywhere |
| `stageC_replay_asm`, `stageC_rival_asm` | `trades_fx_fread.c` + `ohlc_fx.s` | ladder V8 | Raspberry Pi only |

`assembly/sma_fx.s` is not used, as in V8: on the Pi it ran slower than the C `sma_fx.c`.

Stage C's own code only uses C from the lectures: if / else, while and for loops,
functions, arrays, pointers, structs and malloc / free. No `enum`, `switch`, `?:`,
`volatile`, signal handlers, `#error`, `qsort`, `for(;;)`, `unsigned` or `size_t`, and
every `if` has braces. The files copied from Stage B are kept exactly as Stage B wrote them.

### Key decisions (to defend in the report and the demo)
- **Version 0 reads the file during the replay.** When trade *t* is released, `v0` calls
  `next_trade()`, which reads that one line from the file and runs `sscanf` on it, exactly
  like Version 0 in Stages A and B. `mem` takes the trade from the TradeFx array, which was filled once
  before the replay started. So the race compares the real Version 0 against the fastest
  version.
- **The replay clock comes from the TradeFx array.** Every trade's timestamp is in the array, so
  the release time of trade *t* is known before it is read: `start + (τ_t − τ_first) / speed`.
  The program waits for that moment, sleeping only when it is more than 20 ms early and
  checking the clock for the last part, because sleeping can overshoot by milliseconds.
- **Three times per trade:** processing = decided − started; latency = decided −
  *scheduled* release; lag = started − release. Once the engine falls behind, lag and
  latency grow while processing time stays the same. That is the race.
- **Bursts:** many trades share one timestamp, and speed-up compresses market time even
  more. Trades released together queue behind each other, so latency is larger than
  processing time even when the program keeps up overall.
- **Printing, the LED and the status line run after the time is taken**, so they are not
  counted as latency. They still take time and can cause lag, which is why per-signal lines
  are off by default above 100×.
- **The rival bot's reaction time:** `stageC_rival` uses the processing time of the
  triggering trade, which is the brief's rule. The replay shows both: *latency* (a trade
  you haven't reached yet can't be reacted to) and *processing time*.
- **Identical signals:** the brief assumes V0 and the fastest version raise the same
  signals. Ours don't: V0's double + naive SMA gets the sign of `d` wrong on trades where
  short SMA == long SMA (dev_1M: 400 trades differ; see `docs/leroi_stageB_C.md` §2).
  `stageC_rival` therefore uses the **exact fixed-point signals for both versions**. Each
  version brings only its own reaction times, so the money lost is purely the cost of
  speed. It also prints V0's own-signal P&L for comparison.
- **Open position:** if the day ends holding 1 BTC, that buy is not counted. P&L covers
  closed round trips only (otherwise one unmatched buy subtracts about 76k USDT).
- **Percentiles (p50 / p99) are not reported.** They are an optional stretch goal and need
  sorting 8.9 million times; the brief requires the average and worst case only.

## How to run

Paths below assume the CSVs are in `data/`. Pass whatever path your copy uses.
`stageC/BENCHMARK_INSTRUCTIONS.md` has the full measured procedure (5 runs, medians).

```sh
make stageC

# test20 check (windows 3/5): both engines must print exactly 1 BUY at trade 7, 0 SELL
./stageC/stageC_replay data/group_data/test20.csv --short 3 --long 5 --speed 0 --signals --engine v0
./stageC/stageC_replay data/group_data/test20.csv --short 3 --long 5 --speed 0 --signals --engine mem

# dev_1M check: v0 2509 BUY / 2469 SELL, mem 2553 BUY / 2469 SELL
./stageC/stageC_replay data/group_data/dev_1M.csv --speed 0 --quiet --engine v0
./stageC/stageC_replay data/group_data/dev_1M.csv --speed 0 --quiet --engine mem

# Stage C 2+3+4 on the Pi (sudo for the LED, pinned to core 2):
sudo taskset -c 2 ./stageC/stageC_replay data/group_data/stress_full.csv --speed 1 --minutes 10 --led   # real speed, 10 min
sudo taskset -c 2 ./stageC/stageC_replay data/group_data/stress_full.csv --speed 100  --led             # ~14.4 min
sudo taskset -c 2 ./stageC/stageC_replay data/group_data/stress_full.csv --speed 1000 --led             # ~1.5 min

# Stage C 6: rival bot + sweep -> stageC/rival_sweep.dat -> stageC/rival_sweep.png
taskset -c 2 ./stageC/stageC_rival data/group_data/stress_full.csv
gnuplot stageC/plot_rival.gnuplot

# Stage C 5: assembly (on the Pi this writes *_aarch64_O2.s)
make asm

# Race demo: two terminals side by side, same speed. Pick a speed where v0 falls
# behind on the Pi (try 1000, then 5000 / 10000), e.g.
taskset -c 2 ./stageC/stageC_replay data/group_data/stress_full.csv --engine v0  --speed 5000
sudo taskset -c 3 ./stageC/stageC_replay data/group_data/stress_full.csv --engine mem --speed 5000 --led
```

Options: `--engine v0|mem`, `--speed X` (0 = no clock), `--minutes M`, `--from-min M`
(start later in the day, e.g. at a busy period), `--short/--long`, `--rival R_us`, `--led`,
`--signals/--quiet`. If you stop a run with Ctrl-C, the LED keeps its last colour: reboot
the Pi to give the LED back to the system.
