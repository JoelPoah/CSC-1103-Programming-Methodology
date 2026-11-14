"""
stageA_Python_numpy.py - Stage A in Python with NumPy, NAIVE SMA (stretch goal: C versus Python)

The same job, the same maths and the same output as stageA_Python.py, but with
NumPy arrays instead of one trade at a time:
    1) read the whole file into arrays  (engine_numpy/trade_loader.py - np.loadtxt)
    2) running VWAP                     (engine_numpy/vwap.py         - np.cumsum)
    3) one-second OHLC bars             (engine_numpy/ohlc.py         - reduceat)
    4) short and long SMA, NAIVE        (engine_numpy/sma.py          - N array adds)
    5) BUY / SELL crossovers            (engine_numpy/crossover.py    - shifted compare)
    6) signals_numpy.dat + signals_numpy.png, and the test20.csv table
Same naive SMA, added in the same order as sma.c, so the answers are exactly
the C Version 0 answers and the timing comparison is a fair fight:
    stageA_Python.py        pure Python: the loop over trades runs in the interpreter
    stageA_Python_numpy.py  NumPy:       the loops run inside NumPy's compiled C code

How to run (from the project root, or from any other folder):
    py Python/stageA_Python_numpy.py          test20 + dev_1M + signals_numpy.png
    py Python/stageA_Python_numpy.py bench    time BENCH_PATH (stress_full.csv) BENCH_RUNS times
On Mac / Pi type python3 instead of py. Needs numpy:
    Windows: py -m pip install numpy      Pi: sudo apt install python3-numpy

Paths, windows, slice, number of runs and the plot look are all in settings.py.
"""

import os
import statistics
import sys
import time

import settings

try:
    from engine_numpy.stage_a import StageANumpyEngine
except ImportError:
    print("numpy is not installed.")
    print("  Windows: py -m pip install numpy")
    print("  Mac:     python3 -m pip install numpy")
    print("  Pi:      sudo apt install python3-numpy")
    sys.exit(1)


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
    engine = StageANumpyEngine(settings.TEST_SHORT, settings.TEST_LONG)
    return engine.run(settings.TEST20_PATH, print_table=True)


def run_dev():
    """Part B - the whole dev file, summary only, and write the plot slice to signals_numpy.dat."""
    print(f"\n=== dev_1M.csv (short={settings.DEV_SHORT}, long={settings.DEV_LONG}) ===")
    slice_to = settings.SLICE_START + settings.SLICE_LEN - 1
    engine = StageANumpyEngine(settings.DEV_SHORT, settings.DEV_LONG)
    return engine.run(settings.DEV_PATH,
                      slice_from=settings.SLICE_START,
                      slice_to=slice_to,
                      dat_path=settings.SIGNALS_DAT_NUMPY,
                      signals_to_show=settings.SIGNALS_TO_SHOW)


def draw_plot():
    """Part C - draw signals_numpy.png from signals_numpy.dat."""
    print(f"\nDrawing {settings.SIGNALS_PNG_NUMPY} ...")
    try:
        from engine.plot import SignalPlot      # only needs matplotlib if we get this far
    except ImportError:
        print("matplotlib is not installed, skipping the plot (signals_numpy.dat is still written).")
        print("  Windows: py -m pip install matplotlib")
        print("  Mac:     python3 -m pip install matplotlib")
        print("  Pi:      sudo apt install python3-matplotlib")
        return

    plot = SignalPlot(settings.SIGNALS_DAT_NUMPY, settings.SIGNALS_PNG_NUMPY,
                      settings.PLOT_TITLE_NUMPY)
    try:
        plot.draw()
    except OSError:
        print(f"could not read {settings.SIGNALS_DAT_NUMPY} or write {settings.SIGNALS_PNG_NUMPY},"
              " check the paths in settings.py")
        return

    print(f"Saved {settings.SIGNALS_PNG_NUMPY} ({plot.buys} BUY, {plot.sells} SELL in the slice)")
    if plot.buys == 0 or plot.sells == 0:
        print("Heads up: the brief wants at least one BUY and one SELL in the slice,"
              " move SLICE_START in settings.py")


def benchmark():
    """Time BENCH_PATH a few times and print the median (no table, no signals.dat).
    Same measurement as stageA_Python.py: the whole run, reading the file included."""
    if settings.BENCH_RUNS < 1:
        print("BENCH_RUNS in settings.py has to be at least 1")
        return False

    print(f"=== benchmark (NumPy): {settings.BENCH_PATH} (short={settings.DEV_SHORT}, "
          f"long={settings.DEV_LONG}), {settings.BENCH_RUNS} runs ===")
    times = []
    engine = StageANumpyEngine(settings.DEV_SHORT, settings.DEV_LONG)

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
        print("usage: py Python/stageA_Python_numpy.py [bench]")
        return 1

    if not check_test20():
        return 1
    if not run_dev():
        return 1
    draw_plot()
    return 0


if __name__ == "__main__":
    sys.exit(main())
