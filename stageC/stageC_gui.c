/*
 * stageC_gui.c - Stage C live replay window: Version 0 vs fastest vs the rival bot (Lucas)
 *
 * What it does:
 *   A GTK3 window that RUNS the real Stage C replay (stageC_replay, the same
 *   program used for the measured results) and draws what it reports, live:
 *     - price with the BUY (green) / SELL (red) signals
 *     - P&L against the rival bot (reaction = latency, as in the replay)
 *     - lag behind the replay clock
 *     - a scoreboard per engine and the money lost to slow code
 *
 *   The replay is started with --feed N, so it prints one "#F" line every
 *   1/N of the replayed market time and one "#S" line per signal (see
 *   stageC_replay.c). Those lines are printed after each trade's time is
 *   taken, so drawing the window never changes the measured times.
 *
 *   "Run on" picks where the replay runs:
 *     - This computer:      starts stageC/stageC_replay(.exe) here
 *     - Raspberry Pi (ssh): starts it on the Pi with
 *                             ssh -tt <host> "cd <folder> && taskset -c 2 ./stageC/stageC_replay ..."
 *                           so the Pi does the processing (and the LED), and this
 *                           window (e.g. on the Windows laptop) only draws.
 *                           Needs ssh key login (no password prompt).
 *   "LED (Pi)" = Off, the onboard status LED (--led) or breadboard LEDs on
 *   GPIO 17 red / GPIO 27 green (--led-gpio); both run the replay with sudo.
 *   "Engines" = Race (v0 and mem at the same time, pinned to cores 2 and 3 on
 *   the Pi), v0 only, or mem only.
 *
 * Build (from the project root):
 *   make gui
 *   or: gcc -O2 -Wall stageC/stageC_gui.c $(pkg-config --cflags --libs gtk+-3.0) -o stageC/stageC_gui
 *   Windows: GTK3 from MSYS2 (pacman -S mingw-w64-ucrt-x86_64-gtk3), build in the UCRT64 shell
 *   Pi:      sudo apt install libgtk-3-dev
 * Run (from the project root, so the default paths work):
 *   ./stageC/stageC_gui
 *
 * Note: unlike the rest of Stage C, this file has to use GTK's callback style
 * (functions passed as pointers) and two reader threads per run, because a
 * window must keep drawing while the replay's output arrives.
 */
#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>          /* TerminateProcess */
#else
#include <signal.h>           /* kill, SIGTERM */
#endif

#define MAX_RUNS     2        /* race = 2 replays at once           */
#define FEED_SAMPLES 1500     /* --feed value: samples per replay   */
#define MAX_SAMPLES  1600     /* room for FEED_SAMPLES + the ends    */
#define MAX_MARKS    400      /* signal markers kept for the chart  */
#define LOG_MAX      200000   /* characters kept in the log view    */
#define TICK_MS      50       /* redraw every 50 ms                 */

#define RUN_LOCAL 0
#define RUN_PI    1

#define MODE_RACE 0
#define MODE_V0   1
#define MODE_MEM  2

/* "LED (Pi)" choices = the replay's LED options */
#define LED_OFF     0
#define LED_ONBOARD 1         /* --led:      Pi status LED                  */
#define LED_GPIO    2         /* --led-gpio: breadboard, GPIO 17 red, 27 green */

#define FIELD_PRICE 0
#define FIELD_PNL   1
#define FIELD_LAG   2

/* ---- one replay process and everything it has reported ---- */
typedef struct {
    int       used;           /* 1 = part of the current run        */
    int       alive;          /* 1 = process still running          */
    int       readers;        /* reader threads still reading       */
    char      engine[8];      /* "v0" or "mem"                      */
    GPid      pid;
    double    red, green, blue;   /* line colour                    */

    int       have_header;    /* "#H" seen                          */
    int       speed;
    double    rival_us;
    long long total;          /* trades to replay                   */
    long long ts_first;       /* market time of the first trade, us */
    double    span_s;         /* replayed market time, s            */

    int       n;              /* samples so far                     */
    double    t_s[MAX_SAMPLES];       /* market seconds since the first trade */
    double    price[MAX_SAMPLES];     /* USDT                                 */
    double    pnl[MAX_SAMPLES];       /* USDT, reaction = latency             */
    double    lag_ms[MAX_SAMPLES];    /* lag behind the replay clock, ms      */

    long long done;           /* latest values                      */
    double    pnl_proc;       /* USDT, reaction = processing time   */
    double    max_lag_ms;
    double    avg_proc_ns;
    int       buys, sells, won, lost;

    int       n_marks, next_mark;     /* ring of the latest signals */
    double    mark_t[MAX_MARKS], mark_price[MAX_MARKS];
    int       mark_sig[MAX_MARKS];

    gint64    wall_start_us;  /* g_get_monotonic_time() at start    */
    gint64    wall_end_us;    /* 0 while running                    */
} Run;

/* ---- the whole window ---- */
Run    runs[MAX_RUNS];
GMutex lock;                 /* guards runs[] and log_pending (reader threads write) */
GString *log_pending;        /* text waiting to be shown in the log view */

GtkWidget *window, *chart;
GtkWidget *combo_where, *entry_host, *entry_folder, *entry_program, *entry_file;
GtkWidget *combo_mode, *combo_preset, *combo_speed;
GtkWidget *spin_minutes, *spin_from, *spin_short, *spin_long, *spin_rival;
GtkWidget *check_pin, *combo_led, *button_start, *button_stop;
GtkWidget *score[MAX_RUNS][10];
GtkWidget *label_lost, *label_state;
GtkTextBuffer *log_buffer;
GtkWidget *log_view;

