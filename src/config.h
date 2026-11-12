/*
 * config.h - file paths and SMA windows, one place for every version
 *
 * Every main_*.c in src/ includes this, so all versions of the ladder read
 * the same files with the same windows. Paths are relative to the project
 * root, so run the programs from there (e.g. ./bin/v0_stageA_naive_sma dev).
 */
#ifndef CONFIG_H
#define CONFIG_H

/* ---- file paths (relative to the project root) ---- */
#define TEST20_PATH "data/group_data/test20.csv"
#define DEV_PATH    "data/group_data/dev_1M.csv"
#define STRESS_PATH "data/group_data/stress_full.csv"

/* ---- SMA windows ---- */
#define TEST_SHORT 3           /* test20.csv: the same for every group      */
#define TEST_LONG  5
#define DEV_SHORT  35          /* Group 06: dev_1M.csv and stress_full.csv  */
#define DEV_LONG   60

#endif /* CONFIG_H */
