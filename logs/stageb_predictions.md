# Stage B — Prediction Logbook (Leroi)

Predict → Measure → Explain. Each prediction is written **before** the measurement it
predicts; Result and Reason are filled in afterwards.

---

## Prediction 1: Fixed-point (long long) prices vs double

**Date:** 2026-11-03  **Signed:** Leroi
**Status:** prediction written before the fixed-point version was implemented.

**Expected:**

*Speed:* fixed-point will be only slightly faster than double, about **1.1–1.5×** on the
process phase of `stress_full.csv` (laptop and Pi), and **not** a big jump.
- Adding/subtracting whole numbers is a bit quicker for the CPU than adding decimals, and
  the running sums (SMA sum, VWAP sums) are updated every trade.
- But each trade still does the same amount of work (VWAP, OHLC, SMA, crossover), and the
  `Trade` struct stays 32 bytes, so the same amount of memory is read. The work per trade
  is already small (~8.5 ns on the laptop).
- Risk: if the code divides by N every trade to get the SMA, whole-number division is slow
  and could cancel out the gain. Comparing the sums directly
  (`sum_short × 60` vs `sum_long × 35`) avoids dividing at all.

*Pi vs laptop:* the gain may be a little bigger on the Pi, because its simpler CPU feels the
cost of each decimal operation more.

*Correctness:*
- `test20.csv` will still match.
- `dev_1M.csv` signal counts will **change** from the double version (2465 BUY / 2464 SELL)
  to the exact answer (2553 BUY / 2469 SELL). On 17,132 trades the two SMAs are exactly
  equal; with double, rounding leaves a tiny non-zero difference whose sign is random,
  while whole numbers give exactly 0. (The exact counts come from an earlier integer-cents
  check, so this part predicts that the fixed-point engine matches that check.)
- VWAP will match the double value to at least 6–8 decimal places.

**Result:**

| | double | fixed-point | speedup |
|---|---|---|---|
| Laptop, process phase (median of 5) | 79.9 ms | 70.6 ms | 1.13× |
| Pi 5, process phase (median of 5) | ____ ms | ____ ms | ____× |

(Laptop measured 2026-11-03, `stress_full.csv`, `-O2`, `taskset -c 2`.)

- `test20.csv`: all 20 rows and the OHLC bar match `test20_expected.txt`
- `dev_1M.csv` signals: 2553 BUY / 2469 SELL (double: 2465 / 2464)
- `dev_1M.csv` VWAP: 76217.01291611 (double: 76217.01291587; exact: 76217.01291611)
- `stress_full.csv`: 20943 BUY / 20060 SELL, VWAP 76718.67569889 (exact: 76718.67569889)

**Reason:** ____

---

## Findings for the report

### Fixed-point fixes a real precision bug that `test20.csv` can't catch

**Date:** 2026-11-03  **Noted by:** Leroi

- Every `double` version gives a **different, wrong** signal count on `dev_1M.csv`:

  | Version | BUY | SELL | VWAP |
  |---|---|---|---|
  | Stage A (`stageA_stream.c`, double, running sum) | 2465 | 2464 | 76217.01291587 |
  | Stage B naive SMA (double) | 2509 | 2469 | 76217.01291587 |
  | **Stage B fixed-point** | **2553** | **2469** | **76217.01291611** |
  | Exact (integer / Python Decimal check) | 2553 | 2469 | 76217.01291611 |

- Cause: on 17,132 trades in `dev_1M.csv` (156,471 in `stress_full.csv`) the short and long
  SMA are exactly equal (flat prices). With `double`, rounding leaves a tiny non-zero
  difference whose sign is effectively random, so signals appear or disappear. The running
  sum and the naive sum add in a different order, so they round differently.
- Fixed-point (whole-number) sums are exact, so `d = 0` exactly and the brief's `≤ 0` / `> 0`
  rule is applied as written.
- `test20.csv` passes for every version because 20 trades are too few for rounding error to
  matter — a correctness check on a small file does not prove a version is correct on the
  full data.