/* ===================================================================== */
/*  reading the replay's output (reader threads)                         */
/* ===================================================================== */

/* add_log - queue one line for the log view (lock must be held) */
void add_log(Run *r, const char *text) {
    g_string_append_printf(log_pending, "[%s] %s\n", r->engine, text);
}

/* handle_line
 * What:    one line from the replay: "#H" header, "#F" sample, "#S" signal,
 *          anything else goes to the log view (summary, errors)
 * Note:    called by a reader thread with the lock held
 */
void handle_line(Run *r, char *line) {
    char name[16];
    long long done, total, time_us, price, pnl, pnl_proc, lag, max_lag, ts_first, span;
    int speed, s_win, l_win, buys, sells, won, lost, sig, k;
    double rival_us, avg_proc;

    if (strncmp(line, "#H ", 3) == 0) {
        if (sscanf(line, "#H %15s %d %d %d %lf %lld %lld %lld", name, &speed, &s_win, &l_win,
                   &rival_us, &total, &ts_first, &span) == 8) {
            r->have_header = 1;
            r->speed = speed;
            r->rival_us = rival_us;
            r->total = total;
            r->ts_first = ts_first;
            r->span_s = span / 1e6;
            if (r->span_s <= 0.0) {
                r->span_s = 1.0;
            }
        }
    } else if (strncmp(line, "#F ", 3) == 0) {
        if (sscanf(line, "#F %lld %lld %lld %lld %lld %lld %lld %lld %d %d %d %d %lf",
                   &done, &total, &time_us, &price, &pnl, &pnl_proc, &lag, &max_lag, &buys,
                   &sells, &won, &lost, &avg_proc) == 13 && r->have_header) {
            if (r->n < MAX_SAMPLES) {
                k = r->n;
                r->t_s[k] = (time_us - r->ts_first) / 1e6;
                r->price[k] = price / 100.0;
                r->pnl[k] = pnl / 100.0;
                r->lag_ms[k] = lag / 1e6;
                r->n++;
            }
            r->done = done;
            r->pnl_proc = pnl_proc / 100.0;
            r->max_lag_ms = max_lag / 1e6;
            r->avg_proc_ns = avg_proc;
            r->buys = buys;
            r->sells = sells;
            r->won = won;
            r->lost = lost;
        }
    } else if (strncmp(line, "#S ", 3) == 0) {
        if (sscanf(line, "#S %d %lld %lld %lld", &sig, &time_us, &price, &lag) == 4 &&
            r->have_header) {
            k = r->next_mark;
            r->mark_sig[k] = sig;
            r->mark_t[k] = (time_us - r->ts_first) / 1e6;
            r->mark_price[k] = price / 100.0;
            r->next_mark = (k + 1) % MAX_MARKS;
            if (r->n_marks < MAX_MARKS) {
                r->n_marks++;
            }
        }
    } else if (line[0] != '\0') {
        add_log(r, line);
    }
}

/* reader_args - what one reader thread needs */
typedef struct {
    Run  *run;
    FILE *fp;
} ReaderArgs;

/* reader_thread
 * What:    read the replay's output line by line until it ends
 *          (one thread for stdout, one for stderr, so ssh errors are shown too)
 */
gpointer reader_thread(gpointer data) {
    ReaderArgs *a = data;
    char line[1024];
    int len;

    while (fgets(line, sizeof line, a->fp) != NULL) {
        len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[len - 1] = '\0';         /* ssh -tt sends "\r\n" */
            len--;
        }
        g_mutex_lock(&lock);
        handle_line(a->run, line);
        g_mutex_unlock(&lock);
    }
    fclose(a->fp);
    g_mutex_lock(&lock);
    a->run->readers--;
    g_mutex_unlock(&lock);
    g_free(a);
    return NULL;
}

/* start_reader - read one of the child's pipes on its own thread */
void start_reader(Run *r, int fd) {
    ReaderArgs *a = g_new(ReaderArgs, 1);
    GThread *t;

    a->run = r;
    a->fp = fdopen(fd, "r");
    if (a->fp == NULL) {
        g_free(a);
        return;
    }
    r->readers++;
    t = g_thread_new("reader", reader_thread, a);
    g_thread_unref(t);                    /* the thread ends by itself */
}

/* child_exited - the replay process has finished (runs in the GTK main loop) */
void child_exited(GPid pid, gint status, gpointer data) {
    Run *r = data;

    g_mutex_lock(&lock);
    r->alive = 0;
    r->wall_end_us = g_get_monotonic_time();
    g_mutex_unlock(&lock);
    g_spawn_close_pid(pid);
}

/* stop_run - end a replay that is still running (Stop button, closing the window) */
void stop_run(Run *r) {
    if (!r->used || !r->alive) {
        return;
    }
#ifdef _WIN32
    TerminateProcess(r->pid, 1);
#else
    kill(r->pid, SIGTERM);
#endif
}

/* ===================================================================== */
/*  starting a run                                                       */
/* ===================================================================== */

/* entry_text - the text in an entry, with spaces trimmed (caller frees) */
gchar *entry_text(GtkWidget *entry) {
    return g_strstrip(g_strdup(gtk_entry_get_text(GTK_ENTRY(entry))));
}

/* speed_value - the speed chosen in the speed box ("0 (no clock)" -> 0) */
int speed_value(void) {
    gchar *text = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(combo_speed));
    int speed = 0;

    if (text != NULL) {
        speed = atoi(text);
        g_free(text);
    }
    return speed;
}

