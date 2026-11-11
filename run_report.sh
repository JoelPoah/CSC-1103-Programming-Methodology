#!/usr/bin/env bash
#
# run_report.sh - test and time every version on the ladder, write a report
#
#   make                    build everything first
#   ./run_report.sh         full report: correctness + benchmark on stress_full.csv
#   ./run_report.sh quick   same, but benchmark on dev_1M.csv with 3 runs (fast)
#   ./run_report.sh test    correctness only (test20.csv), no timing
#
# Output:
#   results/report.md       the report (also printed on screen)
#   results/logs/           the raw output of every run (benchmark logs)
#   results/ladder.dat/.png speedup of every version against Version 0
#
# Settings (optional), e.g.  RUNS=7 ./run_report.sh
#   RUNS      how many times each program is timed (default 5, median is reported)
#   CORE      CPU core to pin to with taskset (default: the last core)
#   RUN_WITH  a program to run the executables with, e.g. an emulator
#
# Run from the project root. Works in bash on the Pi, Linux, macOS and
# Git Bash on Windows. taskset, perf and memory figures are Linux only.

RUNS=${RUNS:-5}
MODE=stress                 # which data file the benchmark uses
JOB=${1:-full}

if [ "$JOB" = "quick" ]; then
    MODE=dev
    RUNS=${RUNS_QUICK:-3}
fi

REPORT=results/report.md
LOGS=results/logs
EXPECTED=data/group_data/test20_expected.txt

# The group's recorded answer for Version 0 on dev_1M.csv (from the README on main).
# Used as an extra check that Version 0 still gives the same numbers.
KNOWN_DEV_VWAP="76217.01291587"
KNOWN_DEV_SIGNALS="2509 BUY, 2469 SELL"

# ---- the ladder: executable name | what changed in this version ----
LADDER=(
    "v0_stageA_naive_sma|Stage A: helper library (sscanf), double, naive SMA"
    "v1_stageAplus_own_reader|Stage A+: own read_trade() with pointers"
    "v2_stageAplus_fast_parser|Stage A+: fast parser instead of sscanf"
    "v3_stageB_in_memory|Stage B: array of structs, load then process"
    "v4_stageB_running_sma|Stage B: running-sum SMA"
    "v5_stageB_fixed_point|Stage B: fixed-point (long long) prices"
    "v6_stageC_malloc|Stage C: malloc array sized to the file"
    "v7_asm_engine|Stretch: engine in hand-written assembly"
)

# ---- compiler-flag rungs: executable name | flags ----
FLAG_RUNGS=(
    "v0_stageA_naive_sma_O0|V0 at -O0"
    "v0_stageA_naive_sma|V0 at -O2"
    "v0_stageA_naive_sma_O3|V0 at -O3"
    "v6_stageC_malloc_O0|V6 at -O0"
    "v6_stageC_malloc|V6 at -O2"
    "v6_stageC_malloc_O3|V6 at -O3"
    "v6_stageC_malloc_O3_a76|V6 at -O3 -mcpu=cortex-a76"
    "v7_asm_engine_O3|V7 (assembly engine) at -O3"
)

# ======================= small helpers =======================

# say - write a line to the report and to the screen
say() {
    echo "$@" | tee -a "$REPORT"
}

# exists NAME - is bin/NAME built?
exists() {
    [ -f "bin/$1" ] || [ -f "bin/$1.exe" ]
}

# run_version NAME MODE - run one executable (pinned to one core if possible)
run_version() {
    $PIN $RUN_WITH "bin/$1" "$2" 2>&1 | tr -d '\r'
}

# field LABEL - read "LABEL: 123.4 ms" lines from stdin, print the number
field() {
    awk -F': ' -v label="$1" '$1 == label { print $2 + 0 }'
}

# median - print the middle value of the numbers on stdin
median() {
    sort -n | awk '{ value[NR] = $1 } END { if (NR > 0) print value[int((NR + 1) / 2)] }'
}

