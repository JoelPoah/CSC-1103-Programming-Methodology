/*
 * versions.c - WHAT the benchmark measures
 *
 * Every rung of the efficiency ladder is one Program below: the SAME programs
 * that "make ladder" builds into bin/ from src/ (see the Makefile). The name
 * and the source files of each are copied from its Makefile rule, so the -O2
 * build here is the build "make" gives. The benchmark compiles each one again
 * with every Flag; the programs themselves are never changed.
 *
 * If a ladder rule in the Makefile changes, change the matching entry here.
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

/* the Makefile's SRC_ENGINE, SRC_FX_CORE and SRC_FX */
#define SRC_ENGINE  "src/engine/vwap.c src/engine/ohlc.c src/engine/signal.c src/engine/report.c"
#define SRC_FX_CORE "src/engine_fx/vwap_fx.c src/engine_fx/sma_fx.c src/engine_fx/signal_fx.c src/engine/report.c"
#define SRC_FX      SRC_FX_CORE " src/engine_fx/v5-v7_ohlc_fx.c"

/* V0-V2 take the dataset as an argument and only process that one, so they are
 * timed from outside. V3-V8 always run test20, dev_1M and stress_full in one
 * go, so they are timed by their own now_ns() Load + Process lines. */
#define STREAM_V0  DS("stress", "dev"),     "test20", 0
#define STREAM_V12 DS("--stress", "--dev"), "--test", 0
#define IN_MEMORY  DS("", "dev"),           NULL,     1

const Program PROGRAMS[] = {
    { "v0_stageA_naive_sma",
      "Version 0: helper library (fgets + sscanf), double prices, naive SMA, streams the file",
      "src/v0_main_stageA.c " SRC_ENGINE " src/engine/v0-v3_sma_naive.c helper_function/trade_io.c",
      STREAM_V0, 0 },

    { "v1_stageAplus_own_reader",
      "own read_trade() through pointers, still fgets + sscanf",
      "src/v1-v2_main_stageAplus.c src/reader/v1_read_trade_sscanf.c " SRC_ENGINE " src/engine/v0-v3_sma_naive.c",
      STREAM_V12, 0 },

    { "v2_stageAplus_fast_parser",
      "digit-by-digit parser instead of sscanf",
      "src/v1-v2_main_stageAplus.c src/reader/v2-v4_read_trade_fast.c " SRC_ENGINE " src/engine/v0-v3_sma_naive.c",
      STREAM_V12, 0 },

    { "v3_stageB_in_memory",
      "whole file loaded into a global array (fast parser), then processed from memory",
      "src/v3-v4_main_stageB_inmem.c src/store/v3-v4_trades.c src/reader/v2-v4_read_trade_fast.c "
      SRC_ENGINE " src/engine/v0-v3_sma_naive.c",
      IN_MEMORY, 0 },

    { "v4_stageB_running_sma",
      "running-sum SMA instead of the naive SMA",
      "src/v3-v4_main_stageB_inmem.c src/store/v3-v4_trades.c src/reader/v2-v4_read_trade_fast.c "
      SRC_ENGINE " src/engine/v4_sma_running.c",
      IN_MEMORY, 0 },

    { "v5_stageB_fixed_point",
      "fixed-point long long prices, parsed straight to fixed-point; fixed-size global array",
      "src/v5-v8_main_stageB_fixed.c src/store/v5_trades_fx_array.c src/reader/v5-v8_read_trade_fx.c " SRC_FX,
      IN_MEMORY, 0 },

    { "v6_stageC_malloc",
      "array malloc'd to fit the file (file read twice: count, then fill)",
      "src/v5-v8_main_stageB_fixed.c src/store/v6_trades_fx_malloc.c src/reader/v5-v8_read_trade_fx.c " SRC_FX,
      IN_MEMORY, 0 },

    { "v7_stageC_fread",
      "whole file read with one fread, then parsed in memory",
      "src/v5-v8_main_stageB_fixed.c src/store/v7-v8_trades_fx_fread.c src/reader/v5-v8_read_trade_fx.c " SRC_FX,
      IN_MEMORY, 0 },

    { "v8_stageC_asm_ohlc",
      "V7 with the hand-written AArch64 OHLC in place of the C one (Raspberry Pi only)",
      "src/v5-v8_main_stageB_fixed.c src/store/v7-v8_trades_fx_fread.c src/reader/v5-v8_read_trade_fx.c "
      SRC_FX_CORE " src/engine_asm/v8_ohlc_fx.s",
      IN_MEMORY, 1 },
};

const Flag FLAGS[] = {
    { "O0",     "-O0",                  0 },
    { "O1",     "-O1",                  0 },
    { "O2",     "-O2",                  0 },     /* what "make ladder" uses (CFLAGS_L) */
    { "O3",     "-O3",                  0 },
    { "O3_a76", "-O3 -mcpu=cortex-a76", 1 },     /* Cortex-A76 = the Pi 5's CPU */
};

const PyScript PY_SCRIPTS[] = {
    { "stageA_Python",       "Python/stageA_Python.py",
      "pure Python: the loop over trades runs in the interpreter, naive SMA" },
    { "stageA_Python_numpy", "Python/stageA_Python_numpy.py",
      "NumPy: whole file into arrays, loops run in NumPy's compiled code, naive SMA" },
};

const int N_PROGRAMS = sizeof PROGRAMS / sizeof PROGRAMS[0];
const int N_FLAGS    = sizeof FLAGS / sizeof FLAGS[0];
const int N_PY       = sizeof PY_SCRIPTS / sizeof PY_SCRIPTS[0];