/* replay_args
 * What:    the replay's options as one string, e.g.
 *          "data/x.csv --engine mem --speed 1000 ... --feed 1500"
 *          led = LED_OFF, LED_ONBOARD (--led) or LED_GPIO (--led-gpio)
 */
gchar *replay_args(const char *engine, const char *file, int led) {
    char rival[32];
    const char *led_flag = "";

    if (led == LED_ONBOARD) {
        led_flag = " --led";
    } else if (led == LED_GPIO) {
        led_flag = " --led-gpio";
    }
    g_ascii_formatd(rival, sizeof rival, "%g",
                    gtk_spin_button_get_value(GTK_SPIN_BUTTON(spin_rival)));
    return g_strdup_printf("%s --engine %s --speed %d --minutes %d --from-min %d "
                           "--short %d --long %d --rival %s --feed %d%s",
                           file, engine, speed_value(),
                           gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin_minutes)),
                           gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin_from)),
                           gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin_short)),
                           gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin_long)),
                           rival, FEED_SAMPLES, led_flag);
}

/* start_one
 * What:    start one replay process for engine ("v0" / "mem") in runs[slot]
 * Returns: 1 if it started, 0 if not (the reason is in the log)
 */
int start_one(int slot, const char *engine, int core, int led) {
    Run *r = &runs[slot];
    gchar *file = entry_text(entry_file);
    gchar *args = replay_args(engine, file, led);
    gchar *command, *remote;
    gchar **argv = NULL;
    GError *err = NULL;
    gint out_fd, err_fd;
    int where = gtk_combo_box_get_active(GTK_COMBO_BOX(combo_where));
    int ok;

    memset(r, 0, sizeof *r);
    r->used = 1;
    g_strlcpy(r->engine, engine, sizeof r->engine);
    if (strcmp(engine, "v0") == 0) {
        r->red = 0.90; r->green = 0.45; r->blue = 0.05;     /* orange */
    } else {
        r->red = 0.10; r->green = 0.40; r->blue = 0.85;     /* blue   */
    }

    if (where == RUN_PI) {
        gchar *host = entry_text(entry_host);
        gchar *folder = entry_text(entry_folder);
        gchar *prefix;

        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_pin))) {
            prefix = g_strdup_printf("taskset -c %d ", core);
        } else {
            prefix = g_strdup("");
        }
        if (led) {
            gchar *with_sudo = g_strconcat("sudo ", prefix, NULL);
            g_free(prefix);
            prefix = with_sudo;
        }
        remote = g_strdup_printf("cd %s && %s./stageC/stageC_replay %s 2>&1", folder, prefix, args);
        argv = g_new0(gchar *, 8);
        argv[0] = g_strdup("ssh");
        argv[1] = g_strdup("-tt");                   /* remote replay stops when we do */
        argv[2] = g_strdup("-o");
        argv[3] = g_strdup("BatchMode=yes");         /* fail instead of asking a password */
        argv[4] = g_strdup(host);
        argv[5] = remote;
        command = g_strdup_printf("ssh -tt %s \"%s\"", host, remote);
        g_free(host);
        g_free(folder);
        g_free(prefix);
    } else {
        gchar *program = entry_text(entry_program);
        gchar *line = g_strdup_printf("\"%s\" %s", program, args);
        if (!g_shell_parse_argv(line, NULL, &argv, &err)) {
            g_mutex_lock(&lock);
            add_log(r, err->message);
            g_mutex_unlock(&lock);
            g_error_free(err);
            g_free(line);
            g_free(program);
            g_free(file);
            g_free(args);
            r->used = 0;
            return 0;
        }
        command = line;
        g_free(program);
    }

    g_mutex_lock(&lock);
    g_string_append_printf(log_pending, "[%s] $ %s\n", engine, command);
    g_mutex_unlock(&lock);

    ok = g_spawn_async_with_pipes(NULL, argv, NULL,
                                  G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD,
                                  NULL, NULL, &r->pid, NULL, &out_fd, &err_fd, &err);
    if (!ok) {
        g_mutex_lock(&lock);
        add_log(r, err->message);
        g_mutex_unlock(&lock);
        g_error_free(err);
        r->used = 0;
    } else {
        r->alive = 1;
        r->wall_start_us = g_get_monotonic_time();
        start_reader(r, out_fd);
        start_reader(r, err_fd);
        g_child_watch_add(r->pid, child_exited, r);
    }
    g_strfreev(argv);
    g_free(command);
    g_free(file);
    g_free(args);
    return ok;
}

/* any_running - is a replay process or reader thread still going? */
int any_running(void) {
    int i, running = 0;

    g_mutex_lock(&lock);
    for (i = 0; i < MAX_RUNS; i++) {
        if (runs[i].used && (runs[i].alive || runs[i].readers > 0)) {
            running = 1;
        }
    }
    g_mutex_unlock(&lock);
    return running;
}

/* on_start - Start button: one replay, or two for the race */
void on_start(GtkButton *button, gpointer data) {
    int mode = gtk_combo_box_get_active(GTK_COMBO_BOX(combo_mode));
    int led = LED_OFF;
    int i;

    if (gtk_combo_box_get_active(GTK_COMBO_BOX(combo_where)) == RUN_PI) {
        led = gtk_combo_box_get_active(GTK_COMBO_BOX(combo_led));
    }

    if (any_running()) {
        return;
    }
    g_mutex_lock(&lock);
    for (i = 0; i < MAX_RUNS; i++) {
        runs[i].used = 0;
    }
    g_mutex_unlock(&lock);
    gtk_text_buffer_set_text(log_buffer, "", -1);

    /* the LED shows the fastest version's signals (one LED, one engine) */
    if (mode == MODE_RACE) {
        start_one(0, "v0", 2, 0);
        start_one(1, "mem", 3, led);
    } else if (mode == MODE_V0) {
        start_one(0, "v0", 2, led);
    } else {
        start_one(1, "mem", 2, led);
    }
}

