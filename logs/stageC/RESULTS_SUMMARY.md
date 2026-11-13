# Stage C results summary (Raspberry Pi 5)

Measured by Lucas (Group 06) on 13–14 Nov 2026 with commit `b761240`, following
`stageC/BENCHMARK_INSTRUCTIONS.md`. Every number below is the **median of 5 runs** unless it
says otherwise. The raw logs are the `.txt` files in this folder.

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
| Throttling | `vcgencmd get_throttled` = `0x0` after every run; 48–58 °C |
| Data | `stress_full.csv`, 8,928,944 trades (24 h); SMA windows 35 / 60 |

Engines (see `stageC/README.md`). Both use Stage B's modules, copied into `stageC/engine/`:
- **v0** = Version 0: `next_trade()` reads each line from the file (fgets + sscanf), then
  `vwap`, `ohlc`, the naive `sma_naive` and `signal` (double prices).
- **mem** = fastest portable version (same as ladder V7): `load_trades_fx()` reads the file
  with one fread into a malloc'd array of cents, then `vwap_fx`, `ohlc_fx`, `sma_fx` and
  `signal_fx`.

Correctness checks passed first: test20 gives 1 BUY at trade 7, 0 SELL, VWAP 76592.17322550
(both engines, and the `_malloc` and `_asm` variants); dev_1M gives v0 2509 / 2469 and
mem 2553 / 2469 (also for `_asm`).

## Replay: latency (median of 5 runs)

- *Processing* = time inside the engine for one trade.
- *Latency* = decision time − the trade's scheduled release time.
- *Lag* = how far behind the replay clock a trade was started.
- *P&L* = the rival bot at R = 1 µs, with the reaction taken as the latency or as the processing time.

| Engine | Speed | Trades | Processing avg | Avg latency | Worst latency | Max lag | P&L, reaction = latency (USDT) | P&L, reaction = processing (USDT) |
|---|---|---|---|---|---|---|---|---|
| v0  | 1× (10 min) | 51,072    | 1301.4 ns | 25.13 µs  | 0.323 ms | 0.322 ms | +1,099.13   | +1,099.13   |
| v0  | 100×        | 8,928,944 | 1195.8 ns | 41.63 µs  | 2.763 ms | 2.761 ms | +207,375.30 | +207,375.30 |
| v0  | 1000×       | 8,928,944 | 1193.0 ns | 220.11 µs | 9.591 ms | 9.590 ms | +207,375.30 | +207,375.30 |
| mem | 1× (10 min) | 51,072    | 67.0 ns   | 3.26 µs   | 0.042 ms | 0.042 ms | +1,099.85   | +1,106.68   |
| mem | 100×        | 8,928,944 | 51.8 ns   | 3.52 µs   | 0.355 ms | 0.355 ms | +207,719.06 | +209,214.09 |
| mem | 1000×       | 8,928,944 | 50.6 ns   | 5.84 µs   | 0.353 ms | 0.353 ms | +207,587.28 | +209,214.08 |

Worst single processing time (median): v0 21.4 µs / 24.1 µs / 95.4 µs and mem 1.9 µs /
11.4 µs / 11.2 µs (1× / 100× / 1000×).

Every run finished on schedule: wall time 599.655 s at 1×, 863.998 s at 100× and 86.400 s at 1000×.
In the replay, v0 trades on its **own** signals (20386 BUY / 20176 SELL over the full day; 93 / 87 at 1×).
mem trades on the exact fixed-point signals (20943 / 20060; 100 / 84 at 1×).

## Rival bot (`stageC_rival`, median of 5 runs)

Both versions trade on the exact fixed-point signals (20943 BUY / 20060 SELL), each with its
own per-trade processing times as the reaction time.

| | v0 | mem |
|---|---|---|
| Processing time per trade, avg | 1190.6 ns | 50.6 ns |
| Processing time per trade, worst | 88.5 µs | 29.6 µs |
| Whole-file timing pass | 10,950.7 ms | 771.5 ms |

**At R = 1 µs:**

| | Signals won (of 37,063) | P&L (USDT) |
|---|---|---|
| v0  | 0      | +207,415.12 |
| mem | 37,060 | +209,214.42 |
| **Money lost to slow code** = P&L(mem) − P&L(v0) | | **+1,799.30** |

- That is 0.86 % of mem's P&L. P&L counts closed round trips only (18,531); the final open
  buy is not counted.
- All 5 runs gave +1,799.30. mem lost 3–4 signals per run, on the trades where its
  processing time went above 1 µs.
- For comparison, v0 trading on its **own** signals at R = 1 µs: +207,375.30 USDT.

**Where each version starts winning** (sweep R = 0.1 → 100 µs, 31 steps; `rival_sweep.png`):

| | Result |
|---|---|
| v0  | wins 0 % up to R = 1.000 µs, then 97.3–97.6 % at the next step, R = 1.259 µs, and 100 % from R = 3.162 µs. So v0 starts winning between **1.0 and 1.26 µs**. |
| mem | already wins 99.6–99.8 % at the smallest R tested (0.1 µs) and 100 % from R = 0.251 µs. So mem wins at **every R tested (≤ 0.1 µs)**. |

The win percentages change slightly from run to run (ranges above). `stageC/rival_sweep.dat`
and `rival_sweep.png` are from run 5.

**asm variant** (`stageC_rival_asm`, AArch64 `ohlc_fx.s`, 1 run, `rival_asm_run1.txt`):
same signals, mem 50.6 ns avg (same as the C build), won 37,059 of 37,063, money lost +1,799.30.

## Failed, interrupted or questionable runs

| Run | What happened | Used in the medians? |
|---|---|---|
| `replay_mem_1000x_run2`, first attempt (13 Nov 17:49) | The replays were paused on purpose after `replay_mem_1x_run1`. This run had just started and was stopped after a few seconds; its partial log was deleted. | **No.** The Pi was rebooted, the governor set back to `performance`, and mem runs 2–5 were run on 14 Nov 00:56–02:40 with the same binary and commands. That rerun (exit 0) is the run 2 used here. |
| `replay_v0_1x_run3` | Max lag 0.755 ms, the other runs 0.316–0.323 ms. Its other values are inside the range. | Yes. Does not change the median (0.322 ms). |
| `replay_mem_100x_run2` | Max lag 0.709 ms, the other runs 0.353–0.355 ms. Its other values are inside the range. | Yes. Does not change the median (0.355 ms). |

The replays ran in two sessions: 18 runs on 13 Nov (15:13–17:49) and the remaining 12 mem
runs on 14 Nov (00:56–02:40). The mem runs from both sessions match (same signals and VWAPs;
timings within the same range).

Earlier runs of older Stage C code (12 Nov, commits before `b761240`) are not used here. They
are in the git history of this folder and archived on the Pi.
