/*
 * final_report.c - put the results of BOTH machines into one Markdown report,
 * benchmark/results/REPORT.md: the device specs side by side, the ladder,
 * assembly and Python tables, the key results, and every chart.
 *
 * Run it after the benchmark has run on the laptop and on the Pi (it also
 * works with only one machine's results):   ./benchmark/ladder_bench report
 * Open REPORT.md in VS Code (Ctrl+Shift+V) or on GitHub to see it with charts.
 */
#include "bench.h"
#include <string.h>

#define N_MACHINES 2
#define MAX_KEYS   24
static const char *TAG[N_MACHINES]  = { "laptop", "rpi" };
static const char *NAME[N_MACHINES] = { "Laptop", "Raspberry Pi 5" };

static Result c_rows[N_MACHINES][MAX_VERSIONS], py_rows[N_MACHINES][4];
static int nc[N_MACHINES], np[N_MACHINES];
static char keys[N_MACHINES][MAX_KEYS][48], values[N_MACHINES][MAX_KEYS][256];
static int nkeys[N_MACHINES];

/* device.txt: one "Key: value" line per spec */
static void read_device(int m) {
    char path[PATH_LEN], line[512], *colon;
    FILE *f;
    snprintf(path, sizeof path, "%s/%s/device.txt", ROOT, TAG[m]);
    if ((f = fopen(path, "r")) == NULL) return;
    while (nkeys[m] < MAX_KEYS && fgets(line, sizeof line, f) != NULL) {
        line[strcspn(line, "\r\n")] = '\0';
        if ((colon = strstr(line, ": ")) == NULL) continue;
        *colon = '\0';
        snprintf(keys[m][nkeys[m]], 48, "%s", line);
        snprintf(values[m][nkeys[m]], 256, "%s", colon + 2);
        nkeys[m]++;
    }
    fclose(f);
}

static const char *device_value(int m, const char *key) {
    int i;
    for (i = 0; i < nkeys[m]; i++) if (strcmp(keys[m][i], key) == 0) return values[m][i];
    return "-";
}

/* embed a chart if it exists (paths relative to benchmark/results/) */
static void chart(FILE *f, const char *rel, const char *title) {
    char path[PATH_LEN];
    snprintf(path, sizeof path, "%s/%s", ROOT, rel);
    if (file_exists(path)) fprintf(f, "![%s](%s)\n\n", title, rel);
}

/* one chart per stage: the same program at each optimisation level, labelled
 * with its median time and its speedup vs its own -O0 build.
 * Drawn from the saved results, into <machine>/ladder/charts/stage_<program>.png */
static void stage_charts(FILE *f, int m) {
    char dir[PATH_LEN], id[64], name[64], title[256], rel[PATH_LEN];
    Bar bars[8];
    int p, fl, n;

    snprintf(dir, sizeof dir, "%s/%s/ladder/charts", ROOT, TAG[m]);
    for (p = 0; p < N_PROGRAMS; p++) {
        const Result *o0;
        snprintf(id, sizeof id, "%s_O0", PROGRAMS[p].name);
        o0 = find_id(c_rows[m], nc[m], id);
        for (fl = 0, n = 0; fl < N_FLAGS && n < 8; fl++) {
            const Result *r;
            snprintf(id, sizeof id, "%s_%s", PROGRAMS[p].name, FLAGS[fl].tag);
            if ((r = find_id(c_rows[m], nc[m], id)) == NULL || r->st.median <= 0) continue;
            snprintf(bars[n].label, sizeof bars[n].label, "%s", FLAGS[fl].cflags);
            bars[n].value = r->st.median;
            bars[n].colour = COLOURS[fl % 8];             /* one colour per flag set */
            fmt_ms(r->st.median, bars[n].text, sizeof bars[n].text);
            if (o0 && r != o0)
                snprintf(bars[n].text + strlen(bars[n].text), sizeof bars[n].text - strlen(bars[n].text),
                         "  (%.2fx)", o0->st.median / r->st.median);
            n++;
        }
        if (n == 0) continue;
        snprintf(name, sizeof name, "stage_%s", PROGRAMS[p].name);
        snprintf(title, sizeof title, "%s at each optimisation level (%s): median time, speedup vs -O0 in brackets",
                 PROGRAMS[p].name, NAME[m]);
        chart_bars(dir, name, title, "median time (ms), lower is better", bars, n, 0, 0);
        snprintf(rel, sizeof rel, "%s/ladder/charts/%s.png", TAG[m], name);
        chart(f, rel, title);
    }
}