/* on_stop - Stop button */
void on_stop(GtkButton *button, gpointer data) {
    int i;

    for (i = 0; i < MAX_RUNS; i++) {
        stop_run(&runs[i]);
    }
}

/* on_preset - the brief's runs: fill in speed and minutes */
void on_preset(GtkComboBox *box, gpointer data) {
    int p = gtk_combo_box_get_active(box);
    GtkWidget *speed_entry = gtk_bin_get_child(GTK_BIN(combo_speed));

    if (p == 1) {                         /* 1x for 10 minutes */
        gtk_entry_set_text(GTK_ENTRY(speed_entry), "1");
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_minutes), 10);
    } else if (p == 2) {                  /* 100x, whole day   */
        gtk_entry_set_text(GTK_ENTRY(speed_entry), "100");
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_minutes), 0);
    } else if (p == 3) {                  /* 1000x, whole day  */
        gtk_entry_set_text(GTK_ENTRY(speed_entry), "1000");
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_minutes), 0);
    } else if (p == 4) {                  /* race demo         */
        gtk_entry_set_text(GTK_ENTRY(speed_entry), "5000");
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_minutes), 0);
        gtk_combo_box_set_active(GTK_COMBO_BOX(combo_mode), MODE_RACE);
    }
}

/* on_where - Pi-only settings are only usable when running on the Pi */
void on_where(GtkComboBox *box, gpointer data) {
    int pi = gtk_combo_box_get_active(box) == RUN_PI;

    gtk_widget_set_sensitive(entry_host, pi);
    gtk_widget_set_sensitive(entry_folder, pi);
    gtk_widget_set_sensitive(check_pin, pi);
    gtk_widget_set_sensitive(combo_led, pi);
    gtk_widget_set_sensitive(entry_program, !pi);
}

/* on_browse - pick the CSV file */
void on_browse(GtkButton *button, gpointer data) {
    GtkWidget *dialog = gtk_file_chooser_dialog_new("Trade file", GTK_WINDOW(window),
                                                    GTK_FILE_CHOOSER_ACTION_OPEN,
                                                    "_Cancel", GTK_RESPONSE_CANCEL,
                                                    "_Open", GTK_RESPONSE_ACCEPT, NULL);
    gchar *name, *rel;
    gchar *here = g_get_current_dir();

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        name = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        rel = name;
        /* keep it relative to the project root when it is inside it */
        if (g_str_has_prefix(name, here) && strlen(name) > strlen(here) + 1) {
            rel = name + strlen(here) + 1;
        }
        gtk_entry_set_text(GTK_ENTRY(entry_file), rel);
        g_free(name);
    }
    g_free(here);
    gtk_widget_destroy(dialog);
}

/* ===================================================================== */
/*  drawing the charts                                                   */
/* ===================================================================== */

/* sample_value - one value of a sample for a chart field */
double sample_value(Run *r, int field, int k) {
    if (field == FIELD_PRICE) {
        return r->price[k];
    }
    if (field == FIELD_PNL) {
        return r->pnl[k];
    }
    return r->lag_ms[k];
}

/* leader - the run with the most samples (draws the price line) */
Run *leader(void) {
    Run *best = NULL;
    int i;

    for (i = 0; i < MAX_RUNS; i++) {
        if (runs[i].used && runs[i].have_header && (best == NULL || runs[i].n > best->n)) {
            best = &runs[i];
        }
    }
    return best;
}

/* clock_text - market time of day as HH:MM (UTC) */
void clock_text(char *out, int size, long long ts_first, double offset_s) {
    long long sec = (ts_first / 1000000LL + (long long)offset_s) % 86400LL;

    g_snprintf(out, size, "%02lld:%02lld", sec / 3600, (sec / 60) % 60);
}

/* draw_panel
 * What:    one chart: frame, grid, labels, then a line per run (or the leader's
 *          price with signal markers), and a "now" cursor per run
 */
