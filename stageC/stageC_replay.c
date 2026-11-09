/*
 * stageC_replay.c - Stage C real-time replay with LED / terminal signals,
 *                   latency, lag counter and live P&L against the rival bot
 *
 * Trades are released one at a time in timestamp order, on a replay clock:
 *     release time of trade t = (tau_t - tau_first) / speed      (brief, formula 9)
 * The program waits (busy-polls) until each trade's release time, then the
 * chosen engine processes it. For every trade it measures
 *     processing time = decided - started
 *     latency         = decided - released                       (brief, formula 8)
 *     lag             = started - released  (how far behind the replay clock)
 * If the engine is slower than the market, lag builds up and latency grows.
 *
 * Usage (from the project root):
 *   stageC_replay <file.csv> [options]
 *     --engine v0|fast|mem  which version to run (default mem = fastest)
 *     --speed X             replay speed: 1 = real time, 100, 1000 ...
 *                           0 = as fast as possible, no clock (default 1000)
 *     --minutes M           stop after M minutes of MARKET time (0 = all)
 *     --from-min M          start M minutes after the first trade
 *     --short S --long L    SMA windows (default 35 / 60, Group 06)
 *     --rival R             rival reaction time in us (default 1)
 *     --led                 drive the Pi's status LED (green BUY, red SELL)
 *     --signals / --quiet   force per-signal lines on / off (default: on
 *                           for speed <= 100, off above, so printing does
 *                           not flood the terminal)
 *
 * Examples:
 *   real speed, 10 minutes:   stageC_replay data/stress_full.csv --speed 1 --minutes 10 --led
 *   full day at 100x / 1000x: stageC_replay data/stress_full.csv --speed 100
 *                             stageC_replay data/stress_full.csv --speed 1000
 *   race (two terminals):     stageC_replay data/stress_full.csv --engine v0  --speed 5000
 *                             stageC_replay data/stress_full.csv --engine mem --speed 5000 --led
 *   test20 check (3/5):       stageC_replay data/test20.csv --short 3 --long 5 --speed 0 --signals
 *                             -> must show exactly 1 BUY at trade 7, 0 SELL
 *
 * On the Pi, run with sudo for --led, and pin to one core: sudo taskset -c 2 ...
 */
#include "../helper_function/timing.h"   /* must come first (see timing.h) */
#include <stdio.h>
#include <stdlib.h>                      /* atof, atoi */
#include <string.h>                      /* strcmp */
#include <signal.h>                      /* Ctrl-C handler (the C library one) */
#include "../helper_function/led.h"
#include "engine/feed.h"
#include "engine/engine.h"
#include "engine/rival.h"
#include "engine/latency.h"
#include "engine/display.h"

#define STATUS_EVERY_NS 100000000ULL     /* redraw the status bar every 100 ms */
#define SLEEP_IF_MORE_NS 20000000ULL     /* sleep instead of spinning if > 20 ms early */

/* ---- options from the command line ---- */
typedef struct {
    const char *path;
    EngineKind  engine;
    double      speed;          /* 0 = no clock, as fast as possible */
    double      minutes;        /* market minutes to replay, 0 = all */
    double      from_min;       /* skip this many market minutes first */
    int         short_win, long_win;
    double      rival_us;
    int         use_led;
    int         show_signals;   /* -1 = decide from speed */
} Options;

/* set by the Ctrl-C handler so the loop can stop and clean up the LED */
static volatile sig_atomic_t stop_requested = 0;

static void on_ctrl_c(int sig_no) {
    (void)sig_no;
    stop_requested = 1;
}

static void usage(const char *prog) {
    printf("usage: %s <file.csv> [--engine v0|fast|mem] [--speed X] [--minutes M]\n"
           "          [--from-min M] [--short S] [--long L] [--rival R_us]\n"
           "          [--led] [--signals | --quiet]\n", prog);
}

/* parse_options
 * What:    read argv into an Options struct
 * Returns: 1 if OK, 0 on a bad / missing argument
 */
