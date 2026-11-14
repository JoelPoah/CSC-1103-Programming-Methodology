/*
 * report.c - what gets written: one log per version, summary.txt and the
 * results .csv (which "report" reads back from both machines)
 */
#include "bench.h"
#include <stdlib.h>
#include <string.h>

/* the measured ladder version of program "name" at flag set f (NULL: none) */
static const Result *version_of(const char *name, int f) {
    int i;
    for (i = 0; i < n_results; i++) {
        const Result *r = &results[i];
        if (r->ok && r->flag == f && strcmp(r->section, "ladder") == 0 &&
            strcmp(PROGRAMS[r->prog].name, name) == 0)
            return r;
    }
    return NULL;
}

void write_version_log(const Result *r, const char *dir, double base_ms) {
    char path[PATH_LEN], a[32], b[32], mode[96];
    int python = strcmp(r->section, "python") == 0, i;
    FILE *f;

    snprintf(path, sizeof path, "%s/logs/%s.txt", dir, r->id);
    if ((f = fopen(path, "w")) == NULL) return;

    /* what it is and how it was timed */
    fprintf(f, "%s\n==================================================================\n", r->label);
    if (python) {
        fprintf(f, "What:     %s\nRun:      %s\n", PY_SCRIPTS[r->prog].about, r->run_cmd);
        fprintf(f, "Timing:   each run = the script's own bench mode (time.perf_counter_ns,\n"
                   "          read + process stress_full, no plot). The whole bench process\n"
                   "          took %.2f s by now_ns().\n", r->total_ms / 1000);
    } else {
        const Program *p = &PROGRAMS[r->prog];
        fprintf(f, "Rung:     %s\n", p->rung);
        fprintf(f, "Build:    %s\nRun:      %s%s\n", r->build_cmd, PIN, r->run_cmd);
        fprintf(f, p->own_timer
                ? "Timing:   the program's own now_ns() Load time + Process time for the %s section\n"
                  "          (it also runs test20 and dev_1M first; whole-program times below)\n"
                : "Timing:   now_ns() from launch to exit (it only processes %s with this argument)\n",
                DATA_KEY);
        fprintf(f, "Warm-up:  1 run first, not counted\n");
        fprintf(f, "test20:   %s (Final VWAP + BUY/SELL vs %s)\n", r->test20, TEST20_EXPECTED);
    }
    fprintf(f, "\nRESULT on %s: %lld trades, Final VWAP %s, %d BUY, %d SELL\n",
            DATA_KEY, r->trades, r->vwap[0] ? r->vwap : "-", r->buys, r->sells);

    /* every run */
    fprintf(f, "\nRUNS (ms)\n");
    if (r->split) {
        fprintf(f, "  run         time = load    +  process   whole program\n");
        for (i = 0; i < r->n; i++)
            fprintf(f, "  %-3d  %12.3f   %10.3f  %9.3f  %14.3f\n", i + 1, r->time[i], r->load[i], r->proc[i], r->whole[i]);
    } else {
        for (i = 0; i < r->n; i++) fprintf(f, "  run %-3d  %12.3f\n", i + 1, r->time[i]);
    }

    /* statistics */
    fmt_ms(r->st.median, a, sizeof a);
    fprintf(f, "\nSTATISTICS\n  Median:  %s   <- used for every comparison\n", a);
    fmt_ms(r->st.mean, a, sizeof a);
    fprintf(f, "  Mean:    %s\n", a);
    fmt_mode(&r->st, mode, sizeof mode);
    fprintf(f, "  Mode:    %s\n", mode);
    fmt_ms(r->st.min, a, sizeof a);
    fmt_ms(r->st.max, b, sizeof b);
    fprintf(f, "  Min/Max: %s / %s\n", a, b);
    fprintf(f, "  Std dev: %.3f ms (%.2f%% of the mean)\n", r->st.sd, r->st.mean > 0 ? 100 * r->st.sd / r->st.mean : 0);
    if (r->split) {
        fmt_ms(r->st_load.median, a, sizeof a);
        fmt_ms(r->st_proc.median, b, sizeof b);
        fprintf(f, "  Load median %s, Process median %s\n", a, b);
    }
    if (r->trades > 0)
        fprintf(f, "  Time per trade: %.1f ns   Throughput: %.0f trades/s\n",
                r->st.median * 1e6 / (double)r->trades, (double)r->trades / (r->st.median / 1000));

    /* memory and hardware counters */
    if (!python) {
        fprintf(f, "\nMEMORY\n  Peak memory: %.1f MB (%s, one extra untimed run after the timed runs)\n", r->peak_mb,
#ifdef _WIN32
                "peak working set"
#else
                "maximum resident set size"
#endif
                );
        fprintf(f, "\nPERF STAT (that same extra run, whole program)\n");
        if (r->has_perf)
            fprintf(f, "  cycles:           %.0f\n  instructions:     %.0f\n  IPC:              %.2f\n"
                       "  cache references: %.0f\n  cache misses:     %.0f (%.2f%% of references)\n",
                    r->cycles, r->instructions, r->instructions / r->cycles, r->cache_refs, r->cache_misses,
                    r->cache_refs > 0 ? 100 * r->cache_misses / r->cache_refs : 0);
        else
            fprintf(f, "  not measured: perf works on the Pi only (see logs/raw/%s_perf.txt there)\n", r->id);
    }

    /* comparisons */
    fprintf(f, "\nSPEEDUP vs " BASE_LABEL " (Version 0)\n");
    if (!python && r->prog == BASE_PROG && r->flag == BASE_FLAG)
        fprintf(f, "  this IS Version 0, the baseline\n");
    else if (base_ms > 0)
        fprintf(f, "  %.3fx  (%.1f / %.1f ms)%s\n", base_ms / r->st.median, base_ms, r->st.median,
                base_ms < r->st.median ? "  -> slower than Version 0" : "");
    else
        fprintf(f, "  n/a (" BASE_LABEL " has not been measured on this machine)\n");
    /* V8 is V7 with the OHLC in assembly: compare it with V7 at the same flag set */
    if (!python && strcmp(PROGRAMS[r->prog].name, ASM_OHLC_NAME) == 0) {
        const Result *c = version_of(C_OHLC_NAME, r->flag);
        if (c != NULL)
            fprintf(f, "\nvs %s (OHLC in C)\n  Process: %.3fx (%.1f -> %.1f ms)  <- what the assembly changes\n"
                       "  Load + Process: %.3fx\n  Same answer as plain C: %s\n",
                    c->label, c->st_proc.median / r->st_proc.median, c->st_proc.median, r->st_proc.median,
                    c->st.median / r->st.median,
                    strcmp(c->vwap, r->vwap) == 0 && c->buys == r->buys && c->sells == r->sells ? "yes" : "NO - check it");
    }
    fclose(f);
}

