# Stage C results summary (Raspberry Pi 5)

Measured by Lucas (Group 06) on 12–13 Nov 2026, following `stageC/BENCHMARK_INSTRUCTIONS.md`.
Every number below is the **median of 5 runs** unless it says otherwise. The raw logs are the
`.txt` files in this folder.

## Machine

| | |
|---|---|
| Board | Raspberry Pi 5 Model B Rev 1.1 |
| CPU | Arm Cortex-A76 ×4, 1.5–2.4 GHz |
| RAM | 7.9 GiB (+ 2.0 GiB swap) |
| OS | Debian GNU/Linux 13 (trixie) 13.5 |
| Kernel | `6.18.34+rpt-rpi-2712` (aarch64, SMP PREEMPT) |
| Compiler | gcc (Debian 14.2.0-19) 14.2.0 |
| Flags | `make stageC CFLAGS_C="-O2 -Wall -mcpu=cortex-a76"` (builds with no warnings) |
| CPU governor | `performance` on all 4 cores for every timed run (default was `ondemand`) |
| Pinning | `taskset -c 2` (core 2) |
| Throttling | `vcgencmd get_throttled` = `0x0` after every run; 47–58 °C |
| Data | `stress_full.csv`, 8,928,944 trades (24 h); SMA windows 35 / 60 |

Engines (see `stageC/README.md`):
- **v0** = Version 0: `next_trade()` reads each line from the file (fgets + sscanf) + Stage A
  modules (double, naive SMA).
- **mem** = fastest: Leroi's `load_trades_malloc()` array + Leroi's Stage B fixed-point modules.

Correctness checks passed first: test20 gives 1 BUY at trade 7, 0 SELL, VWAP 76592.17322550
(both engines); dev_1M gives v0 2509 / 2469 and mem 2553 / 2469.

## Replay: latency (median of 5 runs)

- *Processing* = time inside the engine for one trade.
- *Latency* = decision time − the trade's scheduled release time.
- *Lag* = how far behind the replay clock a trade was started.
- *P&L* = the rival bot at R = 1 µs, with the reaction taken as the latency or as the processing time.

| Engine | Speed | Trades | Processing avg | Avg latency | Worst latency | Max lag | P&L, reaction = latency (USDT) | P&L, reaction = processing (USDT) |
|---|---|---|---|---|---|---|---|---|
| v0  | 1× (10 min) | 51,072    | 1315.0 ns | 25.64 µs | 0.325 ms | 0.323 ms | +1,099.13   | +1,099.13   |
| v0  | 100×        | 8,928,944 | 1212.4 ns | 42.42 µs | 2.795 ms | 2.794 ms | +207,375.30 | +207,375.30 |
| v0  | 1000×       | 8,928,944 | 1207.0 ns | 225.69 µs | 9.768 ms | 9.767 ms | +207,375.30 | +207,375.30 |
| mem | 1× (10 min) | 51,072    | 67.3 ns   | 3.25 µs  | 0.044 ms | 0.044 ms | +1,099.86   | +1,106.68   |
| mem | 100×        | 8,928,944 | 58.1 ns   | 3.67 µs  | 0.363 ms | 0.363 ms | +207,710.81 | +209,214.09 |
| mem | 1000×       | 8,928,944 | 58.9 ns   | 6.28 µs  | 0.382 ms | 0.382 ms | +207,568.01 | +209,214.09 |

Worst single processing time (median): v0 20.0 µs / 28.1 µs / 35.4 µs and mem 1.7 µs /
11.0 µs / 13.3 µs (1× / 100× / 1000×).

Every run finished on schedule: wall time 599.655 s at 1×, 863.998 s at 100× and 86.400 s at 1000×.
In the replay, v0 trades on its **own** signals (20386 BUY / 20176 SELL over the full day; 93 / 87 at 1×).
mem trades on the exact fixed-point signals (20943 / 20060; 100 / 84 at 1×).

## Rival bot (`stageC_rival`, median of 5 runs)

Both versions trade on the exact fixed-point signals (20943 BUY / 20060 SELL), each with its
own per-trade processing times as the reaction time.

| | v0 | mem |
|---|---|---|
| Processing time per trade, avg | 1207.7 ns | 53.0 ns |
| Processing time per trade, worst | 36.1 µs | 25.5 µs |
| Whole-file timing pass | 11,104 ms | 792.5 ms |

**At R = 1 µs:**

| | Signals won (of 37,063) | P&L (USDT) |
|---|---|---|
| v0  | 0      | +207,415.12 |
| mem | 37,059 | +209,214.42 |
| **Money lost to slow code** = P&L(mem) − P&L(v0) | | **+1,799.30** |

- That is 0.86 % of mem's P&L. P&L counts closed round trips only (18,531); the final open
  buy is not counted.
- Run 1 gave +1,798.92 and runs 2–5 gave +1,799.30. mem lost 3–4 signals per run, on the
  trades where its processing time went above 1 µs.
- For comparison, v0 trading on its **own** signals at R = 1 µs: +207,375.30 USDT.

**Where each version starts winning** (sweep R = 0.1 → 100 µs, 31 steps; `rival_sweep.png`):

| | Result |
|---|---|
| v0  | wins 0 % up to R = 1.000 µs, then 95.7 % at the next step, R = 1.259 µs, and 100 % from R = 3.162 µs. So v0 starts winning between **1.0 and 1.26 µs**. |
| mem | already wins 99.9 % at the smallest R tested (0.1 µs) and 100 % from R = 0.2 µs. So mem wins at **every R tested (≤ 0.1 µs)**. |

The sweep is identical in all 5 runs. `stageC/rival_sweep.dat` is from run 5.

## Failed, interrupted or questionable runs

| Run | What happened | Used in the medians? |
|---|---|---|
| `replay_mem_100x_run5`, first attempt (started 2026-11-12 23:31) | The Pi crashed / lost power during the run. The log was empty. The cause is unknown (no persistent system log). | **No.** Rerun after the reboot on 2026-11-12 23:53 with the governor set back to `performance`. The rerun (exit 0) is the run 5 used here. |
| `replay_mem_1x_run5` | Never started because of the crash; run after the reboot (exit 0). | Yes |
| `replay_v0_1x_run1` | A ~1 s `gnuplot` test (lowest priority, `nice -n 19`) ran on the Pi at ~19:50 during this timed run. | **Yes, flagged.** Its values are inside the range of runs 2–5 on every measure (processing 1315.0 vs 1313.8–1322.5 ns; avg latency 25.47 vs 25.56–25.72 µs; max lag 0.323 vs 0.321–0.334 ms). Without it, the median processing avg would be 1316.1 ns instead of 1315.0 ns. |

The two run-5 reruns match runs 1–4 (same signals and VWAPs; timings inside or at the edge of the range).

Earlier runs of the *old* Stage C code (12 Nov, before the rewrite) are archived on the Pi
in `~/stageC_results_OLD_CODE_2026-11-12/` and are **not** used here.
