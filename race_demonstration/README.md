# Race demonstration: Version 0 against the fastest version, live

Lucas's Stage C race, copied out of `stageC/` on his branch (`origin/lucas`, commit
`e52a3ff`, 20 Nov 2026) so the demo lives in one folder. The code is his, unchanged apart
from the paths (the program is now `race_demonstration/stageC_replay`). The same programs
are also in `dev/stageC/`, which keeps the rest of Stage C (malloc loader, rival sweep,
`test_sort`, assembly listings).

Two copies of `stageC_replay` run the same day of trades (`stress_full.csv`) against the
same replay clock:

- **v0** = Version 0: reads each trade from the file when it is released (`next_trade()`,
  `fgets` + `sscanf`), double prices, naive SMA.
- **mem** = the fastest version (ladder V7): the whole file loaded first with one `fread`,
  fixed-point prices, running-sum SMA. On the Pi, `stageC_replay_asm` is the same with
  Yong Sheng's AArch64 `ohlc_fx.s` (as V8).

When v0 needs more time per trade than the speed-up leaves, it falls behind the clock: its
**lag** grows, its signals come late, and the rival bot gets in first. The scoreboard shows
lag, latency, P&L and the **money lost to slow code** (P&L(mem) − P&L(v0)).

## Files

```
race_demonstration/
  stageC_replay.c     the replay: --engine v0|mem, --speed X, --led / --led-gpio, --feed N
  stageC_gui.c        GTK3 window: runs the race here or on the Pi over ssh, live charts
  plot_live.py        recorded --feed output -> animated GIF of the race
  engine/             the modules the replay links (Stage B's modules + Stage C's loader,
                      rival bot, percentiles, LED driver, ohlc_fx.s)
  media/              race GIFs and final stills: laptop 1000x, Pi 5000x and 10000x
  results/            RACE_RESULTS_pi.md (Pi 5000x / 10000x races) + throttling readings
```

The course helper library comes from `helper_function/` in the project root.

## Build (from the project root)

```sh
make race        # race_demonstration/stageC_replay (+ stageC_replay_asm on the Pi)
make race_gui    # + the GTK3 window race_demonstration/stageC_gui
```

The window needs GTK3: on Windows (MSYS2 UCRT64) `pacman -S mingw-w64-ucrt-x86_64-gtk3
mingw-w64-ucrt-x86_64-pkgconf`, on the Pi `sudo apt install libgtk-3-dev`.

## Run the race

Two terminals side by side, same speed, from the project root. On the Pi, pin each engine
to its own core. At 10000x v0 falls behind for good; at 5000x it only falls behind in
the busy stretches.

```sh
taskset -c 2 ./race_demonstration/stageC_replay data/group_data/stress_full.csv --engine v0  --speed 10000
sudo taskset -c 3 ./race_demonstration/stageC_replay data/group_data/stress_full.csv --engine mem --speed 10000 --led
```

`--led` is the onboard LED, `--led-gpio` the breadboard LEDs (GPIO 17 red, GPIO 27 green),
`--led-every N` flashes on every Nth BUY / SELL. Quick correctness check (no clock):

```sh
./race_demonstration/stageC_replay data/group_data/test20.csv --short 3 --long 5 --speed 0 --signals --engine v0
```

### In the window

```sh
./race_demonstration/stageC_gui
```

**Run on** `This computer` or `Raspberry Pi (ssh)`. With the Pi, the window stays on the
laptop and runs `cd <Pi project folder> && taskset -c 2|3 ./race_demonstration/stageC_replay
... --feed 1500` over `ssh -tt -o BatchMode=yes`, so set up ssh key login first and run
`make race` on the Pi. On a Windows laptop both engines share the CPU with the window, so
those times are a demo, not results.

### Make a GIF

```sh
taskset -c 2 ./race_demonstration/stageC_replay data/group_data/stress_full.csv --engine v0  --speed 5000 --feed 1500 > race_v0_5000x.txt &
taskset -c 3 ./race_demonstration/stageC_replay data/group_data/stress_full.csv --engine mem --speed 5000 --feed 1500 > race_mem_5000x.txt
python race_demonstration/plot_live.py race_v0_5000x.txt race_mem_5000x.txt -o race_demonstration/media/race_5000x_pi.gif --title "Pi 5, 5000x"
```

Needs `pip install matplotlib pillow`.

## Results so far

Raspberry Pi 5, 20 Nov 2026, 3 races per speed (median), details in
`results/RACE_RESULTS_pi.md`:

| speed | time per trade available | v0 | mem |
|---|---:|---|---|
| 10000x | 968 ns | **fell behind for good**: finished 3.09 s late, max lag 3.38 s | on time, max lag 1.0-1.6 ms |
| 5000x | 1,935 ns | kept up overall, but up to 0.55 s behind in bursts | on time, max lag 1.0-1.6 ms |

v0 needs about 1,190 ns per trade on the Pi, which is why 10000x is the speed where it
loses. On the Windows laptop at 1000x (`media/race_1000x_laptop.gif`), v0 averaged
10.1 µs per trade and fell up to 14.2 s behind; money lost +64.23 USDT.

The raw `--feed` recordings behind the Pi GIFs are on Lucas's branch in
`logs/stageC/feed/pi/` (about 20 MB, not copied here).