void draw_panel(cairo_t *cr, double x, double y, double w, double h, const char *title,
                int field, Run *lead) {
    double lo = 0.0, hi = 0.0, v, px, py, span = lead->span_s;
    int i, k, first = 1, j;
    char text[64];

    /* y range over every series drawn in this panel */
    for (i = 0; i < MAX_RUNS; i++) {
        Run *r = &runs[i];
        if (!r->used || (field == FIELD_PRICE && r != lead)) {
            continue;
        }
        for (k = 0; k < r->n; k++) {
            v = sample_value(r, field, k);
            if (first || v < lo) {
                lo = v;
            }
            if (first || v > hi) {
                hi = v;
            }
            first = 0;
        }
    }
    if (field != FIELD_PRICE) {          /* P&L and lag: always show zero */
        if (lo > 0.0) {
            lo = 0.0;
        }
        if (hi < 0.0) {
            hi = 0.0;
        }
    }
    if (hi - lo < 1e-9) {
        hi = lo + 1.0;
    }
    v = (hi - lo) * 0.08;                /* a little room above and below */
    hi = hi + v;
    if (field == FIELD_PRICE || lo < 0.0) {
        lo = lo - v;
    }

    /* frame and title */
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_rectangle(cr, x, y, w, h);
    cairo_fill(cr);
    cairo_set_source_rgb(cr, 0.75, 0.75, 0.75);
    cairo_set_line_width(cr, 1);
    cairo_rectangle(cr, x + 0.5, y + 0.5, w, h);
    cairo_stroke(cr);
    cairo_set_source_rgb(cr, 0.15, 0.15, 0.15);
    cairo_set_font_size(cr, 12);
    cairo_move_to(cr, x + 6, y - 6);
    cairo_show_text(cr, title);

    /* grid and labels: 5 across, 4 up */
    cairo_set_font_size(cr, 10);
    for (j = 0; j <= 4; j++) {
        py = y + h - h * j / 4.0;
        cairo_set_source_rgb(cr, 0.92, 0.92, 0.92);
        cairo_move_to(cr, x, py);
        cairo_line_to(cr, x + w, py);
        cairo_stroke(cr);
        cairo_set_source_rgb(cr, 0.35, 0.35, 0.35);
        v = lo + (hi - lo) * j / 4.0;
        if (field == FIELD_LAG) {
            g_snprintf(text, sizeof text, "%.2f", v);
        } else {
            g_snprintf(text, sizeof text, "%.0f", v);
        }
        cairo_move_to(cr, x - 6 - 6.2 * strlen(text), py + 4);
        cairo_show_text(cr, text);
    }
    for (j = 0; j <= 6; j++) {
        px = x + w * j / 6.0;
        cairo_set_source_rgb(cr, 0.92, 0.92, 0.92);
        cairo_move_to(cr, px, y);
        cairo_line_to(cr, px, y + h);
        cairo_stroke(cr);
        clock_text(text, sizeof text, lead->ts_first, span * j / 6.0);
        cairo_set_source_rgb(cr, 0.35, 0.35, 0.35);
        cairo_move_to(cr, px - 14, y + h + 13);
        cairo_show_text(cr, text);
    }
    if (lo < 0.0 && hi > 0.0) {          /* zero line */
        py = y + h - h * (0.0 - lo) / (hi - lo);
        cairo_set_source_rgb(cr, 0.55, 0.55, 0.55);
        cairo_move_to(cr, x, py);
        cairo_line_to(cr, x + w, py);
        cairo_stroke(cr);
    }

    /* the lines */
    cairo_save(cr);
    cairo_rectangle(cr, x, y, w, h);
    cairo_clip(cr);
    for (i = 0; i < MAX_RUNS; i++) {
        Run *r = &runs[i];
        if (!r->used || r->n == 0 || (field == FIELD_PRICE && r != lead)) {
            continue;
        }
        if (field == FIELD_PRICE) {
            cairo_set_source_rgb(cr, 0.25, 0.25, 0.30);
        } else {
            cairo_set_source_rgb(cr, r->red, r->green, r->blue);
        }
        cairo_set_line_width(cr, 1.6);
        for (k = 0; k < r->n; k++) {
            px = x + w * r->t_s[k] / span;
            py = y + h - h * (sample_value(r, field, k) - lo) / (hi - lo);
            if (k == 0) {
                cairo_move_to(cr, px, py);
            } else {
                cairo_line_to(cr, px, py);
            }
        }
        cairo_stroke(cr);

        /* "now": where this engine has got to in market time */
        px = x + w * r->t_s[r->n - 1] / span;
        cairo_set_source_rgba(cr, r->red, r->green, r->blue, 0.55);
        cairo_set_line_width(cr, 1);
        cairo_move_to(cr, px, y);
        cairo_line_to(cr, px, y + h);
        cairo_stroke(cr);
    }

    /* the latest signals on the price line: green up = BUY, red down = SELL */
    if (field == FIELD_PRICE) {
        for (k = 0; k < lead->n_marks; k++) {
            px = x + w * lead->mark_t[k] / span;
            py = y + h - h * (lead->mark_price[k] - lo) / (hi - lo);
            if (lead->mark_sig[k] > 0) {
                cairo_set_source_rgb(cr, 0.10, 0.65, 0.20);
                cairo_move_to(cr, px, py - 5);
                cairo_line_to(cr, px - 4, py + 3);
                cairo_line_to(cr, px + 4, py + 3);
            } else {
                cairo_set_source_rgb(cr, 0.85, 0.15, 0.15);
                cairo_move_to(cr, px, py + 5);
                cairo_line_to(cr, px - 4, py - 3);
                cairo_line_to(cr, px + 4, py - 3);
            }
            cairo_close_path(cr);
            cairo_fill(cr);
        }
    }
    cairo_restore(cr);
}

/* on_draw - the chart area: price, P&L and lag panels */
gboolean on_draw(GtkWidget *widget, cairo_t *cr, gpointer data) {
    double w = gtk_widget_get_allocated_width(widget);
    double h = gtk_widget_get_allocated_height(widget);
    double left = 78, right = 16, top = 26, gap = 44, ph;
    Run *lead;
    char title[128];
    int i;
    double lx;

    cairo_set_source_rgb(cr, 0.97, 0.97, 0.98);
    cairo_paint(cr);

    g_mutex_lock(&lock);
    lead = leader();
    if (lead == NULL || lead->n == 0) {
        g_mutex_unlock(&lock);
        cairo_set_source_rgb(cr, 0.4, 0.4, 0.4);
        cairo_set_font_size(cr, 14);
        cairo_move_to(cr, 30, 40);
        cairo_show_text(cr, "Choose the settings on the left and press Start.");
        return FALSE;
    }

    ph = (h - top - 2 * gap - 24) / 3.0;
    draw_panel(cr, left, top, w - left - right, ph,
               "Price (USDT)   green = BUY, red = SELL (latest signals)", FIELD_PRICE, lead);
    g_snprintf(title, sizeof title, "P&L against the rival bot (USDT), R = %g us, reaction = latency",
               lead->rival_us);
    draw_panel(cr, left, top + ph + gap, w - left - right, ph, title, FIELD_PNL, lead);
    draw_panel(cr, left, top + 2 * (ph + gap), w - left - right, ph,
               "Lag behind the replay clock (ms)", FIELD_LAG, lead);

    /* legend */
    lx = w - right - 150;
    cairo_set_font_size(cr, 12);
    for (i = 0; i < MAX_RUNS; i++) {
        if (!runs[i].used) {
            continue;
        }
        cairo_set_source_rgb(cr, runs[i].red, runs[i].green, runs[i].blue);
        cairo_rectangle(cr, lx, top + ph + gap - 16, 14, 4);
        cairo_fill(cr);
        cairo_move_to(cr, lx + 18, top + ph + gap - 11);
        if (strcmp(runs[i].engine, "v0") == 0) {
            cairo_show_text(cr, "v0 (Version 0)");
        } else {
            cairo_show_text(cr, "mem (fastest)");
        }
        lx = lx + 120;
        if (lx > w - right - 60) {
            lx = w - right - 270;
        }
    }
    g_mutex_unlock(&lock);
    return FALSE;
}

