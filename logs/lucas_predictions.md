# Prediction Logbook — Lucas (Stage C)

Predict → Measure → Explain. Each prediction is written **before** the measurement it
predicts; Result and Explanation are filled in afterwards.

AI use: Claude explained the Stage C concepts and asked guiding questions. The predicted
values and reasons below are my own answers. I revised them on the same day, before any
Pi run.

---

## Prediction 1: At 1000×, will Version 0 keep up with the replay clock on the Pi 5?

**Date:** 2026-11-12  **Signed:** Lucas
**Status:** written before any Stage C run on the Pi.

**Already known beforehand:**
- Laptop (Windows, MinGW gcc, unpinned, single run): v0 took about 4.6 µs per trade and
  fell up to 1.1 s behind the replay clock at 1000×.
- Leroi measured glibc's `sscanf` at about 335 ns per trade (streaming, Linux laptop).
- Time available per trade at 1000×: stress_full has 8,928,944 trades over 24 h, so the day
  lasts 86.4 s and each trade gets about **9.7 µs** on average.

**Expected:**

| | Prediction |
|---|---|
| v0 time per trade (Pi 5) | about **0.2 µs** |
| Below the 9.7 µs available? | **Yes**, well below |
| v0 keeps up? | **Yes** on average; it can fall slightly behind during bursts |
| v0 maximum lag | **a few milliseconds** |
| mem | **keeps up** |

*Reason:*
- **v0's speed.** Most of v0's time on the laptop is `sscanf`. Linux's `sscanf` (about
  0.335 µs) is about **14× faster** than the Windows MinGW one (4.6 µs). The Pi's CPU is
  slower than my laptop's, but I expect the faster `sscanf` to carry over, giving about
  0.2 µs per trade.
- **Lag.** Trades arrive in bursts (many share one timestamp), so a queue can form during a
  burst. If the time per trade is well under 9.7 µs, the queue clears quickly after the
  burst, so the lag stays small. If it were close to 9.7 µs, the queue would keep growing
  through busy periods and the lag would get much bigger.
- **mem.** mem has already converted the text into numbers (the right variable types)
  before the replay starts. During the race it only does the maths, so it is the fastest.

**Result:** (Pi 5, median of 5 runs, `taskset -c 2`, `performance` governor)

| Engine @ 1000× | processing avg | avg latency | worst latency | max lag |
|---|---|---|---|---|
| v0  | 1.193 µs (1193.0 ns) | 220.1 µs | 9.591 ms | 9.590 ms |
| mem | 0.051 µs (50.6 ns) | 5.84 µs | 0.353 ms | 0.353 ms |

**Explain the difference:** ____

---

## Prediction 2: Rival bot at R = 1 µs on the Pi 5 (stress_full)

**Date:** 2026-11-12  **Signed:** Lucas
**Status:** written before any rival bot run on the Pi.

**Expected:**
- **v0** reacts in **more than 1 µs**, so it will **lose most** signals to the rival.
  ⚠ *To check before running: this was written when I expected v0 to take 5 µs.
  Prediction 1 now says 0.2 µs.*
- **mem** is much faster than the 1 µs rival, so it will **win most** signals.
- **Money lost to slow code** (P&L of mem − P&L of v0): **small**, because each lost signal
  only costs the price change between one trade and the next.

**Result:** (median of 5 runs)

| | signals won (of total) | P&L (USDT) |
|---|---|---|
| v0  | 0 of 37,063 | +207,415.12 |
| mem | 37,060 of 37,063 | +209,214.42 |
| **Money lost** | | +1,799.30 |

**Explain the difference:** ____

---

## Prediction 3: The rival sweep (R = 0.1 µs to 100 µs)

**Date:** 2026-11-12  **Signed:** Lucas
**Status:** written before the sweep was run on the Pi.

**Expected:**
- v0 starts winning once the rival is slower than v0's own processing time, at about
  **R = 5 µs**.
  ⚠ *To check before running: 5 µs was my old v0 estimate. Prediction 1 now says 0.2 µs.*