/* "1.85", or "-" when perf stat did not run */
static const char *ipc_text(const Result *r) {
    static char buf[16];
    if (!r->has_perf) return "-";
    snprintf(buf, sizeof buf, "%.2f", r->instructions / r->cycles);
    return buf;
}

void write_summary(const char *path, int runs) {
    const Result *best = NULL, *base = NULL;
    char t[5][32];
    int i, f2;
    FILE *f = fopen(path, "w");
    if (f == NULL) return;

    fprintf(f, "EFFICIENCY LADDER - Group 06 - %s\n", MACHINE);
    fprintf(f, "=====================================================================================================\n");
    machine_info(f);
    fprintf(f, "Runs:      %d timed runs per version (+1 warm-up, not counted); medians compared\n", runs);
    fprintf(f, "Versions:  the src/ ladder, the same programs \"make ladder\" builds into bin/\n");
    fprintf(f, "Time:      V0, V1, V2: whole program by now_ns();\n"
               "           V3 and up: the program's own now_ns() Load + Process for %s\n", DATA_KEY);
    fprintf(f, "Baseline:  " BASE_LABEL " = Version 0\n");

    fprintf(f, "\nLADDER\n");
    fprintf(f, "%-44s %9s %9s %9s %9s %9s %9s %10s %8s %6s %8s %5s %s\n", "version", "median", "mean",
            "min", "max", "process", "ns/trade", "speedup", "test20", "mode", "peak MB", "IPC", "signals");
    for (i = 0; i < n_results; i++) {
        const Result *r = &results[i];
        if (strcmp(r->section, "ladder") != 0) continue;
        if (!r->ok) { fprintf(f, "%-44s FAILED: %s\n", r->label, r->error); continue; }
        if (r->prog == BASE_PROG && r->flag == BASE_FLAG) base = r;
        if (best == NULL || r->st.median < best->st.median) best = r;
        fmt_ms(r->st.median, t[0], 32);
        fmt_ms(r->st.mean, t[1], 32);
        fmt_ms(r->st.min, t[2], 32);
        fmt_ms(r->st.max, t[3], 32);
        fmt_ms(r->split ? r->st_proc.median : 0, t[4], 32);
        fprintf(f, "%-44s %9s %9s %9s %9s %9s %9.1f %9.2fx %8s %6s %8.1f %5s %d/%d\n", r->label, t[0], t[1],
                t[2], t[3], t[4], r->trades ? r->st.median * 1e6 / (double)r->trades : 0, r->speedup,
                r->test20, r->st.mode_count >= 2 ? "yes" : "none", r->peak_mb, ipc_text(r), r->buys, r->sells);
    }
    fprintf(f, "\nmode: 'yes' = two or more runs equal at 1 ms (value in the version's log), 'none' = all differ.\n"
               "signals: BUY/SELL on %s. Version 0 (double, naive SMA) gives 20386/20176 on stress_full;\n"
               "running-sum and fixed-point versions differ slightly by design (rounding).\n", DATA_KEY);
    if (best != NULL)
        fprintf(f, "\nFASTEST: %s, median %.1f ms = %.2fx faster than Version 0\n", best->label,
                best->st.median, base ? base->st.median / best->st.median : 0);

    /* V6 vs V5: the same engine and parser, only the array differs */
    fprintf(f, "\nMALLOC vs STATIC ARRAY (" MALLOC_NAME " vs " STATIC_NAME ", identical except the array)\n");
    for (f2 = 0; f2 < N_FLAGS; f2++) {
        const Result *st = version_of(STATIC_NAME, f2), *ml = version_of(MALLOC_NAME, f2);
        if (st && ml)
            fprintf(f, "  %-21s static: load %8.1f + process %6.1f ms | malloc: load %8.1f + process %6.1f ms | malloc %+.1f%%\n",
                    FLAGS[f2].cflags, st->st_load.median, st->st_proc.median, ml->st_load.median,
                    ml->st_proc.median, 100 * (ml->st.median - st->st.median) / st->st.median);
    }
    /* V8 vs V7 (Pi): the same program, only the OHLC is hand-written assembly */
    if (IS_PI) {
        fprintf(f, "\nASSEMBLY OHLC vs C OHLC (" ASM_OHLC_NAME " vs " C_OHLC_NAME ", identical except the OHLC)\n");
        for (f2 = 0; f2 < N_FLAGS; f2++) {
            const Result *c = version_of(C_OHLC_NAME, f2), *a = version_of(ASM_OHLC_NAME, f2);
            if (c && a)
                fprintf(f, "  %-21s C: process %6.1f ms | assembly: process %6.1f ms | assembly %.3fx | same answer: %s\n",
                        FLAGS[f2].cflags, c->st_proc.median, a->st_proc.median, c->st_proc.median / a->st_proc.median,
                        strcmp(c->vwap, a->vwap) == 0 && c->buys == a->buys && c->sells == a->sells ? "yes" : "NO - check it");
        }
    }
    fprintf(f, "\nLogs: ladder/logs/   Charts: ladder/charts/\n");
    fclose(f);
}

