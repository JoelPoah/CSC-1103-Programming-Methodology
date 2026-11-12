# Stage C: checks and benchmark runs

Instructions for a group member, or Claude working for them, who has been asked to verify
and benchmark Stage C. Do the four steps in order. Run every command from the project root.

## Goal

1. Confirm the Stage C code builds and gives correct answers on this machine.
2. Produce timing logs for the rival bot and the replay on `stress_full.csv`.

## Before you start

| Check | How | If it fails |
|---|---|---|
| Linux, macOS or Pi | Steps use `taskset`, which is Linux/Pi only. On macOS, remove `taskset -c 2`. | On Windows, Smart App Control may block freshly built `.exe` files. Use another machine or WSL. Do not disable security settings without the owner's say-so. |
| Stage C code is present | `stageC/stageC_replay.c` and `stageC/test_parser.c` exist after `git checkout lucas && git pull` | Stop. Ask Lucas to commit and push branch `lucas`. |
| Data files are present | `data/test20.csv`, `data/dev_1M.csv`, `data/stress_full.csv` (8,928,944 trades) | Stop. The CSVs are gitignored, so copy them from the course folder. Group 06's `test20.csv` and `dev_1M.csv` are needed. |
| Prediction written | The person whose results these are has written their Stage C logbook prediction, dated and signed | Stop and ask them. The prediction must be theirs and made before measuring. **Claude must not write it for them.** |

This code has only been run on Windows so far, so a build or run problem on Linux is
possible. Step 1 is also the check for that.

## Step 1: build and check the parser

```sh
git fetch && git checkout lucas && git pull
make stageC
./stageC/test_parser data/test20.csv data/dev_1M.csv data/stress_full.csv
```

**Pass:** the last line is `PASS`, with 0 mismatches for each file.

## Step 2: check test20 for every engine

```sh
for e in v0 fast mem; do
  ./stageC/stageC_replay data/test20.csv --short 3 --long 5 --speed 0 --signals --engine $e
done
```

**Pass, for each of the three engines:** one `BUY` line at trade 7, and a summary showing
`1 BUY, 0 SELL` and final VWAP `76592.17322550`.

## Step 3: rival bot, 5 runs (a few minutes)

```sh
mkdir -p logs/stageC
for n in 1 2 3 4 5; do
  taskset -c 2 ./stageC/stageC_rival data/stress_full.csv > logs/stageC/rival_run$n.txt
done
```

**Expected signals:** `fast` and `mem` both give 20943 BUY / 20060 SELL and final VWAP
76718.67569889 (the exact answer recorded in `docs/leroi_stageB_C.md`). `v0` gives
different counts on purpose, because of `double` rounding; expect about 20386 BUY / 20176 SELL.

Each run overwrites `stageC/rival_sweep.dat`. Keep the one left by run 5. If gnuplot is
installed, run `gnuplot stageC/plot_rival.gnuplot` to make `stageC/rival_sweep.png`.

## Step 4: replay, 5 runs of each engine at each speed (over 4 hours)

```sh
for e in v0 mem; do for n in 1 2 3 4 5; do
  taskset -c 2 ./stageC/stageC_replay data/stress_full.csv --engine $e --speed 1000 --quiet > logs/stageC/replay_${e}_1000x_run$n.txt
  taskset -c 2 ./stageC/stageC_replay data/stress_full.csv --engine $e --speed 100  --quiet > logs/stageC/replay_${e}_100x_run$n.txt
  taskset -c 2 ./stageC/stageC_replay data/stress_full.csv --engine $e --speed 1 --minutes 10 --quiet > logs/stageC/replay_${e}_1x_run$n.txt
done; done
```

Leave the machine otherwise idle and plugged in, because other work distorts the timings.
If running through Claude, start this as a background job, as it far exceeds a normal
command timeout.

## If something fails

- Stop at the failing step and report the command and its full output.
- Do not change the code, the expected values or the commands to make a check pass. A wrong
  answer here means a bug that the group needs to know about.
- Keep failed or interrupted runs out of the median. Note that they happened.

## What to hand back

1. The whole `logs/stageC/` folder. Logs are `.txt` because `*.log` is gitignored and the
   brief requires them in the submission.
2. `stageC/rival_sweep.dat`, and `.png` if made. `*.dat` is gitignored, so committing it
   needs `git add -f`. Only commit if the person asks.
3. The machine details the brief requires: model, OS, compiler version (`gcc --version`),
   flags (`-O2 -Wall`), CPU governor
   (`cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor`).
4. For each engine and speed, the **median of the 5 runs** of: average latency, worst-case
   latency, maximum lag and P&L. These are in the summary at the end of each log.
5. From the rival logs: P&L for each version and the money lost at R = 1 µs.

## On the Raspberry Pi (CSC1104 runs)

All final marked measurements must come from the Pi. Use the same steps, with these changes:

- Build with `make stageC CFLAGS_C="-O2 -Wall -mcpu=cortex-a76"`.
- Set the CPU governor to `performance` first.
- To show the LED, add `--led` and run with `sudo`.
- Run `make asm`, then annotate the new `*_aarch64_O2.s` listings.
