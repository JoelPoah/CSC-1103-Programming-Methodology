# README — Claude's suggested changes

This file compares the project brief (`CSC1103-1104_Mini_Project_2026-27_Tick-to-Trade_final.pdf`)
against the task delegation in `README.md`. Part 1 lists what is missing, what parameters
were left out and what is miscategorised. Part 2 is a copy of the README in its original
format with the suggestions written in. **Nothing here has been agreed by the group yet —
it is for discussion.**

Legend used in Part 2: `(+)` new item, `(~)` changed item.

---

# Part 1 — Comparison: brief vs README

## 1. Deliverables missing from the README

| Missing deliverable | Where in the brief | Why it matters |
|---|---|---|
| **Problem definition, problem analysis and pseudocode** | Introduction, report contents | Required in the report; nobody owns it |
| **Trade-off analysis** (speed vs readability, fixed-point vs `double`) | Testing & Enhancement 3 (part of 15%) | Nobody owns it |
| **Recommendation of which version to deploy on a power- and memory-limited edge device, and why** | Testing & Enhancement 3 | Nobody owns it |
| **Replay at real speed (1×) for at least 10 minutes** | Stage C 2 | README only lists 100× and 1000×; latency must be reported *at each replay speed* |
| **Defining "Version 0"** | Stage B 4, rival bot, race demo | Every speedup, the rival-bot P&L and the race all compare against it. Version 0 also needs a replay, lag counter and P&L to take part in the race |
| **`make` on the Pi builds every version on the ladder** | Hand-in 2 | `Makefile` currently only builds Stage 0 and Stage A |
| **Report cover page with each member's contribution** | Hand-in 3 | Not assigned |
| **Stretch goals** (up to +10): hand-written assembly kernel, Struct-of-Arrays, p50/p99 latency, C vs Python | Stretch Goals | Optional but worth marks; not assigned |

## 2. Parameters missing from the README

- **Timing rules:** run every measurement **at least 5 times and report the median**; never
  write plot/`.dat` files in timed versions; pin to one core with `taskset` and set the CPU
  governor to `performance`. `perf`, `taskset` and `-mcpu=cortex-a76` work **only on the Pi**.
- **"All final measurements must be taken on the Raspberry Pi 5 issued to your group."**
  Laptop runs cover the CSC1103 part; the Pi numbers are the marked ones.
- **Rival bot rules:**
  - standard rival reaction time **R = 1 µs**;
  - your reaction time = processing time of the triggering trade (`now_ns()`);
  - faster than R → trade at the triggering price, slower → at the next trade's price;
  - start with no BTC, buy/sell **1 BTC** per signal;
  - P&L = sum of selling prices − sum of buying prices;
  - money lost = P&L(fastest) − P&L(Version 0);
  - sweep **R = 0.1 µs → 100 µs**, plot money lost vs R, **explain where each version starts losing**.
- **Race demo:** Version 0 and the fastest version side by side, each window with a lag
  counter and running P&L; fastest version drives the LED; explain why one keeps up.
- **Stage A plot:** the ~2,000-trade slice must contain **at least one BUY and one SELL**.
- **Stage 0 1a:** *N is typed in by the user* (start at 100,000,000). The brief warns that a
  fixed N lets `-O2`/`-O3` skip the loop. `stage0/question_a/a_loop_sum.c` currently has N
  fixed in the code.
- **Stage 0 1c:** turn the LED on before the timed loop and off after it, in your own
  program (currently only `recursion_sum.c` uses the LED).
- **Stage 0 Pi comparison:** Pi vs at least one laptop at `-O0` and `-O2`, in **one logbook
  entry**.
- **Logbook:** **≥ 6 entries in total, at least one per stage (0, A, A+, B, C), each dated and
  signed.** README only says "≥1 per member", which could leave stages with no entry.
- **Every version on the ladder must still pass `test20.csv`.**
- **Ladder needs at least four rungs**, all measured on `stress_full.csv` against Version 0.
- **Kit:** Raspberry Pi 5 **(4 GB)** — relevant for memory use and the deployment
  recommendation.

