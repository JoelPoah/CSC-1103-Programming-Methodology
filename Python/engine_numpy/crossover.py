"""
crossover.py - BUY / SELL signals for every trade at once (NumPy version of engine/crossover.py)

Same rule as the pure Crossover (and signal.c):
    BUY  when short SMA was <= long SMA on the last trade and is now > long SMA
    SELL when short SMA was >= long SMA on the last trade and is now < long SMA
Both SMAs exist from trade long_window on, and a signal needs two trades in a
row, so the first possible signal is at trade long_window + 1.
"""

import numpy as np

from engine.crossover import SIG_BUY, SIG_SELL, SIG_NONE    # same values as signal.h


def crossovers(sma_short, sma_long, long_window):
    """Returns an array with SIG_BUY, SIG_SELL or SIG_NONE for every trade."""
    signal = np.full(len(sma_short), SIG_NONE)

    for_now = slice(long_window, None)            # trades that can have a signal
    for_before = slice(long_window - 1, -1)       # the trade just before each of them

    short_now = sma_short[for_now]
    long_now = sma_long[for_now]
    short_before = sma_short[for_before]
    long_before = sma_long[for_before]

    buy = (short_before <= long_before) & (short_now > long_now)
    sell = (short_before >= long_before) & (short_now < long_now)

    signal[for_now][buy] = SIG_BUY
    signal[for_now][sell] = SIG_SELL
    return signal
