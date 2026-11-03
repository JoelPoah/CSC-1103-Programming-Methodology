"""
stageA_Python.py - Stage A: baseline streaming engine (Python port of stageA_stream.c)

One pass over a trade file, one trade at a time, computing:
  1) running VWAP            (two running sums)
  2) one-second OHLC bars    (four variables, reset every new second)
  3) short and long SMA      (fixed-length queues of the most recent prices)
  4) BUY / SELL signals      (short SMA crossing the long SMA), counted
  5) signals.dat + signals.png for a ~2000-trade slice (via matplotlib)
  6) test20.csv table to compare with data/group_data/test20_expected.txt

Run (from the project root, paths below are relative to it):
    py stageA_Python.py            # test20 + dev_1M + signals.png
    py stageA_Python.py bench      # time stress_full.csv (5 runs, median)
"""
import os
import sys
import time
from collections import deque

import matplotlib
matplotlib.use("Agg")          # draw to a file, no window needed
import matplotlib.pyplot as plt

# ---- paths (relative to the project root) ----
TEST20_PATH = "C:\\Users\\frozl\\Documents\\CSC-1103-Programming-Methodology\\data\\test20.csv"
DEV_PATH = "C:\\Users\\frozl\\Documents\\CSC-1103-Programming-Methodology\\data\\dev_1M.csv"
STRESS_PATH = "C:\\Users\\frozl\\Documents\\CSC-1103-Programming-Methodology\\data\\stress_full.csv"
SIGNALS_DAT = "stageA/signals.dat"
PLOT_SCRIPT = "stageA/plot_signals.gnuplot"
# outputs are written next to this script, wherever you run it from
HERE = os.path.dirname(os.path.abspath(__file__))
SIGNALS_DAT = os.path.join(HERE, "signals.dat")
SIGNALS_PNG = os.path.join(HERE, "signals.png")

# ---- SMA windows ----
TEST_SHORT, TEST_LONG = 3, 5      # test20.csv
DEV_SHORT, DEV_LONG = 35, 60      # Group 06: dev_1M.csv and stress_full.csv

# ---- slice of dev_1M.csv written to signals.dat ----
SLICE_START = 1                   # first trade number in the slice
SLICE_LEN = 2000


def print_bar(sec, o, h, lo, c, n):
    """Print one finished one-second OHLC bar."""
    print(f"{sec:<12d} {o:<16.8f} {h:<16.8f} {lo:<16.8f} {c:<16.8f} {n}")


def run(path, S, L, table, slice_from, slice_to, datpath):
    """
    Stream one file.
      table      : True = print the per-trade table (test20 style), False = summary only
      slice_*    : write trades [slice_from, slice_to] to datpath (datpath None = skip)
    Returns 0 on success, 1 on error.
    """
    if S >= L:
        print(f"bad windows: need short < long (got {S}, {L})")
        return 1

    # 1) VWAP: two running sums
    sum_pq = 0.0
    sum_q = 0.0
    vwap = 0.0

    # 2) OHLC: four variables + trade count for the current second
    cur_sec = None
    o = h = lo = c = 0.0
    bar_n = 0

    # 3) SMA: fixed-length queues (oldest price drops off the left) + running sums
    win_s = deque(maxlen=S)
    win_l = deque(maxlen=L)
    sum_s = 0.0
    sum_l = 0.0

    # 4) signals
    have_prev = False
    prev_s = prev_l = 0.0
    buys = sells = 0
    shown = 0                      # first few signals printed in summary mode

    try:
        f = open(path, "r")
    except OSError:
        print(f"cannot open {path}")
        return 1

    dat = None
    if datpath is not None:
        try:
            dat = open(datpath, "w")
            dat.write("# trade price sma_short sma_long signal(+1 BUY, -1 SELL, 0 none)\n")
        except OSError:
            print(f"cannot open {datpath}")
            dat = None

    if table:
        print(f"{'trade':<6} {'price':<16} {'qty':<12} {'is_buy':<6} {'VWAP':<16} "
              f"{'SMA_short':<16} {'SMA_long':<16} signal")

    with f:
        for i, line in enumerate(f, start=1):
            parts = line.strip().split(",")
            if len(parts) < 6:
                continue
            if i == 1 and not parts[0].lstrip("-").isdigit():
                continue                         # skip a header row if there is one
            p = float(parts[1])                  # price
            q = float(parts[2])                  # qty
            t = int(parts[4])                    # time in microseconds
            is_buy = 1 if parts[5].strip().lower() == "false" else 0   # isBuyerMaker False = BUY
            sec = t // 1_000_000
            sig = 0

            # 1) VWAP
            sum_pq += p * q
            sum_q += q
            vwap = sum_pq / sum_q

            # 2) OHLC: new second -> emit old bar and reset
            if sec != cur_sec:
                if cur_sec is not None and table:
                    print_bar(cur_sec, o, h, lo, c, bar_n)
                cur_sec = sec
                o = h = lo = c = p
                bar_n = 1
            else:
                if p > h:
                    h = p
                if p < lo:
                    lo = p
                c = p
                bar_n += 1

            # 3) SMA: subtract the oldest price (if the window is full), then add the new one
            if len(win_s) == S:
                sum_s -= win_s[0]
            if len(win_l) == L:
                sum_l -= win_l[0]
            win_s.append(p)
            win_l.append(p)
            sum_s += p
            sum_l += p

            have_s = len(win_s) == S
            have_l = len(win_l) == L
            sma_s = sum_s / S if have_s else 0.0
            sma_l = sum_l / L if have_l else 0.0

            # 4) crossover signal: needs both SMAs on two trades in a row
            if have_s and have_l:
                if have_prev:
                    if prev_s <= prev_l and sma_s > sma_l:
                        sig = 1
                        buys += 1
                    elif prev_s >= prev_l and sma_s < sma_l:
                        sig = -1
                        sells += 1
                prev_s = sma_s
                prev_l = sma_l
                have_prev = True

            # output
            if table:
                s_txt = f"{sma_s:<16.8f}" if have_s else f"{'-':<16}"
                l_txt = f"{sma_l:<16.8f}" if have_l else f"{'-':<16}"
                sig_txt = "BUY" if sig == 1 else "SELL" if sig == -1 else ""
                print(f"{i:<6d} {p:<16.8f} {q:<12.8f} {is_buy:<6d} {vwap:<16.8f} "
                      f"{s_txt} {l_txt} {sig_txt}")
            elif sig != 0 and shown < 10:
                print(f"  signal {'BUY ' if sig == 1 else 'SELL'} at trade {i}")
                shown += 1

            # 5) slice for the plot (only once the long SMA exists)
            if dat is not None and slice_from <= i <= slice_to and have_l:
                dat.write(f"{i} {p:.8f} {sma_s:.8f} {sma_l:.8f} {sig}\n")

    # flush the last OHLC bar
    if cur_sec is not None and table:
        print("\nOne-second OHLC bars (second = time / 1,000,000)")
        print(f"{'second':<12} {'open':<16} {'high':<16} {'low':<16} {'close':<16} trades")
        print_bar(cur_sec, o, h, lo, c, bar_n)
    if dat is not None:
        dat.close()

    print(f"\nFinal VWAP: {vwap:.8f}")
    print(f"Signals: {buys} BUY, {sells} SELL")
    return 0


