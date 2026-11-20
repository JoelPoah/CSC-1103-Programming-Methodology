"""
plot_live.py - Stage C: turn recorded replay feeds into an animated GIF (Lucas)

Records of a race (or one engine) made with stageC_replay --feed N, e.g.:

    ./dev/stageC/stageC_replay data/group_data/stress_full.csv --engine v0  --speed 1000 --feed 1500 > logs/stageC/feed/race_v0_1000x.txt
    ./dev/stageC/stageC_replay data/group_data/stress_full.csv --engine mem --speed 1000 --feed 1500 > logs/stageC/feed/race_mem_1000x.txt

    python dev/stageC/plot_live.py logs/stageC/feed/race_v0_1000x.txt logs/stageC/feed/race_mem_1000x.txt \
           -o dev/stageC/race_1000x.gif --title "Pi 5, 1000x"

The animation runs on WALL-CLOCK time, so it shows the race as it happened:
each "#F" sample was produced at  (market time since the first trade) / speed + lag,
so an engine that falls behind the replay clock is drawn behind the other one.

Panels: price, P&L against the rival bot (reaction = latency), lag behind the clock,
and (when both v0 and mem are given) the money lost to slow code = P&L(mem) - P&L(v0)
compared at the same market time.
Needs matplotlib and Pillow (pip install matplotlib pillow).
"""
import argparse
import datetime

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation, PillowWriter

COLOURS = {"v0": "#e67300", "mem": "#1a66d9"}
LABELS = {"v0": "v0 (Version 0)", "mem": "mem (fastest, V7)"}


def read_feed(path):
    """Read one feed file: header values and the samples (lists)."""
    run = {"t": [], "wall": [], "price": [], "pnl": [], "lag": [], "summary": []}
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            p = line.split()
            if not p:
                continue
            if p[0] == "#H":
                run["engine"] = p[1]
                run["speed"] = int(p[2])
                run["rival_us"] = float(p[5])
                run["ts_first"] = int(p[7])
                run["span_s"] = int(p[8]) / 1e6
            elif p[0] == "#F":
                t = (int(p[3]) - run["ts_first"]) / 1e6       # market s since start
                lag_s = int(p[7]) / 1e9
                speed = max(run["speed"], 1)
                run["t"].append(t)
                run["wall"].append(t / speed + lag_s)
                run["price"].append(int(p[4]) / 100)
                run["pnl"].append(int(p[5]) / 100)
                run["lag"].append(lag_s * 1000)
            elif not p[0].startswith("#"):
                run["summary"].append(line.rstrip())
    if "engine" not in run or not run["t"]:
        raise SystemExit(f"{path}: no '#H' / '#F' lines (was it made with --feed?)")
    return run