static void mode_cell(const Stats *s, char *out, size_t size) {
    if (s->mode_count >= 2) fmt_ms(s->mode, out, size);
    else snprintf(out, size, "none");
}

/* the ladder table of one machine */
static void ladder_table(FILE *f, int m) {
    char t[5][32];
    int i;
    fprintf(f, "| Version | What changed | Median | Mean | Mode | Min - Max | ns/trade | Trades/s | Speedup vs V0 | test20 |\n");
    fprintf(f, "|---|---|---:|---:|---:|---:|---:|---:|---:|:---:|\n");
    for (i = 0; i < nc[m]; i++) {
        const Result *r = &c_rows[m][i];
        if (strcmp(r->section, "ladder") != 0) continue;
        fmt_ms(r->st.median, t[0], 32);
        fmt_ms(r->st.mean, t[1], 32);
        mode_cell(&r->st, t[2], 32);
        fmt_ms(r->st.min, t[3], 32);
        fmt_ms(r->st.max, t[4], 32);
        fprintf(f, "| `%s` | %s | **%s** | %s | %s | %s - %s | %.1f | %.0f | ", r->label,
                r->prog >= 0 ? PROGRAMS[r->prog].rung : "", t[0], t[1], t[2], t[3], t[4],
                r->trades ? r->st.median * 1e6 / (double)r->trades : 0,
                r->st.median > 0 ? (double)r->trades / (r->st.median / 1000) : 0);
        if (strcmp(r->id, "stageA_O2") == 0) fprintf(f, "1.00x (Version 0) | %s |\n", r->test20);
        else fprintf(f, "**%.2fx** | %s |\n", r->speedup, r->test20);
    }
    fprintf(f, "\n");
}

/* memory and perf stat counters of every C version (perf: Pi only) */
static void memory_perf_table(FILE *f, int m) {
    int i;
    fprintf(f, "| Version | Peak memory | Cycles | Instructions | IPC | Cache references | Cache misses | Miss rate |\n");
    fprintf(f, "|---|---:|---:|---:|---:|---:|---:|---:|\n");
    for (i = 0; i < nc[m]; i++) {
        const Result *r = &c_rows[m][i];
        fprintf(f, "| `%s` | %.1f MB | ", r->label, r->peak_mb);
        if (r->has_perf)
            fprintf(f, "%.3g | %.3g | %.2f | %.3g | %.3g | %.2f%% |\n", r->cycles, r->instructions,
                    r->instructions / r->cycles, r->cache_refs, r->cache_misses,
                    r->cache_refs > 0 ? 100 * r->cache_misses / r->cache_refs : 0);
        else if (m == 0)                  /* perf is Linux-only: it cannot run on Windows */
            fprintf(f, "n/a | n/a | n/a | n/a | n/a | n/a |\n");
        else                              /* on the Pi: perf ran but gave no counters */
            fprintf(f, "not counted | - | - | - | - | - |\n");
    }
    fprintf(f, "\nPeak memory = %s, measured on one extra, untimed run after the timed runs.\n\n",
            m == 0 ? "Windows peak working set" : "Linux maximum resident set size");
    if (m == 0)
        fprintf(f, "Hardware counters are **n/a on the laptop**: `perf` is a Linux tool and does not run on "
                   "Windows (the brief: \"perf, taskset and the -mcpu=cortex-a76 flag work only on the Pi\"). "
                   "They are in the Raspberry Pi 5 section.\n\n");
    else
        fprintf(f, "Hardware counters: `perf stat` on that same extra run, for the whole program "
                   "(Stage B/C programs also load test20 and dev_1M in it). If a row says *not counted*, "
                   "run `sudo sysctl kernel.perf_event_paranoid=1` and benchmark again.\n\n");
}

/* the assembly table (Pi): each build vs the same program in plain C */
static void asm_table(FILE *f, int m) {
    char t[2][32], plain_id[64];
    int i;
    fprintf(f, "| Version | Median (load + process) | Process median | Process speedup vs plain C | Speedup vs V0 | test20 |\n");
    fprintf(f, "|---|---:|---:|---:|---:|:---:|\n");
    for (i = 0; i < nc[m]; i++) {
        const Result *r = &c_rows[m][i], *c;
        if (strcmp(r->section, "asm") != 0 || r->prog < 0 || r->flag < 0) continue;
        snprintf(plain_id, sizeof plain_id, "%s_%s", PROGRAMS[r->prog].name, FLAGS[r->flag].tag);
        c = find_id(c_rows[m], nc[m], plain_id);
        fmt_ms(r->st.median, t[0], 32);
        fmt_ms(r->st_proc.median, t[1], 32);
        fprintf(f, "| `%s` | %s | %s | ", r->label, t[0], t[1]);
        if (c && r->st_proc.median > 0) fprintf(f, "**%.2fx** | ", c->st_proc.median / r->st_proc.median);
        else fprintf(f, "- | ");
        fprintf(f, "%.2fx | %s |\n", r->speedup, r->test20);
    }
    fprintf(f, "\n");
}

