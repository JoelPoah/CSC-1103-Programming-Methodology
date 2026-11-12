"""
ohlc.py - one-second OHLC bars (Python version of ohlc.c)

Trades are grouped by the whole second they happened in:
    second = time_us // 1,000,000      (integer division drops the microseconds)
When a trade arrives in a new second, the old bar is finished and a new one starts.
"""

MICROS_PER_SEC = 1_000_000


# a finished bar (takes the place of the ohlc_finished_*() getters in C)
class Bar:

    def __init__(self, second, open_price, high, low, close, trades):
        self.second = second
        self.open_price = open_price
        self.high = high
        self.low = low
        self.close = close
        self.trades = trades       # how many trades went into this bar


class OHLC:

    def __init__(self):
        self.active = False        # False until the first trade starts a bar

        # the bar being built right now
        self.second = 0
        self.open_price = 0.0
        self.high = 0.0
        self.low = 0.0
        self.close = 0.0
        self.trades = 0

        self.finished = None       # the last finished Bar

    def update(self, time_us, price):
        """
        Put one trade into the current bar.
        Returns True if this trade closed the previous bar (it is in self.finished).
        """
        second = time_us // MICROS_PER_SEC

        # same second -> just update the bar
        if self.active and second == self.second:
            if price > self.high:
                self.high = price
            if price < self.low:
                self.low = price
            self.close = price
            self.trades += 1
            return False

        # new second -> finish the old bar (if there is one) and start a new one
        closed = False
        if self.active:
            self.finish_bar()
            closed = True
        self.second = second
        self.open_price = price
        self.high = price
        self.low = price
        self.close = price
        self.trades = 1
        self.active = True
        return closed

    def flush(self):
        """After the last trade: finish the bar still open. Returns True if there was one."""
        if not self.active:
            return False
        self.finish_bar()
        self.active = False
        return True

    def finish_bar(self):
        self.finished = Bar(self.second, self.open_price, self.high,
                            self.low, self.close, self.trades)
