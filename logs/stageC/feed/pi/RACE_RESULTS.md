# Stage C race on the Raspberry Pi 5: v0 against mem at 10000x and 5000x

Measured on the Raspberry Pi 5 on 2026-11-20 at commit c25528f, with the governor set to `performance` on all 4 cores.
Each race starts both engines together on stress_full.csv (8,928,944 trades, 86,399.8 s of market time):

```sh
taskset -c 2 ./stageC/stageC_replay data/group_data/stress_full.csv --engine v0  --speed <X> --feed 1500 > race_v0_<X>x_run<N>.txt &
taskset -c 3 ./stageC/stageC_replay data/group_data/stress_full.csv --engine mem --speed <X> --feed 1500 > race_mem_<X>x_run<N>.txt
wait
```

There were 3 races per speed, run one after another (10000x runs 1-3, then 5000x runs 1-3), with 5 s between races and no `--led`.
The median is the median of the 3 runs. Rival bot R = 1 us.

## Did v0 keep up?

| speed | time available per trade (avg) | target wall time = 86,400 s / speed | v0 wall time (median) | v0 max lag (median) | mem wall time (median) | verdict for v0 |
|---|---:|---:|---:|---:|---:|---|
| 10000x | 968 ns | 8.640 s | 11.729 s | 3,381 ms | 8.640 s | **fell behind for good**: finished 3.09 s late (1.36x the target) in all 3 runs |
| 5000x | 1,935 ns | 17.280 s | 17.280 s | 546 ms | 17.280 s | **kept up overall**: finished on time in all 3 runs, but fell up to 0.55 s behind during the busy stretches and caught up again afterwards |

mem kept up at both speeds: it finished on time in every run, with a max lag of 1.0-1.6 ms. v0's processing time was about 1,190 ns per trade at both speeds. That is more than the ~968 ns available per trade at 10000x and less than the ~1,935 ns available at 5000x, which explains why v0 falls behind for good at 10000x but only during bursts at 5000x.

## 10000x, v0

| measure | median | run 1 | run 2 | run 3 |
|---|---:|---:|---:|---:|
| wall time (s) | **11.729** | 11.729 | 11.792 | 11.722 |
| processing avg (ns) | **1,188.9** | 1,188.9 | 1,195.5 | 1,188.2 |
| max lag behind replay clock (ms) | **3,380.678** | 3,380.678 | 3,439.741 | 3,374.050 |
| P&L, reaction = latency (USDT) | **+207,375.30** | +207,375.30 | +207,375.30 | +207,375.30 |
| P&L, reaction = processing time (USDT) | **+207,375.30** | +207,375.30 | +207,375.30 | +207,375.30 |
| signals won, reaction = latency | **0** | 0 | 0 | 0 |
| signals lost, reaction = latency | **39,095** | 39,095 | 39,095 | 39,095 |
| signals won, reaction = processing time | **0** | 0 | 0 | 0 |
| signals lost, reaction = processing time | **39,095** | 39,095 | 39,095 | 39,095 |

## 10000x, mem

| measure | median | run 1 | run 2 | run 3 |
|---|---:|---:|---:|---:|
| wall time (s) | **8.640** | 8.640 | 8.640 | 8.640 |
| processing avg (ns) | **53.5** | 53.5 | 53.7 | 53.4 |
| max lag behind replay clock (ms) | **1.548** | 1.544 | 1.548 | 1.550 |
| P&L, reaction = latency (USDT) | **+207,427.00** | +207,429.18 | +207,423.80 | +207,427.00 |
| P&L, reaction = processing time (USDT) | **+209,214.09** | +209,214.09 | +209,214.09 | +209,214.09 |
| signals won, reaction = latency | **6,346** | 6,346 | 6,333 | 6,350 |
| signals lost, reaction = latency | **30,717** | 30,717 | 30,730 | 30,713 |
| signals won, reaction = processing time | **37,063** | 37,063 | 37,063 | 37,062 |
| signals lost, reaction = processing time | **0** | 0 | 0 | 1 |

## 5000x, v0

