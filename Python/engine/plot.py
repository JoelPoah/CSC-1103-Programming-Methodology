"""
plot.py - draws signals.png from signals.dat (same job as stageA/plot_signals.gnuplot)

Needs matplotlib:
    Windows:  py -m pip install matplotlib
    Mac:      python3 -m pip install matplotlib   (or inside a venv)
    Pi:       sudo apt install python3-matplotlib  (pip is blocked on Raspberry Pi OS)

The BUY / SELL markers are put on the short SMA line, so on the picture you
can check that every signal sits exactly where the two SMA lines cross.
How it looks (size, colours, legend ...) is set in settings.py.
"""

import matplotlib
matplotlib.use("Agg")              # draw straight into a png file, no window pops up
import matplotlib.pyplot as plt

import settings
from engine.crossover import SIG_BUY, SIG_SELL


class SignalPlot:

    def __init__(self, dat_path, png_path, title=None):
        self.dat_path = dat_path
        self.png_path = png_path
        self.title = title                # None = settings.PLOT_TITLE

        # one entry per line of signals.dat
        self.trade = []
        self.price = []
        self.sma_short = []
        self.sma_long = []
        self.signal = []

        self.buys = 0              # markers found in the slice
        self.sells = 0

    def read_dat(self):
        """Load the five columns of signals.dat into the lists above."""
        with open(self.dat_path, "r") as f:
            for line in f:
                if line.startswith("#") or line.strip() == "":
                    continue           # header or empty line
                parts = line.split()
                self.trade.append(int(parts[0]))
                self.price.append(float(parts[1]))
                self.sma_short.append(float(parts[2]))
                self.sma_long.append(float(parts[3]))
                self.signal.append(int(parts[4]))

    def draw(self):
        """Read signals.dat and save signals.png."""
        self.read_dat()

        # pick out the BUY and SELL points
        buy_x = []
        buy_y = []
        sell_x = []
        sell_y = []
        for i in range(len(self.trade)):
            if self.signal[i] == SIG_BUY:
                buy_x.append(self.trade[i])
                buy_y.append(self.sma_short[i])
            elif self.signal[i] == SIG_SELL:
                sell_x.append(self.trade[i])
                sell_y.append(self.sma_short[i])
        self.buys = len(buy_x)
        self.sells = len(sell_x)

        fig, ax = plt.subplots(figsize=(settings.PLOT_WIDTH, settings.PLOT_HEIGHT))
        ax.plot(self.trade, self.price, color=settings.PRICE_COLOUR, linewidth=0.8, label="price")
        ax.plot(self.trade, self.sma_short, color=settings.SHORT_COLOUR, linewidth=1.5, label="SMA short")
        ax.plot(self.trade, self.sma_long, color=settings.LONG_COLOUR, linewidth=1.5, label="SMA long")
        ax.scatter(buy_x, buy_y, marker="^", s=settings.MARKER_SIZE,
                   color=settings.BUY_COLOUR, zorder=5, label="BUY")
        ax.scatter(sell_x, sell_y, marker="v", s=settings.MARKER_SIZE,
                   color=settings.SELL_COLOUR, zorder=5, label="SELL")

        title = self.title
        if title is None:
            title = settings.PLOT_TITLE
        ax.set_title(title)
        ax.set_xlabel("trade number")
        ax.set_ylabel("price (USDT)")
        ax.ticklabel_format(axis="y", useOffset=False)    # show real prices, not "+7.6e4"
        ax.grid(alpha=0.3)
        ax.legend(loc=settings.LEGEND_PLACE)
        fig.tight_layout()
        fig.savefig(self.png_path, dpi=settings.PLOT_DPI)
        plt.close(fig)
