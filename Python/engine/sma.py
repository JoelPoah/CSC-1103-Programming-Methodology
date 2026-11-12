"""
sma.py - NAIVE simple moving average (Python version of sma.c, our Version 0)

The last N prices sit in a list that is used as a circular buffer: the newest
price overwrites the oldest one. value() then adds up ALL N prices again on
every trade, so each trade costs N additions. That is the slow part on
purpose - this is Version 0, the baseline the C running-sum rung
(stageA/stageA_running_sma.c) gets compared against.
"""


class NaiveSMA:

    def __init__(self, window):
        if window < 1:
            print("NaiveSMA: window must be at least 1, using 1")
            window = 1
        self.window = window             # N, how many prices the average covers
        self.prices = [0.0] * window     # circular buffer of the most recent prices
        self.head = 0                    # next slot to overwrite
        self.count = 0                   # prices seen so far, stops growing at window

    def push(self, price):
        """Add the newest price. Once the window is full it overwrites the oldest one."""
        self.prices[self.head] = price
        self.head = (self.head + 1) % self.window    # wraps back to 0 at the end
        if self.count < self.window:
            self.count += 1

    def ready(self):
        # the SMA only exists once the window is full (from trade N onwards)
        return self.count == self.window

    def value(self):
        """Average of the last N prices, re-added from scratch every time (0.0 if not ready)."""
        if not self.ready():
            return 0.0

        # NOTE: plain loop on purpose, do not swap it for sum(self.prices).
        # From Python 3.12 sum() adds floats with extra precision, so the
        # averages come out a tiny bit different from the C version and the
        # BUY / SELL counts on dev_1M.csv stop matching (2553/2471 instead of 2509/2469).
        total = 0.0
        for price in self.prices:
            total += price
        return total / self.window