/* python = 0: the C ladder versions; 1: the Python versions */
void write_csv(const char *path, int python) {
    int i;
    FILE *f = fopen(path, "w");
    if (f == NULL) { printf("cannot write %s\n", path); return; }
    fprintf(f, "section,id,label,program,flag,variant,runs,median_ms,mean_ms,mode_ms,min_ms,max_ms,sd_ms,"
               "load_median_ms,process_median_ms,trades,ns_per_trade,trades_per_s,speedup_vs_v0,"
               "final_vwap,buys,sells,test20,peak_memory_mb,cycles,instructions,ipc,cache_references,cache_misses\n");
    for (i = 0; i < n_results; i++) {
        const Result *r = &results[i];
        int is_py = strcmp(r->section, "python") == 0;
        if (!r->ok || is_py != python) continue;
        fprintf(f, "%s,%s,%s,%s,%s,%s,%d,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%lld,%.2f,%.0f,%.4f,%s,%d,%d,%s,"
                   "%.1f,%.0f,%.0f,%.3f,%.0f,%.0f\n",
                r->section, r->id, r->label,
                is_py ? PY_SCRIPTS[r->prog].name : PROGRAMS[r->prog].name,
                is_py ? "-" : FLAGS[r->flag].tag, is_py ? "python" : "c",
                r->n, r->st.median, r->st.mean, r->st.mode_count >= 2 ? r->st.mode : 0,
                r->st.min, r->st.max, r->st.sd, r->st_load.median, r->st_proc.median, r->trades,
                r->trades ? r->st.median * 1e6 / (double)r->trades : 0,
                r->st.median > 0 ? (double)r->trades / (r->st.median / 1000) : 0,
                r->speedup, r->vwap[0] ? r->vwap : "-", r->buys, r->sells, r->test20[0] ? r->test20 : "-",
                r->peak_mb, r->cycles, r->instructions, r->has_perf ? r->instructions / r->cycles : 0,
                r->cache_refs, r->cache_misses);
    }
    fclose(f);
}

