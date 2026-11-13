/*
 * versions.c - WHAT the benchmark measures
 *
 * Every rung of the efficiency ladder is one Program below, built with each
 * Flag. The stage programs are compiled and run exactly as they are in the
 * repo; only the files linked and the gcc flags change.
 */
#include "bench.h"

/* gcc -DLADDER_QUICK times dev_1M.csv instead of stress_full.csv.
 * Only for trying the tool out quickly - never for the report. */
#ifdef LADDER_QUICK
#define DS(stress, dev) dev
const char *DATA_FILE = "data/group_data/dev_1M.csv";
const char *DATA_KEY  = "dev_1M";
#else
#define DS(stress, dev) stress
const char *DATA_FILE = "data/group_data/stress_full.csv";
const char *DATA_KEY  = "stress_full";
#endif

const Program PROGRAMS[] = {
    { "stageA",
      "Version 0: helper library (fgets + sscanf), double prices, naive SMA, streams the file",
      "stageA/stageA_manual.c stageA/engine/vwap.c stageA/engine/ohlc.c stageA/engine/sma.c "
      "stageA/engine/signal.c stageA/engine/report.c helper_function/trade_io.c",
      NULL, NULL, NULL, NULL, DS("stress", "dev"), "test20", 0 },

    { "stageA_plus",
      "own pointer-based read_trade() + digit parser instead of sscanf, running-sum SMA",
      "stageA_plus/stageA_plus.c stageA_plus/fast_reader.c stageA_plus/engine/vwap.c "
      "stageA_plus/engine/ohlc.c stageA_plus/engine/sma.c stageA_plus/engine/signal.c "
      "stageA_plus/engine/report.c",
      NULL, NULL, NULL, NULL, DS("--stress", "--dev"), "--test", 0 },

    /* the Stage B/C programs always run test20, dev_1M and stress_full in one go,
     * so they are timed by their own now_ns() Load + Process lines */
    { "stageB_inmem",
      "whole file loaded into a static array (helper reader), processed from memory; double, running-sum SMA",
      "stageB/stageB_inmem.c stageB/engine/trades.c stageB/engine/vwap.c stageB/engine/signal.c "
      "stageB/engine/report.c helper_function/trade_io.c",
      "stageB/engine/sma.c", "stageB/engine/ohlc.c", "assembly/sma.s", "assembly/ohlc.s",
      DS("", "dev"), NULL, 1 },

    { "stageB_fixed",
      "static array + fixed-point long long prices (cents); crossover compares sums, no division",
      "stageB/stageB_fixed.c stageB/engine/trades_fx.c stageB/engine/vwap_fx.c "
      "stageB/engine/signal_fx.c stageB/engine/signal.c stageB/engine/report.c helper_function/trade_io.c",
      "stageB/engine/sma_fx.c", "stageB/engine/ohlc_fx.c", "assembly/sma_fx.s", "assembly/ohlc_fx.s",
      "", NULL, 1 },

    { "stageC_malloc",
      "as stageB_inmem, but the array is malloc'd to fit the file (file read twice: count, then fill)",
      "stageC/stageC_malloc.c stageC/engine/trades_malloc.c stageB/engine/vwap.c "
      "stageB/engine/signal.c stageB/engine/report.c helper_function/trade_io.c",
      "stageB/engine/sma.c", "stageB/engine/ohlc.c", "assembly/sma.s", "assembly/ohlc.s",
      DS("", "dev"), NULL, 1 },
};

const Flag FLAGS[] = {
    { "O0",     "-O0",                  0 },
    { "O1",     "-O1",                  0 },
    { "O2",     "-O2",                  0 },
    { "O3",     "-O3",                  0 },
    { "O3_a76", "-O3 -mcpu=cortex-a76", 1 },     /* Cortex-A76 = the Pi 5's CPU */
};

/* which engine files come from assembly/ instead of C (Pi only) */
const Variant VARIANTS[] = {
    { "c",    "C",              "C",                    0, 0 },
    { "sma",  "sma.s",          "sma_fx.s",             1, 0 },
    { "ohlc", "ohlc.s",         "ohlc_fx.s",            0, 1 },
    { "both", "sma.s + ohlc.s", "sma_fx.s + ohlc_fx.s", 1, 1 },
};

const PyScript PY_SCRIPTS[] = {
    { "stageA_Python",       "Python/stageA_Python.py",
      "pure Python: the loop over trades runs in the interpreter, naive SMA" },
    { "stageA_Python_numpy", "Python/stageA_Python_numpy.py",
      "NumPy: whole file into arrays, loops run in NumPy's compiled code, naive SMA" },
};

const int N_PROGRAMS = sizeof PROGRAMS / sizeof PROGRAMS[0];
const int N_FLAGS    = sizeof FLAGS / sizeof FLAGS[0];
const int N_VARIANTS = sizeof VARIANTS / sizeof VARIANTS[0];
const int N_PY       = sizeof PY_SCRIPTS / sizeof PY_SCRIPTS[0];
