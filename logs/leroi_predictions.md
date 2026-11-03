# Prediction Logbook — Leroi (Stage B & C)

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

## Prediction 2 (Stage C): malloc array sized to the file vs fixed global array

**Date:** 2026-11-03  **Signed:** Leroi
**Status:** prediction written before the malloc version was implemented.

**Planned design:** read the file once just to count the trades, `malloc` exactly
`n × sizeof(Trade)` bytes (32 B each), check for `NULL`, read the file again to fill the
array, and `free` it at the end. The `Trade` struct and the engine loop stay the same; only
where the array lives changes (heap instead of a 320 MB global array).

**Expected:**

*Memory reserved (virtual size; Massif with `--pages-as-heap=yes`):*
- Fixed array: always **320 MB** (10,000,000 × 32 B), whatever the file size.
- malloc: exactly what the file needs, **~32 MB** for `dev_1M.csv` and **~286 MB** for
  `stress_full.csv`. That is a big saving on dev_1M (~288 MB) but only ~34 MB on stress_full.

*Memory actually used (peak physical memory, max RSS from `/usr/bin/time -v`):*
- **About the same** for both versions, ~32 MB + a few MB on dev_1M and ~290 MB on
  stress_full.
- The OS only gives real memory to the parts of the global array that the program writes
  to. The empty part of the fixed array is reserved but never handed physical memory, so it
  costs almost nothing. I expect the malloc saving to show up in the reserved size, not in
  the physical memory used.

*Speed:*
- **Process phase:** the same within ~3%. The trades sit one after another in memory in
  both cases and the loop does the same work, so the CPU reads the same bytes in the same
  order.
- **Load phase:** about **10–20% slower**, because the file is read twice. The counting
  pass only looks for newline characters and does not convert any numbers, so it should be
  much cheaper than the real parsing pass (parsing is ~97% of the run time).

*valgrind Memcheck (`--leak-check=full`, on `dev_1M.csv`):*
- **0 errors** and **0 bytes "definitely lost"**, because the array is freed at the end.
- If anything is reported, I expect it to be "still reachable" memory from the file
  handling in the helper library, not from my array.
- The fixed-array version will show almost nothing in the heap summary, because a global
  array is not heap memory.

*Correctness:* the signals, VWAP and OHLC will be identical to `stageB_inmem` on all three
files, and `test20.csv` will still pass.

*Pi vs laptop:* the same memory numbers on both machines, since the struct size is the same.
Both versions fit easily in the Pi 5's 4 GB. The real advantage of malloc on an edge device
is that it works for any file size: the fixed array fails above 10 million trades and wastes
reserved space on small files.

**Result:**

| | Fixed global array | malloc (sized to file) |
|---|---|---|
| `dev_1M`: reserved (Massif peak) | 325.0 MB | 37.0 MB |
| `dev_1M`: max RSS | 34.7 MB | 34.0 MB |
| `stress_full`: reserved (Massif peak) | 325.0 MB | 290.7 MB |
| `stress_full`: max RSS | 289.0 MB | 287.7 MB |
| Memcheck errors / definitely lost (`dev_1M`) | 0 errors / 0 B (laptop, 2026-11-03) | 0 errors / 0 B (laptop, 2026-11-03; "All heap blocks were freed") |

(Memory rows: laptop, 2026-11-03. Massif with `--pages-as-heap=yes`, single run. Max RSS is
the median of 5 runs. "dev_1M" rows = `./prog dev` (test20 + dev_1M); "stress_full" rows =
full run. MB = 10⁶ bytes.)

- `test20.csv`: matches `test20_expected.txt` (malloc version)
- Signals / VWAP match `stageB_inmem`: yes, identical output on all three files (laptop)

**Reason:** ____

---

## Prediction 3: Processing from memory vs re-reading the file (streaming)

**Date:** 2026-11-03  **Signed:** Leroi
**Status:** written before the official (median of 5, pinned) runs. Already known
beforehand: one preliminary, unpinned laptop run of `stageB_inmem` on `stress_full.csv`
gave load 2812 ms and process 86 ms.

**Compared:** a streaming version (reads and processes one trade at a time with
`next_trade()`) vs `stageB_inmem` (load everything into the array first, then process).
Same engine modules, double prices, running-sum SMA, `-O2`, `stress_full.csv`.

**Expected:**

