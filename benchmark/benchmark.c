/*
 * benchmark.c - compile a program at -O0, -O1, -O2 and -O3, time each build
 *               several times with now_ns(), and write a report + chart
 *
 * Build (from the project root):
 *   gcc -O2 -Wall benchmark/benchmark.c -o benchmark/benchmark
 *
 * Run (from the project root, so the programs find their data files):
 *   ./benchmark/benchmark [-n RUNS] [-o OUTDIR] [-t "CHART TITLE"] [-a "PROGRAM ARGS"] [-i]
 *                         LABEL=file.c[,more.c ...] [LABEL2=...]
 *
 *   -i  use the program's own "Time taken: N ns" line instead of whole-program
 *       time (for tiny programs like loop_sum, where start-up hides the work)
 *
 *   e.g.  ./benchmark/benchmark -i -n 5 -t "Loop vs Recursion"
 *             loop_sum=stage0/question_b/loop_sum.c
 *             recursion_sum=stage0/question_b/recursion_sum.c
 *
 *         ./benchmark/benchmark -n 3 -t "Stage A vs Stage B"
 *             stageA=stageA/stageA_manual.c,stageA/engine/vwap.c,stageA/engine/ohlc.c,stageA/engine/sma.c,stageA/engine/signal.c,stageA/engine/report.c,helper_function/trade_io.c
 *             stageB=stageB/stageB_manual.c,stageB/engine/vwap.c,stageB/engine/ohlc.c,stageB/engine/sma.c,stageB/engine/signal.c,stageB/engine/report.c,helper_function/trade_io.c
 *
 * Output (default OUTDIR = benchmark/results):
 *   report.txt     every build: gcc command, each run's time, average
 *                  (the program's own output is thrown away)
 *   results.dat    average ns per program per level (input for gnuplot)
 *   chart.gnuplot  gnuplot script, log-scale grouped bar chart
 *   chart.png      the chart (needs gnuplot on PATH)
 *
 * What is timed (without -i): the whole program, from launch to exit (now_ns()
 * around system()). This includes process start-up, so tiny programs will all
 * show the same tens of milliseconds. One warm-up run per build is not counted.
 */
#include "../helper_function/timing.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#ifdef _WIN32
  #include <direct.h>
  #define MAKE_DIR(p)  _mkdir(p)
  #define EXIT_CODE(s) (s)                /* system() already returns the exit code */
  #define EXE_EXT      ".exe"
  #define NULL_DEV     "NUL"
#else
  #include <sys/stat.h>
  #include <sys/wait.h>
  #define MAKE_DIR(p)  mkdir(p, 0755)
  #define EXIT_CODE(s) (WIFEXITED(s) ? WEXITSTATUS(s) : -1)
  #define EXE_EXT      ""
  #define NULL_DEV     "/dev/null"
#endif

#if defined(_WIN32)
  #define PLATFORM "Windows"
#elif defined(__APPLE__)
  #define PLATFORM "macOS"
#elif defined(__arm__) || defined(__aarch64__)
  #define PLATFORM "Raspberry Pi (Linux ARM)"
#else
  #define PLATFORM "Linux"
#endif

#define MAX_PROGS      8
#define MAX_SRCS       16
#define MAX_RUNS       100
#define NUM_LEVELS     4
#define PATH_LEN       512
#define CMD_LEN        4096
#define DEFAULT_RUNS   5
#define DEFAULT_OUTDIR "benchmark/results"
#define DEFAULT_TITLE  "Execution Time across GCC Optimisation Levels"

static const char *LEVELS[NUM_LEVELS] = { "O0", "O1", "O2", "O3" };

typedef struct {
    char     label[64];
    char     srcs[MAX_SRCS][PATH_LEN];
    int      n_srcs;
    uint64_t times[NUM_LEVELS][MAX_RUNS];
    uint64_t avg[NUM_LEVELS];
    int      ok[NUM_LEVELS];          /* 1 if it compiled and ran OK at this level */
} Program;

static Program progs[MAX_PROGS];
static int     n_progs = 0;

/* ---------- small helpers ---------- */

/* 1234567 -> "1,234,567" */
static void fmt_ns(uint64_t v, char *out) {
    char raw[32];
    int len, i, j = 0;
    snprintf(raw, sizeof raw, "%" PRIu64, v);
    len = (int)strlen(raw);
    for (i = 0; i < len; i++) {
        if (i > 0 && (len - i) % 3 == 0) out[j++] = ',';
        out[j++] = raw[i];
    }
    out[j] = '\0';
}

