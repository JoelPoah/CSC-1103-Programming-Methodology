"""
vwap.py - running VWAP for every trade at once (NumPy version of engine/vwap.py)

VWAP after trade t = sum(price * qty) / sum(qty) over trades 1 .. t.
np.cumsum gives the running total after every trade, adding in file order
like the C loop, so the final VWAP is exactly the same as the C Version 0.
"""

import numpy as np


def running_vwap(price, qty):
    """Returns an array: the VWAP after each trade (every trade has qty > 0)."""
    sum_pq = np.cumsum(price * qty)
    sum_q = np.cumsum(qty)
    return sum_pq / sum_q
