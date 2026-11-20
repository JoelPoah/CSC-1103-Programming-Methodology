# compare_question_a.gnuplot - Stage 0 question a: Windows laptop vs Raspberry Pi 5
# Compares a_loop_sum.c (N entered by user) using the medians in
# results_laptop.dat and results_pi.dat. Run from stage0/question_a:
#   gnuplot compare_question_a.gnuplot   -> ../question_c/compare_chart_a.png

# read the 4 medians (-O0..-O3, column 2) from each machine's data file
array WIN[4]
array PI[4]
do for [i=1:4] {
    stats "results_laptop.dat" using 2 every ::(i-1)::(i-1) nooutput
    WIN[i] = STATS_min
    stats "results_pi.dat" using 2 every ::(i-1)::(i-1) nooutput
    PI[i] = STATS_min
}

set terminal pngcairo size 1400,620 font ",11" background "#fcfcfb"
set output "../question_c/compare_chart_a.png"
set multiplot layout 1,2 title "Sum of 1..N, N = 100,000,000, N entered by user (a\\_loop\\_sum.c), median of 5 runs"

win_col = "#2a78d6"     # same blue as question_c/charts.gnuplot
pi_col  = "#eb6834"     # same orange as question_c/charts.gnuplot
pi_text = "#c05621"     # darker orange so label text stays readable
set xtics ("-O0" 1, "-O1" 2, "-O2" 3, "-O3" 4)
set xrange [0.4:4.6]
set xlabel "gcc optimisation flag"
set grid ytics
set key top right
set boxwidth 0.36
set style fill solid 1.0 border rgb "#fcfcfb"   # same fill as question_c/charts.gnuplot
set offsets 0, 0, graph 0.15, 0      # headroom for the labels above the bars

# left: time per flag, and how many times slower the Pi is
set title "Time (lower is faster)"
set ylabel "Time (ms)"
set yrange [0:*]
plot WIN using ($1-0.19):($2/1e6) with boxes lc rgb win_col title "Windows laptop (Core Ultra 7 258V)", \
     PI  using ($1+0.19):($2/1e6) with boxes lc rgb pi_col  title "Raspberry Pi 5 (Cortex-A76)", \
     WIN using ($1-0.19):($2/1e6):(sprintf("%.1f ms", $2/1e6)) with labels offset -0.6,0.8 notitle, \
     PI  using ($1+0.19):($2/1e6):(sprintf("%.1f ms\n%.1fx slower", $2/1e6, $2/WIN[int($1)])) \
         with labels offset 0,1.6 tc rgb pi_text notitle

# right: speedup of each flag against the same machine's -O0
set title "Speedup vs -O0 on the same machine (higher is better)"
set ylabel "Speedup (x)"
set yrange [0:*]
plot WIN using ($1-0.19):(WIN[1]/$2) with boxes lc rgb win_col title "Windows laptop", \
     PI  using ($1+0.19):(PI[1]/$2)  with boxes lc rgb pi_col  title "Raspberry Pi 5", \
     WIN using ($1-0.19):(WIN[1]/$2):(sprintf("%.2fx", WIN[1]/$2)) with labels offset 0,0.8 notitle, \
     PI  using ($1+0.19):(PI[1]/$2):(sprintf("%.2fx", PI[1]/$2)) with labels offset 0,0.8 tc rgb pi_text notitle

unset multiplot