*Speed (laptop):*
- The in-memory **process phase alone** will be about **30–35× faster** than the whole
  streaming run (~2.9 s vs ~85 ms). Reading and parsing the text with `sscanf` is about 97%
  of the work, and the process phase skips it completely.
- **Load + process together** will take about the **same time as streaming, or up to 5%
  longer**. The file still has to be read and parsed once. On top of that, the in-memory
  version writes 286 MB into the array and then reads it back. So for one pass over the
  data, keeping it in memory does not save time overall. It pays off only when the same
  data is processed again (e.g. several runs or the replay).

*Speed (Pi 5):* the same pattern, with every time about **2–4× longer** than on the laptop.
The share spent parsing should stay around 95% or more.

*Memory:* streaming needs almost nothing (one trade at a time, max RSS of a few MB). The
in-memory version needs ~290 MB of physical memory for stress_full.

*Correctness:* both give identical output (same modules, same order of additions):
VWAP 76718.67569936, 20188 BUY / 20188 SELL on stress_full.

**Result:**

| | Streaming | In-memory: process only | In-memory: load + process |
|---|---|---|---|
| Laptop time (median of 5) | 2988.1 ms | 79.0 ms | 2976.8 ms |
| Laptop speedup vs streaming | 1.00× | 37.8× | 1.004× |
| Pi 5 time (median of 5) | ____ ms | ____ ms | ____ ms |
| Pi 5 speedup vs streaming | 1.00× | ____× | ____× |
| Laptop max RSS (stress_full run) | 1.9 MB | — | 289.0 MB |

(Laptop: 2026-11-03, stress_full, `-O2`, `taskset -c 2`, 5 runs, median.)

- Output identical: yes, on laptop (stress_full VWAP 76718.67569936, 20188 BUY / 20188 SELL
  for both)

**Reason:** ____

---

## Prediction 4: Naive vs running-sum SMA on the Pi 5

**Date:** 2026-11-03  **Signed:** Leroi
**Status:** written before the Pi runs. The laptop result was already measured
(process phase, stress_full, median of 5, pinned): naive 200.4 ms, running sum 76.2 ms,
**2.63×**. So this entry predicts only the Pi.

**Expected:**
- The Pi speedup will be **larger than the laptop's, about 3–5×**.
- The naive version does 95 additions per trade for the SMAs, against 4 for the running
  sum. The laptop's CPU is wide and can run many additions at once, which hides much of the
  extra work. The Pi 5's Cortex-A76 is a simpler core, so the extra additions should cost
  relatively more.
- It will still be nowhere near the ~24× the operation count suggests. VWAP, OHLC and the
  crossover check cost the same in both versions, and the window (35/60 doubles) always
  stays in the L1 cache.

**Result:**

| | Naive | Running sum | Speedup |
|---|---|---|---|
| Laptop, process phase (median of 5) | 200.4 ms | 76.2 ms | 2.63× |
| Pi 5, process phase (median of 5) | ____ ms | ____ ms | ____× |

**Reason:** ____

---

## Prediction 5: Do the laptop and the Pi give exactly the same answers?

**Date:** 2026-11-03  **Signed:** Leroi
**Status:** written before any of my programs has been run on the Pi.

**Expected:**
- **Fixed-point (`stageB_fixed`): identical** on both, to the last digit. Whole-number
  arithmetic has no rounding, so the CPU makes no difference.
- **Double versions, signal counts: identical.** The SMA sums only add and subtract, and
  both CPUs follow the same IEEE 754 rules for double, so each addition rounds the same way.
- **Double versions, VWAP: might differ in the last printed digits.** On ARM, gcc can join
  `sum += p * q` into one "fused multiply-add" instruction that rounds once instead of
  twice. The laptop build does not use FMA by default. If that happens, `sum_pq` drifts
  slightly, but the signals do not change because they do not use VWAP.
- `test20.csv` will pass on the Pi for every version.

**Result:**

| Version | Laptop VWAP / BUY / SELL (stress_full) | Pi 5 VWAP / BUY / SELL (stress_full) | Same? |
|---|---|---|---|
| `stageB_inmem` (double, running sum) | 76718.67569936 / 20188 / 20188 | ____ | ____ |
| `stageB_inmem_naive` (double, naive) | 76718.67569936 / 20386 / 20176 | ____ | ____ |
| `stageB_fixed` (fixed-point) | 76718.67569889 / 20943 / 20060 | ____ | ____ |

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
