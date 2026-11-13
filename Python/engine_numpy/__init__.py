# engine_numpy - the NumPy version of engine/, one file per C module in stageA/engine/
# Instead of one trade at a time, every function works on the WHOLE file at once:
# the trades are numpy arrays and each step is one array operation that runs in C.
# (trade_loader.py is the reader, stage_a.py ties the steps together.)
# The printing and signals.dat are shared with the pure version (engine/report.py).
