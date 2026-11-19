#!/bin/bash
# run_soa.sh - time V5 against V5-SoA (Struct-of-Arrays), same session (Leroi, 2026-11-19)
# One warm-up run of each, then 5 runs alternating V5 and SoA, pinned to core 2.
# Each run's full output is saved; the summary takes stress_full's Load/Process times.
# Then perf stat (one extra run each) and max RSS (3 runs each, logs/leroi_pi/maxrss.c).
# Run from the project root:  bash logs/leroi_laptop/soa_2026-11-19/run_soa.sh
OUT=${OUT:-logs/leroi_laptop/soa_2026-11-19}
V5=./bin/v5_stageB_fixed_point
SOA=./bin/soa_stageB_fixed
mkdir -p "$OUT/runs"

{
echo "date: $(date -Is)"
echo "host: $(uname -n) $(uname -m) $(uname -r)"
echo "gcc: $(gcc --version | head -1)"
echo "governor: $(cat /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor | sort | uniq -c | xargs)"
echo "perf_event_paranoid: $(cat /proc/sys/kernel/perf_event_paranoid)"
} > "$OUT/system.txt"

taskset -c 2 $V5  > "$OUT/runs/v5_warmup.txt"
taskset -c 2 $SOA > "$OUT/runs/soa_warmup.txt"
for i in 1 2 3 4 5; do
    taskset -c 2 $V5  > "$OUT/runs/v5_run$i.txt"
    taskset -c 2 $SOA > "$OUT/runs/soa_run$i.txt"
done

# stress_full is the last of the three files, so its times are the last ones printed
{
echo "run  version  load_ms  process_ms"
for i in 1 2 3 4 5; do
    for v in v5 soa; do
        f="$OUT/runs/${v}_run$i.txt"
        load=$(grep "Load time" "$f" | tail -1 | awk '{print $3}')
        proc=$(grep "Process time" "$f" | tail -1 | awk '{print $3}')
        echo "$i  $v  $load  $proc"
    done
done
} > "$OUT/times.txt"

for v in v5 soa; do
    if [ $v = v5 ]; then b=$V5; else b=$SOA; fi
    perf stat -e cycles,instructions,cache-references,cache-misses -o "$OUT/${v}_perf.txt" \
        taskset -c 2 $b > /dev/null
done

gcc -O2 -Wall logs/leroi_pi/maxrss.c -o logs/leroi_pi/maxrss || exit 1
for v in v5 soa; do
    if [ $v = v5 ]; then b=$V5; else b=$SOA; fi
    for i in 1 2 3; do
        ./logs/leroi_pi/maxrss taskset -c 2 $b 2>&1 > /dev/null | awk '/maxrss_kB/ { print $2 }'
    done > "$OUT/${v}_maxrss_kB.txt"
done
echo done
