/*
 * charts.c - write each chart's numbers to a .dat file, then let gnuplot
 * draw it with one of the fixed scripts in benchmark/charts/:
 *   bars.gnuplot    one bar per version
 *   groups.gnuplot  grouped bars (e.g. one group per program, one bar per flag set)
 * The look of every chart is in benchmark/charts/style.gnuplot.
 */
#include "bench.h"
#include <string.h>

/* chart colours: a colour-blind-checked palette, always used in this order */
const unsigned COLOURS[8] = { 0x2a78d6, 0xeb6834, 0x1baf7a, 0xeda100,
                              0xe87ba4, 0x008300, 0x6250d6, 0xe34948 };

/* open <dir>/data/<name>.dat for writing */
static FILE *open_data(const char *dir, const char *name, char *dat) {
    char folder[PATH_LEN];
    snprintf(folder, sizeof folder, "%s/data", dir);
    mkdir_p(folder);
    snprintf(dat, PATH_LEN, "%s/%s.dat", folder, name);
    return fopen(dat, "w");
}

/* gnuplot -e "dat='...'; out='...'; <vars>" benchmark/charts/<script>.gnuplot */
static void draw(const char *script, const char *dir, const char *name, const char *dat, const char *vars) {
    char cmd[CMD_LEN], png[PATH_LEN], err[PATH_LEN];
    snprintf(png, sizeof png, "%s/%s.png", dir, name);
    snprintf(err, sizeof err, "%s/data/%s_gnuplot.txt", dir, name);
    snprintf(cmd, sizeof cmd, "gnuplot -e \"dat='%s'; out='%s'; %s\" benchmark/charts/%s.gnuplot",
             dat, png, vars, script);
    if (run_cmd(cmd, err, NULL) != 0) {
        printf("  ! no chart %s (is gnuplot installed? see %s)\n", png, err);
        return;
    }
    remove(err);
    printf("  chart: %s\n", png);
}

/* Room for the x labels, so a chart with many bars (the Pi: 50 versions) stays
 * readable: with many bars the labels stand upright (90 degrees) instead of
 * leaning, and the bottom margin grows with the longest label.
 * Returns the bottom margin in text lines; *xrot = the label angle. */
static int label_room(int longest, int upright, int lean, int least, int *xrot) {
    int bm = (int)((upright ? 0.5 : 0.27) * longest) + 3;
    *xrot = upright ? 90 : lean;
    return bm < least ? least : bm;
}

/* lo/hi of the positive values, for the y axis */
static int value_range(const double *v, int n, double *lo, double *hi) {
    int i;
    *lo = 1e300;
    *hi = 0;
    for (i = 0; i < n; i++) {
        if (v[i] > 0 && v[i] < *lo) *lo = v[i];
        if (v[i] > *hi) *hi = v[i];
    }
    return *hi > 0;
}

void chart_bars(const char *dir, const char *name, const char *title, const char *ylabel,
                const Bar *bars, int n, int logy, double ref) {
    char dat[PATH_LEN], vars[1024];
    double v[MAX_VERSIONS], lo, hi;
    FILE *f;
    int i, longest = 0, dense = n > 16, xrot, bm, w;

    for (i = 0; i < n; i++) {
        v[i] = bars[i].value;
        if ((int)strlen(bars[i].label) > longest) longest = (int)strlen(bars[i].label);
    }
    if (n == 0 || !value_range(v, n, &lo, &hi) || (f = open_data(dir, name, dat)) == NULL) return;
    for (i = 0; i < n; i++)    /* index "label" value colour "text on the bar" */
        fprintf(f, "%d \"%s\" %.6f %u \"%s\"\n", i, bars[i].label, bars[i].value, bars[i].colour, bars[i].text);
    fclose(f);
    /* dense (many bars): 34 pixels per bar, upright x labels and upright values */
    bm = label_room(longest, dense, 30, 9, &xrot);
    w = dense ? 200 + n * 34 : n > 10 ? 1700 : 1200;
    snprintf(vars, sizeof vars, "ttl='%s'; sub=''; ylab='%s'; w=%d; h=%d; logy=%d; ylo=%g; yhi=%g; ref=%g; n=%d; "
             "xrot=%d; bm=%d; dense=%d", title, ylabel, w, 650 + bm * 15, logy, lo, hi, ref, n, xrot, bm, dense);
    draw("bars", dir, name, dat, vars);
}

/* vals[row * ncols + col]; 0 = missing. speed_fmt: label bars "1.23x" instead of times.
 * A series with no values at all (e.g. no Python results yet) is left out;
 * every series keeps its own colour either way. */
void chart_groups(const char *dir, const char *name, const char *title, const char *sub,
                  const char *ylabel, char rows[][96], int nrows, char cols[][48], int ncols,
                  const double *vals, int logy, int speed_fmt, double ref) {
    char dat[PATH_LEN], vars[1024], colour_list[64] = "";
    double lo, hi;
    FILE *f;
    int r, c, shown = 0, keep[8], longest = 0, slots, dense, xrot, bm, w;

    if (nrows == 0 || !value_range(vals, nrows * ncols, &lo, &hi) || (f = open_data(dir, name, dat)) == NULL)
        return;
    for (c = 0; c < ncols && c < 8; c++) {                  /* which series have any value */
        keep[c] = 0;
        for (r = 0; r < nrows; r++) if (vals[r * ncols + c] > 0) keep[c] = 1;
        if (keep[c]) {
            snprintf(colour_list + strlen(colour_list), sizeof colour_list - strlen(colour_list), "%d ", c + 1);
            shown++;
        }
    }
    fprintf(f, "\"group\"");                                 /* header row = legend */
    for (c = 0; c < ncols && c < 8; c++) if (keep[c]) fprintf(f, " \"%s\"", cols[c]);
    fprintf(f, "\n");
    for (r = 0; r < nrows; r++) {
        fprintf(f, "\"%s\"", rows[r]);
        for (c = 0; c < ncols && c < 8; c++) {
            double v = vals[r * ncols + c];
            if (!keep[c]) continue;
            if (v > 0) fprintf(f, " %.6f", v);
            else       fprintf(f, " NaN");
        }
        fprintf(f, "\n");
    }
    fclose(f);
    /* slots = bars + one gap per group. dense (many slots): 20 pixels per slot and
     * upright values; many groups: upright x labels too */
    for (r = 0; r < nrows; r++) if ((int)strlen(rows[r]) > longest) longest = (int)strlen(rows[r]);
    slots = nrows * (shown + 1);
    dense = slots > 30;
    bm = label_room(longest, nrows > 12, 20, 7, &xrot);
    w = slots > 18 ? 1700 : 1300;
    if (dense && 220 + slots * 20 > w) w = 220 + slots * 20;
    snprintf(vars, sizeof vars, "ttl='%s'; sub='%s'; ylab='%s'; w=%d; h=%d; logy=%d; ylo=%g; yhi=%g; ref=%g; "
             "ncols=%d; fmt=%d; cidx='%s'; xrot=%d; bm=%d; dense=%d", title, sub, ylabel, w, 700 + bm * 15, logy,
             lo, hi, ref, shown, speed_fmt, colour_list, xrot, bm, dense);
    draw("groups", dir, name, dat, vars);
}
