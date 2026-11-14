/*
 * ladder.c - the C efficiency ladder: every version that "make ladder" builds
 * (V0-V7, + V8 on the Raspberry Pi) at every flag set
 */
#include "bench.h"
#include <string.h>

Result results[MAX_VERSIONS];
int n_results = 0;

#ifdef _WIN32
#define EXE_EXT    ".exe"
#define RUN_PREFIX ""                     /* cmd.exe runs a relative path as is */
#else
#define EXE_EXT    ""
#define RUN_PREFIX "./"                   /* sh needs ./ for a program in a folder */
#endif

/* 1 if this machine can build the program (V8 is AArch64 assembly: Pi only) */
static int can_build(int prog) {
    return !PROGRAMS[prog].pi_only || IS_PI;
}

static Result *find_version(int prog, int flag) {
    int i;
    for (i = 0; i < n_results; i++) {
        Result *r = &results[i];
        if (strcmp(r->section, "ladder") == 0 && r->prog == prog && r->flag == flag) return r;
    }
    return NULL;
}

/* one version: its name, its gcc command and its run command */
static void add_version(int prog, int flag) {
    const Program *p = &PROGRAMS[prog];
    char id[64], exe[PATH_LEN], exe_native[PATH_LEN];
    Result *r;

    if (n_results >= MAX_VERSIONS) return;
    r = &results[n_results++];
    memset(r, 0, sizeof *r);
    r->prog = prog;
    r->flag = flag;
    snprintf(r->section, sizeof r->section, "ladder");
    snprintf(id, sizeof id, "%s_%s", p->name, FLAGS[flag].tag);
    snprintf(r->label, sizeof r->label, "%s %s", p->name, FLAGS[flag].cflags);
    snprintf(exe, sizeof exe, "%sbin/bench/%s/%s%s", RUN_PREFIX, MACHINE, id, EXE_EXT);
    memcpy(r->id, id, sizeof id);
    memcpy(r->exe, exe, sizeof exe);

    /* the Makefile's ladder rule, with this flag set in place of -O2 */
    snprintf(r->build_cmd, sizeof r->build_cmd, "gcc %s -Wall %s -o %s -lm", FLAGS[flag].cflags, p->srcs, exe);
    native_path(exe, exe_native, sizeof exe_native);
    snprintf(r->run_cmd, sizeof r->run_cmd, "%s %s", exe_native, p->args);
}

