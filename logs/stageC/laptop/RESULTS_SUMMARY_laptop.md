# Stage C results summary (laptop, CSC1103 delivery)

Measured by Lucas (Group 06) on 19 Nov 2026, following `stageC/BENCHMARK_INSTRUCTIONS.md`
step 4. Every number is the **median of 5 runs**, taken separately for each column. The raw
logs are the `replay_*.txt` files in this folder. The Pi results are in
`../RESULTS_SUMMARY.md`.

## Machine

| | |
|---|---|
| CPU | Intel Core i7-10510U @ 1.80 GHz (4 cores / 8 threads) |
| OS | Windows 11 Home, running WSL2 Ubuntu 24.04.5 LTS (kernel 6.18.58.1-microsoft-standard-WSL2) |
| Compiler | gcc (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0 |
| Flags | `make stageC` (`-O2 -Wall`) |
| CPU governor | not exposed inside WSL; Windows controls power (plugged in) |
| Pinning | `taskset -c 2` |
| Data | `stress_full.csv`, 8,928,944 trades (24 h); SMA windows 35 / 60 |

Correctness checks passed first: test20 gives 1 BUY at trade 7, 0 SELL, VWAP 76592.17322550
(both engines and `_malloc`); dev_1M gives v0 2509 / 2469 and mem 2553 / 2469. Every replay
run gave the expected signals: full day v0 20386 / 20176, mem 20943 / 20060.

## Replay: latency (median of 5 runs)

| Engine | Speed | Trades | Processing avg | Avg latency | Worst latency | Max lag | P&L, reaction = latency (USDT) | P&L, reaction = processing (USDT) |
|---|---|---|---|---|---|---|---|---|
| v0  | 1× (10 min) | 51,072    | 4313.0 ns | 68.42 µs  | 4.606 ms  | 4.604 ms  | +1,099.13   | +1,101.56   |
| v0  | 100×        | 8,928,944 | 1330.7 ns | 56.29 µs  | 68.093 ms | 68.079 ms | +207,381.13 | +209,025.07 |
| v0  | 1000×       | 8,928,944 | 1156.7 ns | 207.02 µs | 54.457 ms | 54.444 ms | +207,375.28 | +207,795.71 |
| mem | 1× (10 min) | 51,072    | 239.3 ns  | 8.21 µs   | 4.728 ms  | 4.728 ms  | +1,099.18   | +1,106.66   |
| mem | 100×        | 8,928,944 | 55.1 ns   | 8.75 µs   | 48.175 ms | 48.175 ms | +207,805.38 | +209,214.11 |
| mem | 1000×       | 8,928,944 | 41.4 ns   | 18.52 µs  | 43.145 ms | 43.144 ms | +207,812.98 | +209,214.09 |

Worst single processing time (median): v0 187.4 µs / 12.5 ms / 1.84 ms and mem 56.1 µs /
1.21 ms / 161.1 µs at 1× / 100× / 1000×.

## Spread between runs

| Engine | Speed | Avg latency of each run (µs) | Worst latency of each run (ms) |
|---|---|---|---|
| v0  | 1×    | 69.9, 62.9, 68.4, 67.6, 72.9   | 2.88, 19.16, 2.43, 4.61, 15.88 |
| v0  | 100×  | 224.5, 54.9, 94.3, 56.3, 41.0  | 369.99, 59.38, 67.61, 68.09, 73.92 |
| v0  | 1000× | 164.0, 125.7, 207.0, 227.6, 316.2 | 57.33, 46.88, 55.32, 54.46, 47.34 |
| mem | 1×    | 7.4, 15.8, 7.9, 14.6, 8.2      | 3.11, 44.95, 1.48, 4.73, 48.74 |
| mem | 100×  | 8.9, 9.0, 5.8, 8.5, 8.7        | 65.54, 42.33, 34.94, 58.09, 48.17 |
| mem | 1000× | 25.8, 18.5, 15.8, 17.6, 37.4   | 54.67, 32.02, 43.14, 46.41, 34.41 |

## Notes

- The run order and the replaced run are in `NOTES.md`. v0 1× run 1 overlapped the laptop
  sleeping, so it was deleted and run again after the other 29.
- WSL2 is a virtual machine inside Windows, and `taskset` pins to a virtual CPU, not a real
  core. Windows can still pause or move it, and there is no governor control. Worst-case
  numbers here are therefore much noisier than on the Pi.
