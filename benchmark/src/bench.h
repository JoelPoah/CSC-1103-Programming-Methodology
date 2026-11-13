/*
 * bench.h - shared types and functions of the efficiency-ladder benchmark
 *
 * One .c file per job (read them in this order):
 *   main.c      options, which machine this is, which parts to run
 *   versions.c  WHAT is measured: programs, flags, assembly swaps, Python scripts
 *   run.c       compile, run and time ONE version; read its output
 *   stats.c     median, mean, mode, min, max, standard deviation
 *   ladder.c    the C ladder (+ the assembly builds on the Pi)
 *   python.c    the two Python versions vs C
 *   compare.c   laptop vs Pi, fastest C vs fastest Python
 *   report.c    per-version logs, summary.txt, results .csv
 *   charts.c    writes chart data; gnuplot draws it with benchmark/charts/
 *   device.c    the machine's specs (CPU, RAM, OS, compiler...) -> device.txt
 *   final_report.c  both machines' results + specs -> benchmark/results/REPORT.md
 */
#ifndef BENCH_H
#define BENCH_H

#include <stdio.h>

/* every path and label here is short, and snprintf cuts safely if one were
 * not, so gcc's "output may be truncated" guesses are switched off */
#if defined(__GNUC__) && __GNUC__ >= 7
#pragma GCC diagnostic ignored "-Wformat-truncation"
#endif

#define MAX_RUNS        20
#define MAX_VERSIONS    48
#define PATH_LEN        512
#define CMD_LEN         4096
#define RESULTS_DIR     "benchmark/results"
#define TEST20_FILE     "data/group_data/test20.csv"
#define TEST20_EXPECTED "data/group_data/test20_expected.txt"

#if defined(__aarch64__)
#define IS_PI 1                        /* 64-bit ARM: the Raspberry Pi 5 */
#else
#define IS_PI 0
#endif

/* ---- versions.c ---- */
typedef struct {
    const char *name, *rung;           /* rung = what this version changes */
    const char *srcs;                  /* all sources except the SMA / OHLC engine files */
    const char *sma_c, *ohlc_c;        /* C engine files (NULL: already in srcs) */
    const char *sma_s, *ohlc_s;        /* assembly replacements (NULL: no assembly builds) */
    const char *args;                  /* arguments that make it process the timed dataset */
    const char *test_args;             /* test20-only arguments (NULL: test20 is the first
                                          section of every normal run) */
    int own_timer;                     /* 1: time = the program's own Load + Process lines */
} Program;
typedef struct { const char *tag, *cflags; int pi_only; } Flag;
typedef struct { const char *tag, *label, *label_fx; int sma, ohlc; } Variant;
typedef struct { const char *name, *script, *about; } PyScript;

extern const Program  PROGRAMS[];
extern const Flag     FLAGS[];
extern const Variant  VARIANTS[];      /* [0] plain C, [1] sma, [2] ohlc, [3] both */
extern const PyScript PY_SCRIPTS[];
extern const int N_PROGRAMS, N_FLAGS, N_VARIANTS, N_PY;
extern const char *DATA_FILE;          /* the timed dataset: stress_full.csv */
extern const char *DATA_KEY;           /* its "=== ... ===" section name in the output */
#define BASE_PROG 0                    /* stageA at ...                        */
#define BASE_FLAG 2                    /* ... -O2 (FLAGS[2]) = Version 0, the baseline */
#define ASM_FLAG  2                    /* assembly builds use -O2, as assembly/compare_all.sh */

/* ---- stats.c ---- */
typedef struct {
    int n;
    double median, mean, min, max, sd;
    double mode; int mode_count;       /* mode_count < 2: no value repeats */
    double bin;                        /* mode resolution in ms */
} Stats;
Stats stats_of(const double *v, int n, double bin);
void  fmt_ms(double ms, char *out, size_t size);
void  fmt_mode(const Stats *s, char *out, size_t size);

/* ---- one measured version (also one row of a results .csv) ---- */
typedef struct {
    char id[64], label[96], section[8];   /* section: "ladder", "asm" or "python" */
    int prog, flag, variant;              /* python: prog = index in PY_SCRIPTS */
    char exe[PATH_LEN], build_cmd[CMD_LEN], run_cmd[CMD_LEN];
    int ok; char error[200];
    int n;                                /* number of timed runs */
    double time[MAX_RUNS];                /* ms: what every comparison uses */
    double load[MAX_RUNS], proc[MAX_RUNS], whole[MAX_RUNS];
    int split;                            /* 1: load / proc are known */
    double total_ms;                      /* python: the whole bench process */
    Stats st, st_load, st_proc;
    long long trades; char vwap[32]; int buys, sells;
    char test20[16];
    double speedup;                       /* stageA -O2 median / this median, 0 = unknown */
    double peak_mb;                       /* peak memory of the warm-up run (0 = unknown) */
    int has_perf;                         /* 1: the perf stat numbers below are known (Pi) */
    double cycles, instructions, cache_refs, cache_misses;
} Result;
extern Result results[MAX_VERSIONS];
extern int n_results;

/* ---- main.c ---- */
extern char MEMBER[32];                   /* whose results these are (-m NAME) */
extern char ROOT[PATH_LEN];               /* benchmark/results/<member> */
extern char MACHINE[16];                  /* "laptop", "rpi" or "linux" */
extern char PIN[32];                      /* "taskset -c 2 " on Linux, "" on Windows */
extern char PYTHON[128];                  /* command that runs Python */
extern int  HAVE_PERF;                    /* 1: perf is installed (the Pi) */

/* ---- run.c ---- */
void mkdir_p(const char *path);
int  file_exists(const char *path);
void native_path(const char *in, char *out, size_t size);
int  run_cmd(const char *cmd, const char *out_file, double *ms);
int  run_cmd_memory(const char *exe, const char *args, const char *out_file, double *peak_mb);
void first_line(const char *cmd, char *out, size_t size);
int  time_version(Result *r, int runs, const char *raw_dir);

/* ---- ladder.c, python.c, compare.c, final_report.c ---- */
void run_ladder(int runs);
void run_python(void);
void run_compare(void);
void run_final_report(void);

/* ---- device.c ---- */
void save_device_info(void);              /* writes results/<machine>/device.txt */
void machine_info(FILE *f);               /* copies it into a summary */

/* ---- report.c ---- */
void write_version_log(const Result *r, const char *dir, double base_ms);
void write_summary(const char *path, int runs);
void write_csv(const char *path, int python);
int  read_csv(const char *path, Result *rows, int max);
const Result *find_id(const Result *rows, int n, const char *id);
const Result *fastest(const Result *rows, int n);

/* ---- charts.c ---- */
extern const unsigned COLOURS[8];
typedef struct { char label[96]; double value; unsigned colour; char text[32]; } Bar;
void chart_bars(const char *dir, const char *name, const char *title, const char *ylabel,
                const Bar *bars, int n, int logy, double ref);
void chart_groups(const char *dir, const char *name, const char *title, const char *sub,
                  const char *ylabel, char rows[][96], int nrows, char cols[][48], int ncols,
                  const double *vals, int logy, int speed_fmt, double ref);

#endif
