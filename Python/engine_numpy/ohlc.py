"""
ohlc.py - one-second OHLC bars for the whole file at once (NumPy version of engine/ohlc.py)

Trades are grouped by the whole second they happened in:
    second = time_us // 1,000,000      (integer division drops the microseconds)
A new bar starts wherever the second changes from one trade to the next.
"""

import numpy as np

from engine.ohlc import Bar           # the same Bar as the pure version

MICROS_PER_SEC = 1_000_000


def one_second_bars(time_us, price):
    """Returns a list of Bar objects, one per second, in file order."""
    seconds = time_us // MICROS_PER_SEC

    # a bar starts at trade 0 and at every trade whose second is not the one before it
    starts = np.flatnonzero(seconds[1:] != seconds[:-1]) + 1
    starts = np.concatenate(([0], starts))

    # high and low of each bar in one call each
    highs = np.maximum.reduceat(price, starts)
    lows = np.minimum.reduceat(price, starts)

    bars = []
    for b in range(len(starts)):
        first = starts[b]
        if b + 1 < len(starts):
            last = starts[b + 1] - 1       # the trade before the next bar starts
        else:
            last = len(price) - 1          # the last bar ends at the last trade
        bars.append(Bar(int(seconds[first]), float(price[first]), float(highs[b]),
                        float(lows[b]), float(price[last]), int(last - first + 1)))
    return bars
