"""
trade_loader.py - reads a whole trade csv into numpy arrays (NumPy version of trade_reader.py)

Every line has 7 fields:
    trade id, price, qty, quoteQty, time (microseconds), isBuyerMaker, isBestMatch

The pure version reads and splits one line at a time in Python.
Here np.loadtxt reads the whole file in one call and gives one array per column.
"""

import numpy as np


class Trades:

    def __init__(self, price, qty, time_us, is_buy):
        self.price = price         # one entry per trade
        self.qty = qty
        self.time_us = time_us     # microseconds since 1 Jan 1970
        self.is_buy = is_buy       # 1 if a buyer started the trade, None if not loaded
        self.count = len(price)


def load_trades(path, with_side=False):
    """
    Read every trade in the file. Returns a Trades object, or None if the file cannot be read.
    with_side=True also reads isBuyerMaker. Only the test20 table prints it, so the
    other runs skip it (reading text columns is slow).
    """
    print("load_trades: reading", path)
    try:
        # columns 1, 2 and 4 = price, qty and time; unpack=True gives one array each
        price, qty, time_us = np.loadtxt(path, delimiter=",", usecols=(1, 2, 4), unpack=True)
        is_buy = None
        if with_side:
            sides = np.loadtxt(path, delimiter=",", usecols=5, dtype=str)
            # isBuyerMaker False means the buyer started it, so it counts as a BUY
            is_buy = (sides == "False").astype(int)
    except (OSError, ValueError):
        print("load_trades: cannot read", path)
        print("load_trades: check the csv paths in Python/settings.py")
        return None

    # loadtxt reads numbers as decimals; times are whole numbers, so turn them back
    time_us = time_us.astype(np.int64)
    return Trades(price, qty, time_us, is_buy)