static int parse_options(int argc, char *argv[], Options *o) {
    int i;

    o->path = NULL;
    o->engine = ENGINE_MEM;
    o->speed = 1000.0;
    o->minutes = 0.0;
    o->from_min = 0.0;
    o->short_win = 35;
    o->long_win = 60;
    o->rival_us = 1.0;
    o->use_led = 0;
    o->show_signals = -1;

    for (i = 1; i < argc; i++) {
        const char *a = argv[i];
        int has_value = (i + 1 < argc);

        if (strcmp(a, "--engine") == 0 && has_value) {
            if (!engine_parse_kind(argv[++i], &o->engine)) {
                printf("unknown engine '%s' (use v0, fast or mem)\n", argv[i]);
                return 0;
            }
        } else if (strcmp(a, "--speed") == 0 && has_value) {
            o->speed = atof(argv[++i]);
        } else if (strcmp(a, "--minutes") == 0 && has_value) {
            o->minutes = atof(argv[++i]);
        } else if (strcmp(a, "--from-min") == 0 && has_value) {
            o->from_min = atof(argv[++i]);
        } else if (strcmp(a, "--short") == 0 && has_value) {
            o->short_win = atoi(argv[++i]);
        } else if (strcmp(a, "--long") == 0 && has_value) {
            o->long_win = atoi(argv[++i]);
        } else if (strcmp(a, "--rival") == 0 && has_value) {
            o->rival_us = atof(argv[++i]);
        } else if (strcmp(a, "--led") == 0) {
            o->use_led = 1;
        } else if (strcmp(a, "--signals") == 0) {
            o->show_signals = 1;
        } else if (strcmp(a, "--quiet") == 0) {
            o->show_signals = 0;
        } else if (a[0] != '-' && o->path == NULL) {
            o->path = a;
        } else {
            printf("bad argument: %s\n", a);
            return 0;
        }
    }

    if (o->path == NULL) {
        return 0;
    }
    if (o->short_win < 1 || o->short_win >= o->long_win || o->long_win > MAX_WIN) {
        printf("bad windows: need 1 <= short < long <= %d\n", MAX_WIN);
        return 0;
    }
    if (o->speed < 0) {
        printf("speed must be >= 0\n");
        return 0;
    }
    if (o->show_signals < 0) {
        o->show_signals = (o->speed > 0 && o->speed <= 100);
    }
    return 1;
}

/* wait_until
 * What:    do nothing until the clock reaches target (ns)
 * Why spin: sleeping can overshoot by 1-15 ms (Windows timer tick), far
 *          longer than one trade. So we only sleep when we are more than
 *          20 ms early, and busy-poll the clock for the last part.
 *          (Real low-latency systems busy-poll for the same reason.)
 */
static void wait_until(uint64_t target) {
    for (;;) {
        uint64_t now = now_ns();
        if (now >= target || stop_requested) {
            return;
        }
        if (target - now > SLEEP_IF_MORE_NS) {
            sleep_ms(10);
        }
    }
}

/* show_led
 * What:    green on for BUY, red on for SELL (the other one off)
 * Note:    called AFTER the latency is measured: writing to the LED's sysfs
 *          file is a system call that takes microseconds
 */
static void show_led(int sig, int *current) {
    if (sig == *current) {
        return;                         /* already showing this colour */
    }
    if (sig == SIG_BUY) {
        led_off(LED_RED);
        led_on(LED_GREEN);
    } else {
        led_off(LED_GREEN);
        led_on(LED_RED);
    }
    *current = sig;
}