# one_decimal NUMBER - print it with one decimal place, or "-" if there is none
one_decimal() {
    if [ -z "$1" ]; then
        echo "-"
    else
        awk -v x="$1" 'BEGIN { printf "%.1f", x }'
    fi
}

# answers - keep only the lines that hold answers, without trailing spaces
answers() {
    tr -d '\r' | grep -E '^[0-9]|^Final VWAP|^Signals|^One-second|^second' | sed 's/[[:space:]]*$//'
}

# time_version NAME - run NAME $RUNS times on the benchmark file.
# Leaves the results in: T_TOTAL T_PROCESS T_LOAD (median ms), N_TRADES,
# R_VWAP, R_SIGNALS and the raw output in results/logs/NAME_MODE.txt
time_version() {
    local name=$1 log="$LOGS/$1_$MODE.txt" i
    : > "$log"
    for i in $(seq 1 "$RUNS"); do
        echo "--- run $i of $RUNS ---" >> "$log"
        run_version "$name" "$MODE" >> "$log"
    done
    T_TOTAL=$(field "Total time" < "$log" | median)
    T_PROCESS=$(field "Process time" < "$log" | median)
    T_LOAD=$(field "Load time" < "$log" | median)
    N_TRADES=$(field "Trades" < "$log" | head -n 1)
    R_VWAP=$(grep '^Final VWAP' "$log" | head -n 1 | sed 's/Final VWAP: //')
    R_SIGNALS=$(grep '^Signals' "$log" | head -n 1 | sed 's/Signals: //')
}

# ======================= set-up =======================

if [ ! -d src ] || [ ! -d bin ]; then
    echo "Run this from the project root (the folder with the Makefile)."
    exit 1
fi
if ! exists v0_stageA_naive_sma; then
    echo "Nothing is built yet. Type: make"
    exit 1
fi
mkdir -p "$LOGS"
: > "$REPORT"

# pin every run to one core, if taskset exists (Linux / Pi)
PIN=""
if command -v taskset > /dev/null 2>&1 && command -v nproc > /dev/null 2>&1; then
    CORE=${CORE:-$(( $(nproc) - 1 ))}
    PIN="taskset -c $CORE"
fi

GOVERNOR="not available"
if [ -r /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor ]; then
    GOVERNOR=$(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor)
fi

MODEL=$(uname -s -m)
if [ -r /proc/device-tree/model ]; then
    MODEL=$(tr -d '\0' < /proc/device-tree/model)
fi

say "# Tick-to-Trade: automated test report"
say ""
say "## 1. System set-up"
say ""
say "| Item | Value |"
say "|---|---|"
say "| Date | $(date '+%Y-%m-%d %H:%M') |"
say "| Machine | $MODEL |"
say "| OS | $(uname -s -r) |"
say "| Compiler | $(gcc --version 2>/dev/null | head -n 1) |"
say "| CPU governor | $GOVERNOR |"
say "| Pinned to one core | ${PIN:-no (taskset not available)} |"
say "| Runs per measurement | $RUNS (the median is reported) |"
if [ -n "$RUN_WITH" ]; then
    say "| Run with | \`$RUN_WITH\` - **emulated: the timings below do not count** |"
fi
say ""
if [ "$GOVERNOR" != "performance" ] && [ "$GOVERNOR" != "not available" ]; then
    say "> Note: the CPU governor is \`$GOVERNOR\`. For the marked run on the Pi set it to"
    say "> \`performance\`: \`echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor\`"
    say ""
fi

# ======================= 2. correctness =======================

say "## 2. Correctness: test20.csv against test20_expected.txt"
say ""

ALL_PASS=yes
if [ ! -f data/group_data/test20.csv ] || [ ! -f "$EXPECTED" ]; then
    say "**SKIPPED**: data/group_data/test20.csv or test20_expected.txt is missing."
    ALL_PASS=no
