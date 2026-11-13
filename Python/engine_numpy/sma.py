"""
sma.py - NAIVE simple moving average for every trade at once (NumPy version of engine/sma.py)

Same maths as the pure NaiveSMA (and sma.c, our Version 0): the average of the
last N prices is worked out by adding up all N prices again.

    pure Python: for each trade, add its N prices one by one
    NumPy:       for each of the N buffer slots, add that slot for ALL trades at once

Why go slot by slot? The C and pure versions keep the window in a circular
buffer and add slot 0, then slot 1, ... up to slot N-1. Computers round a
little differently depending on the order numbers are added in, so we add in
the SAME order. Then the averages are exactly the same as C, and so are the
BUY / SELL counts (2509 / 2469 on dev_1M.csv).

Which price is in slot j at trade t? Price number k is written into slot k % N,
so slot j was last written (t - j) % N trades ago. That makes it price[t - age].
"""

import numpy as np


def naive_sma(price, window):
    """
    Returns an array with the SMA at every trade.
    The first window - 1 trades have no SMA yet, they get 0.0 (like NaiveSMA.value()).
    """
    n = len(price)
    sma = np.zeros(n)
    if n < window:
        return sma                         # not enough trades for one full window

    t = np.arange(window - 1, n)           # every trade that has a full window
    total = np.zeros(len(t))
    for j in range(window):                # slot 0, slot 1, ... like the C loop
        age = (t - j) % window             # how many trades ago slot j was written
        total += price[t - age]            # add slot j for every trade at once

    sma[window - 1:] = total / window
    return sma