/* "`a` is 1.55x faster than `b`", whichever of the two is faster */
static void faster_text(char *out, size_t size, const char *a, double a_ms, const char *b, double b_ms) {
    if (a_ms <= b_ms) snprintf(out, size, "`%s` is %.2fx faster than `%s`", a, b_ms / a_ms, b);
    else              snprintf(out, size, "`%s` is %.2fx faster than `%s`", b, a_ms / b_ms, a);
}

/* pure Python vs NumPy on one machine: its own chart, drawn from the saved results */
static void python_vs_numpy_chart(FILE *f, int m) {
    char dir[PATH_LEN], rel[PATH_LEN], title[256];
    const Result *pure = find_id(py_rows[m], np[m], "stageA_Python");
    const Result *numpy = find_id(py_rows[m], np[m], "stageA_Python_numpy");
    Bar bars[2];

    if (pure == NULL || numpy == NULL) return;
    snprintf(bars[0].label, sizeof bars[0].label, "%s (pure Python)", pure->label);
    snprintf(bars[1].label, sizeof bars[1].label, "%s (NumPy)", numpy->label);
    bars[0].value = pure->st.median;
    bars[1].value = numpy->st.median;
    bars[0].colour = COLOURS[5];
    bars[1].colour = COLOURS[6];
    fmt_ms(pure->st.median, bars[0].text, sizeof bars[0].text);
    fmt_ms(numpy->st.median, bars[1].text, sizeof bars[1].text);
    if (numpy->st.median < pure->st.median)               /* the faster one carries the speedup */
        snprintf(bars[1].text + strlen(bars[1].text), sizeof bars[1].text - strlen(bars[1].text),
                 "  (%.2fx faster)", pure->st.median / numpy->st.median);
    else
        snprintf(bars[0].text + strlen(bars[0].text), sizeof bars[0].text - strlen(bars[0].text),
                 "  (%.2fx faster)", numpy->st.median / pure->st.median);
    snprintf(dir, sizeof dir, "%s/%s/python/charts", ROOT, TAG[m]);
    snprintf(title, sizeof title, "Pure Python vs NumPy on %s (%s): median of %d runs, lower is better",
             DATA_KEY, NAME[m], pure->n);
    chart_bars(dir, "python_vs_numpy", title, "median time (ms)", bars, 2, 0, 0);
    snprintf(rel, sizeof rel, "%s/python/charts/python_vs_numpy.png", TAG[m]);
    chart(f, rel, "Pure Python vs NumPy");
}

static void python_table(FILE *f, int m) {
    const Result *stageA = find_id(c_rows[m], nc[m], "stageA_O2");
    char t[5][32], cmp[160];
    int i;
    fprintf(f, "| Version | Median | Mean | Mode | Min - Max | ns/trade | vs stageA.c -O2 |\n");
    fprintf(f, "|---|---:|---:|---:|---:|---:|---:|\n");
    for (i = 0; i < np[m]; i++) {
        const Result *r = &py_rows[m][i];
        fmt_ms(r->st.median, t[0], 32);
        fmt_ms(r->st.mean, t[1], 32);
        mode_cell(&r->st, t[2], 32);
        fmt_ms(r->st.min, t[3], 32);
        fmt_ms(r->st.max, t[4], 32);
        fprintf(f, "| `%s` | **%s** | %s | %s | %s - %s | %.1f | ", r->label, t[0], t[1], t[2], t[3], t[4],
                r->trades ? r->st.median * 1e6 / (double)r->trades : 0);
        if (stageA) {
            faster_text(cmp, sizeof cmp, r->label, r->st.median, "stageA.c", stageA->st.median);
            fprintf(f, "%s |\n", cmp);
        } else {
            fprintf(f, "- |\n");
        }
    }
    if (stageA) {
        fmt_ms(stageA->st.median, t[0], 32);
        fprintf(f, "| `stageA.c -O2` (Version 0, C) | **%s** | | | | | - |\n", t[0]);
    }
    fprintf(f, "\n");
}

