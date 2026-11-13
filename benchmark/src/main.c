/*
 * main.c - efficiency ladder benchmark (Group 06)
 *
 * Builds every version of the engine, times each on stress_full.csv with
 * now_ns() (helper_function/timing.h), and writes logs, tables and charts.
 * The stage programs are NOT changed. See benchmark/README.md.
 *
 * Build (from the project root):
 *   gcc -O2 -Wall benchmark/src/main.c benchmark/src/versions.c benchmark/src/run.c
 *       benchmark/src/stats.c benchmark/src/ladder.c benchmark/src/python.c
 *       benchmark/src/compare.c benchmark/src/report.c benchmark/src/charts.c
 *       benchmark/src/device.c benchmark/src/final_report.c -o benchmark/ladder_bench -lm
 * Run (from the project root), always with your name (one set of results per member):
 *   ./benchmark/ladder_bench -m NAME            C ladder, Python, then REPORT.md
 *   ./benchmark/ladder_bench -m NAME c          C ladder only (+ assembly on the Pi)
 *   ./benchmark/ladder_bench -m NAME python     Python only (after "c": it compares to stageA.c)
 *   ./benchmark/ladder_bench -m NAME report     laptop vs Pi charts + REPORT.md from the saved
 *                                               results of both machines (no timing)
 * Options:
 *   -m NAME     whose results: they go to benchmark/results/NAME/<laptop|rpi>/ (required)
 *   -n RUNS     timed runs per C version (default 5, plus 1 warm-up)
 *   -p PYTHON   Python command (default: py on Windows, python3 elsewhere)
 */
#include "bench.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#define change_dir _chdir
#else
#include <unistd.h>
#define change_dir chdir
#endif

char MEMBER[32];
char ROOT[PATH_LEN];
char MACHINE[16];
char PIN[32];
char PYTHON[128];
int  HAVE_PERF = 0;

static int usage(const char *prog) {
    printf("usage: %s -m NAME [all | c | python | report] [-n RUNS] [-p PYTHON]\n"
           "  -m NAME  your name, e.g. -m andrew: results go to benchmark/results/andrew/\n"
           "           (use the SAME name on the laptop and on the Pi)\n", prog);
    return 1;
}

int main(int argc, char **argv) {
    const char *mode = "all";
    int runs = 5, i;

    setvbuf(stdout, NULL, _IONBF, 0);       /* progress shows up straight away */

    /* STEP 1 - options */
#ifdef _WIN32
    snprintf(PYTHON, sizeof PYTHON, "py");
#else
    snprintf(PYTHON, sizeof PYTHON, "python3");
#endif
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-n") == 0 && i + 1 < argc)      runs = atoi(argv[++i]);
        else if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) snprintf(PYTHON, sizeof PYTHON, "%s", argv[++i]);
        else if (strcmp(argv[i], "-m") == 0 && i + 1 < argc) snprintf(MEMBER, sizeof MEMBER, "%s", argv[++i]);
        else if (!strcmp(argv[i], "all") || !strcmp(argv[i], "c") ||
                 !strcmp(argv[i], "python") || !strcmp(argv[i], "report")) mode = argv[i];
        else return usage(argv[0]);
    }
    if (runs < 1 || runs > MAX_RUNS) { printf("-n must be 1 to %d\n", MAX_RUNS); return 1; }

    /* the member name becomes a folder name: lower-case letters, digits, _ and - only */
    if (MEMBER[0] == '\0') return usage(argv[0]);
    for (i = 0; MEMBER[i]; i++) {
        MEMBER[i] = (char)tolower((unsigned char)MEMBER[i]);
        if (!isalnum((unsigned char)MEMBER[i]) && MEMBER[i] != '_' && MEMBER[i] != '-') {
            printf("-m NAME may only use letters, digits, _ and -\n");
            return 1;
        }
    }
    snprintf(ROOT, sizeof ROOT, "%s/%s", RESULTS_DIR, MEMBER);

    /* STEP 2 - work from the project root (every path is relative to it) */
    if (!file_exists(TEST20_FILE) && (change_dir("..") != 0 || !file_exists(TEST20_FILE))) {
        printf("cannot find %s: run from the project root\n", TEST20_FILE);
        return 1;
    }
    if (strcmp(mode, "report") != 0 && !file_exists(DATA_FILE)) {
        printf("cannot find %s (CSV files are git-ignored: copy it there first)\n", DATA_FILE);
        return 1;
    }

    /* STEP 3 - which machine: decided when this program was compiled */
#if defined(_WIN32)
    snprintf(MACHINE, sizeof MACHINE, "laptop");
#elif IS_PI
    snprintf(MACHINE, sizeof MACHINE, "rpi");
#else
    snprintf(MACHINE, sizeof MACHINE, "linux");
#endif
    /* on Linux / the Pi: pin every run to CPU core 2 (the brief's taskset rule),
     * and use perf stat for the hardware counters if perf is installed */
#ifndef _WIN32
    if (system("command -v taskset > /dev/null 2>&1") == 0) snprintf(PIN, sizeof PIN, "taskset -c 2 ");
    else printf("taskset not found: runs are not pinned to one core\n");
    HAVE_PERF = system("command -v perf > /dev/null 2>&1") == 0;
    if (!HAVE_PERF) printf("perf not found: no hardware counters (cycles, IPC, cache misses)\n");
#endif
    {
        char dir[PATH_LEN];
        snprintf(dir, sizeof dir, "%s/%s", ROOT, MACHINE);
        mkdir_p(dir);
    }
    printf("ladder_bench: %s for %s on %s, %d runs per version, %s%s\n", mode, MEMBER, MACHINE, runs,
           DATA_FILE, PIN[0] ? ", pinned to core 2" : "");

    /* STEP 4 - record this machine's specs ("report" only reads saved results) */
    if (strcmp(mode, "report") != 0) save_device_info();

    /* STEP 5 - the parts */
    if (!strcmp(mode, "all") || !strcmp(mode, "c"))      run_ladder(runs);
    if (!strcmp(mode, "all") || !strcmp(mode, "python")) run_python();
    if (!strcmp(mode, "all") || !strcmp(mode, "report")) {
        run_compare();                         /* laptop vs Pi charts */
        run_final_report();                    /* <member>/REPORT.md */
    }
    printf("\ndone. Results in %s/\n", ROOT);
    return 0;
}
