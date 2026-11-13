"""
settings.py - all the numbers and paths you might want to change

Paths are RELATIVE to the project root (the folder that has data/, stageA/,
Python/ ...). stageA_Python.py moves into the project root before it starts,
so they work no matter which folder you run it from.

Look for "EDIT:" - those are the lines meant to be changed.
"""

# ---- csv files ----
# EDIT: this is where the files sit on my laptop. The C programs read
#       data/group_data/test20.csv, data/group_data/dev_1M.csv and
#       data/group_data/stress_full.csv, so use those if your files are there.
TEST20_PATH = "data/test20.csv"
DEV_PATH = "data/dev_1M.csv"
STRESS_PATH = "data/stress_full.csv"

# ---- output files for the plot ----
SIGNALS_DAT = "Python/signals.dat"                 # stageA_Python.py (pure Python)
SIGNALS_PNG = "Python/signals.png"
SIGNALS_DAT_NUMPY = "Python/signals_numpy.dat"     # stageA_Python_numpy.py
SIGNALS_PNG_NUMPY = "Python/signals_numpy.png"

# ---- SMA windows ----
TEST_SHORT = 3       # test20.csv always uses 3 and 5 (same for every group)
TEST_LONG = 5
DEV_SHORT = 35       # EDIT: our group's windows (Group 06, see data/group_data/group_info.txt)
DEV_LONG = 60

# ---- slice of dev_1M.csv that goes into signals.dat / signals.png ----
SLICE_START = 1      # EDIT: first trade number of the slice
SLICE_LEN = 2000     # EDIT: about 2000 trades, it needs at least one BUY and one SELL

# dev_1M.csv has thousands of signals, so only the first few get printed
SIGNALS_TO_SHOW = 10   # EDIT

# ---- benchmark ("bench" mode) ----
BENCH_PATH = STRESS_PATH   # EDIT: DEV_PATH for a quick try, STRESS_PATH for the real numbers
BENCH_RUNS = 5             # EDIT: the brief asks for at least 5 runs, we print the median

# ---- how signals.png looks ----
PLOT_TITLE = "Stage A (Python): SMA crossover signals"
PLOT_TITLE_NUMPY = "Stage A (Python + NumPy): SMA crossover signals"
PLOT_WIDTH = 14            # EDIT: inches, times PLOT_DPI = pixels (14 x 100 = 1400 px like the gnuplot one)
PLOT_HEIGHT = 7
PLOT_DPI = 100
MARKER_SIZE = 80           # EDIT: size of the BUY / SELL triangles
LEGEND_PLACE = "best"      # EDIT: "best", "upper left", "lower right" ...

# same colours as stageA/plot_signals.gnuplot
PRICE_COLOUR = "#bbbbbb"
SHORT_COLOUR = "#1f77b4"
LONG_COLOUR = "#ff7f0e"
BUY_COLOUR = "#2ca02c"
SELL_COLOUR = "#d62728"