def plot_signals(dat_path, png_path):
    """Read signals.dat and draw price, short SMA, long SMA and BUY/SELL markers."""
    trade, price, sma_s, sma_l, sig = [], [], [], [], []
    with open(dat_path) as f:
        for line in f:
            if line.startswith("#") or not line.strip():
                continue
            a = line.split()
            trade.append(int(a[0]))
            price.append(float(a[1]))
            sma_s.append(float(a[2]))
            sma_l.append(float(a[3]))
            sig.append(int(a[4]))

    fig, ax = plt.subplots(figsize=(12, 6))
    ax.plot(trade, price, color="grey", linewidth=0.8, label="Price")
    ax.plot(trade, sma_s, color="orangered", linewidth=1.5, label="Short SMA")
    ax.plot(trade, sma_l, color="royalblue", linewidth=1.5, label="Long SMA")

    # markers sit on the price at the trade where the signal fired
    bx = [t for t, s in zip(trade, sig) if s == 1]
    by = [p for p, s in zip(price, sig) if s == 1]
    sx = [t for t, s in zip(trade, sig) if s == -1]
    sy = [p for p, s in zip(price, sig) if s == -1]
    ax.scatter(bx, by, marker="^", s=80, color="green", zorder=5, label="BUY")
    ax.scatter(sx, sy, marker="v", s=80, color="red", zorder=5, label="SELL")

    ax.set_title("Price, short/long SMA and BUY/SELL signals")
    ax.set_xlabel("Trade number")
    ax.set_ylabel("Price (USDT)")
    ax.grid(alpha=0.3)
    ax.legend(loc="best")
    fig.tight_layout()
    fig.savefig(png_path, dpi=150)
    plt.close(fig)


def benchmark(runs=5):
    """Time stress_full.csv (no table, no .dat file) and report the median."""
    times = []
    for k in range(runs):
        start = time.perf_counter_ns()
        if run(STRESS_PATH, DEV_SHORT, DEV_LONG, False, 0, 0, None):
            return 1
        times.append(time.perf_counter_ns() - start)
        print(f"run {k + 1}: {times[-1] / 1e9:.3f} s")
    median = sorted(times)[len(times) // 2]
    print(f"\nMedian of {runs} runs: {median / 1e9:.3f} s")


def main():
    if len(sys.argv) > 1 and sys.argv[1] == "bench":
        return benchmark()

    # correctness check on the 20-trade file
    print(f"=== test20.csv (short={TEST_SHORT}, long={TEST_LONG}) ===")
    if run(TEST20_PATH, TEST_SHORT, TEST_LONG, True, 0, 0, None):
        return 1

    # full dev file + signals.dat slice
    print(f"\n=== dev_1M.csv (short={DEV_SHORT}, long={DEV_LONG}) ===")
    if run(DEV_PATH, DEV_SHORT, DEV_LONG, False,
           SLICE_START, SLICE_START + SLICE_LEN - 1, SIGNALS_DAT):
        return 1

    # draw signals.png with matplotlib
    print(f"\nDrawing {SIGNALS_PNG} ...")
    try:
        plot_signals(SIGNALS_DAT, SIGNALS_PNG)
        print("Saved signals.png")
    except Exception as e:
        print(f"plot failed ({e}); signals.dat is still written.")
    return 0


if __name__ == "__main__":
    sys.exit(main())