/* ===================================================================== */
/*  scoreboard, log, timer                                               */
/* ===================================================================== */

/* pnl_at - a run's P&L at market time t (latest sample not after t) */
double pnl_at(Run *r, double t) {
    int k = r->n - 1;

    while (k > 0 && r->t_s[k] > t) {
        k--;
    }
    return r->pnl[k];
}

/* update_scoreboard - one row per engine, and the money lost to slow code */
void update_scoreboard(void) {
    char text[10][64], clock[16];
    int i, c;
    Run *v0 = &runs[0], *mem = &runs[1];
    double t, wall;

    for (i = 0; i < MAX_RUNS; i++) {
        Run *r = &runs[i];
        if (!r->used || !r->have_header || r->n == 0) {
            for (c = 0; c < 10; c++) {
                gtk_label_set_text(GTK_LABEL(score[i][c]), "-");
            }
            /* before the first sample the replay is loading the file
             * (v0 with "Start at minute" also reads every earlier line) */
            if (r->used && r->alive) {
                gtk_label_set_text(GTK_LABEL(score[i][9]), "loading...");
            } else if (r->used) {
                gtk_label_set_text(GTK_LABEL(score[i][9]), "ended (see output)");
            }
            continue;
        }
        clock_text(clock, sizeof clock, r->ts_first, r->t_s[r->n - 1]);
        if (r->wall_end_us > 0) {
            wall = (r->wall_end_us - r->wall_start_us) / 1e6;
        } else {
            wall = (g_get_monotonic_time() - r->wall_start_us) / 1e6;
        }
        g_snprintf(text[0], 64, "%.1f %%", 100.0 * r->done / r->total);
        g_snprintf(text[1], 64, "%s  (%.1f s)", clock, wall);
        g_snprintf(text[2], 64, "%.3f", r->lag_ms[r->n - 1]);
        g_snprintf(text[3], 64, "%.3f", r->max_lag_ms);
        g_snprintf(text[4], 64, "%.1f", r->avg_proc_ns);
        g_snprintf(text[5], 64, "%+.2f", r->pnl[r->n - 1]);
        g_snprintf(text[6], 64, "%+.2f", r->pnl_proc);
        g_snprintf(text[7], 64, "%d / %d", r->won, r->won + r->lost);
        g_snprintf(text[8], 64, "%d / %d", r->buys, r->sells);
        if (r->alive) {
            g_snprintf(text[9], 64, "running %dx", r->speed);
        } else {
            g_snprintf(text[9], 64, "finished");
        }
        for (c = 0; c < 10; c++) {
            gtk_label_set_text(GTK_LABEL(score[i][c]), text[c]);
        }
    }

    /* money lost = P&L(mem) - P&L(v0), compared at the same market time
     * (v0 may be behind mem, so use mem's P&L at v0's market time) */
    if (v0->used && mem->used && v0->n > 0 && mem->n > 0) {
        t = v0->t_s[v0->n - 1];
        g_snprintf(text[0], 64, "%+.2f USDT", pnl_at(mem, t) - v0->pnl[v0->n - 1]);
        clock_text(clock, sizeof clock, v0->ts_first, t);
        g_snprintf(text[1], 64, "Money lost to slow code, P&L(mem) - P&L(v0) up to %s:  %s",
                   clock, text[0]);
        gtk_label_set_text(GTK_LABEL(label_lost), text[1]);
    } else if (v0->used && mem->used) {
        gtk_label_set_text(GTK_LABEL(label_lost),
                           "Money lost to slow code: waiting for both engines to start.");
    } else {
        gtk_label_set_text(GTK_LABEL(label_lost),
                           "Money lost to slow code: choose Race to compare v0 and mem.");
    }
}

/* on_tick - every 50 ms: scoreboard, log and chart */
gboolean on_tick(gpointer data) {
    GtkTextIter end;
    int running;

    g_mutex_lock(&lock);
    update_scoreboard();
    if (log_pending->len > 0) {
        gtk_text_buffer_get_end_iter(log_buffer, &end);
        gtk_text_buffer_insert(log_buffer, &end, log_pending->str, -1);
        g_string_truncate(log_pending, 0);
        if (gtk_text_buffer_get_char_count(log_buffer) > LOG_MAX) {
            GtkTextIter a, b;
            gtk_text_buffer_get_start_iter(log_buffer, &a);
            gtk_text_buffer_get_iter_at_offset(log_buffer, &b, LOG_MAX / 4);
            gtk_text_buffer_delete(log_buffer, &a, &b);
        }
        gtk_text_buffer_get_end_iter(log_buffer, &end);
        gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(log_view), &end, 0, FALSE, 0, 0);
    }
    g_mutex_unlock(&lock);

    running = any_running();
    gtk_widget_set_sensitive(button_start, !running);
    gtk_widget_set_sensitive(button_stop, running);
    if (running) {
        gtk_label_set_text(GTK_LABEL(label_state), "Running...");
    } else {
        gtk_label_set_text(GTK_LABEL(label_state), "Ready.");
    }
    gtk_widget_queue_draw(chart);
    return TRUE;                          /* keep the timer */
}

