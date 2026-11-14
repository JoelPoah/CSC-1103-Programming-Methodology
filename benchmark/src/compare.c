/*
 * compare.c - laptop vs Raspberry Pi, and fastest C vs fastest Python.
 * Reads the saved results of BOTH machines, so run it after both have run:
 *   benchmark/results/laptop/...  and  benchmark/results/rpi/...
 */
#include "bench.h"
#include <string.h>

static Result c_rows[2][MAX_VERSIONS], py_rows[2][4];

void run_compare(void) {
    const char *name[2] = { "laptop", "rpi" };
    const char *nice[2] = { "laptop", "Raspberry Pi 5" };
    const Result *base[2], *best_c[2], *best_py[2];
    char dir[PATH_LEN], path[PATH_LEN], title[512], sub[256];
    char rows[MAX_VERSIONS][96], cols[3][48];
    double vals[MAX_VERSIONS * 3];
    int nc[2], np[2], m, i, n;
    Bar bars[4];
    FILE *f;

    /* STEP 1 - read both machines' results (this member's) */
    snprintf(dir, sizeof dir, "%s/compare", ROOT);
    for (m = 0; m < 2; m++) {
        snprintf(path, sizeof path, "%s/%s/c_results.csv", ROOT, name[m]);
        nc[m] = read_csv(path, c_rows[m], MAX_VERSIONS);
        snprintf(path, sizeof path, "%s/%s/python/python_results.csv", ROOT, name[m]);
        np[m] = read_csv(path, py_rows[m], 4);
        base[m] = find_id(c_rows[m], nc[m], BASE_ID);
        best_c[m] = fastest(c_rows[m], nc[m]);
        best_py[m] = fastest(py_rows[m], np[m]);
    }
    printf("\n=== Compare: laptop %d C + %d Python results, rpi %d C + %d Python results ===\n",
           nc[0], np[0], nc[1], np[1]);
    if (nc[0] + nc[1] == 0) { printf("nothing to compare yet: run the benchmark on each machine first\n"); return; }
    mkdir_p(dir);

    /* STEP 2 - compare.txt */
    snprintf(path, sizeof path, "%s/compare.txt", dir);
    if ((f = fopen(path, "w")) == NULL) return;
    fprintf(f, "LAPTOP vs RASPBERRY PI 5, C vs PYTHON - Group 06 (medians on %s)\n", DATA_KEY);
    fprintf(f, "=========================================================================\n\n");
    for (m = 0; m < 2; m++) {
        if (best_c[m] == NULL) { fprintf(f, "%s: no results yet\n\n", nice[m]); continue; }
        fprintf(f, "%s\n  fastest C:      %-40s %10.1f ms", nice[m], best_c[m]->label, best_c[m]->st.median);
        if (base[m]) fprintf(f, "  (%.2fx faster than Version 0, %.1f ms)", base[m]->st.median / best_c[m]->st.median,
                             base[m]->st.median);
        fprintf(f, "\n");
        if (best_py[m])
            fprintf(f, "  fastest Python: %-40s %10.1f ms\n  -> fastest C is %.1fx faster than fastest Python\n",
                    best_py[m]->label, best_py[m]->st.median, best_py[m]->st.median / best_c[m]->st.median);
        else
            fprintf(f, "  fastest Python: (no Python results)\n");
        fprintf(f, "\n");
    }
    if (best_c[0] && best_c[1])
        fprintf(f, "Fastest laptop vs fastest Pi: the laptop is %.2fx faster (%.1f vs %.1f ms)\n\n",
                best_c[1]->st.median / best_c[0]->st.median, best_c[0]->st.median, best_c[1]->st.median);

    fprintf(f, "SAME VERSION ON BOTH MACHINES\n");
    for (i = 0; i < nc[0]; i++) {
        const Result *pi = find_id(c_rows[1], nc[1], c_rows[0][i].id);
        if (pi) fprintf(f, "  %-40s laptop %10.1f ms   Pi %10.1f ms   Pi takes %.2fx as long\n",
                        c_rows[0][i].label, c_rows[0][i].st.median, pi->st.median, pi->st.median / c_rows[0][i].st.median);
    }
    fprintf(f, "\nMALLOC vs STATIC ARRAY at -O2 (" MALLOC_NAME " vs " STATIC_NAME ")\n");
    for (m = 0; m < 2; m++) {
        const Result *s = find_id(c_rows[m], nc[m], STATIC_NAME "_O2"), *ml = find_id(c_rows[m], nc[m], MALLOC_NAME "_O2");
        if (s && ml)
            fprintf(f, "  %-7s static %.1f ms (load %.1f, process %.1f) | malloc %.1f ms (load %.1f, process %.1f) | malloc %+.1f%%\n",
                    name[m], s->st.median, s->st_load.median, s->st_proc.median, ml->st.median,
                    ml->st_load.median, ml->st_proc.median, 100 * (ml->st.median - s->st.median) / s->st.median);
    }
    fclose(f);
    printf("compare: %s\n", path);

    /* STEP 3 - charts */
    snprintf(dir, sizeof dir, "%s/compare/charts", ROOT);

    /* every version both machines ran */
    snprintf(cols[0], 48, "laptop");
    snprintf(cols[1], 48, "Raspberry Pi 5");
    for (i = 0, n = 0; i < nc[0]; i++) {
        const Result *pi = find_id(c_rows[1], nc[1], c_rows[0][i].id);
        if (pi == NULL) continue;
        snprintf(rows[n], 96, "%s", c_rows[0][i].label);
        vals[n * 2] = c_rows[0][i].st.median;
        vals[n * 2 + 1] = pi->st.median;
        n++;
    }
    chart_groups(dir, "laptop_vs_rpi_all_versions", "Every version: laptop vs Raspberry Pi 5 (medians), lower is better",
                 "", "median time (ms, log scale)", rows, n, cols, 2, vals, 1, 0, 0);

    /* the fastest version on each machine, next to its Version 0 */
    for (m = 0, n = 0; m < 2; m++) {
        if (base[m]) {
            snprintf(bars[n].label, 96, "%s: " BASE_LABEL " (Version 0)", name[m]);
            bars[n].value = base[m]->st.median; bars[n].colour = COLOURS[m];
            fmt_ms(base[m]->st.median, bars[n].text, 32); n++;
        }
        if (best_c[m]) {
            snprintf(bars[n].label, 96, "%s fastest: %s", name[m], best_c[m]->label);
            bars[n].value = best_c[m]->st.median; bars[n].colour = COLOURS[m];
            fmt_ms(best_c[m]->st.median, bars[n].text, 32); n++;
        }
    }
    chart_bars(dir, "fastest_laptop_vs_rpi", "Fastest version on each machine vs its Version 0 (blue = laptop, orange = Pi)",
               "median time (ms, log scale)", bars, n, 1, 0);

    /* fastest C vs fastest Python on each machine */
    snprintf(cols[0], 48, BASE_LABEL " (Version 0)");
    snprintf(cols[1], 48, "fastest C");
    snprintf(cols[2], 48, "fastest Python");
    for (m = 0, n = 0; m < 2; m++) {
        if (best_c[m] == NULL && best_py[m] == NULL) continue;
        snprintf(rows[n], 96, "%s", nice[m]);
        vals[n * 3]     = base[m] ? base[m]->st.median : 0;
        vals[n * 3 + 1] = best_c[m] ? best_c[m]->st.median : 0;
        vals[n * 3 + 2] = best_py[m] ? best_py[m]->st.median : 0;
        n++;
    }
    snprintf(title, sizeof title, "Fastest C vs fastest Python on each machine (medians), lower is better");
    snprintf(sub, sizeof sub, "laptop: %s / %s     Pi: %s / %s",
             best_c[0] ? best_c[0]->label : "-", best_py[0] ? best_py[0]->label : "-",
             best_c[1] ? best_c[1]->label : "-", best_py[1] ? best_py[1]->label : "-");
    chart_groups(dir, "fastest_c_vs_fastest_python", title, sub, "median time (ms, log scale)",
                 rows, n, cols, 3, vals, 1, 0, 0);
}
