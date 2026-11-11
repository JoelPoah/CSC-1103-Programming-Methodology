/*
 * config.h - the group's settings, shared by every version on the ladder
 *
 * Change a path or a window here and every version picks it up.
 * All paths are relative to the project root, so always run the programs
 * from the project root (e.g. ./bin/v0_stageA_naive_sma test20).
 */
#ifndef CONFIG_H
#define CONFIG_H

/* ---- the three data files (CSV files are git-ignored) ---- */
#define TEST20_PATH "data/group_data/test20.csv"
#define DEV_PATH    "data/group_data/dev_1M.csv"
#define STRESS_PATH "data/stress_full.csv"

/* ---- SMA windows ---- */
#define TEST_SHORT  3          /* test20.csv: the same for every group */
#define TEST_LONG   5
#define GROUP_SHORT 35         /* Group 06: dev_1M.csv and stress_full.csv */
#define GROUP_LONG  60

/* ---- Stage A plot: slice of dev_1M.csv written to signals.dat ---- */
#define SIGNALS_DAT "results/signals.dat"
#define PLOT_SCRIPT "tools/plot_signals.gnuplot"
#define SLICE_START 1          /* first trade number in the slice */
#define SLICE_LEN   2000

#endif /* CONFIG_H */