else
    answers < "$EXPECTED" > "$LOGS/test20_expected_answers.txt"
    say "| Version | test20 |"
    say "|---|---|"
    for entry in "${LADDER[@]}" "${FLAG_RUNGS[@]}"; do
        name=${entry%%|*}
        if ! exists "$name"; then
            continue
        fi
        run_version "$name" test20 > "$LOGS/${name}_test20.txt"
        if answers < "$LOGS/${name}_test20.txt" | diff -q - "$LOGS/test20_expected_answers.txt" > /dev/null; then
            say "| $name | PASS |"
        else
            say "| $name | **FAIL** (compare $LOGS/${name}_test20.txt with $EXPECTED) |"
            ALL_PASS=no
        fi
    done
    say ""
    if [ "$ALL_PASS" = "yes" ]; then
        say "Every version gives exactly the expected table, VWAP, signals and OHLC bar."
    else
        say "**At least one version does not match. Fix it before trusting any timing.**"
    fi
fi
say ""

if [ "$JOB" = "test" ]; then
    echo "Report written to $REPORT"
    [ "$ALL_PASS" = "yes" ] && exit 0
    exit 1
fi

# ======================= 3. the ladder =======================

if [ "$MODE" = "stress" ]; then
    DATA_FILE=data/stress_full.csv
else
    DATA_FILE=data/group_data/dev_1M.csv
fi

say "## 3. Efficiency ladder on $DATA_FILE"
say ""
if [ ! -f "$DATA_FILE" ]; then
    say "**SKIPPED**: $DATA_FILE is missing. Put the file there (or change the path in"
    say "src/config.h and rebuild). \`./run_report.sh quick\` uses dev_1M.csv instead."
    if [ "$MODE" = "stress" ] && [ -f data/group_data/stress_full.csv ]; then
        say "(Found data/group_data/stress_full.csv - move it up one folder to data/.)"
    fi
    echo "Report written to $REPORT"
    exit 1
fi
if [ "$MODE" != "stress" ]; then
    say "> Quick mode: timed on dev_1M.csv. The marked ladder must use stress_full.csv."
    say ""
fi

say "| Version | What changed | Total ms | ns / trade | Trades / s | Speedup | Load ms | Process ms | Signals | Same as V0? |"
say "|---|---|---:|---:|---:|---:|---:|---:|---|---|"

