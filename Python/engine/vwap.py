"""
vwap.py - running VWAP (Python version of vwap.c)

VWAP = sum(price * qty) / sum(qty) over every trade so far.
Only two running sums are kept, so it never needs more memory.
"""


class VWAP:

    def __init__(self):
        self.sum_pq = 0.0      # running sum of price * qty
        self.sum_q = 0.0       # running sum of qty

    def update(self, price, qty):
        self.sum_pq += price * qty
        self.sum_q += qty

    def value(self):
        # 0.0 before any volume came in, so we never divide by zero
        if self.sum_q > 0:
            return self.sum_pq / self.sum_q
        return 0.0