/* on_close - stop any replay before the window goes */
void on_close(GtkWidget *widget, gpointer data) {
    on_stop(NULL, NULL);
    gtk_main_quit();
}

/* ===================================================================== */
/*  building the window                                                  */
/* ===================================================================== */

/* add_row - a label and a widget on one row of the settings grid */
void add_row(GtkWidget *grid, int row, const char *text, GtkWidget *widget) {
    GtkWidget *label = gtk_label_new(text);

    gtk_label_set_xalign(GTK_LABEL(label), 0.0);
    gtk_grid_attach(GTK_GRID(grid), label, 0, row, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), widget, 1, row, 1, 1);
}

/* spin - a number box */
GtkWidget *spin(double lo, double hi, double step, double value, int digits) {
    GtkWidget *s = gtk_spin_button_new_with_range(lo, hi, step);

    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(s), digits);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(s), value);
    return s;
}

/* build_settings - the left column */
GtkWidget *build_settings(void) {
    GtkWidget *grid = gtk_grid_new();
    GtkWidget *file_box, *browse, *buttons, *title;
    int row = 0;

    gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 10);

    title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title), "<b>Where</b>");
    gtk_label_set_xalign(GTK_LABEL(title), 0.0);
    gtk_grid_attach(GTK_GRID(grid), title, 0, row++, 2, 1);

    combo_where = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_where), "This computer");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_where), "Raspberry Pi (ssh)");
    gtk_combo_box_set_active(GTK_COMBO_BOX(combo_where), RUN_LOCAL);
    add_row(grid, row++, "Run on", combo_where);

    entry_host = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(entry_host), "pi@raspberrypi.local");
    add_row(grid, row++, "Pi ssh host", entry_host);

    entry_folder = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(entry_folder), "~/CSC-1103-Programming-Methodology");
    add_row(grid, row++, "Pi project folder", entry_folder);

    entry_program = gtk_entry_new();
#ifdef _WIN32
    gtk_entry_set_text(GTK_ENTRY(entry_program), "stageC/stageC_replay.exe");
#else
    gtk_entry_set_text(GTK_ENTRY(entry_program), "./stageC/stageC_replay");
#endif
    add_row(grid, row++, "Replay program", entry_program);

    check_pin = gtk_check_button_new_with_label("taskset: v0 on core 2, mem on core 3");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_pin), TRUE);
    add_row(grid, row++, "Pin (Pi)", check_pin);

    combo_led = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_led), "Off");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_led), "Onboard status LED (sudo)");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_led),
                                   "Breadboard: GPIO 17 red, GPIO 27 green (sudo)");
    gtk_combo_box_set_active(GTK_COMBO_BOX(combo_led), LED_OFF);
    add_row(grid, row++, "LED (Pi)", combo_led);

    title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title), "<b>Replay</b>");
    gtk_label_set_xalign(GTK_LABEL(title), 0.0);
    gtk_grid_attach(GTK_GRID(grid), title, 0, row++, 2, 1);

    file_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    entry_file = gtk_entry_new();
    if (g_file_test("data/group_data/stress_full.csv", G_FILE_TEST_EXISTS) ||
        !g_file_test("data/stress_full.csv", G_FILE_TEST_EXISTS)) {
        gtk_entry_set_text(GTK_ENTRY(entry_file), "data/group_data/stress_full.csv");
    } else {
        gtk_entry_set_text(GTK_ENTRY(entry_file), "data/stress_full.csv");
    }
    browse = gtk_button_new_with_label("...");
    g_signal_connect(browse, "clicked", G_CALLBACK(on_browse), NULL);
    gtk_box_pack_start(GTK_BOX(file_box), entry_file, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(file_box), browse, FALSE, FALSE, 0);
    add_row(grid, row++, "Trade file", file_box);

    combo_mode = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_mode), "Race: v0 vs mem");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_mode), "v0 only (Version 0)");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_mode), "mem only (fastest, V7)");
    gtk_combo_box_set_active(GTK_COMBO_BOX(combo_mode), MODE_RACE);
    add_row(grid, row++, "Engines", combo_mode);

    combo_preset = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_preset), "(custom)");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_preset), "Brief: 1x for 10 minutes");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_preset), "Brief: 100x, whole day");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_preset), "Brief: 1000x, whole day");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_preset), "Race demo: 5000x");
    gtk_combo_box_set_active(GTK_COMBO_BOX(combo_preset), 0);
    g_signal_connect(combo_preset, "changed", G_CALLBACK(on_preset), NULL);
    add_row(grid, row++, "Preset", combo_preset);

    combo_speed = gtk_combo_box_text_new_with_entry();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_speed), "1");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_speed), "10");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_speed), "100");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_speed), "1000");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_speed), "5000");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_speed), "10000");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_speed), "0 (no clock)");
    gtk_combo_box_set_active(GTK_COMBO_BOX(combo_speed), 3);
    add_row(grid, row++, "Speed (x)", combo_speed);

    spin_minutes = spin(0, 1440, 1, 0, 0);
    add_row(grid, row++, "Minutes (0 = all)", spin_minutes);
    spin_from = spin(0, 1439, 1, 0, 0);
    add_row(grid, row++, "Start at minute", spin_from);
    spin_short = spin(1, 1000, 1, 35, 0);
    add_row(grid, row++, "Short SMA", spin_short);
    spin_long = spin(2, 1000, 1, 60, 0);
    add_row(grid, row++, "Long SMA", spin_long);

    title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title), "<b>Rival bot</b>");
    gtk_label_set_xalign(GTK_LABEL(title), 0.0);
    gtk_grid_attach(GTK_GRID(grid), title, 0, row++, 2, 1);
    spin_rival = spin(0.1, 100, 0.1, 1.0, 2);
    add_row(grid, row++, "Reaction R (us)", spin_rival);

    buttons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    button_start = gtk_button_new_with_label("Start");
    button_stop = gtk_button_new_with_label("Stop");
    gtk_widget_set_sensitive(button_stop, FALSE);
    g_signal_connect(button_start, "clicked", G_CALLBACK(on_start), NULL);
    g_signal_connect(button_stop, "clicked", G_CALLBACK(on_stop), NULL);
    gtk_box_pack_start(GTK_BOX(buttons), button_start, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(buttons), button_stop, TRUE, TRUE, 0);
    gtk_grid_attach(GTK_GRID(grid), buttons, 0, row++, 2, 1);

    label_state = gtk_label_new("Ready.");
    gtk_label_set_xalign(GTK_LABEL(label_state), 0.0);
    gtk_grid_attach(GTK_GRID(grid), label_state, 0, row++, 2, 1);

    g_signal_connect(combo_where, "changed", G_CALLBACK(on_where), NULL);
    on_where(GTK_COMBO_BOX(combo_where), NULL);
    return grid;
}