: > results/ladder.dat
V0_TOTAL=""
V0_SIGNALS=""
V0_VWAP=""
for entry in "${LADDER[@]}"; do
    name=${entry%%|*}
    what=${entry#*|}
    if ! exists "$name"; then
        say "| $name | $what | not built on this machine | | | | | | | |"
        continue
    fi
    echo "  timing $name ..." >&2
    time_version "$name"
    if [ -z "$V0_TOTAL" ]; then
        V0_TOTAL=$T_TOTAL
        V0_SIGNALS=$R_SIGNALS
        V0_VWAP=$R_VWAP
    fi
    same="yes"
    if [ "$R_SIGNALS" != "$V0_SIGNALS" ]; then
        same="**no**"
    fi
    row=$(awk -v t="$T_TOTAL" -v n="$N_TRADES" -v v0="$V0_TOTAL" 'BEGIN {
        printf "%.1f | %.1f | %.0f | %.2fx", t, t * 1e6 / n, n / (t / 1000), v0 / t }')
    speedup=$(awk -v t="$T_TOTAL" -v v0="$V0_TOTAL" 'BEGIN { printf "%.3f", v0 / t }')
    say "| $name | $what | $row | $(one_decimal "$T_LOAD") | $(one_decimal "$T_PROCESS") | $R_SIGNALS | $same |"
    echo "${name%%_*} $speedup" >> results/ladder.dat
done
say ""
say "Trades in the file: $N_TRADES. Version 0 final VWAP: $V0_VWAP."
say "Speedup = Version 0 total time / this version's total time."
say "Load + Process are shown for the in-memory versions; the streaming versions (V0-V2) do both at once."
say ""
say "About \"Same as V0?\": a double cannot hold most prices exactly, so when the short"
say "and long SMA are (nearly) equal, rounding decides which one looks bigger. The"
say "naive sum (V0-V3), the running sum (V4) and exact whole numbers (V5+) round"
say "differently, so a few signals move. The fixed-point versions are the exact ones."
say ""

if [ "$MODE" = "dev" ]; then
    if [ "$V0_VWAP" = "$KNOWN_DEV_VWAP" ] && [ "$V0_SIGNALS" = "$KNOWN_DEV_SIGNALS" ]; then
        say "Version 0 on dev_1M.csv matches the group's recorded answer ($KNOWN_DEV_VWAP, $KNOWN_DEV_SIGNALS): **yes**"
    else
        say "Version 0 on dev_1M.csv matches the group's recorded answer ($KNOWN_DEV_VWAP, $KNOWN_DEV_SIGNALS): **no** (got $V0_VWAP, $V0_SIGNALS)"
    fi
    say ""
fi

if command -v gnuplot > /dev/null 2>&1; then
    if gnuplot tools/plot_ladder.gnuplot 2> /dev/null; then
        say "Speedup graph: results/ladder.png"
        say ""
    fi
fi

# ======================= 4. compiler flags =======================

say "## 4. Compiler flags"
say ""
say "| Build | Total ms | Process ms | Speedup vs V0 at -O2 |"
say "|---|---:|---:|---:|"
C_O3_PROCESS=""
C_A76_PROCESS=""
ASM_PROCESS=""
for entry in "${FLAG_RUNGS[@]}"; do
    name=${entry%%|*}
    what=${entry#*|}
    if ! exists "$name"; then
        say "| $what | not built on this machine | | |"
        continue
    fi
    echo "  timing $name ..." >&2
    time_version "$name"
    speedup=$(awk -v t="$T_TOTAL" -v v0="$V0_TOTAL" 'BEGIN { printf "%.2fx", v0 / t }')
    say "| $what | $(one_decimal "$T_TOTAL") | $(one_decimal "$T_PROCESS") | $speedup |"
    case "$name" in
        v6_stageC_malloc_O3)     C_O3_PROCESS=$T_PROCESS ;;
        v6_stageC_malloc_O3_a76) C_A76_PROCESS=$T_PROCESS ;;
        v7_asm_engine_O3)        ASM_PROCESS=$T_PROCESS ;;
    esac
done
say ""

# ======================= 5. assembly against the compiler =======================

say "## 5. Can hand-written assembly beat the compiler?"
say ""
if [ -z "$ASM_PROCESS" ]; then
    say "**Not measured on this machine.** The assembly engine (src/engine_asm/) is AArch64"
    say "code, so v7 is only built on 64-bit ARM Linux. Run \`make\` and this script on the Pi."
else
    say "V6 and V7 are the same program (same main, same loader, same data in memory)."
    say "Only the four engine files differ: C compiled at -O3, or our assembly."
    say "The engine only runs in the Process phase, so that is the time compared."
    say ""
    say "| Engine | Process ms (median of $RUNS) |"
    say "|---|---:|"
    say "| C, gcc -O3 | $C_O3_PROCESS |"
    if [ -n "$C_A76_PROCESS" ]; then
        say "| C, gcc -O3 -mcpu=cortex-a76 | $C_A76_PROCESS |"
    fi
    say "| Hand-written assembly | $ASM_PROCESS |"
    say ""
    # compare against the best compiler build; within 3% counts as a tie
    verdict=$(awk -v c="$C_O3_PROCESS" -v c2="$C_A76_PROCESS" -v a="$ASM_PROCESS" 'BEGIN {
        best = c; if (c2 != "" && c2 + 0 < best + 0) best = c2
        gain = (best - a) / best * 100
        if (gain > 3)       printf "**YES** - the assembly engine is %.1f%% faster than the best compiler build.", gain
        else if (gain < -3) printf "**NO** - the best compiler build is %.1f%% faster than the assembly engine.", -gain
        else                printf "**TIE** - within 3%% of each other (%.1f%%), which is inside the run-to-run noise.", gain
    }')
    say "Verdict: $verdict"
    say ""
    say "Why: see \"Assembly against the compiler\" in README.md. In short, the assembly"
    say "replaces the divide the compiler must use for \`% window\` with a compare, and uses"
    say "no branches in the SMA and OHLC updates; everything else the compiler already"
    say "does as well as we can."
    if [ -n "$RUN_WITH" ]; then
        say ""
        say "> These numbers came from an emulator, so the verdict above is NOT valid."
        say "> Only a run on the real Pi counts."
    fi