/* one bullet list of the headline numbers */
static void key_results(FILE *f) {
    const Result *best[N_MACHINES], *base[N_MACHINES], *py[N_MACHINES];
    int m;
    for (m = 0; m < N_MACHINES; m++) {
        best[m] = fastest(c_rows[m], nc[m]);
        base[m] = find_id(c_rows[m], nc[m], "stageA_O2");
        py[m] = fastest(py_rows[m], np[m]);
    }
    for (m = 0; m < N_MACHINES; m++) {
        const Result *s = find_id(c_rows[m], nc[m], "stageB_inmem_O2");
        const Result *ml = find_id(c_rows[m], nc[m], "stageC_malloc_O2");
        if (best[m] == NULL) continue;
        fprintf(f, "- **%s - fastest version:** `%s`, %.1f ms", NAME[m], best[m]->label, best[m]->st.median);
        if (base[m]) fprintf(f, " = **%.2fx** faster than Version 0 (`stageA -O2`, %.1f ms)",
                             base[m]->st.median / best[m]->st.median, base[m]->st.median);
        fprintf(f, ".\n");
        if (np[m] == 2) {
            const Result *slower = (py[m] == &py_rows[m][0]) ? &py_rows[m][1] : &py_rows[m][0];
            fprintf(f, "- **%s - Python:** `%s` is the faster Python version, %.2fx faster than `%s`.\n",
                    NAME[m], py[m]->label, slower->st.median / py[m]->st.median, slower->label);
        }
        if (py[m] && base[m]) {
            char a[160], b[160];
            faster_text(a, sizeof a, "stageA.c -O2", base[m]->st.median, py[m]->label, py[m]->st.median);
            faster_text(b, sizeof b, best[m]->label, best[m]->st.median, py[m]->label, py[m]->st.median);
            fprintf(f, "- **%s - C vs Python:** %s; %s.\n", NAME[m], a, b);
        }
        if (s && ml)
            fprintf(f, "- **%s - malloc vs static array (-O2):** `stageC_malloc` %.1f ms vs `stageB_inmem` %.1f ms "
                       "(%+.1f%%; load %.1f vs %.1f ms, process %.1f vs %.1f ms).\n",
                    NAME[m], ml->st.median, s->st.median, 100 * (ml->st.median - s->st.median) / s->st.median,
                    ml->st_load.median, s->st_load.median, ml->st_proc.median, s->st_proc.median);
    }
    if (best[0] && best[1])
        fprintf(f, "- **Laptop vs Pi:** the fastest laptop version is %.2fx faster than the fastest Pi version.\n",
                best[1]->st.median / best[0]->st.median);
    fprintf(f, "\n");
}