/* build_scoreboard - one row per engine */
GtkWidget *build_scoreboard(void) {
    const char *heads[11] = { "Engine", "Done", "Market time (wall)", "Lag ms", "Max lag ms",
                              "Proc avg ns", "P&L (latency)", "P&L (processing)",
                              "Won / signals", "BUY / SELL", "State" };
    const char *names[MAX_RUNS] = {
        "<span foreground='#e67300'><b>v0</b></span>",
        "<span foreground='#1a66d9'><b>mem</b></span>" };
    GtkWidget *grid = gtk_grid_new();
    GtkWidget *label;
    int i, c;

    gtk_grid_set_column_spacing(GTK_GRID(grid), 14);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 2);
    for (c = 0; c < 11; c++) {
        label = gtk_label_new(NULL);
        gchar *m = g_markup_printf_escaped("<small><b>%s</b></small>", heads[c]);
        gtk_label_set_markup(GTK_LABEL(label), m);
        g_free(m);
        gtk_label_set_xalign(GTK_LABEL(label), 0.0);
        gtk_grid_attach(GTK_GRID(grid), label, c, 0, 1, 1);
    }
    for (i = 0; i < MAX_RUNS; i++) {
        label = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(label), names[i]);
        gtk_label_set_xalign(GTK_LABEL(label), 0.0);
        gtk_grid_attach(GTK_GRID(grid), label, 0, i + 1, 1, 1);
        for (c = 0; c < 10; c++) {
            score[i][c] = gtk_label_new("-");
            gtk_label_set_xalign(GTK_LABEL(score[i][c]), 0.0);
            gtk_grid_attach(GTK_GRID(grid), score[i][c], c + 1, i + 1, 1, 1);
        }
    }
    label_lost = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(label_lost), 0.0);
    gtk_grid_attach(GTK_GRID(grid), label_lost, 0, MAX_RUNS + 1, 11, 1);
    return grid;
}

int main(int argc, char *argv[]) {
    GtkWidget *paned, *right, *scroll, *frame;

    gtk_init(&argc, &argv);
    g_mutex_init(&lock);
    log_pending = g_string_new("");

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Stage C live replay: Version 0 vs fastest vs rival bot");
    gtk_window_set_default_size(GTK_WINDOW(window), 1400, 860);
    g_signal_connect(window, "destroy", G_CALLBACK(on_close), NULL);

    paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_container_add(GTK_CONTAINER(window), paned);
    scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_NEVER,
                                   GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scroll), build_settings());
    gtk_paned_pack1(GTK_PANED(paned), scroll, FALSE, FALSE);

    right = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(right), 8);
    gtk_paned_pack2(GTK_PANED(paned), right, TRUE, FALSE);

    gtk_box_pack_start(GTK_BOX(right), build_scoreboard(), FALSE, FALSE, 0);

    chart = gtk_drawing_area_new();
    gtk_widget_set_size_request(chart, 700, 460);
    g_signal_connect(chart, "draw", G_CALLBACK(on_draw), NULL);
    gtk_box_pack_start(GTK_BOX(right), chart, TRUE, TRUE, 0);

    log_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(log_view), FALSE);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(log_view), TRUE);
    log_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(log_view));
    scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_set_size_request(scroll, -1, 150);
    gtk_container_add(GTK_CONTAINER(scroll), log_view);
    frame = gtk_frame_new("Replay output (summary at the end)");
    gtk_container_add(GTK_CONTAINER(frame), scroll);
    gtk_box_pack_start(GTK_BOX(right), frame, FALSE, FALSE, 0);

    g_timeout_add(TICK_MS, on_tick, NULL);
    gtk_widget_show_all(window);
    gtk_main();
    return 0;
}