## 3. Miscategorised items

| Item | README says | Brief actually says |
|---|---|---|
| **valgrind** | Leroi, Stage B | Not named in the brief. The memory comparison is **Stage C 1** ("compare memory use"), plus "memory usage for each version" in every member's benchmark write-up. valgrind is the tool for those numbers |
| **`perf stat`** | Yong Sheng only | **Every member** reports `perf stat` (cycles, instructions, IPC, cache misses) in their own Pi benchmark write-up; Yong Sheng also does it per ladder rung |
| **Per-member benchmark** | "Run `stress_full` benchmark" | A full write-up: system set-up (Pi model, OS, compiler version, flags, governor), time **and throughput for every version**, `perf stat`, **memory usage for each version** |
| **Running-sum SMA** | "Fast running-sum SMA" | Build **both** a naive SMA (re-add the window each trade) and a running-sum SMA, and compare the times |
| **Report file location** | `docs/PMCOAReportGroupxx.pdf` | In the root of the `UoG-PMCOA_Group06` folder **and** submitted separately. The brief spells it both `PMCOA…` and `PMCOS…` — check with the lecturer |
| **Logbook requirement** | "≥1 signed entry per member" | ≥6 entries, ≥1 per stage, dated and signed |

---

# Part 2 — Suggested edited README

# CSC-1103-Programming-Methodology