void run_final_report(void) {
    char path[PATH_LEN], rel[PATH_LEN];
    int m, i, j;
    FILE *f;

    /* STEP 1 - read both machines' device specs and results */
    for (m = 0; m < N_MACHINES; m++) {
        read_device(m);
        snprintf(path, sizeof path, "%s/%s/c_results.csv", ROOT, TAG[m]);
        nc[m] = read_csv(path, c_rows[m], MAX_VERSIONS);
        snprintf(path, sizeof path, "%s/%s/python/python_results.csv", ROOT, TAG[m]);
        np[m] = read_csv(path, py_rows[m], 4);
    }
    snprintf(path, sizeof path, "%s/REPORT.md", ROOT);
    if ((f = fopen(path, "w")) == NULL) { printf("cannot write %s\n", path); return; }

    /* STEP 2 - title, set-up and method */
    fprintf(f, "# Efficiency Ladder Benchmark - Group 06\n\n");
    fprintf(f, "Made by `benchmark/ladder_bench report` from the saved results in this folder. "
               "Every time is the **median of %d runs on `%s`** (1 warm-up run first, not counted), "
               "measured with `now_ns()` from `helper_function/timing.h`. "
               "Version 0 = `stageA -O2` (helper library, `double`, naive SMA).\n\n",
            nc[0] ? c_rows[0][0].n : nc[1] ? c_rows[1][0].n : 5, DATA_FILE);

    fprintf(f, "## 1. Test set-up\n\n| | %s | %s |\n|---|---|---|\n", NAME[0], NAME[1]);
    for (m = 0; m < N_MACHINES; m++)                     /* every key, in the order first seen */
        for (i = 0; i < nkeys[m]; i++) {
            int seen = 0;
            for (j = 0; m == 1 && j < nkeys[0]; j++) if (strcmp(keys[0][j], keys[1][i]) == 0) seen = 1;
            if (!seen) fprintf(f, "| %s | %s | %s |\n", keys[m][i], device_value(0, keys[m][i]), device_value(1, keys[m][i]));
        }
    fprintf(f, "\n## 2. How each version was timed\n\n"
               "- `stageA`, `stageA_plus`: `now_ns()` around the whole program (they only process the dataset).\n"
               "- `stageB_inmem`, `stageB_fixed`, `stageC_malloc`: they always run test20, dev_1M and stress_full in one go, "
               "so the time is their own `now_ns()` **Load + Process** lines for the `%s` part.\n"
               "- Python: each script's own `bench` mode (5 runs, `time.perf_counter_ns`).\n"
               "- Correctness: every version's test20 Final VWAP and BUY/SELL count is checked against `test20_expected.txt`.\n"
               "- Mode: times rounded to 1 ms (C) or 100 ms (Python); `none` = no two runs equal.\n"
               "- Raw numbers for every run: `<machine>/ladder/logs/<version>.txt`.\n\n", DATA_KEY);

    /* STEP 3 - the headline numbers */
    fprintf(f, "## 3. Key results\n\n");
    key_results(f);

    /* STEP 4 - one section per machine: ladder, assembly, charts */
    for (m = 0; m < N_MACHINES; m++) {
        fprintf(f, "## %d. Efficiency ladder - %s\n\n", 4 + m, NAME[m]);
        if (nc[m] == 0) {
            fprintf(f, "_Not measured yet: run `./benchmark/ladder_bench` on the %s, then `report` again._\n\n", NAME[m]);
            continue;
        }
        ladder_table(f, m);
        snprintf(rel, sizeof rel, "%s/ladder/charts/ladder_speedup.png", TAG[m]);
        chart(f, rel, "Speedup vs Version 0");
        snprintf(rel, sizeof rel, "%s/ladder/charts/ladder_flags.png", TAG[m]);
        chart(f, rel, "Compiler flags");
        snprintf(rel, sizeof rel, "%s/ladder/charts/ladder_load_vs_process.png", TAG[m]);
        chart(f, rel, "Load vs process");
        fprintf(f, "### Each stage at each optimisation level\n\n");
        stage_charts(f, m);
        fprintf(f, "### Memory and hardware counters (perf stat)\n\n");
        memory_perf_table(f, m);
        snprintf(rel, sizeof rel, "%s/ladder/charts/ladder_memory.png", TAG[m]);
        chart(f, rel, "Peak memory");
        for (i = 0; i < nc[m]; i++) if (strcmp(c_rows[m][i].section, "asm") == 0) break;
        if (i < nc[m]) {
            fprintf(f, "### Hand-written assembly (`assembly/`) linked in place of the C engine files\n\n");
            asm_table(f, m);
            snprintf(rel, sizeof rel, "%s/asm/charts/asm_speedup_vs_plain_c.png", TAG[m]);
            chart(f, rel, "Assembly vs plain C");
            snprintf(rel, sizeof rel, "%s/asm/charts/asm_speedup_vs_stageA.png", TAG[m]);
            chart(f, rel, "Assembly vs Version 0");
        }
    }

    /* STEP 5 - Python, then laptop vs Pi */
    fprintf(f, "## 6. C vs Python\n\n");
    for (m = 0; m < N_MACHINES; m++) {
        if (np[m] == 0) continue;
        fprintf(f, "### %s\n\n", NAME[m]);
        python_table(f, m);
        python_vs_numpy_chart(f, m);
        snprintf(rel, sizeof rel, "%s/python/charts/python_vs_c_time.png", TAG[m]);
        chart(f, rel, "Python vs C");
    }
    if (np[0] + np[1] == 0) fprintf(f, "_No Python results yet._\n\n");

    fprintf(f, "## 7. Laptop vs Raspberry Pi 5\n\n");
    if (nc[0] && nc[1]) {
        fprintf(f, "| Version | Laptop | Pi | Pi takes ... as long |\n|---|---:|---:|---:|\n");
        for (i = 0; i < nc[0]; i++) {
            const Result *pi = find_id(c_rows[1], nc[1], c_rows[0][i].id);
            if (pi) fprintf(f, "| `%s` | %.1f ms | %.1f ms | %.2fx |\n", c_rows[0][i].label,
                            c_rows[0][i].st.median, pi->st.median, pi->st.median / c_rows[0][i].st.median);
        }
        fprintf(f, "\n");
        chart(f, "compare/charts/fastest_laptop_vs_rpi.png", "Fastest laptop vs fastest Pi");
        chart(f, "compare/charts/fastest_c_vs_fastest_python.png", "Fastest C vs fastest Python");
        chart(f, "compare/charts/laptop_vs_rpi_all_versions.png", "Every version on both machines");
    } else {
        fprintf(f, "_Needs results from both machines in `benchmark/results/laptop/` and `benchmark/results/rpi/`._\n");
    }
    fclose(f);
    printf("\nreport: %s\n", path);
}
