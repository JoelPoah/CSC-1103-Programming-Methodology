/*
 * python.c - stageA_Python.py vs stageA_Python_numpy.py, then the faster one
 * vs stageA.c (Version 0) and vs the fastest C version on this machine.
 *
 * Each script's own "bench" mode runs stress_full BENCH_RUNS (5) times in one
 * process and prints "run N: X s" (time.perf_counter_ns). Those are the runs.
 * The whole process is also timed with now_ns() as a cross-check.
 */
#include "bench.h"
#include <string.h>

void run_python(void) {
    char dir[PATH_LEN], raw[PATH_LEN], path[PATH_LEN], line[512], version[128], title[256];
    Result c_rows[MAX_VERSIONS];
    const Result *stageA, *fastest_c;
    Result *py[2] = { NULL, NULL }, *faster = NULL, *slower = NULL;
    Bar bars[6];
    int i, n, nc;
    FILE *f;

    snprintf(dir, sizeof dir, "%s/%s/python", ROOT, MACHINE);
    snprintf(raw, sizeof raw, "%s/logs/raw", dir);
    mkdir_p(raw);
    snprintf(line, sizeof line, "%s --version", PYTHON);
    first_line(line, version, sizeof version);
    printf("\n=== Python (%s: %s) ===\n", PYTHON, version);

    /* STEP 1 - run each script's bench mode and read its runs */
    for (i = 0; i < N_PY && n_results < MAX_VERSIONS; i++) {
        Result *r = py[i] = &results[n_results++];
        double ms, s;
        int k;
        memset(r, 0, sizeof *r);
        r->prog = i;
        snprintf(r->section, sizeof r->section, "python");
        snprintf(r->id, sizeof r->id, "%s", PY_SCRIPTS[i].name);
        snprintf(r->label, sizeof r->label, "%s.py", PY_SCRIPTS[i].name);
        snprintf(r->run_cmd, sizeof r->run_cmd, "%s %s bench", PYTHON, PY_SCRIPTS[i].script);
        snprintf(path, sizeof path, "%s/%s_output.txt", raw, r->id);
        printf("\n[%d/%d] %s   (5 runs in one process; pure Python takes minutes per run)\n",
               i + 1, N_PY, r->run_cmd);
        if (run_cmd(r->run_cmd, path, &ms) != 0) {
            snprintf(r->error, sizeof r->error, "failed (numpy missing? wrong Python?), see %s", path);
            printf("  ! %s\n", r->error);
            continue;
        }
        r->total_ms = ms;
        if ((f = fopen(path, "r")) == NULL) continue;
        while (fgets(line, sizeof line, f) != NULL) {
            if (sscanf(line, "run %d: %lf s", &k, &s) == 2 && r->n < MAX_RUNS) {
                r->time[r->n++] = s * 1000;
                printf("  run %d: %.3f s\n", k, s);
            }
            else if (sscanf(line, "Trades: %lld", &r->trades) == 1) ;
            else if (sscanf(line, "Final VWAP: %31s", r->vwap) == 1) ;
            else sscanf(line, "Signals: %d BUY, %d SELL", &r->buys, &r->sells);
        }
        fclose(f);
        if (r->n == 0) {
            snprintf(r->error, sizeof r->error, "no \"run N: X s\" lines, see %s", path);
            printf("  ! %s\n", r->error);
            continue;
        }
        r->st = stats_of(r->time, r->n, 100.0);      /* mode rounded to 0.1 s */
        snprintf(r->test20, sizeof r->test20, "-");
        r->ok = 1;
        printf("  median %.3f s (whole process %.1f s by now_ns)\n", r->st.median / 1000, ms / 1000);
    }

    /* STEP 2 - Python vs Python, and the C numbers from this machine's c_results.csv */
    if (py[0] && py[1] && py[0]->ok && py[1]->ok) {
        faster = py[0]->st.median < py[1]->st.median ? py[0] : py[1];
        slower = faster == py[0] ? py[1] : py[0];
    } else if (py[0] && py[0]->ok) faster = py[0];
    else if (py[1] && py[1]->ok) faster = py[1];
    snprintf(path, sizeof path, "%s/%s/c_results.csv", ROOT, MACHINE);
    nc = read_csv(path, c_rows, MAX_VERSIONS);
    stageA = find_id(c_rows, nc, "stageA_O2");
    fastest_c = fastest(c_rows, nc);
    for (i = 0; i < N_PY; i++)
        if (py[i] && py[i]->ok && stageA) py[i]->speedup = stageA->st.median / py[i]->st.median;

    /* STEP 3 - logs, csv, summary */
    for (i = 0; i < N_PY; i++)
        if (py[i] && py[i]->ok) write_version_log(py[i], dir, stageA ? stageA->st.median : 0);
    snprintf(path, sizeof path, "%s/python_results.csv", dir);
    write_csv(path, 1);

    snprintf(path, sizeof path, "%s/summary.txt", dir);
    if ((f = fopen(path, "w")) != NULL) {
        fprintf(f, "PYTHON vs C - Group 06 - %s\n=====================================================\n", MACHINE);
        machine_info(f);
        fprintf(f, "Python:    %s (%s)\n\n", version, PYTHON);
        for (i = 0; i < N_PY; i++) {
            char a[32], b[32];
            if (!py[i]) continue;
            if (!py[i]->ok) { fprintf(f, "%-26s FAILED: %s\n", PY_SCRIPTS[i].name, py[i]->error); continue; }
            fmt_ms(py[i]->st.median, a, sizeof a);
            fmt_ms(py[i]->st.mean, b, sizeof b);
            fprintf(f, "%-26s median %10s   mean %10s   %d runs\n", py[i]->label, a, b, py[i]->n);
        }
        if (stageA) fprintf(f, "%-26s median %8.1f ms   (C Version 0, from ../c_results.csv)\n", "stageA.c -O2", stageA->st.median);
        else        fprintf(f, "(no ../c_results.csv yet: run './benchmark/ladder_bench c' first)\n");
        fprintf(f, "\n");
        if (faster && slower)
            fprintf(f, "1) %s is the FASTER Python version: %.2fx faster than %s.\n",
                    faster->label, slower->st.median / faster->st.median, slower->label);
        if (faster && stageA) {
            if (stageA->st.median <= faster->st.median)
                fprintf(f, "2) stageA.c (-O2) is %.2fx faster than %s.\n", faster->st.median / stageA->st.median, faster->label);
            else
                fprintf(f, "2) %s is %.2fx faster than stageA.c (-O2).\n", faster->label, stageA->st.median / faster->st.median);
        }
        if (faster && fastest_c)
            fprintf(f, "3) The fastest C version here (%s, %.1f ms) is %.1fx faster than %s.\n",
                    fastest_c->label, fastest_c->st.median, faster->st.median / fastest_c->st.median, faster->label);
        fprintf(f, "\nLogs: python/logs/   Charts: python/charts/\n");
        fclose(f);
        printf("\nsummary: %s\n", path);
    }

    /* STEP 4 - charts */
    snprintf(dir, sizeof dir, "%s/%s/python/charts", ROOT, MACHINE);
    n = 0;
    if (stageA) {
        snprintf(bars[n].label, 96, "stageA.c -O2 (Version 0)");
        bars[n].value = stageA->st.median; bars[n].colour = COLOURS[0];
        fmt_ms(stageA->st.median, bars[n].text, 32); n++;
    }
    for (i = 0; i < N_PY; i++) {
        if (!py[i] || !py[i]->ok) continue;
        snprintf(bars[n].label, 96, "%s", py[i]->label);
        bars[n].value = py[i]->st.median; bars[n].colour = COLOURS[5 + i];
        fmt_ms(py[i]->st.median, bars[n].text, 32); n++;
    }
    if (fastest_c && stageA && fastest_c != stageA) {
        snprintf(bars[n].label, 96, "fastest C: %s", fastest_c->label);
        bars[n].value = fastest_c->st.median; bars[n].colour = COLOURS[3];
        fmt_ms(fastest_c->st.median, bars[n].text, 32); n++;
    }
    snprintf(title, sizeof title, "Python vs C on %s (%s, medians), lower is better", DATA_KEY, MACHINE);
    chart_bars(dir, "python_vs_c_time", title, "median time (ms, log scale)", bars, n, 1, 0);
}
