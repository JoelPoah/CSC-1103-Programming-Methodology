"""
stage_a.py - Stage A with NumPy (NumPy version of engine/stage_a.py)

Same job and same output as the pure StageAEngine, but in whole-file steps
instead of one trade at a time:
    a) read the whole file into arrays               (trade_loader.py)
    b) VWAP, OHLC bars, both naive SMAs, the signals  (vwap.py, ohlc.py, sma.py, crossover.py)
    c) print / write whatever this run needs          (engine/report.py, shared with the pure version)
The OHLC bars are printed after the table instead of in between. For
test20.csv that is the same thing, because it has only one bar.
"""

import numpy as np

from engine.report import Report, DatFile
from engine_numpy.trade_loader import load_trades
from engine_numpy.vwap import running_vwap
from engine_numpy.ohlc import one_second_bars
from engine_numpy.sma import naive_sma
from engine_numpy.crossover import crossovers, SIG_BUY, SIG_SELL, SIG_NONE


class StageANumpyEngine:

    def __init__(self, short_window, long_window):
        self.short_window = short_window
        self.long_window = long_window
        self.report = Report()
        self.trades = 0            # how many trades the last run() read (the benchmark needs it)

    def run(self, path, print_table=False, slice_from=0, slice_to=0,
            dat_path=None, signals_to_show=10):
        """
        Process one whole trade file. Same arguments and return value as
        StageAEngine.run() in engine/stage_a.py.
        """

        # step 1 - check the windows
        if self.short_window < 1 or self.short_window >= self.long_window:
            print(f"bad windows: need 1 <= short < long (got {self.short_window}, {self.long_window})")
            return False

        # step 2 - the whole file, in one go
        trades = load_trades(path, with_side=print_table)
        if trades is None:
            return False

        # step 3 - every calculation, each one over all trades at once
        vwap = running_vwap(trades.price, trades.qty)
        bars = one_second_bars(trades.time_us, trades.price)
        short_sma = naive_sma(trades.price, self.short_window)
        long_sma = naive_sma(trades.price, self.long_window)
        signal = crossovers(short_sma, long_sma, self.long_window)

        # step 4 - output
        if print_table:
            self.print_table(trades, vwap, short_sma, long_sma, signal, bars)
        else:
            self.print_first_signals(signal, signals_to_show)

        if dat_path is not None:
            if not self.write_dat(dat_path, trades.price, short_sma, long_sma, signal,
                                  slice_from, slice_to):
                return False

        # step 5 - summary
        buys = int(np.sum(signal == SIG_BUY))
        sells = int(np.sum(signal == SIG_SELL))
        self.report.summary(float(vwap[-1]), buys, sells)
        self.trades = trades.count
        return True

    def print_table(self, trades, vwap, short_sma, long_sma, signal, bars):
        """test20 style: one row per trade, then the OHLC bars."""
        self.report.table_header()
        for i in range(trades.count):
            have_short = i >= self.short_window - 1      # i counts from 0, trades from 1
            have_long = i >= self.long_window - 1
            self.report.trade_row(i + 1, trades.price[i], trades.qty[i], trades.is_buy[i],
                                  vwap[i], have_short, short_sma[i],
                                  have_long, long_sma[i], signal[i])
        self.report.bar_header()
        for bar in bars:
            self.report.bar(bar)

    def print_first_signals(self, signal, signals_to_show):
        """Summary mode: only the first few signals, like the pure version."""
        shown = 0
        for i in np.flatnonzero(signal != SIG_NONE):     # the trades that have a signal
            if shown == signals_to_show:
                break
            self.report.signal_line(signal[i], i + 1)
            shown += 1

    def write_dat(self, dat_path, price, short_sma, long_sma, signal, slice_from, slice_to):
        """signals.dat: the trades in the slice that have a long SMA (about 2000 rows)."""
        dat = DatFile(dat_path)
        if not dat.open():
            return False
        first = max(slice_from, self.long_window)        # trade numbers, counting from 1
        last = min(slice_to, len(price))
        for trade_no in range(first, last + 1):
            i = trade_no - 1
            dat.write_row(trade_no, price[i], short_sma[i], long_sma[i], signal[i])
        dat.close()
        return True
