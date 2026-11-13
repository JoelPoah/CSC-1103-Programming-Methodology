/*
 * ladder.c - the C efficiency ladder: every program at every flag set,
 * plus (Raspberry Pi only) the assembly builds
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

static Result *find_version(int prog, int flag, int variant) {
    int i;
    for (i = 0; i < n_results; i++) {
        Result *r = &results[i];
        if (strcmp(r->section, "python") != 0 && r->prog == prog && r->flag == flag && r->variant == variant)
            return r;
    }
    return NULL;
}

/* one version: its name, its gcc command and its run command */
static void add_version(int prog, int flag, int variant) {
    const Program *p = &PROGRAMS[prog];
    const Variant *v = &VARIANTS[variant];
    int fixed = p->sma_s != NULL && strstr(p->sma_s, "_fx") != NULL;
    char id[64], exe[PATH_LEN], exe_native[PATH_LEN], srcs[CMD_LEN];
    Result *r;

    if (n_results >= MAX_VERSIONS) return;
    r = &results[n_results++];
    memset(r, 0, sizeof *r);
    r->prog = prog;
    r->flag = flag;
    r->variant = variant;
    snprintf(r->section, sizeof r->section, "%s", variant ? "asm" : "ladder");

    if (variant == 0) {
        snprintf(id, sizeof id, "%s_%s", p->name, FLAGS[flag].tag);
        snprintf(r->label, sizeof r->label, "%s %s", p->name, FLAGS[flag].cflags);
    } else {
        snprintf(id, sizeof id, "%s_%s_%s", p->name, FLAGS[flag].tag, v->tag);
        snprintf(r->label, sizeof r->label, "%s %s + %s", p->name, FLAGS[flag].cflags,
                 fixed ? v->label_fx : v->label);
    }
    snprintf(exe, sizeof exe, "%sbin/bench/%s/%s%s", RUN_PREFIX, MACHINE, id, EXE_EXT);
    memcpy(r->id, id, sizeof id);
    memcpy(r->exe, exe, sizeof exe);

    /* the SMA / OHLC engine files come from C or from assembly/ */
    snprintf(srcs, sizeof srcs, "%s%s%s%s%s", p->srcs,
             p->sma_c ? " " : "", p->sma_c ? (v->sma ? p->sma_s : p->sma_c) : "",
             p->ohlc_c ? " " : "", p->ohlc_c ? (v->ohlc ? p->ohlc_s : p->ohlc_c) : "");
    snprintf(r->build_cmd, sizeof r->build_cmd, "gcc %s -Wall %s -o %s -lm", FLAGS[flag].cflags, srcs, exe);
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
    snprintf(title, sizeof title, "Speedup vs Version 0 (stageA -O2 = 1x, dashed), median of %d runs on %s, %s",
             runs, DATA_KEY, MACHINE);
    chart_bars(dir, "ladder_speedup", title, "speedup (x), higher is better", bars, n, 0, 1.0);

    /* 2) median time of every version: one group per program, one bar per flag set */
    for (f = 0; f < N_FLAGS; f++)
        if (!FLAGS[f].pi_only || IS_PI) { colflag[nc] = f; snprintf(cols[nc++], 48, "%s", FLAGS[f].cflags); }
    for (p = 0; p < N_PROGRAMS; p++) {
        snprintf(rows[p], 96, "%s", PROGRAMS[p].name);
        for (f = 0; f < nc; f++) {
            const Result *r = find_version(p, colflag[f], 0);
            vals[p * nc + f] = (r && r->ok) ? r->st.median : 0;
        }
    }
    snprintf(title, sizeof title, "Compiler flags: median time of each version (%s), lower is better", MACHINE);
    chart_groups(dir, "ladder_flags", title, "", "median time (ms, log scale)",
                 rows, N_PROGRAMS, cols, nc, vals, 1, 0, 0);

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

/* Pi only: each assembly swap against the same program in plain C */
static void asm_charts(const char *dir) {
    char rows[8][96], cols[4][48], title[256];
    double vs_c[8 * 3], vs_a[8 * 4];
    int p, v, nr = 0;

    for (v = 0; v < N_VARIANTS; v++) snprintf(cols[v], 48, "%s", VARIANTS[v].label);
    for (p = 0; p < N_PROGRAMS; p++) {
        const Result *c = find_version(p, ASM_FLAG, 0);
        if (PROGRAMS[p].sma_s == NULL || c == NULL || !c->ok) continue;
        snprintf(rows[nr], 96, "%s %s", PROGRAMS[p].name, FLAGS[ASM_FLAG].cflags);
        for (v = 0; v < N_VARIANTS; v++) {
            const Result *r = find_version(p, ASM_FLAG, v);
            int ok = r != NULL && r->ok;
            vs_a[nr * 4 + v] = ok ? r->speedup : 0;
            if (v > 0) vs_c[nr * 3 + v - 1] = ok ? c->st_proc.median / r->st_proc.median : 0;
        }
        nr++;
    }
    snprintf(title, sizeof title, "Assembly: process-time speedup vs the same build in plain C (= 1x)");
    chart_groups(dir, "asm_speedup_vs_plain_c", title, "fixed-point builds use sma_fx.s / ohlc_fx.s",
                 "speedup (x)", rows, nr, cols + 1, 3, vs_c, 0, 1, 1.0);
    snprintf(title, sizeof title, "Assembly builds: speedup vs Version 0 (stageA -O2 = 1x), load + process");
    chart_groups(dir, "asm_speedup_vs_stageA", title, "fixed-point builds use sma_fx.s / ohlc_fx.s",
                 "speedup (x)", rows, nr, cols, 4, vs_a, 0, 1, 1.0);
}

void run_ladder(int runs) {
    char dir[2][PATH_LEN], raw[2][PATH_LEN], path[PATH_LEN];
    const Result *base;
    int p, f, v, i;

    /* STEP 1 - list the versions: every program x every flag set, and on the
     * Pi the three assembly swaps of each in-memory program (at -O2) */
    for (p = 0; p < N_PROGRAMS; p++)
        for (f = 0; f < N_FLAGS; f++)
            if (!FLAGS[f].pi_only || IS_PI) add_version(p, f, 0);
    if (IS_PI) {
        for (p = 0; p < N_PROGRAMS; p++)
            if (PROGRAMS[p].sma_s != NULL)
                for (v = 1; v < N_VARIANTS; v++) add_version(p, ASM_FLAG, v);
    } else {
        printf("(no assembly builds: the .s files are AArch64, they run on the Raspberry Pi)\n");
    }

    /* STEP 2 - folders: results/<machine>/{ladder,asm}/logs/raw and bin/bench/<machine> */
    for (i = 0; i < 2; i++) {
        snprintf(dir[i], PATH_LEN, "%s/%s/%s", ROOT, MACHINE, i ? "asm" : "ladder");
        snprintf(raw[i], PATH_LEN, "%s/logs/raw", dir[i]);
        if (i == 0 || IS_PI) mkdir_p(raw[i]);
    }
    snprintf(path, sizeof path, "bin/bench/%s", MACHINE);
    mkdir_p(path);

    /* STEP 3 - compile, check and time every version */
    for (i = 0; i < n_results; i++) {
        Result *r = &results[i];
        printf("\n[%d/%d] %s\n", i + 1, n_results, r->label);
        time_version(r, runs, raw[r->variant ? 1 : 0]);
    }

    /* STEP 4 - speedup of every version vs Version 0 (stageA -O2), on medians */
    base = find_version(BASE_PROG, BASE_FLAG, 0);
    if (base != NULL && !base->ok) base = NULL;
    for (i = 0; i < n_results; i++)
        if (results[i].ok && base != NULL) results[i].speedup = base->st.median / results[i].st.median;

    /* STEP 5 - one log per version, charts, results .csv, summary.txt */
    for (i = 0; i < n_results; i++)
        if (results[i].ok)
            write_version_log(&results[i], dir[results[i].variant ? 1 : 0], base ? base->st.median : 0);
    printf("\nCharts:\n");
    snprintf(path, sizeof path, "%s/charts", dir[0]);
    ladder_charts(path, base, runs);
    snprintf(path, sizeof path, "%s/charts", dir[1]);
    if (IS_PI) asm_charts(path);
    snprintf(path, sizeof path, "%s/%s/c_results.csv", ROOT, MACHINE);
    write_csv(path, 0);
    snprintf(path, sizeof path, "%s/%s/summary.txt", ROOT, MACHINE);
    write_summary(path, runs);
    printf("\nsummary: %s\n", path);
}