fi
say ""

# ======================= 6. memory =======================

say "## 6. Memory"
say ""
if /usr/bin/time -v true > /dev/null 2>&1 && [ -z "$RUN_WITH" ]; then
    say "| Version | Peak memory in use (max RSS, MB) | Trade array (MB) |"
    say "|---|---:|---:|"
    for entry in "${LADDER[@]}"; do
        name=${entry%%|*}
        if ! exists "$name"; then
            continue
        fi
        /usr/bin/time -v "bin/$name" "$MODE" > "$LOGS/${name}_memory.txt" 2>&1
        rss=$(awk -F': ' '/Maximum resident set size/ { printf "%.1f", $2 / 1024 }' "$LOGS/${name}_memory.txt")
        array=$(awk -F': ' '/^Trade array/ { printf "%.1f", $2 / 1e6 }' "$LOGS/${name}_memory.txt")
        say "| $name | $rss | ${array:--} |"
    done
    say ""
    say "V0-V2 stream the file and keep only the SMA windows, so they stay tiny."
    say "V3-V5 reserve a fixed 10,000,000-slot array (320 MB) however small the file is;"
    say "the system only hands over the pages that are actually written, which is why"
    say "the peak in use is lower than 320 MB. V6 asks malloc for exactly what the file needs."
else
    say "Not measured: needs GNU time (/usr/bin/time -v). On the Pi or Linux: sudo apt install time"
fi
say ""

# ======================= 7. hardware counters =======================

say "## 7. Hardware counters (perf stat)"
say ""
if command -v perf > /dev/null 2>&1 && [ -z "$RUN_WITH" ] && perf stat -e cycles true > /dev/null 2>&1; then
    say "| Version | Cycles | Instructions | IPC | Cache misses |"
    say "|---|---:|---:|---:|---:|"
    for entry in "${LADDER[@]}"; do
        name=${entry%%|*}
        if ! exists "$name"; then
            continue
        fi
        $PIN perf stat -x, -e cycles,instructions,cache-misses "bin/$name" "$MODE" > /dev/null 2> "$LOGS/${name}_perf.txt"
        cycles=$(awk -F, '$3 ~ /cycles/ { print $1 }' "$LOGS/${name}_perf.txt" | head -n 1)
        instructions=$(awk -F, '$3 ~ /instructions/ { print $1 }' "$LOGS/${name}_perf.txt" | head -n 1)
        misses=$(awk -F, '$3 ~ /cache-misses/ { print $1 }' "$LOGS/${name}_perf.txt" | head -n 1)
        ipc=$(awk -v c="$cycles" -v i="$instructions" 'BEGIN { if (c > 0) printf "%.2f", i / c; else print "-" }')
        say "| $name | $cycles | $instructions | $ipc | $misses |"
    done
else
    say "Not measured: perf is not available here. It is installed on the Pi"
    say "(run this script with sudo there if perf says it is not permitted)."
fi
say ""
say "Raw output of every run: $LOGS/"

echo ""
echo "Report written to $REPORT"
[ "$ALL_PASS" = "yes" ] && exit 0
exit 1
