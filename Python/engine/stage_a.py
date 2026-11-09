"""
stage_a.py - the Stage A main loop (Python version of run() in stageA/stageA_manual.c)

One pass over a trade file, one trade at a time:
    a) read the trade
    b) update VWAP, the OHLC bar, both naive SMAs and the crossover signal
    c) print / write whatever this run needs
"""

from engine.trade_reader import TradeReader
from engine.vwap import VWAP
from engine.ohlc import OHLC
from engine.sma import NaiveSMA
from engine.crossover import Crossover, SIG_NONE
from engine.report import Report, DatFile


class StageAEngine:

    def __init__(self, short_window, long_window):
        self.short_window = short_window
        self.long_window = long_window
        self.report = Report()
        self.trades = 0            # how many trades the last run() read (the benchmark needs it)

    def run(self, path, print_table=False, slice_from=0, slice_to=0,
            dat_path=None, signals_to_show=10):
        """
        Process one whole trade file.
            print_table      True = one row per trade (test20 style), False = summary only
            slice_from/to    trades in this range go into dat_path (for the plot)
            dat_path         where signals.dat goes, None = do not write one
            signals_to_show  summary mode only prints the first few signals
        Returns True if it worked, False if something went wrong.
        """

        # step 1 - check the windows
        if self.short_window < 1 or self.short_window >= self.long_window:
            print(f"bad windows: need 1 <= short < long (got {self.short_window}, {self.long_window})")
            return False

        # step 2 - open the trade file
        reader = TradeReader(path)
        if not reader.open():
            return False

        # step 3 - open signals.dat, only if we asked for one
        dat = None
        if dat_path is not None:
            dat = DatFile(dat_path)
            if not dat.open():
                reader.close()
                return False

        # step 4 - fresh calculators for this file
        vwap = VWAP()
        ohlc = OHLC()
        short_sma = NaiveSMA(self.short_window)
        long_sma = NaiveSMA(self.long_window)
        crossover = Crossover()
        signals_shown = 0

        # step 5 - table header (test20 only)
        if print_table:
            self.report.table_header()

        # step 6 - the main loop, one trade at a time
        while reader.next_trade():
            price = reader.price
            signal = SIG_NONE

            # 6a) VWAP
            vwap.update(price, reader.qty)

            # 6b) OHLC - if this trade started a new second, print the bar it just closed
            if ohlc.update(reader.time_us, price) and print_table:
                self.report.bar(ohlc.finished)

            # 6c) SMA - push into both windows, each one re-adds its whole window
            short_sma.push(price)
            long_sma.push(price)
            have_short = short_sma.ready()
            have_long = long_sma.ready()
            short_value = 0.0
            long_value = 0.0
            if have_short:
                short_value = short_sma.value()
            if have_long:
                long_value = long_sma.value()

            # 6d) crossover, only once both SMAs exist
            if have_short and have_long:
                signal = crossover.update(short_value, long_value)

            # 6e) output
            if print_table:
                self.report.trade_row(reader.number, price, reader.qty, reader.is_buy,
                                      vwap.value(), have_short, short_value,
                                      have_long, long_value, signal)
            elif signal != SIG_NONE and signals_shown < signals_to_show:
                self.report.signal_line(signal, reader.number)
                signals_shown += 1

            # 6f) plot slice (only once the long SMA exists)
            in_slice = reader.number >= slice_from and reader.number <= slice_to
            if dat is not None and have_long and in_slice:
                dat.write_row(reader.number, price, short_value, long_value, signal)

        # step 7 - after the loop: the last bar is still open, finish it
        if ohlc.flush() and print_table:
            self.report.bar_header()
            self.report.bar(ohlc.finished)
        if dat is not None:
            dat.close()
        reader.close()

        # step 8 - summary
        self.report.summary(vwap.value(), crossover.buys, crossover.sells)
        self.trades = reader.number
        return True
