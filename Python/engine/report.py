"""
report.py - all the printing and the signals.dat file (Python version of report.c)

The format strings are copied from report.c, so the output is the same as
the C Version 0. The 20 rows of the test20 table match
data/group_data/test20_expected.txt (the header names and the order of the
OHLC / summary part follow the C program, not that file).
If you change a column width here, check test20 again.
"""

from engine.crossover import SIG_BUY, SIG_SELL


class Report:

    def table_header(self):
        print("%-6s %-16s %-12s %-6s %-16s %-16s %-16s %s"
              % ("trade", "price", "qty", "is_buy", "VWAP", "SMA_short", "SMA_long", "signal"))

    def trade_row(self, trade_no, price, qty, is_buy, vwap,
                  have_short, sma_short, have_long, sma_long, signal):
        # one table row, an SMA that does not exist yet is printed as "-"
        line = "%-6d %-16.8f %-12.8f %-6d %-16.8f " % (trade_no, price, qty, is_buy, vwap)

        if have_short:
            line += "%-16.8f " % sma_short
        else:
            line += "%-16s " % "-"

        if have_long:
            line += "%-16.8f " % sma_long
        else:
            line += "%-16s " % "-"

        if signal == SIG_BUY:
            line += "BUY"
        elif signal == SIG_SELL:
            line += "SELL"
        print(line)

    def signal_line(self, signal, trade_no):
        # summary mode (big files): one short line per signal
        if signal == SIG_BUY:
            print("  signal BUY  at trade %d" % trade_no)
        elif signal == SIG_SELL:
            print("  signal SELL at trade %d" % trade_no)

    def bar_header(self):
        print("\nOne-second OHLC bars (second = time / 1,000,000)")
        print("%-12s %-16s %-16s %-16s %-16s %s"
              % ("second", "open", "high", "low", "close", "trades"))

    def bar(self, bar):
        print("%-12d %-16.8f %-16.8f %-16.8f %-16.8f %d"
              % (bar.second, bar.open_price, bar.high, bar.low, bar.close, bar.trades))

    def summary(self, vwap, buys, sells):
        print("\nFinal VWAP: %.8f" % vwap)
        print("Signals: %d BUY, %d SELL" % (buys, sells))


# signals.dat for the plot, one line per trade: trade price sma_short sma_long signal
class DatFile:

    def __init__(self, path):
        self.path = path
        self.file = None

    def open(self):
        # open for writing and put the header line in, returns True if it worked
        try:
            self.file = open(self.path, "w")
        except OSError:
            print("DatFile: cannot open", self.path)
            return False
        self.file.write("# trade price sma_short sma_long signal(+1 BUY, -1 SELL, 0 none)\n")
        return True

    def write_row(self, trade_no, price, sma_short, sma_long, signal):
        self.file.write("%d %.8f %.8f %.8f %d\n" % (trade_no, price, sma_short, sma_long, signal))

    def close(self):
        if self.file is not None:
            self.file.close()
            self.file = None