- mem starts winning at R ≈ ____

*Reason:* you win a signal when your reaction time is shorter than R. So each version
starts winning when R grows past that version's processing time.

**Result:** v0 starts winning at R ≈ 1.26 µs (0 % at 1.00 µs, 97.3–97.6 % at 1.259 µs); mem at R ≈ ≤ 0.1 µs (99.6–99.8 % already at the smallest R tested, 100 % from 0.251 µs) (from `rival_sweep.png`).

**Explain the difference:** ____

---

## Prediction 4: Replay on my laptop (WSL) compared with the Pi 5

**Date:** 2026-11-19  **Signed:** Lucas
**Status:** written before any laptop replay run (5 runs each, v0 and mem, at 1×, 100×
and 1000×, `taskset -c 2` in WSL Ubuntu 24.04).

**Already known beforehand:**
- Pi 5 at 1000× (median of 5): v0 takes 1.19 µs per trade (avg latency 220 µs, worst
  9.6 ms); mem takes 0.05 µs per trade (worst 0.35 ms).
- Time available per trade: about 9.7 µs at 1000×, 97 µs at 100×, 9.7 ms at 1×.
- WSL uses the Linux (glibc) `sscanf`, the same as the Pi, not the slower MinGW one.

**Expected:**

| | Prediction |
|---|---|
| v0 time per trade (laptop) | **faster** than the Pi, by about **0.5 µs** (so about **0.7 µs**) |
| mem time per trade (laptop) | **faster** than the Pi |
| v0 keeps up at 1× / 100× / 1000×? | **Yes / Yes / Yes** on average |
| mem keeps up at 1× / 100× / 1000×? | **Yes / Yes / Yes** |
| Worst-case latency vs the Pi | **bigger** |
| P&L | **changes** between speeds and between laptop and Pi |

*Reason:*
- **Speed.** My laptop's CPU is faster than the Pi's, and both now use the same Linux
  `sscanf`, so both engines should process each trade faster on the laptop.
- **Keeping up.** About 0.7 µs per trade is well below the time available at every speed
  (9.7 ms at 1×, 97 µs at 100×, 9.7 µs at 1000×), so v0 should keep up on average at all
  three. Like on the Pi, it may still fall slightly behind during bursts at 1000×.
- **Worst case.** WSL runs inside Windows, so Windows can interrupt it at any time. This
  affects both the average and the worst case, but a single long interruption shows up
  much more in the worst case.
- **P&L.** At higher speeds more trades arrive in a shorter time, so I expect the results,
  and therefore the P&L, to differ between speeds and between machines.

**Result:** (laptop, WSL2, median of 5 runs, `taskset -c 2`; full table in
`logs/stageC/laptop/RESULTS_SUMMARY_laptop.md`)

| Engine | Speed | processing avg | avg latency | worst latency | max lag | P&L, reaction = latency (USDT) |
|---|---|---|---|---|---|---|
| v0  | 1×    | 4.31 µs (4313.0 ns) | 68.42 µs  | 4.606 ms  | 4.604 ms  | +1,099.13 |
| v0  | 100×  | 1.33 µs (1330.7 ns) | 56.29 µs  | 68.093 ms | 68.079 ms | +207,381.13 |
| v0  | 1000× | 1.16 µs (1156.7 ns) | 207.02 µs | 54.457 ms | 54.444 ms | +207,375.28 |
| mem | 1×    | 0.24 µs (239.3 ns)  | 8.21 µs   | 4.728 ms  | 4.728 ms  | +1,099.18 |
| mem | 100×  | 0.055 µs (55.1 ns)  | 8.75 µs   | 48.175 ms | 48.175 ms | +207,805.38 |
| mem | 1000× | 0.041 µs (41.4 ns)  | 18.52 µs  | 43.145 ms | 43.144 ms | +207,812.98 |

Every run gave the same signals at a given speed (full day: v0 20386 / 20176, mem
20943 / 20060).

**Explain the difference:** ____