/* cmd.exe does not accept / in the program path, so swap to \ on Windows */
static void native_path(const char *in, char *out) {
    strcpy(out, in);
#ifdef _WIN32
    for (char *p = out; *p; p++) {
        if (*p == '/') *p = '\\';
    }
#endif
}

/* "label=a.c,b.c" -> fills one Program; returns 1 if OK */
static int parse_program(const char *spec) {
    const char *eq = strchr(spec, '=');
    Program *p;
    char list[CMD_LEN];
    char *tok;

    if (n_progs >= MAX_PROGS) {
        printf("too many programs (max %d)\n", MAX_PROGS);
        return 0;
    }
    if (eq == NULL || eq == spec || eq[1] == '\0') {
        printf("bad program \"%s\": use LABEL=file.c[,more.c]\n", spec);
        return 0;
    }
    p = &progs[n_progs];
    memset(p, 0, sizeof *p);
    snprintf(p->label, sizeof p->label, "%.*s", (int)(eq - spec), spec);

    snprintf(list, sizeof list, "%s", eq + 1);
    for (tok = strtok(list, ","); tok != NULL; tok = strtok(NULL, ",")) {
        if (p->n_srcs >= MAX_SRCS) {
            printf("too many source files for %s (max %d)\n", p->label, MAX_SRCS);
            return 0;
        }
        snprintf(p->srcs[p->n_srcs++], PATH_LEN, "%s", tok);
    }
    n_progs++;
    return 1;
}

/* last "Time taken: N" printed by the program; returns 1 if found */
static int read_internal_time(const char *path, uint64_t *ns) {
    FILE *f = fopen(path, "r");
    char line[1024];
    unsigned long long v;
    int found = 0;

    if (f == NULL) return 0;
    while (fgets(line, sizeof line, f) != NULL) {
        char *t = strstr(line, "Time taken:");
        if (t != NULL && sscanf(t, "Time taken: %llu", &v) == 1) {
            *ns = (uint64_t)v;
            found = 1;
        }
    }
    fclose(f);
    return found;
}

/* ---------- build and time ---------- */

/* gcc -Ox -Wall srcs -o exe; returns 1 if it compiled */
static int build(const Program *p, int lvl, const char *exe, char *gcc_cmd) {
    int i;
    size_t n = (size_t)snprintf(gcc_cmd, CMD_LEN, "gcc -%s -Wall", LEVELS[lvl]);
    for (i = 0; i < p->n_srcs; i++) {
        n += (size_t)snprintf(gcc_cmd + n, CMD_LEN - n, " \"%s\"", p->srcs[i]);
    }
    snprintf(gcc_cmd + n, CMD_LEN - n, " -o \"%s\" -lm", exe);

    printf("  %s\n", gcc_cmd);
    return system(gcc_cmd) == 0;
}

/* run exe (1 warm-up + runs timed). Its output is thrown away, except with
 * internal = 1, where it goes to out_path so the "Time taken: N ns" line can be read.
 * Returns the warm-up exit code; nothing is timed if it is not 0 */
static int time_runs(Program *p, int lvl, const char *exe, const char *args,
                     const char *out_path, int runs, int internal) {
    char cmd[CMD_LEN], exe_native[PATH_LEN], out_native[PATH_LEN];
    uint64_t sum = 0, start, end, t;
    int r, code, warned = 0;

    native_path(exe, exe_native);
    native_path(out_path, out_native);
    snprintf(cmd, sizeof cmd, "%s %s < %s > \"%s\" 2> %s", exe_native, args, NULL_DEV,
             internal ? out_native : NULL_DEV, NULL_DEV);

    code = EXIT_CODE(system(cmd));            /* warm-up: loads exe + data into cache */
    if (code != 0) return code;

    for (r = 0; r < runs; r++) {
        start = now_ns();
        system(cmd);
        end = now_ns();
        t = end - start;
        if (internal && !read_internal_time(out_path, &t) && !warned) {
            printf("    warning: no \"Time taken:\" line, using whole-program time\n");
            warned = 1;
        }
        p->times[lvl][r] = t;
        sum += t;
    }
    p->avg[lvl] = sum / (uint64_t)runs;
    return 0;
}

/* ---------- report and chart ---------- */