static void ladder_charts(const char *dir, const Result *base, int runs) {
    Bar bars[MAX_VERSIONS];
    char title[256], rows[MAX_VERSIONS][96], cols[8][48];
    double vals[MAX_VERSIONS * 8];
    int i, p, f, n = 0, nc = 0, colflag[8];

    /* 1) the efficiency ladder: speedup vs Version 0, without Version 0 itself */
    for (i = 0; base != NULL && i < n_results; i++) {
        const Result *r = &results[i];
        if (!r->ok || r == base || strcmp(r->section, "ladder") != 0) continue;
        snprintf(bars[n].label, sizeof bars[n].label, "%s", r->label);
        bars[n].value = r->speedup;
        bars[n].colour = COLOURS[r->prog % 8];
        snprintf(bars[n].text, sizeof bars[n].text, "%.2fx", r->speedup);
        n++;
    }
    snprintf(title, sizeof title, "Speedup vs Version 0 (" BASE_LABEL " = 1x, dashed), median of %d runs on %s, %s",
             runs, DATA_KEY, MACHINE);
    chart_bars(dir, "ladder_speedup", title, "speedup (x, log scale), higher is better", bars, n, 1, 1.0);

    /* 2) median time of every version: one group per program, one bar per flag set */
    for (f = 0; f < N_FLAGS; f++)
        if (!FLAGS[f].pi_only || IS_PI) { colflag[nc] = f; snprintf(cols[nc++], 48, "%s", FLAGS[f].cflags); }
    n = 0;
    for (p = 0; p < N_PROGRAMS; p++) {
        if (!can_build(p)) continue;
        snprintf(rows[n], 96, "%s", PROGRAMS[p].name);
        for (f = 0; f < nc; f++) {
            const Result *r = find_version(p, colflag[f]);
            vals[n * nc + f] = (r && r->ok) ? r->st.median : 0;
        }
        n++;
    }
    snprintf(title, sizeof title, "Compiler flags: median time of each version (%s), lower is better", MACHINE);
    chart_groups(dir, "ladder_flags", title, "", "median time (ms, log scale)",
                 rows, n, cols, nc, vals, 1, 0, 0);

    /* 3) the in-memory versions: loading the file vs processing it from memory */
    snprintf(cols[0], 48, "Load (read + parse the file)");
    snprintf(cols[1], 48, "Process (engine loop)");
    n = 0;
    for (i = 0; i < n_results; i++) {
        const Result *r = &results[i];
        if (!r->ok || !r->split || strcmp(r->section, "ladder") != 0) continue;
        snprintf(rows[n], 96, "%s", r->label);
        vals[n * 2] = r->st_load.median;
        vals[n * 2 + 1] = r->st_proc.median;
        n++;
    }
    snprintf(title, sizeof title, "In-memory versions: load vs process time (%s, medians)", MACHINE);
    chart_groups(dir, "ladder_load_vs_process", title, "", "median time (ms, log scale)",
                 rows, n, cols, 2, vals, 1, 0, 0);

    /* 4) peak memory of every version */
    n = 0;
    for (i = 0; i < n_results; i++) {
        const Result *r = &results[i];
        if (!r->ok || r->peak_mb <= 0 || strcmp(r->section, "ladder") != 0) continue;
        snprintf(bars[n].label, sizeof bars[n].label, "%s", r->label);
        bars[n].value = r->peak_mb;
        bars[n].colour = COLOURS[r->prog % 8];
        snprintf(bars[n].text, sizeof bars[n].text, "%.0f MB", r->peak_mb);
        n++;
    }
    snprintf(title, sizeof title, "Peak memory of each version (%s), lower is better", MACHINE);
    chart_bars(dir, "ladder_memory", title, "peak memory (MB, log scale)", bars, n, 1, 0);
}

void run_ladder(int runs) {
    char dir[PATH_LEN], raw[PATH_LEN], path[PATH_LEN];
    const Result *base;
    int p, f, i;

    /* STEP 1 - list the versions: every program x every flag set */
    for (p = 0; p < N_PROGRAMS; p++) {
        if (!can_build(p)) {
            printf("(no %s: it links AArch64 assembly, it runs on the Raspberry Pi)\n", PROGRAMS[p].name);
            continue;
        }
        for (f = 0; f < N_FLAGS; f++)
            if (!FLAGS[f].pi_only || IS_PI) add_version(p, f);
    }

    /* STEP 2 - folders: results/<member>/<machine>/ladder/logs/raw and bin/bench/<machine> */
    snprintf(dir, sizeof dir, "%s/%s/ladder", ROOT, MACHINE);
    snprintf(raw, sizeof raw, "%s/logs/raw", dir);
    mkdir_p(raw);
    snprintf(path, sizeof path, "bin/bench/%s", MACHINE);
    mkdir_p(path);

    /* STEP 3 - compile, check and time every version */
    for (i = 0; i < n_results; i++) {
        Result *r = &results[i];
        printf("\n[%d/%d] %s\n", i + 1, n_results, r->label);
        time_version(r, runs, raw);
    }

    /* STEP 4 - speedup of every version vs Version 0 (V0 at -O2), on medians */
    base = find_version(BASE_PROG, BASE_FLAG);
    if (base != NULL && !base->ok) base = NULL;
    for (i = 0; i < n_results; i++)
        if (results[i].ok && base != NULL) results[i].speedup = base->st.median / results[i].st.median;

    /* STEP 5 - one log per version, charts, results .csv, summary.txt */
    for (i = 0; i < n_results; i++)
        if (results[i].ok) write_version_log(&results[i], dir, base ? base->st.median : 0);
    printf("\nCharts:\n");
    snprintf(path, sizeof path, "%s/charts", dir);
    ladder_charts(path, base, runs);
    snprintf(path, sizeof path, "%s/%s/c_results.csv", ROOT, MACHINE);
    write_csv(path, 0);
    snprintf(path, sizeof path, "%s/%s/summary.txt", ROOT, MACHINE);
    write_summary(path, runs);
    printf("\nsummary: %s\n", path);
}
