# Stage C stretch goal: p50 / p99 latency on the Raspberry Pi 5

Measured on the Raspberry Pi 5 at commit c25528f. The CPU governor was `performance` on all four cores, and every timed
program was pinned to core 2 with `taskset -c 2`. The runs went one after another, never in parallel. Machine details
are in `machine_info.txt`, and the full output of each run is in the `.txt` file next to this one.

## a) test_sort: middle-pivot quicksort against qsort() and Joel's last-pivot sort

Times in ms. Joel's sort is only run when n <= 20000 (`JOEL_MAX` in `stageC/test_sort.c`), because it
is O(n^2) on timing, sorted and equal data. That is why it shows as skipped at n = 8,928,944.

| n | data kind | qsort ms | middle ms | joel ms | pass/fail |
|---|---|---:|---:|---:|---|
| 20,000 | random | 2.13 | 1.62 | 1.45 | pass |
| 20,000 | timing | 1.66 | 0.84 | 8.55 | pass |
| 20,000 | sorted | 0.66 | 0.33 | 393.05 | pass |
| 20,000 | equal | 0.66 | 0.49 | 167.87 | pass |
| 8,928,944 | random | 1574.79 | 963.08 | skipped | pass |
| 8,928,944 | timing | 1087.77 | 443.39 | skipped | pass |
| 8,928,944 | sorted | 475.86 | 196.33 | skipped | pass |
| 8,928,944 | equal | 475.60 | 355.14 | skipped | pass |

Both runs also print `percentile() hand-worked checks: pass` and `ALL CHECKS PASS`.

## b) Replay with `--percentiles`, stress_full.csv (8,928,944 trades), 1000x, 5 runs per engine

Command: `taskset -c 2 ./stageC/stageC_replay data/group_data/stress_full.csv --engine <e> --speed 1000 --quiet --percentiles`,
with v0 and mem alternating (v0 run 1, mem run 1, v0 run 2, ...). The median is the median of the 5 runs.
p50 and p99 use the nearest-rank method on all 8,928,944 values.

### v0 (Version 0: fgets + sscanf, double prices)

| measure | median | run 1 | run 2 | run 3 | run 4 | run 5 |
|---|---:|---:|---:|---:|---:|---:|
| processing p50 (ns) | **1,167** | 1,167 | 1,167 | 1,167 | 1,167 | 1,167 |
| processing p99 (ns) | **2,018** | 2,018 | 2,018 | 2,018 | 2,037 | 2,000 |
| latency p50 (ns) | **67,035** | 67,035 | 67,078 | 67,034 | 70,638 | 66,915 |
| latency p99 (ns) | **3,510,834** | 3,510,834 | 3,514,396 | 3,509,251 | 3,767,493 | 3,508,078 |
| sort time, 2 lists (ms) | **1,152.3** | 1,157.4 | 1,144.9 | 1,153.4 | 1,152.3 | 1,145.3 |
| avg latency (ns) | **218,781.9** | 218,781.9 | 219,001.7 | 218,541.5 | 234,614.8 | 218,374.6 |
| worst latency (ns) | **9,543,360** | 9,529,775 | 9,564,162 | 9,543,360 | 10,002,462 | 9,523,612 |
| max lag (ms) | **9.542** | 9.529 | 9.563 | 9.542 | 10.001 | 9.522 |

### mem (fastest portable, fread + cents, as ladder V7)

| measure | median | run 1 | run 2 | run 3 | run 4 | run 5 |
|---|---:|---:|---:|---:|---:|---:|
| processing p50 (ns) | **55** | 55 | 55 | 55 | 55 | 55 |
| processing p99 (ns) | **75** | 74 | 167 | 75 | 148 | 74 |
| latency p50 (ns) | **2,920** | 2,910 | 2,940 | 2,920 | 2,921 | 2,862 |
| latency p99 (ns) | **60,331** | 59,832 | 60,331 | 60,383 | 60,424 | 58,947 |
| sort time, 2 lists (ms) | **1,016.7** | 1,019.6 | 1,023.3 | 1,010.5 | 1,015.4 | 1,016.7 |
| avg latency (ns) | **6,139.1** | 6,120.1 | 6,156.5 | 6,140.2 | 6,139.1 | 6,053.0 |
| worst latency (ns) | **366,587** | 369,564 | 366,587 | 364,454 | 364,054 | 369,121 |
| max lag (ms) | **0.367** | 0.370 | 0.367 | 0.364 | 0.364 | 0.369 |

Ratio v0 / mem, using the medians: processing p50 21.2x, processing p99 26.9x, latency p50 23.0x, latency p99 58.2x.

## c) Throttling and temperature after each run

| run (in the order run) | wall time (s) | load (ms) | exit | get_throttled | temp |
|---|---:|---:|---:|---|---|
| v0 run 1 | 86.400 | 9132.9 | 0 | 0x0 | 49.4'C |
| mem run 1 | 86.400 | 2197.1 | 0 | 0x0 | 48.8'C |
| v0 run 2 | 86.400 | 2196.5 | 0 | 0x0 | 49.9'C |
| mem run 2 | 86.400 | 2195.8 | 0 | 0x0 | 50.5'C |
| v0 run 3 | 86.400 | 2218.1 | 0 | 0x0 | 50.5'C |
| mem run 3 | 86.400 | 2217.4 | 0 | 0x0 | 51.0'C |
| v0 run 4 | 86.400 | 2194.9 | 0 | 0x0 | 51.6'C |
| mem run 4 | 86.400 | 2199.8 | 0 | 0x0 | 53.2'C |
| v0 run 5 | 86.400 | 2200.8 | 0 | 0x0 | 53.2'C |
| mem run 5 | 86.400 | 2199.5 | 0 | 0x0 | 53.8'C |

`get_throttled` read 0x0 after every run, so the Pi did not throttle or show under-voltage at any point.

## Notes

- v0 run 1 took 9,133 ms to load, against about 2,200 ms for every later run. It was the first run after the Pi booted (about 15 minutes earlier), so the 285 MB file was most likely not yet in the page cache. Loading happens before the replay clock starts, so this does not affect any timed number above (that run's percentiles match runs 2 to 5).
- mem's processing p99 is either about 74 ns or much higher (74, 167, 75, 148, 74 ns across the 5 runs). At about 55 ns per trade, a small disturbance in 1% of the trades is enough to move it (the cause was not measured). This is why the median is given.
- Each engine's signals and final VWAP were the same in all 5 runs: v0 20386 BUY / 20176 SELL, VWAP 76718.67569936; mem 20943 BUY / 20060 SELL, VWAP 76718.67569889.
- Sort time is for both lists together (processing times and latencies, 2 x 8,928,944 values), measured after the replay, so it is not counted in any latency.
- v0 run 4 was a little worse than the other four (avg latency 234,615 ns against about 218,700 ns, worst 10.0 ms against about 9.5 ms) but no slower in wall time or processing p50. Its temperature and get_throttled were normal. It is about 7% off, not much slower, so it was kept, and the medians are hardly affected.
