"""
stageA_Python.py - Stage A in Python, NAIVE SMA (stretch goal: C versus Python)

Does the same job as stageA/stageA_manual.c, our C Version 0:
    1) running VWAP                (engine/vwap.py)
    2) one-second OHLC bars        (engine/ohlc.py)
    3) short and long SMA, NAIVE   (engine/sma.py - re-adds the whole window every trade)
    4) BUY / SELL crossovers       (engine/crossover.py)
    5) signals.dat + signals.png   (engine/report.py + engine/plot.py)
    6) the test20.csv table, to compare with data/group_data/test20_expected.txt
Same naive SMA as sma.c, so the C vs Python timing is a fair fight.

How to run (from the project root, or from any other folder):
    py Python/stageA_Python.py          test20 + dev_1M + signals.png
    py Python/stageA_Python.py bench    time BENCH_PATH (stress_full.csv) BENCH_RUNS times
On Mac / Pi type python3 instead of py.
bench on stress_full.csv is slow: about 2 minutes per run on my laptop (so 10+ minutes
for 5 runs), and longer on the Pi. Plug the laptop in and close other programs first.

Paths, windows, slice, number of runs and the plot look are all in settings.py.
"""

import os
import statistics
import sys
import time

import settings
from engine.stage_a import StageAEngine


def go_to_project_root():
    """
    Every path in settings.py is relative to the project root, so move there
    first. This file lives in <project root>/Python/, so the root is one folder up.
    """
    this_folder = os.path.dirname(__file__)
    os.chdir(os.path.join(this_folder, ".."))


def check_test20():
    """Part A - the 20-trade file with the full table. The 20 rows should match test20_expected.txt."""
    print(f"=== test20.csv (short={settings.TEST_SHORT}, long={settings.TEST_LONG}) ===")
    engine = StageAEngine(settings.TEST_SHORT, settings.TEST_LONG)
    return engine.run(settings.TEST20_PATH, print_table=True)


def run_dev():
    """Part B - the whole dev file, summary only, and write the plot slice to signals.dat."""
    print(f"\n=== dev_1M.csv (short={settings.DEV_SHORT}, long={settings.DEV_LONG}) ===")
    slice_to = settings.SLICE_START + settings.SLICE_LEN - 1
    engine = StageAEngine(settings.DEV_SHORT, settings.DEV_LONG)
    return engine.run(settings.DEV_PATH,
                      slice_from=settings.SLICE_START,
                      slice_to=slice_to,
                      dat_path=settings.SIGNALS_DAT,
                      signals_to_show=settings.SIGNALS_TO_SHOW)


def draw_plot():
    """Part C - draw signals.png from signals.dat."""
    print(f"\nDrawing {settings.SIGNALS_PNG} ...")
    try:
        from engine.plot import SignalPlot      # only needs matplotlib if we get this far
    except ImportError:
        print("matplotlib is not installed, skipping the plot (signals.dat is still written).")
        print("  Windows: py -m pip install matplotlib")
        print("  Mac:     python3 -m pip install matplotlib")
        print("  Pi:      sudo apt install python3-matplotlib")
        return

    plot = SignalPlot(settings.SIGNALS_DAT, settings.SIGNALS_PNG)
    try:
        plot.draw()
    except OSError:
        print(f"could not read {settings.SIGNALS_DAT} or write {settings.SIGNALS_PNG},"
              " check the paths in settings.py")
        return

    print(f"Saved {settings.SIGNALS_PNG} ({plot.buys} BUY, {plot.sells} SELL in the slice)")
    if plot.buys == 0 or plot.sells == 0:
        print("Heads up: the brief wants at least one BUY and one SELL in the slice,"
              " move SLICE_START in settings.py")


def benchmark():
    """Time BENCH_PATH a few times and print the median (no table, no signals.dat)."""
    if settings.BENCH_RUNS < 1:
        print("BENCH_RUNS in settings.py has to be at least 1")
        return False

    print(f"=== benchmark: {settings.BENCH_PATH} (short={settings.DEV_SHORT}, "
          f"long={settings.DEV_LONG}), {settings.BENCH_RUNS} runs ===")
    times = []
    engine = StageAEngine(settings.DEV_SHORT, settings.DEV_LONG)

    for run_no in range(1, settings.BENCH_RUNS + 1):
        # perf_counter_ns is Python's version of now_ns() from timing.h
        start = time.perf_counter_ns()
        ok = engine.run(settings.BENCH_PATH, signals_to_show=0)
        elapsed = time.perf_counter_ns() - start
        if not ok:
            return False
        times.append(elapsed)
        print(f"run {run_no}: {elapsed / 1e9:.3f} s\n")

    if engine.trades == 0:
        print("no trades in", settings.BENCH_PATH)
        return False

    median = statistics.median(times)     # middle run (average of the two middle ones if the count is even)
    per_trade = median / engine.trades
    trades_per_sec = engine.trades / (median / 1e9)

    print(f"Median of {settings.BENCH_RUNS} runs: {median / 1e9:.3f} s")
    print(f"Trades: {engine.trades}")
    print(f"Time per trade: {per_trade:.1f} ns")
    print(f"Throughput: {trades_per_sec:.0f} trades per second")
    return True


def main():
    go_to_project_root()

    mode = "normal"
    if len(sys.argv) > 1:
        mode = sys.argv[1]

    if mode == "bench":
        if benchmark():
            return 0
        return 1

    if mode != "normal":
        print("unknown mode:", mode)
        print("usage: py Python/stageA_Python.py [bench]")
        return 1

    if not check_test20():
        return 1
    if not run_dev():
        return 1
    draw_plot()
    return 0


if __name__ == "__main__":
    sys.exit(main())