| measure | median | run 1 | run 2 | run 3 |
|---|---:|---:|---:|---:|
| wall time (s) | **17.280** | 17.280 | 17.280 | 17.280 |
| processing avg (ns) | **1,189.3** | 1,189.3 | 1,189.3 | 1,189.4 |
| max lag behind replay clock (ms) | **545.613** | 545.881 | 545.613 | 544.677 |
| P&L, reaction = latency (USDT) | **+207,375.30** | +207,375.30 | +207,375.30 | +207,375.30 |
| P&L, reaction = processing time (USDT) | **+207,375.30** | +207,375.30 | +207,375.30 | +207,375.30 |
| signals won, reaction = latency | **0** | 0 | 0 | 0 |
| signals lost, reaction = latency | **39,095** | 39,095 | 39,095 | 39,095 |
| signals won, reaction = processing time | **0** | 0 | 0 | 0 |
| signals lost, reaction = processing time | **39,095** | 39,095 | 39,095 | 39,095 |

## 5000x, mem

| measure | median | run 1 | run 2 | run 3 |
|---|---:|---:|---:|---:|
| wall time (s) | **17.280** | 17.280 | 17.280 | 17.280 |
| processing avg (ns) | **53.4** | 53.5 | 53.4 | 53.3 |
| max lag behind replay clock (ms) | **1.030** | 1.276 | 1.030 | 1.030 |
| P&L, reaction = latency (USDT) | **+207,458.32** | +207,458.32 | +207,456.57 | +207,462.78 |
| P&L, reaction = processing time (USDT) | **+209,214.09** | +209,214.09 | +209,214.09 | +209,214.09 |
| signals won, reaction = latency | **7,971** | 7,960 | 7,973 | 7,971 |
| signals lost, reaction = latency | **29,092** | 29,103 | 29,090 | 29,092 |
| signals won, reaction = processing time | **37,062** | 37,062 | 37,063 | 37,062 |
| signals lost, reaction = processing time | **1** | 1 | 0 | 1 |

## Money lost to slow code: P&L(mem) - P&L(v0), reaction = latency

| speed | run 1 | run 2 | run 3 | median |
|---|---:|---:|---:|---:|
| 10000x | +53.88 | +48.50 | +51.70 | **+51.70** |
| 5000x | +83.02 | +81.27 | +87.48 | **+83.02** |

In USDT over the whole day. A positive number means mem earned more than v0.

## Throttling and temperature for each race

| race | before | after | exit codes |
|---|---|---|---|
| 10000x run 1 | throttled=0x0 temp=49.4'C | throttled=0x0 temp=54.9'C | v0=0 mem=0 |
| 10000x run 2 | throttled=0x0 temp=51.6'C | throttled=0x0 temp=55.4'C | v0=0 mem=0 |
| 10000x run 3 | throttled=0x0 temp=53.2'C | throttled=0x0 temp=57.1'C | v0=0 mem=0 |
| 5000x run 1 | throttled=0x0 temp=54.3'C | throttled=0x0 temp=57.6'C | v0=0 mem=0 |
| 5000x run 2 | throttled=0x0 temp=54.3'C | throttled=0x0 temp=58.2'C | v0=0 mem=0 |
| 5000x run 3 | throttled=0x0 temp=55.4'C | throttled=0x0 temp=59.3'C | v0=0 mem=0 |

`get_throttled` read 0x0 before and after every race (no throttling or under-voltage), and every run exited with 0. The raw readings are in `race_conditions.txt`.

## Notes

- Signals and final VWAP were the same in every run of each engine. v0: 20386 BUY, 20176 SELL   final VWAP 76718.67569936. mem: 20943 BUY, 20060 SELL   final VWAP 76718.67569889.
- v0's P&L was +207,375.30 USDT in all 6 races, with both reactions, and v0 won 0 of its 39,095 signals. This is the same as in the 1000x runs, where v0 did keep up. With R = 1 us and v0 taking about 1.19 us per trade, the rival bot got in first on every v0 signal at all three speeds (1000x, 5000x and 10000x). So at these speeds the money lost changes because of **mem's** P&L (with reaction = latency, mem wins fewer signals at 10000x than at 5000x), not because v0 falls further behind.
- The two engines ran at the same time on cores 2 and 3, so they shared the memory bus and L3 cache. These numbers are for a race; they are not the same thing as the one-engine-at-a-time runs in `logs/stageC/percentiles/`.
- GIFs and stills (made from run 1 of each speed with `stageC/plot_live.py`): `stageC/race_10000x_pi.gif`, `stageC/race_10000x_pi_final.png`, `stageC/race_5000x_pi.gif`, `stageC/race_5000x_pi_final.png`. matplotlib 3.11.2 and Pillow 13.0.0 were installed with pip in a venv on the Pi (`~/.venvs/plotlive`), outside the repo.
