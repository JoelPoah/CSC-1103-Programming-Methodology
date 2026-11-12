"""
crossover.py - BUY / SELL signals from the short and long SMA (Python version of signal.c)

(Called crossover.py and not signal.py because Python already has a module named signal.)

    BUY  when short SMA was <= long SMA on the last trade and is now > long SMA
    SELL when short SMA was >= long SMA on the last trade and is now < long SMA
A signal needs both SMAs on two trades in a row, so the first one can only
happen at trade long_window + 1.
"""

# same names and values as signal.h
SIG_BUY = 1
SIG_SELL = -1
SIG_NONE = 0


class Crossover:

    def __init__(self):
        self.prev_short = 0.0      # short SMA on the previous trade
        self.prev_long = 0.0       # long SMA on the previous trade
        self.have_prev = False     # True once there is a previous pair to compare with
        self.buys = 0
        self.sells = 0

    def update(self, sma_short, sma_long):
        """Call once per trade (only when both SMAs are ready). Returns SIG_BUY, SIG_SELL or SIG_NONE."""
        signal = SIG_NONE

        if self.have_prev:
            if self.prev_short <= self.prev_long and sma_short > sma_long:
                signal = SIG_BUY       # short just went above long
                self.buys += 1
            elif self.prev_short >= self.prev_long and sma_short < sma_long:
                signal = SIG_SELL      # short just went below long
                self.sells += 1

        # remember this trade for next time, whatever happened above
        self.prev_short = sma_short
        self.prev_long = sma_long
        self.have_prev = True
        return signal