/* "Time taken: a ns, b ns, ... // average: X ns (Y ms)" for one level */
static void write_times(FILE *report, const Program *p, int lvl, int runs) {
    char buf[32];
    int r;

    fprintf(report, "Time taken: ");
    for (r = 0; r < runs; r++) {
        fmt_ns(p->times[lvl][r], buf);
        fprintf(report, "%s ns%s", buf, r < runs - 1 ? ", " : "");
    }
    fmt_ns(p->avg[lvl], buf);
    fprintf(report, " // average: %s ns (%.2f ms)\n\n", buf, p->avg[lvl] / 1e6);
    printf("    average: %s ns (%.2f ms)\n", buf, p->avg[lvl] / 1e6);
}

/* table of averages with the speed-up against -O0 */
static void write_summary(FILE *report) {
    char buf[32], cell[48];
    int i, lvl;

    fprintf(report, "\nSUMMARY (average ns, speed-up vs -O0 in brackets):\n");
    fprintf(report, "%-20s", "program");
    for (lvl = 0; lvl < NUM_LEVELS; lvl++) fprintf(report, "  %-22s", LEVELS[lvl]);
    fprintf(report, "\n");

    for (i = 0; i < n_progs; i++) {
        const Program *p = &progs[i];
        fprintf(report, "%-20s", p->label);
        for (lvl = 0; lvl < NUM_LEVELS; lvl++) {
            fmt_ns(p->avg[lvl], buf);
            if (!p->ok[lvl]) {
                snprintf(cell, sizeof cell, "-");
            } else if (p->ok[0] && p->avg[lvl] > 0) {
                snprintf(cell, sizeof cell, "%s (%.2fx)", buf, (double)p->avg[0] / (double)p->avg[lvl]);
            } else {
                snprintf(cell, sizeof cell, "%s", buf);
            }
            fprintf(report, "  %-22s", cell);
        }
        fprintf(report, "\n");
    }
    fprintf(report, "\nFINDINGS:\n1. \n2. \n3. \n");
}

/* 1 if the program was timed at one level or more (gnuplot rejects an all-NaN column) */
static int any_ok(const Program *p) {
    int lvl;
    for (lvl = 0; lvl < NUM_LEVELS; lvl++) {
        if (p->ok[lvl]) return 1;
    }
    return 0;
}

static void write_chart(const char *outdir, const char *title) {
    char dat_path[PATH_LEN], gp_path[PATH_LEN], cmd[CMD_LEN];
    uint64_t lo = UINT64_MAX, hi = 0;
    FILE *f;
    int i, lvl, cols = 0;

    /* results.dat: one row per level, one column per program that ran */
    snprintf(dat_path, sizeof dat_path, "%s/results.dat", outdir);
    f = fopen(dat_path, "w");
    if (f == NULL) { printf("cannot write %s\n", dat_path); return; }
    fprintf(f, "level");
    for (i = 0; i < n_progs; i++) {
        if (any_ok(&progs[i])) { fprintf(f, " \"%s\"", progs[i].label); cols++; }
    }
    fprintf(f, "\n");
    for (lvl = 0; lvl < NUM_LEVELS; lvl++) {
        fprintf(f, "-%s", LEVELS[lvl]);
        for (i = 0; i < n_progs; i++) {
            uint64_t avg = progs[i].avg[lvl];
            if (!any_ok(&progs[i])) continue;
            if (!progs[i].ok[lvl]) { fprintf(f, " NaN"); continue; }
            fprintf(f, " %" PRIu64, avg);
            if (avg > 0 && avg < lo) lo = avg;
            if (avg > hi) hi = avg;
        }
        fprintf(f, "\n");
    }
    fclose(f);
    if (hi == 0) { printf("nothing was timed, no chart\n"); return; }

    /* chart.gnuplot: grouped bars, log y axis, same look as question_c */
    snprintf(gp_path, sizeof gp_path, "%s/chart.gnuplot", outdir);
    f = fopen(gp_path, "w");
    if (f == NULL) { printf("cannot write %s\n", gp_path); return; }
    fprintf(f,
        "# Generated by benchmark.c. Run from the project root:  gnuplot %s\n"
        "set terminal pngcairo size 1500,900 font \"Arial,12\"\n"
        "set output \"%s/chart.png\"\n"
        "set title \"%s (%s)\" noenhanced font \",14\"\n"
        "set ylabel \"Execution Time (ns, log scale)\"\n"
        "set logscale y\n"
        "set format y \"10^{%%L}\"\n"
        "set yrange [%" PRIu64 ":%" PRIu64 "]\n"
        "set style data histogram\n"
        "set style histogram cluster gap 1\n"
        "set style fill solid 1.0 noborder\n"
        "set boxwidth 0.9\n"
        "set key top right box opaque\n"
        "set grid ytics\n"
        "colors = \"#0072B2 #FF7000 #2CA02C #D62728 #9467BD #8C564B #E377C2 #7F7F7F\"\n"
        "plot for [i=2:%d] \"%s\" using i:xtic(1) title columnheader(i) noenhanced lc rgb word(colors, i-1)\n",
        gp_path, outdir, title, PLATFORM, lo / 2 > 0 ? lo / 2 : 1, hi * 2, cols + 1, dat_path);
    fclose(f);

    snprintf(cmd, sizeof cmd, "gnuplot \"%s\"", gp_path);
    printf("\nRunning %s ...\n", cmd);
    if (system(cmd) != 0) {
        printf("gnuplot failed or is not installed; run the command above once it is.\n");
    } else {
        printf("chart written to %s/chart.png\n", outdir);
    }
}

