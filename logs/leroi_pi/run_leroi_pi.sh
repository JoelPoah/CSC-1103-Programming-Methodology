#!/usr/bin/env bash
# run_leroi_pi.sh - Leroi's valgrind evidence for the src/ ladder (Stage B / Stage C memory)
#
# Time, throughput, max RSS and perf stat for every version come from the group's
# benchmark tool (./benchmark/ladder_bench -m leroi). This script adds what that tool
# does not measure: valgrind Memcheck (no leaks / invalid accesses) and Massif (peak
# memory) for the in-memory rungs, mainly V5 (fixed global array) vs V6 (malloc).
#
# It also measures max RSS of every -O2 ladder version with maxrss.c (see why there).
#
# Run from the project root (a few minutes per version on the Pi):
#   bash logs/leroi_pi/run_leroi_pi.sh                     # results in logs/leroi_pi/
#   OUT=logs/leroi_laptop bash logs/leroi_pi/run_leroi_pi.sh   # on the laptop
#   STRESS_MASSIF=1 ...                                    # also Massif on stress_full (slow)
# valgrind uses "dev" (test20 + dev_1M): it is 20-50x slower, far too slow for stress_full.

set -u
OUT=${OUT:-logs/leroi_pi}
DATA=data/group_data
mkdir -p "$OUT/valgrind"

for f in test20.csv dev_1M.csv; do
    [ -f "$DATA/$f" ] || { echo "missing $DATA/$f - copy the CSVs from the laptop first"; exit 1; }
done
command -v valgrind >/dev/null || { echo "missing valgrind (sudo apt install valgrind)"; exit 1; }

# ---------- system details ladder_bench's device.txt does not record ----------
{
    echo "date:      $(date '+%Y-%m-%d %H:%M %Z')"
    echo "git:       $(git rev-parse --short HEAD) ($(git rev-parse --abbrev-ref HEAD))"
    echo "model:     $(tr -d '\0' < /proc/device-tree/model 2>/dev/null)"
    echo "kernel:    $(uname -srm)"
    echo "page size: $(getconf PAGESIZE) bytes"
    echo "valgrind:  $(valgrind --version)"
    echo
    free -h
} > "$OUT/system.txt"
cat "$OUT/system.txt"

# ---------- build the ladder exactly as "make ladder" does (-O2 -Wall) ----------
make -s ladder || exit 1
size bin/v3_stageB_in_memory bin/v4_stageB_running_sma bin/v5_stageB_fixed_point bin/v6_stageC_malloc \
    > "$OUT/size_bss.txt"                           # .bss = the fixed global array (320 MB)
cat "$OUT/size_bss.txt"

# ---------- Memcheck on every in-memory rung, Massif on V5 vs V6 ----------
# V3, V4: double, global Trade array      V5: fixed-point, global TradeFx array
# V6:     fixed-point, malloc'd array     (V5 vs V6 differ only in where the array lives)
for v in v3_stageB_in_memory v4_stageB_running_sma v5_stageB_fixed_point v6_stageC_malloc; do
    echo; echo "== $v: Memcheck"
    valgrind --leak-check=full --show-leak-kinds=all ./bin/$v dev \
        > /dev/null 2> "$OUT/valgrind/${v}_memcheck.txt"
    grep -E 'ERROR SUMMARY|definitely lost|All heap blocks|total heap usage' "$OUT/valgrind/${v}_memcheck.txt" \
        | sed 's/==[0-9]*== /  /'
done

# Massif: --pages-as-heap=yes also counts the global array (it is not heap memory),
# so it gives the "reserved" figure; the plain run counts malloc'd memory only.
mpeak() { awk -F= '/mem_heap_B/ { if ($2 > m) m = $2 } END { printf "%.3f MB", m / 1e6 }' "$1"; }
echo; echo "== Massif peak (dev run)" | tee "$OUT/massif_summary.txt"
for v in v5_stageB_fixed_point v6_stageC_malloc; do
    valgrind --tool=massif --pages-as-heap=yes --massif-out-file="$OUT/valgrind/${v}_massif_pages.out" \
        ./bin/$v dev > /dev/null 2>&1
    valgrind --tool=massif --massif-out-file="$OUT/valgrind/${v}_massif_heap.out" \
        ./bin/$v dev > /dev/null 2>&1
    echo "  $v: reserved (pages-as-heap) $(mpeak "$OUT/valgrind/${v}_massif_pages.out"), heap only $(mpeak "$OUT/valgrind/${v}_massif_heap.out")" \
        | tee -a "$OUT/massif_summary.txt"
done

# optional (slow, ~10x the dev run): the same Massif peaks on the full run (stress_full)
if [ "${STRESS_MASSIF:-0}" = 1 ]; then
    for v in v5_stageB_fixed_point v6_stageC_malloc; do
        valgrind --tool=massif --pages-as-heap=yes --massif-out-file="$OUT/valgrind/${v}_massif_pages_stress.out" \
            ./bin/$v > /dev/null 2>&1
        echo "  $v (stress_full): reserved (pages-as-heap) $(mpeak "$OUT/valgrind/${v}_massif_pages_stress.out")" \
            | tee -a "$OUT/massif_summary.txt"
    done
fi

# ---------- max RSS (physical memory) of every -O2 ladder version, median of 3 ----------
# ladder_bench's peak memory cannot go below its own ~15 MB (see maxrss.c), which hides
# how little the streaming versions (V0-V2) use, so measure it here with a tiny wrapper.
gcc -O2 -Wall logs/leroi_pi/maxrss.c -o logs/leroi_pi/maxrss || exit 1
rss_mb() {   # rss_mb <program> [args]  -> median of 3 runs, in MB (10^6 bytes)
    for n in 1 2 3; do
        ./logs/leroi_pi/maxrss taskset -c 2 "$@" 2>&1 > /dev/null | awk '/maxrss_kB/ { print $2 }'
    done | sort -n | awk 'NR == 2 { printf "%.1f", $1 * 1024 / 1e6 }'
}
echo; echo "== max RSS, median of 3 (MB = 10^6 bytes)" | tee "$OUT/maxrss.txt"
LADDER="v0_stageA_naive_sma:stress v1_stageAplus_own_reader:--stress v2_stageAplus_fast_parser:--stress
        v3_stageB_in_memory: v4_stageB_running_sma: v5_stageB_fixed_point: v6_stageC_malloc: v7_stageC_fread:"
[ -x bin/v8_stageC_asm_ohlc ] && LADDER="$LADDER v8_stageC_asm_ohlc:"
for entry in $LADDER; do
    v=${entry%%:*}; arg=${entry#*:}
    printf "  %-28s full run %8s MB" "$v" "$(rss_mb ./bin/$v $arg)" | tee -a "$OUT/maxrss.txt"
    case $v in v[3-8]*) printf "   dev run %7s MB" "$(rss_mb ./bin/$v dev)" | tee -a "$OUT/maxrss.txt";; esac
    echo | tee -a "$OUT/maxrss.txt"
done
echo "  (V0-V2 stream stress_full only; V3+ always run test20 + dev_1M first, \"dev\" stops before stress_full)" \
    | tee -a "$OUT/maxrss.txt"

echo; echo "done - results in $OUT/ (on the Pi: copy that folder back to the laptop)"