/* read a results .csv back (the columns written above) */
int read_csv(const char *path, Result *rows, int max) {
    char line[2048], *col[32];
    int n = 0;
    FILE *f = fopen(path, "r");
    if (f == NULL) return 0;
    if (fgets(line, sizeof line, f) == NULL) { fclose(f); return 0; }     /* header */
    while (n < max && fgets(line, sizeof line, f) != NULL) {
        Result *r = &rows[n];
        char *p = line;
        int k, ncols = 0;
        line[strcspn(line, "\r\n")] = '\0';
        while (ncols < 32) {                              /* split on commas */
            col[ncols++] = p;
            if ((p = strchr(p, ',')) == NULL) break;
            *p++ = '\0';
        }
        if (ncols < 23) continue;
        memset(r, 0, sizeof *r);
        snprintf(r->section, sizeof r->section, "%s", col[0]);
        snprintf(r->id, sizeof r->id, "%s", col[1]);
        snprintf(r->label, sizeof r->label, "%s", col[2]);
        r->prog = r->flag = -1;                               /* names back to table indexes */
        for (k = 0; k < N_PROGRAMS; k++) if (strcmp(col[3], PROGRAMS[k].name) == 0) r->prog = k;
        for (k = 0; k < N_PY; k++)       if (strcmp(col[3], PY_SCRIPTS[k].name) == 0) r->prog = k;
        for (k = 0; k < N_FLAGS; k++)    if (strcmp(col[4], FLAGS[k].tag) == 0) r->flag = k;
        r->n = atoi(col[6]);
        r->st.n = r->n;
        r->st.median = atof(col[7]);
        r->st.mean = atof(col[8]);
        r->st.mode = atof(col[9]);
        r->st.mode_count = r->st.mode > 0 ? 2 : 0;           /* 0 in the csv = no mode */
        r->st.min = atof(col[10]);
        r->st.max = atof(col[11]);
        r->st.sd = atof(col[12]);
        r->split = atof(col[13]) > 0;
        r->st_load.median = atof(col[13]);
        r->st_proc.median = atof(col[14]);
        r->trades = atoll(col[15]);
        r->speedup = atof(col[18]);
        snprintf(r->vwap, sizeof r->vwap, "%s", col[19]);
        r->buys = atoi(col[20]);
        r->sells = atoi(col[21]);
        snprintf(r->test20, sizeof r->test20, "%s", col[22]);
        if (ncols >= 29) {                                    /* memory + perf columns */
            r->peak_mb = atof(col[23]);
            r->cycles = atof(col[24]);
            r->instructions = atof(col[25]);
            r->cache_refs = atof(col[27]);
            r->cache_misses = atof(col[28]);
            r->has_perf = r->cycles > 0 && r->instructions > 0;
        }
        r->ok = 1;
        n++;
    }
    fclose(f);
    return n;
}

const Result *find_id(const Result *rows, int n, const char *id) {
    int i;
    for (i = 0; i < n; i++) if (strcmp(rows[i].id, id) == 0) return &rows[i];
    return NULL;
}

const Result *fastest(const Result *rows, int n) {
    const Result *best = NULL;
    int i;
    for (i = 0; i < n; i++)
        if (rows[i].st.median > 0 && (best == NULL || rows[i].st.median < best->st.median)) best = &rows[i];
    return best;
}