### necessary downloads
[text](https://sourceforge.net/projects/gnuplot/files/latest/download)
!Make sure to add to path and reset vscode
`choco install make`

# Stage A
run from root directory
`gcc -O2 -Wall stageA/stageA_stream.c helper_function/trade_io.c -o stageA/stageA_stream
./stageA/stageA_stream`

Things you may want to improve
The BUY/SELL markers plot at the short-SMA value, which is the crossing point.
The program doesn't check whether the numbers match the expected file automatically.
signals.dat only has rows from trade 60 onward, because it skips trades before the long SMA exists.

# (+) Benchmark rules (apply to every timing in the report and logbook)
- Benchmark on `stress_full.csv` (8,928,944 trades); every version must still pass `test20.csv`
- Time with `now_ns()`; run each measurement >= 5 times and report the median
- Never write `signals.dat` / plot files in a version you are timing
- Pin to one core with `taskset`, CPU governor `performance` (perf, taskset, -mcpu: Pi only)
- Report time, time per trade (ns), throughput (trades/s), speedup vs Version 0
- All final measurements on the group's Raspberry Pi 5 (4GB); laptop for CSC1103
- Version 0 = ______ (to be agreed by the group)

# (+) Rival bot parameters
- Standard rival reaction time R = 1 us; sweep R = 0.1 us -> 100 us
- Your reaction time = processing time of the triggering trade (now_ns())
- Faster than R -> trade at triggering price; slower -> next trade's price
- Start with 0 BTC, trade 1 BTC per signal; P&L = sum(sell) - sum(buy)
- Money lost = P&L(fastest version) - P&L(Version 0); explain where each version starts losing

```
UoG-PMCOA_Group06/
├── .gitignore
├── Makefile                       # (~) Top-level build: `make` on the Pi builds EVERY ladder version
├── README.md                      # Build, run, and benchmark instructions[cite: 14]
├── PMCOAReportGroup06.pdf         # (~) Master report PDF in the folder root (also submitted separately)
│
├── helper_function/               # Shared headers and utility libraries provided by module
│   ├── timing.h                   # High-resolution nanosecond clock[cite: 13]
│   ├── led.h                      # Status LED driver / laptop dummy printer[cite: 13]
│   ├── trade_io.h                 # Module helper reader header[cite: 13]
│   ├── trade_io.c                 # Module helper reader source[cite: 13]
│   └── test_helpers.c             # Provided setup test file[cite: 6, 13]
│
├── data/group_data/               # (~) CSV test files (git-ignored if large)
│   ├── test20.csv                 # 20-row sanity check dataset
│   ├── test20_expected.txt        # Expected answers for short=3, long=5
│   ├── group_info.txt             # Your group's assigned window sizes[cite: 4]
│   ├── dev_1M.csv                 # 1M development dataset[cite: 3]
│   └── stress_full.csv            # (+) Full day, used for ALL marked benchmarks
│
├── stage0/                        # Baseline exploration programs (Weeks 5-6)[cite: 6]
│   ├── test_helpers.c             # Provided setup test file[cite: 6, 13]
│   ├── loop_sum.c                 # (~) Sum 1 to N loop, N typed in by user (start 100M), O0..O3, LED on/off
│   └── recur_vs_iter.c            # Recursion vs iteration call-stack test (N=10k)[cite: 6]
│
├── stageA/                        # Stage A: Baseline Streaming Engine (Weeks 6-7)[cite: 6]
│   ├── stageA_stream.c            # Main engine (VWAP, OHLC, circular SMA, signals)[cite: 7]
│   ├── plot_signals.gnuplot       # Gnuplot script to render the PNG chart[cite: 11]
│   ├── signals.dat                # (~) ~2000-trade slice with >=1 BUY and >=1 SELL
│   └── signals.png                # Generated visualization plot[cite: 7, 11]
│
├── stageA_plus/                   # (Stub for Jaren) Custom reader & fast parser[cite: 7]
│   └── .gitkeep
│
├── stageB/                        # (Leroi & Yong Sheng) In-memory struct array & ladder[cite: 7, 8]
│   ├── engine/                    # (+) Shared modules (vwap, ohlc, sma, signal, report) + variants
│   ├── stageB_manual.c            # (+) Stage B streaming engine with struct modules
│   ├── stageB_inmem.c             # (+) In-memory array, double; -DNAIVE_SMA builds the naive SMA
│   └── stageB_fixed.c             # (+) In-memory array, fixed-point (long long) prices
│
├── stageC/                        # (Stub for Lucas & Leroi) Replay, rival bot & dynamic memory[cite: 8]
│   └── .gitkeep
│
├── logs/                          # Benchmark outputs & predictions logbook
│   ├── predictions.md             # (~) Shared logbook: >=6 entries, >=1 per stage, dated + signed
│   ├── perf_results/              # (~) perf stat raw outputs per member (EVERY member)
│   └── memory/                    # (+) valgrind / max-RSS outputs per version
│
└── docs/                          # Report assets and presentation slides
    └── figures/                   # Speedup, latency and money-lost graphs, diagrams[cite: 14]
```


### Task Delegation
General Delegation but not restricted to 
```
+-------------------------------------------------------------------------------+
|                                  PROJECT TEAM                                 |
+-------------------------------------------------------------------------------+
| Joel (Lead & System Baseline)                                                 |
| - Stage 0: Repo setup, Makefile, directory structure, Overleaf                |
| - Stage A: Core streaming engine (VWAP, OHLC, SMA, signals)                   |
| - Gatekeeper: test20.csv verification across all stages                       |
| - (+) Agree Version 0 with the group (baseline for ladder, rival bot, race)   |
| - (~) Makefile: `make` on the Pi builds EVERY ladder version                  |
| - (+) Report: problem definition, problem analysis, cover page contributions  |
| - (+) Logbook: >=1 Stage A entry                                              |
+-------------------------------------------------------------------------------+
| Yong Sheng (Compiler Optimization & Architecture)                             |
| - Stage 0: Loop (N=100M, N typed in by user) & recursive vs iterative         |
| - (+) Stage 0: LED on/off around the timed loop in own program                |
| - Stage B: Optimization flags (-O0 to -O3, -mcpu=cortex-a76)                  |
| - (~) Ladder perf stat (cycles, IPC, cache misses) on each rung               |
| - Final: Efficiency ladder table/graph & annotated assembly                   |
| - (+) Stretch (optional): hand-written AArch64/NEON kernel vs -O3             |
+-------------------------------------------------------------------------------+
| Jaren (Fast I/O & Custom Parsing)                                             |
| - Stage A+: Replace helper with pointer-based read_trade()                    |
| - Stage A+: In-place fast digit parser (remove slow sscanf)                   |
| - Benchmark: I/O throughput speedup on stress_full.csv                        |
| - (+) Logbook: >=1 Stage A+ entry                                             |
+-------------------------------------------------------------------------------+
| Leroi (Memory Architecture & Numeric Scaling)                                 |
| - Stage B: Trade structs & global in-memory array                             |
| - (~) Stage B: naive SMA AND running-sum SMA, compare times                   |
| - Stage B: Fixed-point conversion (long long) & overflow math                 |
| - (~) Stage C: malloc array sized to file, compare memory (valgrind)          |
| - (+) Report: trade-off analysis (speed/readability, fixed vs double)         |
| - (+) Report: deploy recommendation for power/memory-limited Pi (4GB)         |
| - (+) Stretch (optional): Struct-of-Arrays + cache-line explanation           |
+-------------------------------------------------------------------------------+
| Lucas (Hardware I/O, Simulation & Presentation)                               |
| - Stage A: Gnuplot pipeline (signals.dat -> signals.png)                      |
| - (~) Stage C: Replay at 1x (>=10 min), 100x, 1000x & latency metrics         |
| - (~) Stage C: Rival bot (R=1us std, sweep 0.1us-100us), P&L, money lost      |
| - Stage C: Pi LED signals & side-by-side terminal Race Demo                   |
| - (+) Logbook: >=1 Stage C entry                                              |
| - (+) Stretch (optional): p50 / p99 latency (sorting)                         |
+-------------------------------------------------------------------------------+
| ALL 5 MEMBERS (Mandatory Individual Marks)                                    |
| - (~) stress_full.csv benchmark write-up on own Laptop + Pi 5 (10%):          |
|       set-up, time + throughput for EVERY version, perf stat, memory          |
| - (~) Logbook (5%): >=6 entries total, >=1 per stage, dated + signed          |
| - (+) Pseudocode for own stage(s) in the report                               |
| - Speak for ~3 mins with camera on in presentation video                      |
+-------------------------------------------------------------------------------+



                                  =======================================
                                                 STAGE 0
                                        Environment & Baseline
                                  =======================================
                                                     |
             +---------------------------------------+---------------------------------------+
             |                                                                               |
      [Joel]                                                                    [Yong Sheng]
      - Initialize Git repo & directory structure                               - Summation benchmark (N=100M,
      - Set up Overleaf master report                                             (~) N typed in by user)
      - Distribute timing.h, led.h, test_helpers.c                                under -O0, -O1, -O2, -O3
                                                                                - Recursive vs Iterative test (N=10k)
                                                                                - (+) LED on/off around timed loop
             |                                                                               |
             +---------------------------------------+---------------------------------------+
                                                     |
                                            [Lucas & Yong Sheng]
                                            - Pi 5 boot, SSH, sudo setup
                                            - Verify led.h with test_helpers.c
                                            - (~) Laptop vs Pi at -O0 and -O2,
                                              in ONE logbook entry (Stage 0)
                                                     |
                                                     v
                                  =======================================
                                                 STAGE A
                                             Streaming Core
                                  =======================================
                                                     |
             +---------------------------------------+---------------------------------------+
             |                                                                               |
       [Joel]                                                                      [Lucas]
       - Implement baseline streaming engine                                       - Gnuplot visual pipeline
         (trade_io helper library)                                                 - Generate signals.dat
       - Compute running VWAP                                                        (~) slice has >=1 BUY & >=1 SELL
       - Build 1-second OHLC bars                                                  - Render signals.png showing
       - Implement circular SMA buffers                                              SMA crossover points
       - Generate BUY / SELL signals                                                       |
       - Verify output matches test20.csv                                                  |
       - (+) Logbook: >=1 Stage A entry                                                    |
             |                                                                               |
             +---------------------------------------+---------------------------------------+
                                                     |
                                                     v
                                  =======================================
                                                 STAGE A+
                                           Fast I/O & Parsing
                                  =======================================
                                                     |
                                                  [Jaren]
                                                  - Custom read_trade() with pointers
                                                  - Fast digit/float parser (no sscanf)
                                                  - Benchmark speedup on stress_full.csv
                                                  - Verify test20.csv correctness
                                                  - (+) Logbook: >=1 Stage A+ entry
                                                     |
                                                     v
                                  =======================================
                                                 STAGE B
                                        In-Memory Engine & Ladder
                                  =======================================
                                                     |
             +---------------------------------------+---------------------------------------+
             |                                       |                                       |
          [Leroi]                              [Joel & Yong Sheng]                     [Yong Sheng]
          - Global array of Trade structs      - (+) Agree Version 0              - Benchmark compiler flags:
          - (~) Naive SMA vs running-sum         (baseline for every speedup,       -O0, -O2, -O3, -mcpu=cortex-a76
            SMA, compare times                    rival bot and race)              - Run perf stat (cycles, IPC,
          - Fixed-point math (long long)                                             cache misses) on each rung
          - Check & prevent 64-bit overflow                                        - Build Efficiency Ladder
          - (+) Logbook: >=1 Stage B entry                                           (>=4 rungs, vs Version 0,
             |                                                                       on stress_full.csv)
             |                                                                       table and speedup graph
             |                                                                               |
             +---------------------------------------+---------------------------------------+
                                                     |
                                                     v
                                  =======================================
                                                 STAGE C
                                         Replay, Latency & Rival
                                  =======================================
                                                     |
             +---------------------------------------+---------------------------------------+
             |                                       |                                       |
          [Leroi]                                 [Lucas]                               [Yong Sheng]
          - Replace static array with             - Real-time replay loop                 - Generate assembly
            dynamic malloc allocation               (~) 1x (>=10 min), 100x, 1000x          listing (gcc -S -O2)
          - (~) Compare memory use:               - (~) Avg & worst-case latency          - Annotate main loop
            fixed array vs malloc                   at EACH replay speed                    instructions
            (valgrind Memcheck + Massif)          - Rival bot P&L simulation                     |
          - Memory footprint analysis               (~) R=1us standard,                          |
                                                    sweep R = 0.1us to 100us,                   |
                                                    money-lost graph                            |
                                                  - LED / ANSI terminal signals                  |
                                                  - Direct side-by-side Race Demo               |
                                                    (V0 vs fastest, lag counter, P&L)           |
                                                  - (+) Logbook: >=1 Stage C entry               |
                                                     |                                       |
             +---------------------------------------+---------------------------------------+
                                                     |
                                                     v
                                  =======================================
                                            FINAL DELIVERABLES
                                  =======================================
                                                     |
    +------------------------------------------------+------------------------------------------------+
    |                                                |                                                |
[All 5 Members (Mandatory)]                    [All 5 Members]                               [Lucas & Joel]
- (~) Individual benchmark write-up on         - Presentation video (~3 min/person,          - Final demo video (5 mins)
  stress_full.csv (Laptop & Pi 5 per member):    15 mins total, cameras on)                    with race & LED, laptop + Pi
  set-up, time + throughput for EVERY          - Report write-up for respective sections     - Final UoG-PMCOA.zip
  version, perf stat, memory per version         in Overleaf master template                   packaging
- (~) Logbook: >=6 entries total, >=1 per      - (+) Pseudocode for own stage(s)
  stage, dated + signed
                                               [Joel]                                        [Leroi]
                                               - (+) Problem definition & analysis           - (+) Trade-off analysis (speed vs
                                               - (+) Cover page: member contributions          readability, fixed-point vs double)
                                               - (~) Makefile builds every ladder version    - (+) Deploy recommendation for a
                                                                                               power/memory-limited Pi (4GB)

                                  ---------------------------------------
                                     (+) STRETCH GOALS (optional, +10)
                                  ---------------------------------------
                                  - Yong Sheng: hand-written AArch64/NEON kernel vs -O3
                                  - Leroi: Struct-of-Arrays, explain with cache lines
                                  - Lucas: p50 / p99 latency (needs sorting)
                                  - Unassigned: C vs Python version of Stage A


```
