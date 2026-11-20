# plot_question_a.gnuplot - Stage 0 question a: loop time at each -O level
# Reads results_<machine>.dat (median ns per flag) and writes one chart per machine.
# Run from stage0/question_a:
#   gnuplot -e "machine='laptop'" plot_question_a.gnuplot   -> ../question_c/windows_chart_a.png
#   gnuplot -e "machine='pi'"     plot_question_a.gnuplot   -> ../question_c/rpi_chart_a.png
# Both machines side by side: gnuplot compare_question_a.gnuplot -> ../question_c/compare_chart_a.png

if (!exists("machine")) machine = "laptop"
datafile = sprintf("results_%s.dat", machine)
name     = (machine eq "pi") ? "Raspberry Pi 5" : "Laptop"

# -O0 medians (first data row) are the baseline for the speedup labels
stats datafile using 2 every ::0::0 nooutput
base_user = STATS_min
stats datafile using 3 every ::0::0 nooutput
base_fixed = STATS_min

set terminal pngcairo size 1000,600 font ",11" background "#fcfcfb"
set output (machine eq "pi") ? "../question_c/rpi_chart_a.png" : "../question_c/windows_chart_a.png"
set title sprintf("Sum of 1..N, N = 100,000,000 (%s, median of 5 runs)", name)

set xlabel "gcc optimisation flag"
set ylabel "Time (ms)"
set xrange [-0.6:3.6]
set yrange [0:*]
set offsets 0, 0, graph 0.15, 0      # headroom for the labels above the bars
set grid ytics
set key top right
set boxwidth 0.36
set style fill solid 1.0 border rgb "#fcfcfb"   # same fill as question_c/charts.gnuplot

# label = time and speedup vs -O0; a time under 1 us means the loop was removed
lbl(t, base) = (t < 1000) ? "< 0.1 us\nloop removed" : sprintf("%.1f ms\n%.2fx", t/1e6, base/t)

plot datafile using 0:(NaN):xtic(1) notitle, \
     ''       using ($0-0.19):($2/1e6) with boxes lc rgb "#2a78d6" title "N entered by user (a\\_loop\\_sum.c)", \
     ''       using ($0+0.19):($3/1e6) with boxes lc rgb "#eb6834" title "N fixed in program (a\\_loop\\_sum\\_fixed.c)", \
     ''       using ($0-0.19):($2/1e6):(lbl($2, base_user))  with labels offset 0,1.6 notitle, \
     ''       using ($0+0.19):($3/1e6):(lbl($3, base_fixed)) with labels offset 0,1.6 tc rgb "#c05621" notitle