int main(int argc, char *argv[]) {
    Options opt;
    Feed feed;
    Engine eng;
    Rival rival, rival_proc;
    LatStats proc_stats, lat_stats;
    long long first, last, i, done = 0;
    long long ts0, end_ts;
    uint64_t t_load0, t_load1, start_ns, wall_end, next_status;
    double max_lag_ms = 0.0, lag_ms = 0.0;
    int led_state = SIG_NONE;
    int printed_signals = 0;

    if (!parse_options(argc, argv, &opt)) {
        usage(argv[0]);
        return 1;
    }
    display_init();
    signal(SIGINT, on_ctrl_c);

    /* ---- LOAD: file -> memory (not part of the replay) ---- */
    t_load0 = now_ns();
    if (!feed_load(&feed, opt.path)) {
        return 1;
    }
    if (!engine_init(&eng, opt.engine, opt.short_win, opt.long_win, &feed)) {
        feed_free(&feed);
        return 1;
    }
    t_load1 = now_ns();

    /* ---- pick the slice to replay: [first, last) ---- */
    ts0 = feed.ts[0] + (long long)(opt.from_min * 60e6);
    first = 0;
    while (first < feed.n && feed.ts[first] < ts0) {
        first++;
    }
    if (first >= feed.n) {
        printf("--from-min is past the end of the file\n");
        engine_free(&eng);
        feed_free(&feed);
        return 1;
    }
    ts0 = feed.ts[first];
    end_ts = opt.minutes > 0 ? ts0 + (long long)(opt.minutes * 60e6) : -1;
    last = feed.n;
    if (end_ts >= 0) {
        last = first;
        while (last < feed.n && feed.ts[last] < end_ts) {
            last++;
        }
    }

    if (!lat_init(&proc_stats, last - first) || !lat_init(&lat_stats, last - first)) {
        printf("malloc failed for the latency samples\n");
        lat_free(&proc_stats);
        engine_free(&eng);
        feed_free(&feed);
        return 1;
    }
    rival_init(&rival, opt.rival_us);
    rival_init(&rival_proc, opt.rival_us);

    printf("Stage C replay  engine=%s  speed=%gx  windows=%d/%d  rival R=%g us\n",
           engine_name(opt.engine), opt.speed, opt.short_win, opt.long_win, opt.rival_us);
    printf("file %s: %lld trades, replaying %lld (trades %lld..%lld), %.1f market minutes\n",
           opt.path, feed.n, last - first, first + 1, last,
           (feed.ts[last - 1] - ts0) / 60e6);
    printf("load %.1f ms, memory: feed %.1f MB%s\n\n", (t_load1 - t_load0) / 1e6,
           feed_memory(&feed) / 1e6,
           opt.engine == ENGINE_MEM ? " + parsed trades" : "");

    if (opt.use_led) {
        led_init();
    }

    /* ---- REPLAY ---- */
    start_ns = now_ns();
    if (opt.speed > 0) {
        start_ns += 1000000ULL;            /* first trade released in 1 ms */
    }
    next_status = start_ns;

    for (i = first; i < last && !stop_requested; i++) {
        uint64_t release = 0, t_start, t_done, now;
        long long price;
        int sig;

        /* 1) when is this trade released? (speed 0: right now) */
        if (opt.speed > 0) {
            release = start_ns + (uint64_t)((double)(feed.ts[i] - ts0) * 1000.0 / opt.speed);
            wait_until(release);
        }

        /* 2) process it, timing only the engine */
        t_start = now_ns();
        sig = engine_step(&eng, &feed, i, &price);
        t_done = now_ns();
        if (opt.speed <= 0) {
            release = t_start;             /* no clock: latency = processing time */
        }

        lat_add(&proc_stats, t_done - t_start);
        lat_add(&lat_stats, t_done - release);
        lag_ms = (t_start > release) ? (t_start - release) / 1e6 : 0.0;
        if (lag_ms > max_lag_ms) {
            max_lag_ms = lag_ms;
        }

        /* 3) rival bot: fill a lost order at this trade's price, then react.
         *    Two versions are kept:
         *      rival      reaction = latency (release -> decision). Includes
         *                 lag and queueing: a trade we have not reached yet
         *                 cannot be reacted to. Shown live, used for the race.
         *      rival_proc reaction = processing time only (the brief's rule). */
        rival_on_trade(&rival, price);
        rival_on_trade(&rival_proc, price);
        if (sig != SIG_NONE) {
            rival_on_signal(&rival, sig, (long long)(t_done - release), price);
            rival_on_signal(&rival_proc, sig, (long long)(t_done - t_start), price);
            if (opt.use_led) {
                show_led(sig, &led_state);
            }
            if (opt.show_signals) {
                display_signal(sig, i + 1, feed.ts[i], price / (double)PRICE_SCALE,
                               (long long)(t_done - release), rival.pnl / (double)PRICE_SCALE);
                printed_signals++;
            }
        }
        done++;

        /* 4) status bar (after timing, so printing is not counted as latency) */
        now = now_ns();
        if (now >= next_status) {
            display_status(engine_name(opt.engine), opt.speed, feed.ts[i], done,
                           last - first, lag_ms, max_lag_ms,
                           rival.pnl / (double)PRICE_SCALE, rival.wins,
                           rival.wins + rival.losses, engine_buys(&eng), engine_sells(&eng));
            next_status = now + STATUS_EVERY_NS;
        }
    }
    wall_end = now_ns();
    display_status(engine_name(opt.engine), opt.speed, feed.ts[first + done - 1], done,
                   last - first, lag_ms, max_lag_ms, rival.pnl / (double)PRICE_SCALE,
                   rival.wins, rival.wins + rival.losses, engine_buys(&eng), engine_sells(&eng));
    printf("\n");

    if (opt.use_led) {
        led_off(LED_GREEN);
        led_off(LED_RED);
        led_cleanup();
    }

    /* ---- SUMMARY ---- */
    printf("\n==== summary (engine=%s, speed=%gx) %s====\n", engine_name(opt.engine),
           opt.speed, stop_requested ? "STOPPED EARLY " : "");
    printf("trades replayed   %lld   market time %.1f s   wall time %.3f s\n", done,
           (feed.ts[first + done - 1] - ts0) / 1e6, (wall_end - start_ns) / 1e9);
    printf("throughput        %.0f trades/s (wall)\n", done / ((wall_end - start_ns) / 1e9));
    printf("signals           %d BUY, %d SELL   final VWAP %.8f\n",
           engine_buys(&eng), engine_sells(&eng), engine_vwap(&eng));
    printf("processing time   avg %.1f ns   p50 %.0f ns   p99 %.0f ns   worst %llu ns\n",
           lat_mean(&proc_stats), lat_percentile(&proc_stats, 50),
           lat_percentile(&proc_stats, 99), (unsigned long long)proc_stats.max);
    printf("latency (release->decision)\n"
           "                  avg %.1f ns   p50 %.0f ns   p99 %.0f ns   worst %llu ns\n",
           lat_mean(&lat_stats), lat_percentile(&lat_stats, 50),
           lat_percentile(&lat_stats, 99), (unsigned long long)lat_stats.max);
    printf("max lag behind replay clock  %.3f ms\n", max_lag_ms);
    printf("rival bot (R=%g us)%s\n", opt.rival_us,
           rival.holding ? "   (1 BTC still held at the end, not counted)" : "");
    printf("  reaction = latency          P&L %+.2f USDT over %d round trips, won %d / lost %d\n",
           rival.pnl / (double)PRICE_SCALE, rival.round_trips, rival.wins, rival.losses);
    printf("  reaction = processing time  P&L %+.2f USDT over %d round trips, won %d / lost %d\n",
           rival_proc.pnl / (double)PRICE_SCALE, rival_proc.round_trips,
           rival_proc.wins, rival_proc.losses);
    if (printed_signals == 0 && !opt.show_signals) {
        printf("(per-signal lines were off; use --signals to show them)\n");
    }

    lat_free(&proc_stats);
    lat_free(&lat_stats);
    engine_free(&eng);
    feed_free(&feed);
    return 0;
}