int main(int argc, char **argv) {
    int runs = DEFAULT_RUNS;
    const char *outdir = DEFAULT_OUTDIR;
    const char *title  = DEFAULT_TITLE;
    const char *args   = "";
    int internal = 0;
    char report_path[PATH_LEN], exe[PATH_LEN], out_path[PATH_LEN], gcc_cmd[CMD_LEN];
    FILE *report;
    int i, lvl, code;

    setvbuf(stdout, NULL, _IONBF, 0);         /* keep our lines in order with gcc's output */

    /* STEP 1 - read the options and programs */
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-i") == 0)                      internal = 1;
        else if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) runs   = atoi(argv[++i]);
        else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) outdir = argv[++i];
        else if (strcmp(argv[i], "-t") == 0 && i + 1 < argc) title  = argv[++i];
        else if (strcmp(argv[i], "-a") == 0 && i + 1 < argc) args   = argv[++i];
        else if (!parse_program(argv[i])) return 1;
    }
    if (n_progs == 0) {
        printf("usage: %s [-n RUNS] [-o OUTDIR] [-t TITLE] [-a ARGS] [-i] LABEL=file.c[,more.c] ...\n", argv[0]);
        return 1;
    }
    if (runs < 1 || runs > MAX_RUNS) {
        printf("runs must be 1..%d\n", MAX_RUNS);
        return 1;
    }
    if (strlen(outdir) > 300) {
        printf("output folder name too long\n");
        return 1;
    }

    /* STEP 2 - open the report */
    MAKE_DIR(outdir);                         /* fails harmlessly if it already exists */
    snprintf(report_path, sizeof report_path, "%s/report.txt", outdir);
    snprintf(out_path, sizeof out_path, "%s/last_output.txt", outdir);
    report = fopen(report_path, "w");
    if (report == NULL) {
        printf("cannot write %s (does the parent folder exist?)\n", report_path);
        return 1;
    }
    fprintf(report, "%s:\nRuns per build: %d (plus 1 warm-up run, not counted)\n", PLATFORM, runs);
    fprintf(report, "Timing: %s\n\n", internal
            ? "the program's own \"Time taken:\" line (now_ns() inside the program)"
            : "whole program from launch to exit, includes process start-up");

    /* STEP 3 - build and time every program at every level */
    for (i = 0; i < n_progs; i++) {
        Program *p = &progs[i];
        for (lvl = 0; lvl < NUM_LEVELS; lvl++) {
            snprintf(exe, sizeof exe, "%.300s/%.63s_%s%s", outdir, p->label, LEVELS[lvl], EXE_EXT);
            printf("[%s -%s]\n", p->label, LEVELS[lvl]);

            p->ok[lvl] = build(p, lvl, exe, gcc_cmd);
            fprintf(report, "[%s]%s\n", gcc_cmd, lvl == 0 ? " //default" : "");
            if (!p->ok[lvl]) {
                fprintf(report, "compile failed\n\n");
                continue;
            }

            code = time_runs(p, lvl, exe, args, out_path, runs, internal);
            if (code != 0) {
                p->ok[lvl] = 0;
                printf("    FAILED: program exited with status %d, not timed\n", code);
                fprintf(report, "program failed (exit status %d), not timed\n\n", code);
                continue;
            }
            write_times(report, p, lvl, runs);
        }
    }

    /* STEP 4 - summary table, then the chart */
    write_summary(report);
    fclose(report);
    printf("\nreport written to %s\n", report_path);
    write_chart(outdir, title);
    return 0;
}
