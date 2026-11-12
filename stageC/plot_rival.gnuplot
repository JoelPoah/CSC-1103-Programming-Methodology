# plot_rival.gnuplot - money lost to slow code vs the rival's reaction time
# Run from the project root after stageC_rival:
#   gnuplot stageC/plot_rival.gnuplot      -> stageC/rival_sweep.png
set terminal pngcairo size 1100,700
set output "stageC/rival_sweep.png"

set multiplot layout 2,1 title "Rival bot sweep (stress\\_full.csv, windows 35/60)"

set logscale x 10
set grid
set xlabel "Rival reaction time R (us)"
set key top left

# top: money lost = P&L(fastest) - P&L(Version 0)
set ylabel "Money lost (USDT)"
plot "stageC/rival_sweep.dat" using 1:4 with linespoints lw 2 pt 7 title "money lost = P&L(mem) - P&L(v0)" noenhanced

# bottom: how often each version beats the rival
set ylabel "Signals won vs rival (%)"
set yrange [0:105]
plot "stageC/rival_sweep.dat" using 1:5 with linespoints lw 2 title "v0 (Version 0)", \
     ""                       using 1:6 with linespoints lw 2 title "mem (fastest)"

unset multiplot
