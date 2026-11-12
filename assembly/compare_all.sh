#!/bin/bash
# compare_all.sh - time ALL eight Stage B builds in one session:
#
#   program            engine        SMA        OHLC
#   stageB_inmem       double        sma.c      ohlc.c        (pure C)
#   stageB_inmem_sma   double        sma.s      ohlc.c
#   stageB_inmem_ohlc  double        sma.c      ohlc.s
#   stageB_inmem_asm   double        sma.s      ohlc.s
#   stageB_fixed       fixed-point   sma_fx.c   ohlc_fx.c     (pure C)
#   stageB_fixed_sma   fixed-point   sma_fx.s   ohlc_fx.c
#   stageB_fixed_ohlc  fixed-point   sma_fx.c   ohlc_fx.s
#   stageB_fixed_asm   fixed-point   sma_fx.s   ohlc_fx.s
#
# stageB_inmem.c and stageB_fixed.c are never changed; only the linked
# engine files differ. Every assembly build is checked against the pure-C
# build of its own engine before timing.
#
# Method matches logs/leroi_predictions.md: each timing run is pinned to CPU
# core 2 with "taskset -c 2", and the MEDIAN Process time is reported.
#
# Run on the Raspberry Pi (from any folder):
#   bash assembly/compare_all.sh          (5 runs each)
#   bash assembly/compare_all.sh 10       (10 runs each)

cd "$(dirname "$0")/.." || exit 1

RUNS=${1:-5}
PROGS="stageB_inmem stageB_inmem_sma stageB_inmem_ohlc stageB_inmem_asm \
       stageB_fixed stageB_fixed_sma stageB_fixed_ohlc stageB_fixed_asm"

COMMON="stageB/engine/signal.c stageB/engine/report.c helper_function/trade_io.c"
IM="stageB/stageB_inmem.c stageB/engine/trades.c stageB/engine/vwap.c $COMMON"
FX="stageB/stageB_fixed.c stageB/engine/trades_fx.c stageB/engine/vwap_fx.c \
    stageB/engine/signal_fx.c $COMMON"

# 1) Compile all eight builds
build() {
    out=$1; shift
    gcc -O2 -Wall "$@" -o "stageB/$out" || { echo "build of $out failed"; exit 1; }
}
echo "Compiling 8 builds..."
build stageB_inmem      $IM stageB/engine/sma.c    stageB/engine/ohlc.c
build stageB_inmem_sma  $IM assembly/sma.s         stageB/engine/ohlc.c
build stageB_inmem_ohlc $IM stageB/engine/sma.c    assembly/ohlc.s
build stageB_inmem_asm  $IM assembly/sma.s         assembly/ohlc.s
build stageB_fixed      $FX stageB/engine/sma_fx.c stageB/engine/ohlc_fx.c
build stageB_fixed_sma  $FX assembly/sma_fx.s      stageB/engine/ohlc_fx.c
build stageB_fixed_ohlc $FX stageB/engine/sma_fx.c assembly/ohlc_fx.s
build stageB_fixed_asm  $FX assembly/sma_fx.s      assembly/ohlc_fx.s

# 2) Check each assembly build prints exactly what the pure-C build of the
#    same engine prints (timing lines removed). The double and fixed-point
#    engines are NOT compared with each other: they give different signal
#    counts on purpose (fixed-point is exact, double has rounding).
check() {
    ref=$1; shift
    ./stageB/$ref | grep -v "time:" > /tmp/ref_$ref.txt
    for p in "$@"; do
        if ./stageB/$p | grep -v "time:" | diff -q /tmp/ref_$ref.txt - > /dev/null; then
            echo "  $p: IDENTICAL to $ref"
        else
            echo "  $p: DIFFERENT from $ref - stopping"
            exit 1
        fi
    done
}
echo "Checking outputs..."
check stageB_inmem stageB_inmem_sma stageB_inmem_ohlc stageB_inmem_asm
check stageB_fixed stageB_fixed_sma stageB_fixed_ohlc stageB_fixed_asm

# 3) Time all builds. Runs are interleaved (all 8, then all 8 again, ...) so
#    warm-up or CPU heating affects every build equally. Each run is pinned
#    to core 2 so the OS can't move it between cores mid-run.
PIN=""
if command -v taskset > /dev/null 2>&1; then PIN="taskset -c 2"; fi
echo "Timing $RUNS runs per build, pinned with: ${PIN:-nothing (taskset not found)}"
for prog in $PROGS; do : > /tmp/times_$prog.txt; done
for i in $(seq "$RUNS"); do
    echo "  round $i of $RUNS"
    for prog in $PROGS; do
        $PIN ./stageB/$prog >> /tmp/times_$prog.txt
    done
done

# 4) Per-build detail: median / min / avg / max Process time per dataset.
#    The medians are also saved for the summary table below.
#    (The sort is written out by hand because mawk, the default awk on
#    Raspberry Pi OS, has no sort function.)
: > /tmp/medians.txt
for prog in $PROGS; do
    echo "--- $prog ($RUNS runs) ---"
    awk -v P="$prog" '
        /^=== /{sec=$2} /cannot open|too many trades/{err[sec]=$0}
        /Process time/{t=$3+0; n[sec]++; v[sec, n[sec]]=t; s[sec]+=t}
        END{printf "  %-16s %10s %10s %10s %10s\n", "dataset", "median", "min", "avg", "max";
            split("test20.csv dev_1M.csv stress_full.csv", k, " ");
            for(i=1;i<=3;i++){ d=k[i]; m=n[d];
                if(!m){ printf "  %-16s   not run: %s\n", d, (d in err ? err[d] : "no data"); continue }
                for(j=1;j<=m;j++) a[j]=v[d, j];
                for(j=2;j<=m;j++){ x=a[j]; for(q=j-1; q>=1 && a[q]>x; q--) a[q+1]=a[q]; a[q+1]=x }
                med = (m%2) ? a[(m+1)/2] : (a[m/2]+a[m/2+1])/2;
                printf "  %-16s %10.3f %10.3f %10.3f %10.3f ms\n", d, med, a[1], s[d]/m, a[m];
                print P, d, med >> "/tmp/medians.txt" }}' \
        /tmp/times_$prog.txt
done

# 5) Summary: every build's median, compared with both pure-C builds.
#    Negative % = faster than that baseline.
echo
echo "================ SUMMARY (median Process time) ================"
awk -v order="$PROGS" '
    { med[$1, $2] = $3 }
    END{ np = split(order, p, " ");
         split("dev_1M.csv stress_full.csv", k, " ");
         for(i=1;i<=2;i++){ d=k[i];
             if(!((p[1], d) in med)) continue;
             bi = med["stageB_inmem", d]; bf = med["stageB_fixed", d];
             printf "\n%s\n", d;
             printf "  %-18s %12s %16s %16s\n", "build", "median", "vs inmem (C)", "vs fixed (C)";
             for(j=1;j<=np;j++){ m = med[p[j], d];
                 printf "  %-18s %9.3f ms %+15.1f%% %+15.1f%%\n", p[j], m, (m/bi-1)*100, (m/bf-1)*100 }
             printf "  pure C: fixed-point is %.2fx faster than double\n", bi/bf } }' \
    /tmp/medians.txt
