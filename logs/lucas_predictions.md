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
| v0  | ____ | ____ | ____ | ____ |
| mem | ____ | ____ | ____ | ____ |

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
| v0  | ____ | ____ |
| mem | ____ | ____ |
| **Money lost** | | ____ |

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

**Result:** v0 starts winning at R ≈ ____ µs; mem at R ≈ ____ µs (from `rival_sweep.png`).

**Explain the difference:** ____