def clock(ts_first, offset_s):
    """Market time of day (UTC) as HH:MM."""
    d = datetime.datetime.fromtimestamp(ts_first / 1e6 + offset_s, datetime.timezone.utc)
    return d.strftime("%H:%M")


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[1])
    ap.add_argument("feeds", nargs="+", help="feed files from stageC_replay --feed")
    ap.add_argument("-o", "--out", default="dev/stageC/race.gif")
    ap.add_argument("--title", default="", help="e.g. the machine the feeds came from")
    ap.add_argument("--frames", type=int, default=150)
    ap.add_argument("--fps", type=int, default=10)
    args = ap.parse_args()

    runs = [read_feed(p) for p in args.feeds]
    lead = max(runs, key=lambda r: len(r["t"]))
    span = lead["span_s"]
    wall_end = max(r["wall"][-1] for r in runs)

    by_name = {r["engine"]: r for r in runs}
    race = "v0" in by_name and "mem" in by_name
    n_ax = 4 if race else 3
    fig, axes = plt.subplots(n_ax, 1, figsize=(9, 2.1 * n_ax + 1.2), dpi=80, sharex=True)
    ax_p, ax_pnl, ax_lag = axes[0], axes[1], axes[2]
    title = f"Stage C live replay at {lead['speed']}x, rival R = {lead['rival_us']:g} us"
    if args.title:
        title += f"  ({args.title})"
    fig.suptitle(title, fontsize=12)

    ax_p.set_ylabel("price (USDT)")
    ax_pnl.set_ylabel("P&L vs rival (USDT)")
    ax_lag.set_ylabel("lag behind clock (s)")
    ax_p.set_ylim(min(lead["price"]) * 0.998, max(lead["price"]) * 1.002)
    pnl_all = [v for r in runs for v in r["pnl"]]
    ax_pnl.set_ylim(min(0, min(pnl_all)) * 1.05, max(pnl_all) * 1.08)
    lag_max = max(max(r["lag"]) for r in runs) / 1000
    ax_lag.set_ylim(0, max(lag_max * 1.1, 0.001))
    if race:
        # money lost at each v0 sample: mem's P&L at that market time minus v0's
        v0, mem = by_name["v0"], by_name["mem"]
        lost, k = [], 0
        for t, pv in zip(v0["t"], v0["pnl"]):
            while k + 1 < len(mem["t"]) and mem["t"][k + 1] <= t:
                k += 1
            lost.append(mem["pnl"][k] - pv)
        v0["lost"] = lost
        ax_lost = axes[3]
        ax_lost.set_ylabel("money lost (USDT)")
        ax_lost.axhline(0, color="grey", lw=0.8)
        ax_lost.set_ylim(min(0, min(lost)) * 1.1 - 1, max(lost) * 1.1 + 1)
        lost_line, = ax_lost.plot([], [], color="#b02020", lw=1.8,
                                  label="P&L(mem) - P&L(v0), same market time")
        ax_lost.legend(loc="upper left")
    ax_last = axes[-1]
    ticks = [span * k / 6 for k in range(7)]
    ax_last.set_xticks(ticks)
    ax_last.set_xticklabels([clock(lead["ts_first"], s) for s in ticks])
    ax_last.set_xlabel("market time (UTC)")
    for ax in axes:
        ax.set_xlim(0, span)
        ax.grid(alpha=0.3)

    price_line, = ax_p.plot([], [], color="#333344", lw=1)
    lines, cursors = [], []
    for r in runs:
        c = COLOURS.get(r["engine"], "black")
        lines.append((ax_pnl.plot([], [], color=c, lw=1.8, label=LABELS.get(r["engine"]))[0],
                      ax_lag.plot([], [], color=c, lw=1.5)[0]))
        cursors.append([ax.axvline(0, color=c, alpha=0.5, lw=1) for ax in axes])
    ax_pnl.legend(loc="upper left")
    info = fig.text(0.5, 0.005, "", ha="center", va="bottom", fontsize=9, family="monospace")
    fig.tight_layout(rect=(0, 0.075, 1, 0.97))

    def upto(r, wall):
        """How many samples of run r had been produced by this wall time."""
        n = 0
        while n < len(r["wall"]) and r["wall"][n] <= wall:
            n += 1
        return n

    def frame(i):
        wall = wall_end * (i + 1) / args.frames
        n_lead = upto(lead, wall)
        price_line.set_data(lead["t"][:n_lead], lead["price"][:n_lead])
        text = [f"wall {wall:6.1f} s"]
        for r, (l_pnl, l_lag), cur in zip(runs, lines, cursors):
            n = upto(r, wall)
            l_pnl.set_data(r["t"][:n], r["pnl"][:n])
            l_lag.set_data(r["t"][:n], [v / 1000 for v in r["lag"][:n]])
            if n > 0:
                for c in cur:
                    c.set_xdata([r["t"][n - 1]] * 2)
                text.append(f"{r['engine']}: {clock(r['ts_first'], r['t'][n - 1])} "
                            f"lag {r['lag'][n - 1] / 1000:5.2f} s  P&L {r['pnl'][n - 1]:+.2f}")
        if race:
            n = upto(by_name["v0"], wall)
            lost_line.set_data(by_name["v0"]["t"][:n], by_name["v0"]["lost"][:n])
            if n > 0:
                text[0] += (f"    money lost so far {by_name['v0']['lost'][n - 1]:+.2f} USDT")
        info.set_text(text[0] + "\n" + "   |   ".join(text[1:]))
        return []

    anim = FuncAnimation(fig, frame, frames=args.frames, blit=False)
    anim.save(args.out, writer=PillowWriter(fps=args.fps))
    frame(args.frames - 1)
    still = args.out.rsplit(".", 1)[0] + "_final.png"
    fig.savefig(still)
    print(f"wrote {args.out} ({args.frames} frames) and {still}")


if __name__ == "__main__":
    main()
