# Draws the efficiency ladder: speedup of every version against Version 0
# Columns of results/ladder.dat: 1 version name  2 speedup
# Written by run_report.sh; to redraw by hand:  gnuplot tools/plot_ladder.gnuplot
set terminal pngcairo size 1000,600 enhanced
set output "results/ladder.png"
set title "Efficiency ladder: speedup against Version 0"
set xlabel "version"
set ylabel "speedup (Version 0 time / version time)"
set grid ytics
set style fill solid 0.8
set boxwidth 0.6
set yrange [0:*]
unset key

plot "results/ladder.dat" using 0:2:xtic(1) with boxes lc rgb "#1f77b4", \
     "" using 0:2:(sprintf("%.1fx", $2)) with labels offset 0,0.7
